// A pass-through stream that mirrors everything read from the link into a
// file: `--dump PATH` records exactly what the client received, which is the
// quickest way to tell a decoder bug from a link problem.

namespace Yt2.Client;

public sealed class DumpStream(Stream inner, Stream dump) : Stream
{
    public override bool CanRead => true;
    public override bool CanSeek => false;
    public override bool CanWrite => false;
    public override long Length => throw new NotSupportedException();

    public override long Position
    {
        get => throw new NotSupportedException();
        set => throw new NotSupportedException();
    }

    public override int Read(byte[] buffer, int offset, int count)
    {
        int n = inner.Read(buffer, offset, count);
        if (n > 0)
        {
            dump.Write(buffer, offset, n);
        }
        return n;
    }

    public override int ReadByte()
    {
        int b = inner.ReadByte();
        if (b >= 0)
        {
            dump.WriteByte((byte)b);
        }
        return b;
    }

    public override void Flush() => dump.Flush();

    public override long Seek(long offset, SeekOrigin origin) => throw new NotSupportedException();

    public override void SetLength(long value) => throw new NotSupportedException();

    public override void Write(byte[] buffer, int offset, int count) => throw new NotSupportedException();
}
