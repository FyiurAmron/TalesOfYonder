// DOSBox-X's nullmodem link escapes every 0xFF byte on the wire (it is sent
// twice, telnet style - see CNullModem::transmitByte in nullmodem.cpp).  The
// emulator does the unescaping on its own receive side, so a peer has to do
// the same: this wrapper turns the link back into a raw byte stream.
//
// OBSOLETE: the client speaks the folder link (--follow/--input), and a
// shared directory crosses a machine boundary just as well.  The serial
// transport is kept only for fun.
//
// This is a deliberate DOSBox feature, not a bug: binary transfers over a
// nullmodem port are expected to cope with it.  `transparent:1` in the
// serial1 line turns both the escaping and telnet handling off - note the
// value must be numeric (`transparent:true` is silently ignored) - and a link
// configured that way needs --raw instead of this wrapper.  Recordings from
// the file backend are raw either way.

namespace Yt2.Client;

public sealed class NullModemStream(Stream inner) : Stream
{
    private readonly byte[] _raw = new byte[4096];
    private readonly byte[] _one = new byte[1];
    private int _rawLength;
    private int _rawOffset;
    private bool _escapePending;

    /// <summary>A 0xFF followed by something else is a telnet command, not data.</summary>
    public long SkippedCommands { get; private set; }

    public override bool CanRead => true;
    public override bool CanSeek => false;
    public override bool CanWrite => true;
    public override long Length => throw new NotSupportedException();

    public override long Position
    {
        get => throw new NotSupportedException();
        set => throw new NotSupportedException();
    }

    public override int Read(byte[] buffer, int offset, int count)
    {
        while (true)
        {
            if (_rawOffset >= _rawLength)
            {
                _rawLength = inner.Read(_raw, 0, _raw.Length);
                _rawOffset = 0;
                if (_rawLength <= 0)
                {
                    return 0;
                }
            }

            int written = 0;
            while (_rawOffset < _rawLength && written < count)
            {
                byte b = _raw[_rawOffset++];
                if (_escapePending)
                {
                    _escapePending = false;
                    if (b == 0xFF)
                    {
                        buffer[offset + written++] = 0xFF;
                    }
                    else
                    {
                        SkippedCommands++;
                    }
                    continue;
                }

                if (b == 0xFF)
                {
                    _escapePending = true;      // the next byte decides
                    continue;
                }

                buffer[offset + written++] = b;
            }

            if (written > 0)
            {
                return written;
            }
            // only escape bytes so far: read on
        }
    }

    public override int ReadByte()
    {
        int n = Read(_one, 0, 1);
        return n == 0 ? -1 : _one[0];
    }

    public override void Write(byte[] buffer, int offset, int count)
    {
        for (int i = 0; i < count; i++)
        {
            byte b = buffer[offset + i];
            if (b == 0xFF)
            {
                inner.WriteByte(0xFF);          // escaped: 0xFF arrives as 0xFF 0xFF
            }
            inner.WriteByte(b);
        }
    }

    public override void WriteByte(byte value) => Write([value], 0, 1);

    public override void Flush() => inner.Flush();

    public override long Seek(long offset, SeekOrigin origin) => throw new NotSupportedException();

    public override void SetLength(long value) => throw new NotSupportedException();
}
