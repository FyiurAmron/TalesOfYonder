// Client-side sample bank: the Creative Voice File effects embedded in WORLD.DAT.
//
// The seam reports an effect with the file offset from the game's own SFX
// pointer table (AUDIO sample, docs/protocol.md).  The client reads the VOC
// header and blocks at that offset; every shipped record is 8-bit unsigned
// PCM (codec 0), which is all this decoder accepts.  Parsed records are cached
// by offset, so a repeated footstep costs nothing after the first play.
//
// WORLD.DAT holds 80 of these records; the seam sends the offset instead of
// an id, which keeps this class independent of the game's table layout and
// of the executable.

namespace Yt2.Client;

/// <summary>One decoded VOC record.</summary>
public sealed class Sample
{
    /// <summary>Sample rate in Hz (the VOC time constant is a reciprocal).</summary>
    public double Rate { get; init; }

    /// <summary>Unsigned 8-bit PCM, centred on 128.</summary>
    public byte[] Pcm { get; init; } = [];

    public double Duration => Rate > 0 ? Pcm.Length / Rate : 0.0;
}

/// <summary>Reads the game's VOC effects from WORLD.DAT.</summary>
public sealed class SampleBank
{
    private static ReadOnlySpan<byte> Signature => "Creative Voice File\x1a"u8;

    private readonly byte[] _world;
    private readonly Dictionary<uint, Sample?> _cache = [];

    public string Path { get; }

    private SampleBank(string path, byte[] world)
    {
        Path = path;
        _world = world;
    }

    /// <summary>Loads WORLD.DAT; null when the path is missing or unreadable.</summary>
    public static SampleBank? TryLoad(string? path)
    {
        if (string.IsNullOrWhiteSpace(path) || !File.Exists(path))
        {
            return null;
        }
        try
        {
            return new SampleBank(System.IO.Path.GetFullPath(path), File.ReadAllBytes(path));
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
        {
            return null;
        }
    }

    /// <summary>
    /// Decodes the VOC record at a WORLD.DAT file offset.  A null result means
    /// the offset does not carry a supported VOC file; the caller falls back
    /// to silence (or, for effect 3, to the PC-speaker beep).
    /// </summary>
    public Sample? Get(uint offset)
    {
        if (offset < 0x1A || (long)offset + 0x1A > _world.Length)
        {
            return null;
        }
        if (_cache.TryGetValue(offset, out Sample? cached))
        {
            return cached;
        }
        Sample? sample = Parse((int)offset);
        _cache[offset] = sample;
        return sample;
    }

    private Sample? Parse(int start)
    {
        if (!_world.AsSpan(start).StartsWith(Signature))
        {
            return null;
        }
        int dataOffset = _world[start + 0x14] | (_world[start + 0x15] << 8);
        int at = start + dataOffset;
        if (dataOffset < 0x1A || at > _world.Length)
        {
            return null;
        }

        double rate = 0.0;
        using var pcm = new MemoryStream();
        while (at + 4 <= _world.Length)
        {
            byte type = _world[at];
            if (type == 0)                          // terminator
            {
                return Build(rate, pcm);
            }
            int length = _world[at + 1] | (_world[at + 2] << 8) | (_world[at + 3] << 16);
            int body = at + 4;
            if (body + length > _world.Length)
            {
                return null;
            }
            switch (type)
            {
                case 1 when length >= 2:            // sound data
                {
                    int timeConstant = _world[body];
                    int codec = _world[body + 1];
                    if (codec != 0)
                    {
                        return null;                // only 8-bit unsigned PCM is shipped
                    }
                    if (pcm.Length == 0)
                    {
                        rate = RateFromTimeConstant(timeConstant);
                    }
                    pcm.Write(_world.AsSpan(body + 2, length - 2));
                    break;
                }

                case 2:                             // continuation of the last block
                    pcm.Write(_world.AsSpan(body, length));
                    break;

                case 8 when length >= 4:            // extended sound data
                {
                    int timeConstant = _world[body];
                    int codec = _world[body + 1];
                    if (codec != 0)
                    {
                        return null;
                    }
                    if (pcm.Length == 0)
                    {
                        rate = RateFromTimeConstant(timeConstant);
                    }
                    pcm.Write(_world.AsSpan(body + 4, length - 4));
                    break;
                }

                case 9 when length >= 1:            // new time constant
                    if (pcm.Length == 0)
                    {
                        rate = RateFromTimeConstant(_world[body]);
                    }
                    break;

                default:                            // silence, marker, text, ...
                    break;
            }
            at = body + length;
        }
        return Build(rate, pcm);
    }

    private static Sample? Build(double rate, MemoryStream pcm)
    {
        if (rate <= 0.0 || pcm.Length == 0)
        {
            return null;
        }
        return new Sample { Rate = rate, Pcm = pcm.ToArray() };
    }

    private static double RateFromTimeConstant(int value) =>
        1_000_000.0 / (256 - (value & 0xFF));
}
