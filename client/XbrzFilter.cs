// The frame filter: how the engine's frame becomes the pixels the window
// shows.  "nearest" hands the frame to the graphics driver as it is (the
// driver scales it with the nearest neighbour, so the pixels stay crisp);
// "xbrz" scales it in the client with the xBRZ filter, which smooths the
// pixel art instead of enlarging it block by block.

namespace Yt2.Client;

/// <summary>Scales a frame with xBRZ before it goes to the window.</summary>
internal sealed class XbrzFilter
{
    private readonly int _scale;
    private uint[] _source = [];
    private uint[] _scaled = [];

    public XbrzFilter(int scale) => _scale = scale;

    /// <summary>The last scaled frame, as the texture's own ARGB8888 words.</summary>
    public uint[] Last => _scaled;

    /// <summary>
    /// Scales a frame.  The input is the client's little endian RGBA byte
    /// order and the output is 0xAARRGGBB words, which in memory is exactly
    /// the ARGB8888 layout the texture wants - so the scaled pixels go to
    /// the window without a conversion.
    /// </summary>
    /// <param name="dirtyRows">The rows that changed; they are scaled with
    /// their neighbours, because a blend looks two rows up and down.  An empty
    /// list scales the whole frame.</param>
    /// <param name="dirtyAll">The whole frame has to be scaled again (a mode
    /// change, a reset or a new palette changes every pixel).</param>
    public uint[] Apply(byte[] rgba, int width, int height, IReadOnlyList<int> dirtyRows,
        bool dirtyAll)
    {
        int pixels = width * height;
        if (_source.Length != pixels)
        {
            _source = new uint[pixels];
            _scaled = new uint[pixels * _scale * _scale];
        }

        if (dirtyAll || dirtyRows.Count == 0)
        {
            Convert(rgba, 0, height, width);
            Xbrz.Scale(_source, width, height, _scaled, _scale);
        }
        else
        {
            // the changed rows, in runs, each with the neighbour above and
            // below: their blends change with the row that changed
            int at = 0;
            while (at < dirtyRows.Count)
            {
                int start = dirtyRows[at];
                int end = start + 1;
                at++;
                while (at < dirtyRows.Count && dirtyRows[at] < end)
                {
                    at++;
                }
                Convert(rgba, start, end, width);
                Xbrz.Scale(_source, width, height, _scaled, _scale,
                    Math.Max(start - 1, 0), Math.Min(end + 1, height));
            }
        }
        return _scaled;
    }

    /// <summary>Turns the client's RGBA bytes into the words xBRZ works on.</summary>
    private void Convert(byte[] rgba, int firstRow, int lastRow, int width)
    {
        for (int y = firstRow; y < lastRow; y++)
        {
            int row = y * width;
            for (int x = 0; x < width; x++)
            {
                int o = (row + x) * 4;
                _source[row + x] = (uint)(rgba[o + 3] << 24 | rgba[o + 2] << 16 |
                                         rgba[o + 1] << 8 | rgba[o]);
            }
        }
    }
}
