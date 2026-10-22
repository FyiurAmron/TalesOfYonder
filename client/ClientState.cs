// Client-side display state: an 8-bit framebuffer replica, a 256-entry palette
// (6-bit DAC values) and the counters the stats line reports.  The reader
// thread writes, the render loop reads; a single lock keeps it simple.

using System.Buffers.Binary;
using System.Text;

namespace Yt2.Client;

public sealed class ClientStats
{
    public long Frames;
    public long Records;
    public long Rects;
    public long Scrolls;
    public long PaletteUpdates;
    public long ControlMessages;
    public long EngineErrors;
    public long UnknownFamilies;
    public long UnknownMessages;
    public long Resyncs;
    public long ResyncBytes;
    public long ModeChanges;
    public long BadRects;
    public long SequenceGaps;
    public long Bytes;
    /// <summary>Frames actually pushed to the window (not just decoded).</summary>
    public long Presented;
    /// <summary>Total milliseconds spent inside the renderer's Present call.</summary>
    public double PresentMs;
    /// <summary>Total milliseconds spent inside the xBRZ filter.</summary>
    public double FilterMs;
    /// <summary>Frames the xBRZ filter scaled.</summary>
    public long Filtered;
    public int Width;
    public int Height;

    public override string ToString() =>
        $"frames={Frames} rects={Rects} " +
        (Scrolls > 0 ? $"scrolls={Scrolls} " : string.Empty) +
        $"palette={PaletteUpdates} control={ControlMessages} " +
        $"records={Records} bytes={Bytes} shown={Presented} " +
        (Presented > 0 ? $"render={PresentMs / Presented:F1}ms " : string.Empty) +
        (Filtered > 0 ? $"filter={FilterMs / Filtered:F1}ms " : string.Empty) +
        $"unknown={UnknownFamilies + UnknownMessages} " +
        (EngineErrors > 0 ? $"engine_errors={EngineErrors} " : string.Empty) +
        (Resyncs > 0 ? $"resync={Resyncs}({ResyncBytes}B) " : string.Empty) +
        (BadRects > 0 ? $"bad_rects={BadRects} " : string.Empty) +
        (SequenceGaps > 0 ? $"gaps={SequenceGaps} " : string.Empty) +
        $"mode={Width}x{Height}x8";
}

public sealed class ClientState(int width, int height)
{
    private readonly object _lock = new();

    public int Width { get; private set; } = width;
    public int Height { get; private set; } = height;

    /// <summary>Framebuffer replica (indexed, one byte per pixel).</summary>
    public byte[] Indices { get; private set; } = new byte[width * height];

    /// <summary>Palette replica, 6-bit DAC values, RGB triples.</summary>
    public byte[] Palette { get; private set; } = new byte[768];

    public ClientStats Stats { get; } = new() { Width = width, Height = height };

    public bool PaletteDirty { get; private set; } = true;

    /// <summary>
    /// True when a complete frame (a frame_end) is waiting to be shown.  Rects
    /// and palette spans only change the replica; presenting them one by one
    /// would upload the whole frame dozens of times and show half-drawn
    /// frames, which is both slow and visibly flickers.
    /// </summary>
    private bool _presentPending = true;

    private volatile bool _sawMode;
    private volatile bool _needsKeyframe;

    /// <summary>
    /// At least one set_mode has been decoded.  The render loop keeps asking
    /// for a keyframe until this is true: the engine only sends set_mode when
    /// the mode changes, so a client that joins during the VBE intro and
    /// misses that one message would otherwise stay on the wrong screen size
    /// forever.
    /// </summary>
    public bool SawMode => _sawMode;

    /// <summary>The engine sent a rect that cannot exist in our mode.</summary>
    public bool NeedsKeyframe => _needsKeyframe;

    public void ClearKeyframeRequest() => _needsKeyframe = false;
    public bool IndicesDirty { get; private set; } = true;

    private readonly List<int> _dirtyRows = [];
    private bool _dirtyAll = true;

    /// <summary>
    /// The rows that changed since the last frame, and whether the whole
    /// frame has to be scaled again (a mode change, a reset or a new palette
    /// changes every pixel).  The filter scales the changed rows and their
    /// neighbours, because a blend looks two rows up and down.
    /// </summary>
    public (List<int> Rows, bool All) TakeDirty()
    {
        lock (_lock)
        {
            var rows = new List<int>(_dirtyRows);
            _dirtyRows.Clear();
            bool all = _dirtyAll;
            _dirtyAll = false;
            return (rows, all);
        }
    }

    /// <summary>One-line messages from the engine (hello, errors, bye).</summary>
    public event Action<string>? Notice;

    /// <summary>Protocol version the engine announced, -1 until its hello.</summary>
    public int EngineVersion { get; private set; } = -1;

    /// <summary>Engine role byte from its hello (0 = engine).</summary>
    public int EngineRole { get; private set; } = -1;

    public bool Apply(FrameHeader header, byte[] payload, int length, long bytesRead, out string? error)
    {
        error = null;
        lock (_lock)
        {
            Stats.Records++;
            Stats.Bytes = bytesRead;
            if (header.Family == Family.Control)
            {
                return ApplyControl(payload, length);
            }
            if (header.Family != Family.Video)
            {
                Stats.UnknownFamilies++;
                return false;
            }
            if (header.Codec > 1)
            {
                error = $"unsupported codec {header.Codec} at sequence {header.Sequence}";
                return false;
            }
            if (length == 0)
            {
                error = $"empty payload at sequence {header.Sequence}";
                return false;
            }

            var kind = (VideoMessage)payload[0];
            switch (kind)
            {
                case VideoMessage.SetMode:
                {
                    if (length < 6)
                    {
                        error = "short set_mode";
                        return false;
                    }
                    int w = BinaryPrimitives.ReadUInt16LittleEndian(payload.AsSpan(1));
                    int h = BinaryPrimitives.ReadUInt16LittleEndian(payload.AsSpan(3));
                    byte bpp = payload[5];
                    if (bpp != 8 || w == 0 || h == 0 || (long)w * h > 1_000_000)
                    {
                        error = $"unsupported mode {w}x{h}x{bpp}";
                        return false;
                    }
                    if (w != Width || h != Height)
                    {
                        Width = w;
                        Height = h;
                        Indices = new byte[w * h];
                        IndicesDirty = true;
                        Stats.ModeChanges++;
                    }
                    // a new mode means new pixels everywhere
                    _dirtyAll = true;
                    Stats.Width = w;
                    Stats.Height = h;
                    _sawMode = true;
                    _needsKeyframe = false;
                    _presentPending = true;
                    return false;
                }

                case VideoMessage.Palette:
                {
                    if (length < 5)
                    {
                        error = "short palette";
                        return false;
                    }
                    int start = BinaryPrimitives.ReadUInt16LittleEndian(payload.AsSpan(1));
                    int count = BinaryPrimitives.ReadUInt16LittleEndian(payload.AsSpan(3));
                    if (start + count > 256 || length < 5 + count * 3)
                    {
                        error = $"bad palette range {start}+{count} (payload {length})";
                        return false;
                    }
                    payload.AsSpan(5, count * 3).CopyTo(Palette.AsSpan(start * 3));
                    PaletteDirty = true;
                    // the colours of every pixel change
                    _dirtyAll = true;
                    Stats.PaletteUpdates++;
                    return true;
                }

                case VideoMessage.Rect:
                {
                    if (length < 9)
                    {
                        error = "short rect";
                        return false;
                    }
                    int x = BinaryPrimitives.ReadUInt16LittleEndian(payload.AsSpan(1));
                    int y = BinaryPrimitives.ReadUInt16LittleEndian(payload.AsSpan(3));
                    int rw = BinaryPrimitives.ReadUInt16LittleEndian(payload.AsSpan(5));
                    int rh = BinaryPrimitives.ReadUInt16LittleEndian(payload.AsSpan(7));
                    if (rw == 0 || rh == 0)
                    {
                        error = $"bad rect {x},{y} {rw}x{rh} (payload {length})";
                        return false;
                    }
                    if (x + rw > Width || y + rh > Height)
                    {
                        // The frame is intact, so the stream is not desynced:
                        // the engine is drawing a mode we do not know yet (it
                        // switched while we were connecting).  Do not resync;
                        // ask for a keyframe, which carries set_mode.
                        Stats.BadRects++;
                        _needsKeyframe = true;
                        return false;
                    }
                    if (header.Codec == 0)
                    {
                        if (length < 9 + rw * rh)
                        {
                            error = $"bad rect {x},{y} {rw}x{rh} (payload {length})";
                            return false;
                        }
                        var src = payload.AsSpan(9, rw * rh);
                        for (int row = 0; row < rh; row++)
                        {
                            src.Slice(row * rw, rw).CopyTo(Indices.AsSpan((y + row) * Width + x));
                            _dirtyRows.Add(y + row);
                        }
                    }
                    else if (header.Codec == 1)
                    {
                        // run-length coded rows: 00 nn repeats the previous
                        // pixel, 01 nn vv repeats vv, 02 nn takes nn literal
                        // pixels and 03 ends the row
                        if (!DecodeRuns(payload, length, x, y, rw, rh, out string? runError))
                        {
                            error = runError;
                            return false;
                        }
                    }
                    else
                    {
                        Stats.UnknownMessages++;
                        return false;
                    }
                    IndicesDirty = true;
                    Stats.Rects++;
                    return true;
                }

                case VideoMessage.FrameEnd:
                    Stats.Frames++;
                    _presentPending = true;
                    return false;

                case VideoMessage.Scroll:
                {
                    // The whole picture moved by dy rows (positive: down).  The
                    // rows that scroll in, and every row that did not move with
                    // it, follow as rects in this frame.  A scroll applies to
                    // what has arrived so far, presented or not.
                    if (length < 2)
                    {
                        error = "short scroll";
                        return false;
                    }
                    int dy = (sbyte)payload[1];
                    if (dy == 0)
                    {
                        Stats.Scrolls++;
                        return true;
                    }
                    if (dy <= -Height || dy >= Height)
                    {
                        error = $"scroll {dy} does not fit {Height} rows";
                        return false;
                    }
                    if (dy > 0)
                    {
                        // content moves down: row y takes the old row y-dy
                        Array.Copy(Indices, 0, Indices, dy * Width, (Height - dy) * Width);
                        Array.Clear(Indices, 0, dy * Width);
                    }
                    else
                    {
                        // content moves up: row y takes the old row y+|dy|
                        int up = -dy;
                        Array.Copy(Indices, up * Width, Indices, 0, (Height - up) * Width);
                        Array.Clear(Indices, (Height - up) * Width, up * Width);
                    }
                    // every row moved, so the filter has to rebuild the frame
                    IndicesDirty = true;
                    _dirtyAll = true;
                    Stats.Scrolls++;
                    return true;
                }

                case VideoMessage.Reset:
                    Array.Clear(Indices);
                    IndicesDirty = true;
                    _dirtyAll = true;
                    _presentPending = true;
                    return true;

                default:
                    Stats.UnknownMessages++;
                    return false;
            }
        }
    }

    /// <summary>Handles a CONTROL message (called with the lock held).</summary>
    private bool ApplyControl(byte[] payload, int length)
    {
        if (length < 1)
        {
            Stats.UnknownMessages++;
            return false;
        }
        Stats.ControlMessages++;
        switch ((ControlMessage)payload[0])
        {
            case ControlMessage.Hello when length >= 3:
                EngineRole = payload[1];
                EngineVersion = payload[2];
                Notice?.Invoke($"engine hello: role {payload[1]}, protocol v{payload[2]}");
                return false;
            case ControlMessage.Bye:
                Notice?.Invoke("engine said bye");
                return false;
            case ControlMessage.Error when length >= 3:
                ushort code = BinaryPrimitives.ReadUInt16LittleEndian(payload.AsSpan(1));
                string text = Encoding.ASCII.GetString(payload, 3, length - 3);
                Stats.EngineErrors++;
                Notice?.Invoke($"engine error {code}: {text}");
                return false;
            case ControlMessage.Debug when length >= 2:
                Notice?.Invoke($"engine debug: {Encoding.ASCII.GetString(payload, 1, length - 1)}");
                return false;
            default:
                Stats.UnknownMessages++;
                return false;
        }
    }

    /// <summary>Palette snapshot as 8-bit RGBA (little-endian byte order ARGB).</summary>
    public byte[] SnapshotFrame()
    {
        lock (_lock)
        {
            _presentPending = false;
            var rgba = new byte[Width * Height * 4];
            for (int i = 0; i < Width * Height; i++)
            {
                int c = Indices[i] * 3;
                byte r = Expand(Palette[c]);
                byte g = Expand(Palette[c + 1]);
                byte b = Expand(Palette[c + 2]);
                int o = i * 4;
                rgba[o] = b;
                rgba[o + 1] = g;
                rgba[o + 2] = r;
                rgba[o + 3] = 0xFF;
            }
            PaletteDirty = false;
            IndicesDirty = false;
            return rgba;
        }
    }

    public bool NeedsPresent()
    {
        lock (_lock)
        {
            return _presentPending;
        }
    }

    /// <summary>
    /// Decodes run-length coded rows into the framebuffer.  The runs are:
    /// 00 nn repeats the previous pixel, 01 nn vv repeats vv, 02 nn takes nn
    /// literal pixels and 03 ends the row.
    /// </summary>
    private bool DecodeRuns(byte[] payload, int length, int x, int y, int rw, int rh,
        out string? error)
    {
        error = null;
        int at = 9;
        for (int row = 0; row < rh; row++)
        {
            int written = 0;
            byte previous = 0;
            bool have = false;
            while (written < rw)
            {
                if (at >= length)
                {
                    error = $"truncated row {y + row} ({written} of {rw} pixels)";
                    return false;
                }
                byte op = payload[at++];
                if (op == 3)
                {
                    break;                      // the end of the row
                }
                int count;
                byte value;
                if (op == 0)
                {
                    if (!have)
                    {
                        error = $"row {y + row} repeats a pixel that was never sent";
                        return false;
                    }
                    if (at >= length)
                    {
                        error = $"truncated row {y + row}";
                        return false;
                    }
                    count = payload[at++];
                    value = previous;
                }
                else if (op == 1)
                {
                    if (at + 1 >= length)
                    {
                        error = $"truncated row {y + row}";
                        return false;
                    }
                    count = payload[at++];
                    value = payload[at++];
                }
                else if (op == 2)
                {
                    if (at >= length)
                    {
                        error = $"truncated row {y + row}";
                        return false;
                    }
                    count = payload[at++];
                    if (at + count > length || written + count > rw)
                    {
                        error = $"row {y + row} runs past the row";
                        return false;
                    }
                    for (int i = 0; i < count; i++)
                    {
                        byte literal = payload[at++];
                        Indices[(y + row) * Width + x + written + i] = literal;
                    }
                    written += count;
                    previous = payload[at - 1];
                    have = true;
                    continue;
                }
                else
                {
                    error = $"unknown run {op} in row {y + row}";
                    return false;
                }

                if (count == 0 || written + count > rw)
                {
                    error = $"row {y + row} runs past the row";
                    return false;
                }
                Indices.AsSpan((y + row) * Width + x + written, count).Fill(value);
                written += count;
                previous = value;
                have = true;
            }
            if (written != rw)
            {
                error = $"row {y + row} is {written} pixels short";
                return false;
            }
            _dirtyRows.Add(y + row);
        }
        return true;
    }

    /// <summary>6-bit DAC value to 8-bit, the usual v &lt;&lt; 2 | v &gt;&gt; 4 expansion.</summary>
    private static byte Expand(byte v)
    {
        v &= 0x3F;
        return (byte)((v << 2) | (v >> 4));
    }
}
