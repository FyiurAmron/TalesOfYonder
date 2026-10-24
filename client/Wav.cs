// WAV writer shared by the offline renderers (`--dro` and `--file ... --wav`).

namespace Yt2.Client;

internal static class Wav
{
    public static void Write(string path, short[] pcm, int rate)
    {
        using var stream = File.Create(path);
        using var w = new BinaryWriter(stream);
        int dataBytes = pcm.Length * 2;
        w.Write("RIFF"u8);
        w.Write(36 + dataBytes);
        w.Write("WAVE"u8);
        w.Write("fmt "u8);
        w.Write(16);
        w.Write((short)1);
        w.Write((short)2);
        w.Write(rate);
        w.Write(rate * 2 * 2);
        w.Write((short)4);
        w.Write((short)16);
        w.Write("data"u8);
        w.Write(dataBytes);
        foreach (short sample in pcm)
        {
            w.Write(sample);
        }
    }
}
