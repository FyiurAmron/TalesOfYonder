// Timed records used by the folder link. STREAM.BIN and INPUT.BIN keep the
// v0 protocol frame intact, with a PIT timestamp and frame length per record.

using System.Buffers.Binary;
using System.Diagnostics;

namespace Yt2.Client;

internal enum TimedBinDirection : byte
{
    EngineToClient = 0,
    ClientToEngine = 1,
}

internal readonly record struct TimedBinEvent(uint PitStamp, byte[] Frame);

internal static class TimedBinFormat
{
    public const int HeaderSize = 16;
    public const int EventHeaderSize = 8;
    public const uint PitRate = 1_193_182;
    public const int MaxFrame = FrameReader.HeaderSize + 2048;

    public static bool HasHeader(string path)
    {
        using var file = new FileStream(path, FileMode.Open, FileAccess.Read,
                                        FileShare.ReadWrite | FileShare.Delete);
        Span<byte> magic = stackalloc byte[4];
        return ReadUpTo(file, magic) == magic.Length && magic.SequenceEqual("YT2B"u8);
    }

    public static bool TryReadFileHeader(FileStream file, TimedBinDirection expected)
    {
        long position = file.Position;
        file.Seek(0, SeekOrigin.Begin);
        Span<byte> header = stackalloc byte[HeaderSize];
        int read = ReadUpTo(file, header);
        file.Seek(position, SeekOrigin.Begin);
        if (read < 4 || !header[..4].SequenceEqual("YT2B"u8))
        {
            return false;
        }
        if (read != HeaderSize)
        {
            throw new InvalidDataException("short timed BIN header");
        }
        ValidateHeader(header, expected);
        return true;
    }

    public static void WriteHeader(Stream stream, TimedBinDirection direction)
    {
        Span<byte> header = stackalloc byte[HeaderSize];
        "YT2B"u8.CopyTo(header);
        BinaryPrimitives.WriteUInt16LittleEndian(header[4..], 1);
        BinaryPrimitives.WriteUInt16LittleEndian(header[6..], HeaderSize);
        BinaryPrimitives.WriteUInt32LittleEndian(header[8..], PitRate);
        header[12] = (byte)direction;
        header[13] = header[14] = header[15] = 0;
        stream.Write(header);
    }

    public static void ValidateHeader(ReadOnlySpan<byte> header, TimedBinDirection expected)
    {
        if (header.Length != HeaderSize || !header[..4].SequenceEqual("YT2B"u8) ||
            BinaryPrimitives.ReadUInt16LittleEndian(header[4..]) != 1 ||
            BinaryPrimitives.ReadUInt16LittleEndian(header[6..]) != HeaderSize ||
            BinaryPrimitives.ReadUInt32LittleEndian(header[8..]) != PitRate ||
            header[12] != (byte)expected || header[13] != 0 || header[14] != 0 || header[15] != 0)
        {
            throw new InvalidDataException("unsupported timed BIN header");
        }
    }

    public static ulong PitDeltaNanoseconds(uint delta) =>
        (ulong)delta * 1_000_000_000UL / PitRate;

    private static int ReadUpTo(Stream stream, Span<byte> buffer)
    {
        int total = 0;
        while (total < buffer.Length)
        {
            int count = stream.Read(buffer[total..]);
            if (count == 0)
            {
                break;
            }
            total += count;
        }
        return total;
    }
}

/// <summary>
/// Stamps client-to-engine records on one monotonic PIT timeline.  The clock
/// runs from the source's creation; the first observed server stamp seeds the
/// epoch when no input has been written yet.  Once a frame has been issued the
/// epoch must not jump: the handshake is usually written before the first
/// output frame arrives, and re-anchoring later would turn the gap into a
/// multi-second delay when the input file is replayed.
/// </summary>
internal sealed class PitClock
{
    private readonly object _sync = new();
    private readonly long _started = Stopwatch.GetTimestamp();
    private uint _basePit;
    private uint _lastIssued;
    private bool _hasBase;
    private bool _hasIssued;

    public void Observe(uint pitStamp)
    {
        lock (_sync)
        {
            if (_hasBase)
            {
                return;
            }
            _hasBase = true;
            // Align to the server only when no client record has been written
            // yet; otherwise keep the epoch the first records were written on.
            if (!_hasIssued)
            {
                _basePit = unchecked(pitStamp - ElapsedPit(Stopwatch.GetTimestamp()));
            }
        }
    }

    public uint Next()
    {
        lock (_sync)
        {
            uint now = unchecked(_basePit + ElapsedPit(Stopwatch.GetTimestamp()));
            if (_hasIssued && unchecked((int)(now - _lastIssued)) < 0)
            {
                now = _lastIssued;
            }
            _lastIssued = now;
            _hasIssued = true;
            return now;
        }
    }

    private uint ElapsedPit(long now)
    {
        long elapsed = now - _started;
        ulong delta = (ulong)elapsed * TimedBinFormat.PitRate /
                      (ulong)Stopwatch.Frequency;
        return unchecked((uint)delta);
    }
}

internal sealed class TimedBinReader : IDisposable
{
    private readonly FileStream _file;
    private readonly TimedBinDirection _direction;

    public TimedBinReader(string path, TimedBinDirection direction)
    {
        _file = File.OpenRead(path);
        _direction = direction;
        Span<byte> header = stackalloc byte[TimedBinFormat.HeaderSize];
        ReadExactly(_file, header, "short timed BIN header");
        TimedBinFormat.ValidateHeader(header, direction);
    }

    public bool TryRead(out TimedBinEvent item)
    {
        item = default;
        Span<byte> eventHeader = stackalloc byte[TimedBinFormat.EventHeaderSize];
        int first = _file.ReadByte();
        if (first < 0)
        {
            return false;
        }
        eventHeader[0] = (byte)first;
        if (!TryReadTail(_file, eventHeader[1..]))
        {
            return false;
        }
        uint stamp = BinaryPrimitives.ReadUInt32LittleEndian(eventHeader);
        uint length = BinaryPrimitives.ReadUInt32LittleEndian(eventHeader[4..]);
        if (length < FrameReader.HeaderSize || length > TimedBinFormat.MaxFrame)
        {
            throw new InvalidDataException("invalid timed BIN frame length");
        }
        var frame = new byte[(int)length];
        if (!TryReadTail(_file, frame))
        {
            return false;
        }
        ValidateFrame(frame, _direction);
        item = new TimedBinEvent(stamp, frame);
        return true;
    }

    public void Dispose() => _file.Dispose();

    internal static void ValidateFrame(ReadOnlySpan<byte> frame, TimedBinDirection direction)
    {
        if (BinaryPrimitives.ReadUInt16LittleEndian(frame) != FrameReader.Magic || frame[2] != 0 ||
            frame[3] is < (byte)Family.Video or > (byte)Family.Files ||
            BinaryPrimitives.ReadUInt32LittleEndian(frame[10..]) != frame.Length - FrameReader.HeaderSize ||
            (direction == TimedBinDirection.ClientToEngine &&
             frame[3] is not ((byte)Family.Input) and not ((byte)Family.Control)))
        {
            throw new InvalidDataException("invalid v0 frame in timed BIN");
        }
    }

    internal static bool TryReadTail(Stream stream, Span<byte> bytes)
    {
        while (!bytes.IsEmpty)
        {
            int read = stream.Read(bytes);
            if (read == 0)
            {
                return false;
            }
            bytes = bytes[read..];
        }
        return true;
    }

    private static void ReadExactly(Stream stream, Span<byte> bytes, string message)
    {
        if (!TryReadTail(stream, bytes))
        {
            throw new InvalidDataException(message);
        }
    }
}

/// <summary>Unwraps timed engine records into a protocol byte stream.</summary>
internal sealed class TimedBinFrameStream : Stream
{
    private readonly TimedBinReader _reader;
    private readonly bool _pace;
    private readonly long _started = Stopwatch.GetTimestamp();
    private uint _origin;
    private bool _hasOrigin;
    private byte[]? _frame;
    private int _offset;
    private bool _disposed;

    public TimedBinFrameStream(string path, bool pace)
    {
        _reader = new TimedBinReader(path, TimedBinDirection.EngineToClient);
        _pace = pace;
    }

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
                return 0;
            }
            if (!_hasOrigin)
            {
                _origin = item.PitStamp;
                _hasOrigin = true;
            }
            if (_pace)
            {
                WaitUntil(TimedBinFormat.PitDeltaNanoseconds(unchecked(item.PitStamp - _origin)));
            }
            _frame = item.Frame;
            _offset = 0;
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
            ulong elapsed = checked((ulong)(Stopwatch.GetElapsedTime(_started).Ticks * 100));
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

/// <summary>Wraps each complete client protocol frame as an INPUT.BIN record.</summary>
internal sealed class TimedBinWriterStream(Stream destination, PitClock clock) : Stream
{
    private bool _disposed;

    public override void Write(byte[] buffer, int offset, int count)
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
            return;
        }
        if (count < FrameReader.HeaderSize)
        {
            throw new InvalidDataException("short frame written to timed BIN transport");
        }
        TimedBinReader.ValidateFrame(buffer.AsSpan(offset, count), TimedBinDirection.ClientToEngine);
        byte[] record = new byte[TimedBinFormat.EventHeaderSize + count];
        BinaryPrimitives.WriteUInt32LittleEndian(record, clock.Next());
        BinaryPrimitives.WriteUInt32LittleEndian(record.AsSpan(4), (uint)count);
        buffer.AsSpan(offset, count).CopyTo(record.AsSpan(TimedBinFormat.EventHeaderSize));
        destination.Write(record);
    }

    public override void Flush() => destination.Flush();
    public override bool CanRead => false;
    public override bool CanSeek => false;
    public override bool CanWrite => !_disposed;
    public override long Length => throw new NotSupportedException();
    public override long Position
    {
        get => throw new NotSupportedException();
        set => throw new NotSupportedException();
    }
    public override int Read(byte[] buffer, int offset, int count) => throw new NotSupportedException();
    public override long Seek(long offset, SeekOrigin origin) => throw new NotSupportedException();
    public override void SetLength(long value) => throw new NotSupportedException();

    protected override void Dispose(bool disposing)
    {
        if (disposing && !_disposed)
        {
            _disposed = true;
            destination.Dispose();
        }
        base.Dispose(disposing);
    }
}
