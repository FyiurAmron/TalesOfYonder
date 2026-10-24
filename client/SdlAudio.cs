// SDL3 audio output for the client.
//
// The synth is rendered on the main thread and pushed into an SDL audio
// stream, so the reader thread that applies register writes never blocks on
// the audio callback and there is no callback marshalling in the binding.
// The stream is kept about 100 ms ahead; a slow-frame stall simply drains it.

using System.Runtime.InteropServices;

namespace Yt2.Client;

internal static partial class SdlAudio
{
    private const string Library = "SDL3";
    private const uint InitAudio = 0x00000010;
    private const ushort FormatS16Little = 0x8010;
    private const uint DefaultPlayback = 0xFFFFFFFF;
    private const int Channels = 2;

    [StructLayout(LayoutKind.Sequential)]
    private struct AudioSpec
    {
        public ushort Format;
        public ushort Padding;
        public int Channels;
        public int Freq;
    }

    [LibraryImport(Library)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static partial bool SDL_InitSubSystem(uint flags);

    [LibraryImport(Library)]
    private static partial IntPtr SDL_OpenAudioDeviceStream(uint device, ref AudioSpec spec,
        IntPtr callback, IntPtr userdata);

    [LibraryImport(Library)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static partial bool SDL_ResumeAudioStreamDevice(IntPtr stream);

    [LibraryImport(Library)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static partial bool SDL_PutAudioStreamData(IntPtr stream, IntPtr data, int len);

    [LibraryImport(Library)]
    private static partial int SDL_GetAudioStreamQueued(IntPtr stream);

    [LibraryImport(Library)]
    private static partial void SDL_DestroyAudioStream(IntPtr stream);

    internal static int Queued(IntPtr stream) => SDL_GetAudioStreamQueued(stream);
    internal static bool Put(IntPtr stream, IntPtr data, int len) =>
        SDL_PutAudioStreamData(stream, data, len);
    internal static void Destroy(IntPtr stream) => SDL_DestroyAudioStream(stream);

    public static SdlAudioStream? TryOpen(int rate, out string error)
    {
        error = "";
        if (!SDL_InitSubSystem(InitAudio))
        {
            error = Sdl.LastError();
            return null;
        }
        var spec = new AudioSpec
        {
            Format = FormatS16Little,
            Channels = Channels,
            Freq = rate,
        };
        IntPtr stream = SDL_OpenAudioDeviceStream(DefaultPlayback, ref spec, IntPtr.Zero, IntPtr.Zero);
        if (stream == IntPtr.Zero)
        {
            error = Sdl.LastError();
            return null;
        }
        if (!SDL_ResumeAudioStreamDevice(stream))
        {
            error = Sdl.LastError();
            SDL_DestroyAudioStream(stream);
            return null;
        }
        return new SdlAudioStream(stream, rate);
    }
}

internal sealed unsafe class SdlAudioStream : IDisposable
{
    private const int MaxFrames = 1024;
    private readonly IntPtr _stream;
    private readonly short[] _samples = new short[MaxFrames * 2];

    public SdlAudioStream(IntPtr stream, int rate)
    {
        _stream = stream;
        Rate = rate;
    }

    public int Rate { get; }
    public long Pushed { get; private set; }
    public long Pumps { get; private set; }
    public long Underruns { get; private set; }

    /// <summary>Keeps the stream about 100 ms ahead of the device.</summary>
    public void Pump(Opl audio)
    {
        int target = Rate / 10 * 2 * 2;      // 100 ms, s16 stereo
        int queued = SdlAudio.Queued(_stream);
        if (queued == 0 && Pumps > 0)
        {
            Underruns++;
        }
        for (int i = 0; i < 8 && queued < target; i++)
        {
            int frames = Math.Min(MaxFrames, (target - queued) / (2 * 2));
            if (frames <= 0)
            {
                break;
            }
            audio.Render(_samples, frames);
            fixed (short* p = _samples)
            {
                if (!SdlAudio.Put(_stream, (IntPtr)p, frames * 2 * 2))
                {
                    break;
                }
            }
            queued += frames * 2 * 2;
            Pushed += frames;
            Pumps++;
        }
    }

    public void Dispose()
    {
        SdlAudio.Destroy(_stream);
    }
}
