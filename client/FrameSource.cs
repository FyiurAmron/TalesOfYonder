// Frame sources: the live serial link (a DOSBox-X nullmodem TCP socket) and
// a recorded stream file.  Both read the same v0 framing, but the nullmodem
// link escapes 0xFF (see NullModemStream), so a live link gets that wrapper
// unless --raw says the peer sends raw bytes.

using System.Net.Sockets;

namespace Yt2.Client;

public abstract class FrameSource : IDisposable
{
    public abstract Stream Stream { get; }

    public virtual void Dispose() => Stream.Dispose();

    /// <summary>
    /// Opens the replay file, or connects the live link.  <paramref name="pump"/>
    /// runs while a TCP connect is retried: the window is created first, and
    /// without this it would be frozen for the whole connect timeout (the OS
    /// then calls it "not responding").  A false return cancels the wait.
    /// </summary>
    public static FrameSource Open(Options options, Func<bool>? pump = null)
    {
        // A windowed client retries until the window closes: the server is
        // started by hand, often after the client, and killing DOSBox must not
        // take the client down with it.
        int timeout = pump is null ? options.ConnectTimeoutSeconds : 0;
        return options.Follow is not null
            ? new FollowSource(options.Follow)
            : options.File is not null
                ? new FileSource(options.File)
                : new TcpSource(options.Host, options.Port, timeout, options.Raw, pump);
    }

    private sealed class FollowSource(string path) : FrameSource
    {
        private readonly FollowStream _stream = new(path);

        public override Stream Stream => _stream;
    }

    /// <summary>
    /// Follows a file the engine appends to (the engine owns and creates it).
    /// Readers see the bytes as they land; when the writer truncates the file
    /// to zero (its pseudo-ring wrap), the read position falls past the end and
    /// we rewind to zero.  The engine restarts the stream with a keyframe, so
    /// the caller just keeps decoding.
    /// </summary>
    private sealed class FollowStream(string path) : Stream
    {
        private FileStream? _file;
        private long _lastLength;
        private bool _saidWaiting;

        public override int Read(byte[] buffer, int offset, int count)
        {
            while (true)
            {
                FileStream? file = _file;
                if (file is null)
                {
                    try
                    {
                        // The engine owns STREAM.BIN and creates it at start-up.
                        // Never create it here: a reader that did could keep
                        // DOSBox from opening the file on Windows, which used
                        // to leave the client black with no link at all.
                        _file = file = new FileStream(path, FileMode.Open,
                            FileAccess.Read, FileShare.ReadWrite | FileShare.Delete, 1);
                        Console.Error.WriteLine($"follow: reading {path}");
                    }
                    catch (Exception ex) when (ex is FileNotFoundException or DirectoryNotFoundException)
                    {
                        if (!_saidWaiting)
                        {
                            Console.Error.WriteLine($"follow: waiting for {path}");
                            _saidWaiting = true;
                        }
                        Thread.Sleep(25);
                        continue;
                    }
                    catch (IOException)
                    {
                        // the engine may be creating/truncating it right now
                        Thread.Sleep(25);
                        continue;
                    }
                }
                long length;
                try
                {
                    length = file.Length;
                }
                catch (IOException)
                {
                    Thread.Sleep(5);
                    continue;
                }
                long position = file.Position;
                if (length < position || length < _lastLength)
                {
                    Console.Error.WriteLine(
                        $"follow: file wrapped (pos {position}, len {length}, " +
                        $"last {_lastLength}); restarting at 0");
                    file.Seek(0, SeekOrigin.Begin);
                    _lastLength = 0;
                    continue;
                }
                _lastLength = length;
                if (position < length)
                {
                    int want = (int)Math.Min(count, length - position);
                    int read = file.Read(buffer, offset, want);
                    if (read > 0)
                    {
                        return read;
                    }
                }
                Thread.Sleep(5);
            }
        }

        public override void Flush() => _file?.Flush();
        public override bool CanRead => true;
        public override bool CanSeek => false;
        public override bool CanWrite => false;
        public override long Length => _file?.Length ?? 0;
        public override long Position
        {
            get => _file?.Position ?? 0;
            set => throw new NotSupportedException();
        }
        public override long Seek(long offset, SeekOrigin origin) => throw new NotSupportedException();
        public override void SetLength(long value) => throw new NotSupportedException();
        public override void Write(byte[] buffer, int offset, int count) => throw new NotSupportedException();
        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                _file?.Dispose();
            }
            base.Dispose(disposing);
        }
    }

    /// <summary>
    /// Appends framed input for the engine.  The file is a pseudo-ring: at the
    /// limit it is truncated to zero and writing restarts at the start; the
    /// engine notices the shrink and rewinds.
    /// </summary>
    public sealed class RingFileStream : Stream
    {
        private readonly FileStream _file;
        private readonly long _limit;

        public RingFileStream(string path, long limit)
        {
            _limit = limit;
            _file = new FileStream(path, FileMode.OpenOrCreate, FileAccess.ReadWrite,
                                   FileShare.ReadWrite | FileShare.Delete, 1);
            // a new session starts a fresh log; the engine sees the shrink and
            // rewinds its read position
            _file.SetLength(0);
        }

        public override void Write(byte[] buffer, int offset, int count)
        {
            if (_file.Position + count > _limit)
            {
                _file.SetLength(0);
                _file.Position = 0;
            }
            _file.Write(buffer, offset, count);
            _file.Flush();
        }

        public override void Flush() => _file.Flush();
        public override bool CanRead => false;
        public override bool CanSeek => false;
        public override bool CanWrite => true;
        public override long Length => _file.Length;
        public override long Position
        {
            get => _file.Position;
            set => throw new NotSupportedException();
        }
        public override int Read(byte[] buffer, int offset, int count) => throw new NotSupportedException();
        public override long Seek(long offset, SeekOrigin origin) => throw new NotSupportedException();
        public override void SetLength(long value) => _file.SetLength(value);
        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                _file.Dispose();
            }
            base.Dispose(disposing);
        }
    }

    private sealed class FileSource(string path) : FrameSource
    {
        private readonly FileStream _stream = File.OpenRead(path);

        public override Stream Stream => _stream;
    }

    private sealed class TcpSource : FrameSource
    {
        private readonly TcpClient _client;
        private readonly Stream _stream;

        public TcpSource(string host, int port, int timeoutSeconds, bool raw, Func<bool>? pump)
        {
            _client = Connect(host, port, timeoutSeconds, pump);
            _stream = raw ? _client.GetStream() : new NullModemStream(_client.GetStream());
        }

        public override Stream Stream => _stream;

        private static TcpClient Connect(string host, int port, int timeoutSeconds,
                                         Func<bool>? pump)
        {
            var deadline = timeoutSeconds <= 0
                ? DateTime.MaxValue
                : DateTime.UtcNow.AddSeconds(timeoutSeconds);
            var client = new TcpClient();
            while (true)
            {
                try
                {
                    client.Connect(host, port);
                    client.NoDelay = true;
                    // The engine streams a frame as one burst; a small receive
                    // buffer makes the host side of the link stall, and a
                    // stalled link loses bytes in the emulator's serial FIFO.
                    client.Client.ReceiveBufferSize = 1 << 20;
                    client.Client.SetSocketOption(SocketOptionLevel.Socket,
                                                  SocketOptionName.KeepAlive, true);
                    return client;
                }
                catch (SocketException) when (DateTime.UtcNow < deadline)
                {
                    if (pump is not null && !pump())
                    {
                        throw new OperationCanceledException("window closed while connecting");
                    }
                    Thread.Sleep(50);
                }
            }
        }

        public override void Dispose()
        {
            _client.Dispose();
        }
    }
}
