// Offline renderer for a protocol-v0 recording.
//
// `--file rec.bin --wav out.wav` parses the recording with the normal client
// decoder and renders its AUDIO family in order, honouring the per-write PIT
// stamps.  This is how a folder-link capture is checked (and heard) without
// the engine, and it exercises the same Opl timeline the live client uses; the
// only difference is that the timestamps are anchored to an absolute axis
// (Opl.FreeRun) instead of the live render front.

namespace Yt2.Client;

internal static class StreamAudio
{
    public static int Run(string path, string wavPath)
    {
        var state = new ClientState(320, 200);
        state.Audio.FreeRun = true;
        using (var file = File.OpenRead(path))
        {
            var reader = new FrameReader(file);
            while (reader.TryRead(out var header, out var payload, out int length))
            {
                state.Apply(header, payload, length, reader.BytesRead, out _);
            }
            Console.WriteLine($"client: stream audio: {reader.BytesRead} bytes, " +
                              $"{state.Audio.Scheduled - state.Audio.SpeakerMessages} timed writes, " +
                              $"{state.Audio.SpeakerMessages} speaker edges, " +
                              $"{state.Audio.Batches} batches");
        }
        Opl opl = state.Audio;
        int rate = opl.Rate;
        if (opl.NextFrame <= 0)
        {
            Console.Error.WriteLine($"client: {path} carries no timed audio");
            return 1;
        }
        long total = opl.NextFrame + rate;          // one second of tail
        Console.WriteLine($"client: stream audio: {opl.NextFrame / (double)rate:F2} s, " +
                          $"{rate} Hz");

        using var stream = File.Create(wavPath);
        using var w = new BinaryWriter(stream);
        w.Write("RIFF"u8);
        w.Write(0);                                 // patched at the end
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
        w.Write(0);                                 // patched at the end

        var pcm = new short[8192 * 2];
        var bytes = new byte[pcm.Length * 2];
        long peak = 0;
        double sum = 0.0;
        long done = 0;
        while (done < total)
        {
            int frames = (int)Math.Min(8192, total - done);
            opl.Render(pcm, frames);
            for (int i = 0; i < frames; i++)
            {
                int v = pcm[i * 2];
                peak = Math.Max(peak, Math.Abs((long)v));
                sum += (double)v * v;
            }
            Buffer.BlockCopy(pcm, 0, bytes, 0, frames * 4);
            w.Write(bytes, 0, frames * 4);
            done += frames;
        }
        long end = stream.Position;
        stream.Seek(4, SeekOrigin.Begin);
        w.Write((int)(end - 8));
        stream.Seek(40, SeekOrigin.Begin);
        w.Write((int)(end - 44));

        double rms = Math.Sqrt(sum / Math.Max(1, done));
        Console.WriteLine($"client: synth {wavPath}: {total / (double)rate:F2} s, " +
                          $"{rate} Hz stereo s16, peak {peak / 327.68:F1}% fs, rms {rms:F0}");
        return 0;
    }
}
