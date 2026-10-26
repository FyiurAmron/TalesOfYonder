// Full-duplex, time-stamped session capture for the asm server/client boundary.

using System.Buffers.Binary;
using System.Diagnostics;

namespace Yt2.Client;

internal enum SessionDirection : byte
{
    EngineToClient = 0,
    ClientToEngine = 1,
}

internal readonly record struct SessionEvent(
    ulong TimeNanoseconds, SessionDirection Direction, byte[] Frame);

internal static class SessionFrame
{
    public static byte[] Encode(FrameHeader header, ReadOnlySpan<byte> payload)
    {
        var frame = new byte[FrameReader.HeaderSize + payload.Length];
        BinaryPrimitives.WriteUInt16LittleEndian(frame, FrameReader.Magic);
        frame[2] = header.Version;
        frame[3] = (byte)header.Family;
        frame[4] = header.Flags;
        frame[5] = header.Codec;
        BinaryPrimitives.WriteUInt32LittleEndian(frame.AsSpan(6), header.Sequence);
        BinaryPrimitives.WriteUInt32LittleEndian(frame.AsSpan(10), (uint)payload.Length);
        payload.CopyTo(frame.AsSpan(FrameReader.HeaderSize));
        return frame;
    }
}

internal sealed class SessionRecorder : IDisposable
{
    private const ulong Timebase = 1_000_000_000;
    private readonly object _sync = new();
    private readonly FileStream _file;
    private long _started;
    private bool _hasOrigin;
    private bool _disposed;

    public SessionRecorder(string path)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(path))!);
        _file = File.Create(path);
        Span<byte> header = stackalloc byte[16];
        "YT2S"u8.CopyTo(header);
        BinaryPrimitives.WriteUInt16LittleEndian(header[4..], 1);
        BinaryPrimitives.WriteUInt16LittleEndian(header[6..], 16);
        BinaryPrimitives.WriteUInt64LittleEndian(header[8..], Timebase);
        _file.Write(header);
        _file.Flush();
        Console.WriteLine($"client: recording timed session {Path.GetFullPath(path)}");
    }

    public void Record(SessionDirection direction, byte[] frame)
    {
        lock (_sync)
        {
            ObjectDisposedException.ThrowIf(_disposed, this);
            long now = Stopwatch.GetTimestamp();
            if (!_hasOrigin)
            {
                _started = now;
                _hasOrigin = true;
            }
            ulong time = checked((ulong)(Stopwatch.GetElapsedTime(_started, now).Ticks * 100));
            Span<byte> record = stackalloc byte[16];
            BinaryPrimitives.WriteUInt64LittleEndian(record, time);
            record[8] = (byte)direction;
            record[9] = record[10] = record[11] = 0;
            BinaryPrimitives.WriteUInt32LittleEndian(record[12..], (uint)frame.Length);
            _file.Write(record);
            _file.Write(frame);
            _file.Flush();
        }
    }

    public void Dispose()
    {
        lock (_sync)
        {
            if (_disposed)
            {
                return;
            }
            _disposed = true;
            _file.Dispose();
        }
    }
}

internal sealed class SessionReader : IDisposable
{
    private const int HeaderSize = 16;
    private const int EventHeaderSize = 16;
    private const uint MaxFrame = FrameReader.HeaderSize + 2048;
    private readonly FileStream _file;
    private ulong _lastTime;
    private bool _haveTime;

    public SessionReader(string path)
    {
        _file = File.OpenRead(path);
        Span<byte> header = stackalloc byte[HeaderSize];
        ReadExactly(_file, header, "short YT2S header");
        if (!header[..4].SequenceEqual("YT2S"u8) ||
            BinaryPrimitives.ReadUInt16LittleEndian(header[4..]) != 1 ||
            BinaryPrimitives.ReadUInt16LittleEndian(header[6..]) != HeaderSize ||
            BinaryPrimitives.ReadUInt64LittleEndian(header[8..]) != 1_000_000_000)
        {
            throw new InvalidDataException("unsupported YT2S session header");
        }
    }

    public ulong LastTimeNanoseconds { get; private set; }

    public static bool IsSessionFile(string path)
    {
        using var file = File.OpenRead(path);
        Span<byte> magic = stackalloc byte[4];
        int read = 0;
        while (read < magic.Length)
        {
            int count = file.Read(magic[read..]);
            if (count == 0)
            {
                return false;
            }
            read += count;
        }
        return magic.SequenceEqual("YT2S"u8);
    }

    public bool TryRead(out SessionEvent sessionEvent)
    {
        sessionEvent = default;
        Span<byte> record = stackalloc byte[EventHeaderSize];
        int first = _file.ReadByte();
        if (first < 0)
        {
            return false;
        }
        record[0] = (byte)first;
        ReadExactly(_file, record[1..], "truncated YT2S event header");
        ulong time = BinaryPrimitives.ReadUInt64LittleEndian(record);
        var direction = (SessionDirection)record[8];
        uint length = BinaryPrimitives.ReadUInt32LittleEndian(record[12..]);
        if (direction is not (SessionDirection.EngineToClient or SessionDirection.ClientToEngine) ||
            record[9] != 0 || record[10] != 0 || record[11] != 0 ||
            length < FrameReader.HeaderSize || length > MaxFrame ||
            (_haveTime && time < _lastTime))
        {
            throw new InvalidDataException("invalid or non-monotonic YT2S event");
        }
        var frame = new byte[(int)length];
        ReadExactly(_file, frame, "truncated YT2S frame");
        ValidateFrame(frame);
        _lastTime = LastTimeNanoseconds = time;
        _haveTime = true;
        sessionEvent = new SessionEvent(time, direction, frame);
        return true;
    }

    private static void ValidateFrame(ReadOnlySpan<byte> frame)
    {
        if (BinaryPrimitives.ReadUInt16LittleEndian(frame) != FrameReader.Magic || frame[2] != 0 ||
            frame[3] is < (byte)Family.Video or > (byte)Family.Files ||
            BinaryPrimitives.ReadUInt32LittleEndian(frame[10..]) != frame.Length - FrameReader.HeaderSize)
        {
            throw new InvalidDataException("invalid v0 frame in YT2S session");
        }
    }

    private static void ReadExactly(Stream stream, Span<byte> bytes, string message)
    {
        while (!bytes.IsEmpty)
        {
            int read = stream.Read(bytes);
            if (read <= 0)
            {
                throw new InvalidDataException(message);
            }
            bytes = bytes[read..];
        }
    }

    public void Dispose() => _file.Dispose();
}

internal sealed class SessionOutputStream : Stream
{
    private readonly SessionReader _reader;
    private readonly long _playStarted = Stopwatch.GetTimestamp();
    private byte[]? _frame;
    private int _offset;
    private volatile bool _disposed;

    public SessionOutputStream(string path) => _reader = new SessionReader(path);

    public override int Read(byte[] buffer, int offset, int count)
    {
        ArgumentNullException.ThrowIfNull(buffer);
        ArgumentOutOfRangeException.ThrowIfNegative(offset);
        ArgumentOutOfRangeException.ThrowIfNegative(count);
        if (offset + count > buffer.Length)
        {
            throw new ArgumentException("offset and count exceed the buffer");
        }
        if (count == 0)
        {
            return 0;
        }
        while (_frame is null && !_disposed)
        {
            if (!_reader.TryRead(out var item))
            {
                WaitUntil(_reader.LastTimeNanoseconds);
                return 0;
            }
            if (item.Direction != SessionDirection.EngineToClient)
            {
                continue;
            }
            WaitUntil(item.TimeNanoseconds);
            if (!_disposed)
            {
                _frame = item.Frame;
                _offset = 0;
            }
        }
        if (_disposed || _frame is null)
        {
            return 0;
        }
        int copied = Math.Min(count, _frame.Length - _offset);
        _frame.AsSpan(_offset, copied).CopyTo(buffer.AsSpan(offset));
        _offset += copied;
        if (_offset == _frame.Length)
        {
            _frame = null;
        }
        return copied;
    }

    private void WaitUntil(ulong timeNanoseconds)
    {
        while (!_disposed)
        {
            ulong elapsed = checked((ulong)(Stopwatch.GetElapsedTime(_playStarted).Ticks * 100));
            if (elapsed >= timeNanoseconds)
            {
                return;
            }
            ulong remaining = timeNanoseconds - elapsed;
            Thread.Sleep((int)Math.Clamp(remaining / 1_000_000, 1, 5));
        }
    }

    public override bool CanRead => !_disposed;
    public override bool CanSeek => false;
    public override bool CanWrite => false;
    public override long Length => throw new NotSupportedException();
    public override long Position
    {
        get => throw new NotSupportedException();
        set => throw new NotSupportedException();
    }
    public override void Flush() { }
    public override long Seek(long offset, SeekOrigin origin) => throw new NotSupportedException();
    public override void SetLength(long value) => throw new NotSupportedException();
    public override void Write(byte[] buffer, int offset, int count) => throw new NotSupportedException();

    protected override void Dispose(bool disposing)
    {
        if (disposing && !_disposed)
        {
            _disposed = true;
            _reader.Dispose();
        }
        base.Dispose(disposing);
    }
}

internal static class SessionInputReplay
{
    public static void Run(string path, Stream destination, CancellationToken cancellationToken,
                           long started, Action<byte[]>? onWritten)
    {
        if (TimedBinFormat.HasHeader(path))
        {
            RunTimedBin(path, destination, cancellationToken, started, onWritten);
            return;
        }

        int sent = 0;
        using var reader = new SessionReader(path);
        while (!cancellationToken.IsCancellationRequested && reader.TryRead(out var item))
        {
            if (item.Direction != SessionDirection.ClientToEngine || !IsGameInput(item.Frame))
            {
                continue;
            }
            WaitUntil(started, item.TimeNanoseconds, cancellationToken);
            if (cancellationToken.IsCancellationRequested)
            {
                break;
            }
            destination.Write(item.Frame);
            destination.Flush();
            onWritten?.Invoke(item.Frame);
            sent++;
        }
        Console.WriteLine($"client: replayed {sent} timed input frame(s)");
    }

    private static void RunTimedBin(string path, Stream destination,
                                    CancellationToken cancellationToken, long started,
                                    Action<byte[]>? onWritten)
    {
        int sent = 0;
        uint origin = 0;
        bool hasOrigin = false;
        using var reader = new TimedBinReader(path, TimedBinDirection.ClientToEngine);
        while (!cancellationToken.IsCancellationRequested && reader.TryRead(out var item))
        {
            if (!hasOrigin)
            {
                origin = item.PitStamp;
                hasOrigin = true;
            }
            if (!IsGameInput(item.Frame))
            {
                continue;
            }
            WaitUntil(started, TimedBinFormat.PitDeltaNanoseconds(
                unchecked(item.PitStamp - origin)), cancellationToken);
            if (cancellationToken.IsCancellationRequested)
            {
                break;
            }
            destination.Write(item.Frame);
            destination.Flush();
            onWritten?.Invoke(item.Frame);
            sent++;
        }
        Console.WriteLine($"client: replayed {sent} timed input frame(s)");
    }

    private static bool IsGameInput(ReadOnlySpan<byte> frame) =>
        frame.Length >= FrameReader.HeaderSize + 1 &&
        frame[3] == (byte)Family.Input;

    private static void WaitUntil(long started, ulong timeNanoseconds, CancellationToken token)
    {
        while (!token.IsCancellationRequested)
        {
            ulong elapsed = checked((ulong)(Stopwatch.GetElapsedTime(started).Ticks * 100));
            if (elapsed >= timeNanoseconds)
            {
                return;
            }
            ulong remaining = timeNanoseconds - elapsed;
            int delay = (int)Math.Clamp(remaining / 1_000_000, 1, 5);
            if (token.WaitHandle.WaitOne(delay))
            {
                return;
            }
        }
    }
}
