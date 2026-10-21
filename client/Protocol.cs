// Protocol v0 framing and VIDEO payload parsing (docs/protocol.md).
//
// Little-endian, unpadded, transport independent.  The engine side lives in
// engine/asm/swreg/src/seam.asm; this is the client half.  Unknown families are counted and
// skipped (forward compatibility), malformed records are an error.
//
// A live link can be joined mid-message (DOSBox-X drops what the guest wrote
// before a client connected), so the reader resynchronises on the 'YT' magic
// instead of failing, and a decoded message that looks wrong asks for another
// resync - a payload can contain the two magic bytes by accident.

using System.Buffers.Binary;

namespace Yt2.Client;

public enum Family : byte
{
    Video = 1,
    Audio = 2,
    Input = 3,
    Tick = 4,
    Control = 5,
    Files = 6,
}

public enum VideoMessage : byte
{
    SetMode = 0,
    Palette = 1,
    Rect = 2,
    FrameEnd = 3,
    Reset = 4,
}

public enum ControlMessage : byte
{
    Hello = 0,
    Bye = 1,
    Error = 2,
    Debug = 3,
    KeyframeRequest = 4,
}

public enum InputMessage : byte
{
    Key = 0,
    MouseMove = 1,
    MouseButton = 2,
    Quit = 3,
}

public readonly record struct FrameHeader(
    byte Version,
    Family Family,
    byte Flags,
    byte Codec,
    uint Sequence,
    uint Length);

/// <summary>Reads framed v0 records from a stream.</summary>
public sealed class FrameReader(Stream stream)
{
    public const int HeaderSize = 14;
    public const ushort Magic = 0x5954;
    // v0 video messages are at most 14+786 bytes; anything larger is a false
    // boundary in payload data, not a record.
    private const uint MaxPayload = 2048;

    private readonly byte[] _header = new byte[HeaderSize];
    private byte[] _payload = new byte[1024];
    private bool _forceResync;
    private bool _haveSequence;
    private uint _nextSequence;

    public long BytesRead { get; private set; }
    public long Resyncs { get; private set; }
    public long ResyncBytes { get; private set; }
    public long SequenceGaps { get; private set; }

    /// <summary>
    /// Bytes of a frame that were cut off when the stream ended (0 when the
    /// stream ended on a record boundary).  The engine is killed at the end of
    /// a run, so the last frame can be incomplete; that is the end of the
    /// stream, not a broken link.
    /// </summary>
    public int TruncatedBytes { get; private set; }

    /// <summary>
    /// Asks the next read to scan for a message boundary instead of trusting
    /// the current position (set after a decoded message looked wrong).
    /// </summary>
    public void RequestResync()
    {
        _forceResync = true;
        _haveSequence = false;
    }

    /// <summary>Reads the next frame; false on a clean end of stream.</summary>
    public bool TryRead(out FrameHeader header, out byte[] payload, out int length)
    {
        payload = _payload;
        length = 0;
        while (true)
        {
            if (!TryReadHeader(out header))
            {
                return false;
            }
            if (header.Length > _payload.Length)
            {
                _payload = new byte[Math.Max(header.Length, _payload.Length * 2)];
                payload = _payload;
            }
            int want = (int)header.Length;
            if (want > 0 && !ReadExactly(_payload, 0, want))
            {
                // The stream ended inside a frame: the sender was cut off
                // mid-record.  Report it as the end of the stream instead of
                // a broken link.
                TruncatedBytes = want;
                return false;
            }
            length = want;
            return true;
        }
    }

    private bool TryReadHeader(out FrameHeader header)
    {
        header = default;
        while (true)
        {
            if (!ReadExactly(_header, 0, 2))
            {
                return false;
            }

            bool positioned = BinaryPrimitives.ReadUInt16LittleEndian(_header) == Magic;
            if (!positioned)
            {
                Resyncs++;
                _forceResync = false;
                if (!ScanForMagic())
                {
                    return false;
                }
            }
            else if (_forceResync)
            {
                _forceResync = false;
            }

            if (!ReadExactly(_header, 2, HeaderSize - 2))
            {
                return false;
            }

            var candidate = new FrameHeader(
                _header[2],
                (Family)_header[3],
                _header[4],
                _header[5],
                BinaryPrimitives.ReadUInt32LittleEndian(_header.AsSpan(6)),
                BinaryPrimitives.ReadUInt32LittleEndian(_header.AsSpan(10)));

            // Reject headers that cannot be v0 video records: the payload cap
            // rules out all but one in a couple of billion accidental 'YT'
            // pairs, and a resync may legitimately skip whole messages, so the
            // sequence number is only reported, never used to reject.
            if (candidate.Version != 0 || candidate.Length > MaxPayload ||
                candidate.Family is < Family.Video or > Family.Files)
            {
                Resyncs++;
                continue;
            }

            if (_haveSequence && candidate.Sequence != _nextSequence)
            {
                SequenceGaps++;
            }
            _haveSequence = true;
            _nextSequence = candidate.Sequence + 1;
            header = candidate;
            return true;
        }
    }

    /// <summary>Slides a two-byte window forward until it spells the magic.</summary>
    private bool ScanForMagic()
    {
        while (true)
        {
            byte previous = _header[1];
            int next = stream.ReadByte();
            if (next < 0)
            {
                return false;
            }
            BytesRead++;
            ResyncBytes++;
            _header[0] = previous;
            _header[1] = (byte)next;
            if (BinaryPrimitives.ReadUInt16LittleEndian(_header) == Magic)
            {
                return true;
            }
        }
    }

    private bool ReadExactly(byte[] buffer, int offset, int count)
    {
        while (count > 0)
        {
            int n = stream.Read(buffer, offset, count);
            if (n <= 0)
            {
                return false;
            }
            offset += n;
            count -= n;
            BytesRead += n;
        }
        return true;
    }
}

/// <summary>Writes framed v0 records (the client-to-engine direction).</summary>
/// <remarks>
/// The engine side parses these in <c>engine/asm/swreg/src/seam.asm</c>; the
/// layouts are in <c>docs/protocol.md</c>.  Sequence numbers are per direction,
/// so this writer counts its own.  Every message is flushed immediately: the
/// engine polls, and a key that sits in a buffer until the next frame is a key
/// the player notices.
/// </remarks>
public sealed class FrameWriter(Stream stream)
{
    private uint _sequence;

    /// <summary>Set after a failed write: the link is gone and the client reconnects.</summary>
    public bool Dead { get; private set; }

    public void Write(Family family, ReadOnlySpan<byte> payload)
    {
        if (Dead)
        {
            return;
        }
        Span<byte> header = stackalloc byte[FrameReader.HeaderSize];
        BinaryPrimitives.WriteUInt16LittleEndian(header, FrameReader.Magic);
        header[2] = 0;                     // protocol version
        header[3] = (byte)family;
        header[4] = 0;                     // flags
        header[5] = 0;                     // codec raw
        BinaryPrimitives.WriteUInt32LittleEndian(header[6..], _sequence++);
        BinaryPrimitives.WriteUInt32LittleEndian(header[10..], (uint)payload.Length);
        try
        {
            stream.Write(header);
            stream.Write(payload);
            stream.Flush();
        }
        catch (Exception ex) when (ex is IOException or ObjectDisposedException)
        {
            // A dead socket surfaces on the next write, whichever thread gets
            // there first; the reader reports the loss, and a windowed client
            // must survive it instead of dying in a callback.
            Dead = true;
        }
    }

    /// <summary>hello(role = client, version = 0): the engine answers with its own.</summary>
    public void Hello() => Write(Family.Control, [0, 1, 0]);

    /// <summary>keyframe_request: set_mode, the full palette and a full-frame pass.</summary>
    public void KeyframeRequest() => Write(Family.Control, [4]);

    public void Bye() => Write(Family.Control, [1]);

    /// <summary>key(down = true, scancode, ascii); ASCII 0 means an extended key.</summary>
    public void Key(byte scan, byte ascii) => Write(Family.Input, [0, 1, scan, ascii]);

    /// <summary>mouse_move(dx, dy), relative and in game pixels.</summary>
    public void MouseMove(short dx, short dy)
    {
        Span<byte> payload = stackalloc byte[5];
        payload[0] = 1;
        BinaryPrimitives.WriteInt16LittleEndian(payload[1..], dx);
        BinaryPrimitives.WriteInt16LittleEndian(payload[3..], dy);
        Write(Family.Input, payload);
    }

    /// <summary>mouse_abs(x, y): absolute pointer position in game pixels.</summary>
    public void MouseAbs(ushort x, ushort y)
    {
        Span<byte> payload = stackalloc byte[5];
        payload[0] = 3;
        BinaryPrimitives.WriteUInt16LittleEndian(payload[1..], x);
        BinaryPrimitives.WriteUInt16LittleEndian(payload[3..], y);
        Write(Family.Input, payload);
    }

    /// <summary>mouse_button(button, down): 0 left, 1 middle, 2 right.</summary>
    public void MouseButton(byte button, bool down) =>
        Write(Family.Input, [2, button, down ? (byte)1 : (byte)0]);
}
