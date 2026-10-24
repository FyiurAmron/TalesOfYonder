// Offline renderer for a DOSBox raw OPL capture (DRO 2.0).
//
// This is the synth's test bench, not the live path: the capture carries a
// timestamp for every register write, so `--dro music.dro --wav music.wav`
// plays the original driver's stream through the same Opl2 core the live
// client uses.  The paired `.wav` from DOSBox-X (tools/dosbox/audio.sh,
// engine/asm/sbfmdrv/test/run.sh MUSIC_CAP=both) is the reference.

using System.Buffers.Binary;
using System.Text;

namespace Yt2.Client;

internal static class DroRenderer
{
    private readonly record struct Write(int Ms, int Bank, int Reg, int Val);

    public static int Run(string path, string? wavPath)
    {
        List<Write> writes;
        uint lengthMs;
        byte hardware;
        try
        {
            (writes, lengthMs, hardware) = Load(path);
        }
        catch (Exception ex) when (ex is IOException or InvalidDataException)
        {
            Console.Error.WriteLine($"client: {ex.Message}");
            return 1;
        }

        const int rate = 49716;
        var chip = new Opl2(rate);
        int totalFrames = (int)(((long)lengthMs + 1000) * rate / 1000);
        var pcm = new short[totalFrames * 2];
        var mix = new int[2048];
        int pos = 0;
        int noteOns = 0;

        void RenderTo(int target)
        {
            while (pos < target)
            {
                int frames = Math.Min(mix.Length, target - pos);
                Array.Clear(mix, 0, frames);
                chip.RenderMono(mix, frames);
                for (int i = 0; i < frames; i++)
                {
                    int s = Math.Clamp(mix[i], short.MinValue, short.MaxValue);
                    pcm[(pos + i) * 2] = (short)s;
                    pcm[(pos + i) * 2 + 1] = (short)s;
                }
                pos += frames;
            }
        }

        foreach (var w in writes)
        {
            RenderTo((int)((long)w.Ms * rate / 1000));
            if (w.Bank != 0)
            {
                continue;                       // the driver is a single OPL2
            }
            chip.Write(w.Reg, w.Val);
            if ((w.Reg is >= 0xB0 and <= 0xB8 && (w.Val & 0x20) != 0) ||
                (w.Reg == 0xBD && (w.Val & 0x1F) != 0))
            {
                noteOns++;
            }
        }
        RenderTo(totalFrames);

        long peak = 0;
        double sum = 0.0;
        for (int i = 0; i < pcm.Length; i += 2)
        {
            int v = pcm[i];
            peak = Math.Max(peak, Math.Abs(v));
            sum += (double)v * v;
        }
        double rms = Math.Sqrt(sum / Math.Max(1, pcm.Length / 2));
        Console.WriteLine($"client: DRO {path}: {writes.Count} writes, {noteOns} note-ons, " +
                          $"{lengthMs} ms, hardware {hardware}");
        wavPath ??= Path.ChangeExtension(path, ".synth.wav");
        Wav.Write(wavPath, pcm, rate);
        Console.WriteLine($"client: synth {wavPath}: {totalFrames / (double)rate:F2} s, " +
                          $"{rate} Hz stereo s16, peak {peak / 327.68:F1}% fs, rms {rms:F0}");
        return 0;
    }

    private static (List<Write> Writes, uint LengthMs, byte Hardware) Load(string path)
    {
        byte[] data = File.ReadAllBytes(path);
        if (data.Length < 26 || Encoding.ASCII.GetString(data, 0, 8) != "DBRAWOPL")
        {
            throw new InvalidDataException($"{path} is not a DBRAWOPL capture");
        }
        ushort major = BinaryPrimitives.ReadUInt16LittleEndian(data.AsSpan(8));
        ushort minor = BinaryPrimitives.ReadUInt16LittleEndian(data.AsSpan(10));
        if (major != 2 || minor != 0)
        {
            throw new InvalidDataException($"{path}: unsupported DRO version {major}.{minor}");
        }
        uint pairs = BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(12));
        uint lengthMs = BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(16));
        byte hardware = data[20];
        byte format = data[21];
        byte compression = data[22];
        byte shortCode = data[23];
        byte longCode = data[24];
        int mapLength = data[25];
        if (format != 0 || compression != 0)
        {
            throw new InvalidDataException($"{path}: unsupported format/compression " +
                                           $"{format}/{compression}");
        }
        if (26 + mapLength > data.Length)
        {
            throw new InvalidDataException($"{path}: truncated codemap");
        }
        byte[] map = data[26..(26 + mapLength)];
        var writes = new List<Write>((int)Math.Min(pairs, 1_000_000));
        int time = 0;
        for (int i = 26 + mapLength; i + 1 < data.Length; i += 2)
        {
            byte code = data[i];
            byte val = data[i + 1];
            if (code == shortCode)
            {
                time += val + 1;
                continue;
            }
            if (code == longCode)
            {
                time += (val + 1) * 256;
                continue;
            }
            int bank = (code >> 7) & 1;
            int index = code & 0x7F;
            if (index < map.Length)
            {
                writes.Add(new Write(time, bank, map[index], val));
            }
        }
        return (writes, lengthMs, hardware);
    }
}
