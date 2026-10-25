// Yt2Client - companion client for the SWREG.EXE seam build.
//
//   dotnet run --project client                 # connect to localhost:2324
//   dotnet run --project client -- --file rec.bin --headless
//
// The seam (engine/asm/swreg/src/seam.asm, built by engine/asm/swreg/tools/build.sh --variant serial) speaks
// protocol v0 VIDEO messages over COM1; DOSBox-X bridges that port to a TCP
// socket with `serial1=nullmodem port:2324`.  See docs/client.md.
//
// Note the nullmodem link doubles every 0xFF byte on the wire; TcpSource
// undoes that with NullModemStream (--raw turns it off).

using System.Globalization;
using System.Net.Sockets;

namespace Yt2.Client;

public sealed class Options
{
    public string? File;
    public string? Follow;
    public string? Input;
    public string? World;
    public string Host = "127.0.0.1";
    public int Port = 2324;
    public bool Headless;
    public int Scale = 3;
    public string Filter = "nearest";
    public double StatsSeconds = 5;
    public int ConnectTimeoutSeconds = 30;
    public bool Keyframe = true;
    public double KeyframeEvery;
    public bool Hold;
    public bool NoAudio;
    public string? Dro;
    public string? Wav;
    public string? Ppm;
    public string? Dump;
    public string? DumpFrame;
    public bool Raw;
    public List<ScriptedKey> Keys = [];
    public List<ScriptedMove> Moves = [];
    public List<ScriptedClick> Clicks = [];

    public static Options Parse(string[] args)
    {
        var options = new Options();
        for (int i = 0; i < args.Length; i++)
        {
            string arg = args[i];
            string Next(string name) =>
                i + 1 < args.Length ? args[++i] : throw new ArgumentException($"{name} needs a value");
            switch (arg)
            {
                case "--file":
                    options.File = Next(arg);
                    break;
                case "--follow":
                    options.Follow = Next(arg);
                    break;
                case "--input":
                    options.Input = Next(arg);
                    break;
                case "--world":
                    options.World = Next(arg);
                    break;
                case "--tcp":
                    ParseEndpoint(Next(arg), options);
                    break;
                case "--host":
                    options.Host = Next(arg);
                    break;
                case "--port":
                    options.Port = int.Parse(Next(arg), CultureInfo.InvariantCulture);
                    break;
                case "--headless":
                    options.Headless = true;
                    break;
                case "--scale":
                    options.Scale = int.Parse(Next(arg), CultureInfo.InvariantCulture);
                    break;
                case "--filter":
                    options.Filter = Next(arg);
                    break;
                case "--stats":
                    options.StatsSeconds = double.Parse(Next(arg), CultureInfo.InvariantCulture);
                    break;
                case "--connect-timeout":
                    options.ConnectTimeoutSeconds = int.Parse(Next(arg), CultureInfo.InvariantCulture);
                    break;
                case "--no-keyframe":
                    options.Keyframe = false;
                    break;
                case "--keyframe-every":
                    options.KeyframeEvery = double.Parse(Next(arg), CultureInfo.InvariantCulture);
                    break;
                case "--hold":
                    options.Hold = true;
                    break;
                case "--no-audio":
                    options.NoAudio = true;
                    break;
                case "--dro":
                    options.Dro = Next(arg);
                    break;
                case "--wav":
                    options.Wav = Next(arg);
                    break;
                case "--ppm":
                    options.Ppm = Next(arg);
                    break;
                case "--dump":
                    options.Dump = Next(arg);
                    break;
                case "--dump-frame":
                    options.DumpFrame = Next(arg);
                    break;
                case "--raw":
                    options.Raw = true;
                    break;
                case "--key":
                    options.Keys.Add(ParseKey(Next(arg)));
                    break;
                case "--mouse":
                    options.Moves.Add(ParseMove(Next(arg)));
                    break;
                case "--click":
                    options.Clicks.Add(ParseClick(Next(arg)));
                    break;
                case "--help":
                case "-h":
                    Usage();
                    Environment.Exit(0);
                    break;
                default:
                    throw new ArgumentException($"unknown argument {arg}");
            }
        }
        return options;
    }

    private static void ParseEndpoint(string text, Options options)
    {
        int colon = text.LastIndexOf(':');
        if (colon > 0)
        {
            options.Host = text[..colon];
            options.Port = int.Parse(text[(colon + 1)..], CultureInfo.InvariantCulture);
        }
        else
        {
            options.Port = int.Parse(text, CultureInfo.InvariantCulture);
        }
    }

    /// <summary>Parses SECONDS:SCAN:ASCII (hex with 0x, decimal otherwise).</summary>
    private static ScriptedKey ParseKey(string text)
    {
        string[] parts = text.Split(':');
        if (parts.Length != 3)
        {
            throw new ArgumentException($"--key wants SECONDS:SCAN:ASCII, got '{text}'");
        }
        return new ScriptedKey(
            double.Parse(parts[0], CultureInfo.InvariantCulture),
            ParseByte(parts[1]),
            ParseByte(parts[2]));
    }

    private static ScriptedMove ParseMove(string text)
    {
        string[] parts = text.Split(':');
        if (parts.Length != 3)
        {
            throw new ArgumentException($"--mouse wants SECONDS:DX:DY, got '{text}'");
        }
        return new ScriptedMove(
            double.Parse(parts[0], CultureInfo.InvariantCulture),
            short.Parse(parts[1], CultureInfo.InvariantCulture),
            short.Parse(parts[2], CultureInfo.InvariantCulture));
    }

    private static ScriptedClick ParseClick(string text)
    {
        string[] parts = text.Split(':');
        if (parts.Length != 3)
        {
            throw new ArgumentException($"--click wants SECONDS:BUTTON:0|1, got '{text}'");
        }
        return new ScriptedClick(
            double.Parse(parts[0], CultureInfo.InvariantCulture),
            ParseByte(parts[1]),
            parts[2] != "0");
    }

    private static byte ParseByte(string text) =>
        text.StartsWith("0x", StringComparison.OrdinalIgnoreCase)
            ? byte.Parse(text[2..], NumberStyles.HexNumber, CultureInfo.InvariantCulture)
            : byte.Parse(text, CultureInfo.InvariantCulture);

    private static void Usage()
    {
        Console.WriteLine("""
            usage: Yt2Client [options]

              --tcp [host:]port   connect to the DOSBox-X nullmodem socket
                                  (default 127.0.0.1:2324)
              --file PATH         replay a recording instead of connecting
              --follow PATH       follow the server's STREAM.BIN in place
                                  (the folder link; the product transport)
              --input PATH        send input to the server's INPUT.BIN
              --world PATH        WORLD.DAT for the VOC sound effects
                                  (default: next to --follow / --file, then
                                  server/game/WORLD.DAT next to the client)
              --headless          no window; decode and report only
              --scale N           window scale (default 3)
              --filter NAME       nearest (default) or xbrz: the pixel art is
                                  scaled by the client with the xBRZ filter
                                  instead of block by block
              --stats SECONDS     stats interval, 0 disables (default 5)
              --connect-timeout N give up after N seconds (headless; default 30)
                                  a window keeps waiting until it is closed
              --no-keyframe       do not ask the engine for a keyframe on connect
              --hold              keep the window open after a recording ends
              --no-audio          do not open an audio device
              --dro PATH          render a DOSBox DRO OPL capture with the synth
              --wav PATH          with --dro or --file: render the audio to this
                                  WAV instead of opening a device
              --ppm PATH          write the last frame as a PPM file
              --dump PATH         record every byte received to PATH
              --raw               do not unescape the nullmodem 0xFF doubling
              --key S:SCAN:ASCII  press a key S seconds after starting
              --mouse S:DX:DY     move the mouse DX,DY game pixels at S seconds
              --click S:BUTTON:0|1  button edge (0 left, 1 middle, 2 right)
                                  (all repeatable; live link only, test hooks)
            """);
    }
}

/// <summary>One scripted key press for --key (tests and demos).</summary>
public readonly record struct ScriptedKey(double At, byte Scan, byte Ascii);

/// <summary>One scripted relative mouse move for --mouse, in game pixels.</summary>
public readonly record struct ScriptedMove(double At, short Dx, short Dy);

/// <summary>One scripted button edge for --click.</summary>
public readonly record struct ScriptedClick(double At, byte Button, bool Down);

internal static class Program
{
    /// <summary>
    /// Finds the WORLD.DAT that carries the VOC effect bank.  The live folder
    /// link always has it next to STREAM.BIN; the bundle's offline smoke test
    /// finds it under server/game; --world overrides both.
    /// </summary>
    private static string? ResolveWorldPath(Options options)
    {
        if (!string.IsNullOrEmpty(options.World))
        {
            return File.Exists(options.World) ? Path.GetFullPath(options.World) : null;
        }

        var candidates = new List<string>();
        void AddNear(string? path)
        {
            if (string.IsNullOrEmpty(path))
            {
                return;
            }
            string? dir = Path.GetDirectoryName(Path.GetFullPath(path));
            if (dir is not null)
            {
                candidates.Add(Path.Combine(dir, "WORLD.DAT"));
            }
        }

        AddNear(options.Follow);
        AddNear(options.File);
        string baseDir = AppContext.BaseDirectory;
        candidates.Add(Path.Combine(Environment.CurrentDirectory, "WORLD.DAT"));
        candidates.Add(Path.Combine(baseDir, "WORLD.DAT"));
        candidates.Add(Path.Combine(baseDir, "server", "game", "WORLD.DAT"));
        candidates.Add(Path.Combine(baseDir, "..", "server", "game", "WORLD.DAT"));
        candidates.Add(Path.Combine(baseDir, "..", "..", "server", "game", "WORLD.DAT"));

        foreach (string candidate in candidates)
        {
            if (File.Exists(candidate))
            {
                return Path.GetFullPath(candidate);
            }
        }
        return null;
    }

    private static int Main(string[] args)
    {
        // Redirected stdout is block-buffered, so a running client would keep
        // its startup and stats lines invisible in a log (and in the window
        // gate) until it exits.  Keep the console live.
        Console.SetOut(new StreamWriter(Console.OpenStandardOutput()) { AutoFlush = true });
        Console.SetError(new StreamWriter(Console.OpenStandardError()) { AutoFlush = true });

        Options options;
        try
        {
            options = Options.Parse(args);
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"client: {ex.Message}");
            return 2;
        }

        try
        {
            return Run(options);
        }
        catch (OperationCanceledException)
        {
            return 0;                       // the window was closed while connecting
        }
        catch (Exception ex) when (ex is IOException or SocketException or InvalidDataException)
        {
            Console.Error.WriteLine($"client: {ex.Message}");
            return 1;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"client: {ex.GetType().Name}: {ex.Message}");
            Console.Error.WriteLine("client: --headless tests the decoder without a window");
            return 1;
        }
    }

    private static int Run(Options options)
    {
        if (options.Filter is not ("nearest" or "xbrz"))
        {
            Console.Error.WriteLine($"client: unknown filter '{options.Filter}' " +
                                    "(nearest or xbrz)");
            return 2;
        }
        if (options.Dro is not null)
        {
            return DroRenderer.Run(options.Dro, options.Wav);
        }

        string? worldPath = ResolveWorldPath(options);
        if (options.File is not null && options.Wav is not null)
        {
            return StreamAudio.Run(options.File, options.Wav, worldPath);
        }
        SampleBank? samples = SampleBank.TryLoad(worldPath);
        if (samples is not null)
        {
            Console.WriteLine($"client: samples {samples.Path}");
        }
        else
        {
            Console.WriteLine("client: samples unavailable (WORLD.DAT not found); " +
                              "SFX stay silent");
        }
        // xBRZ scales the frame in the client, by the window scale factor.
        XbrzFilter? filter = options.Filter == "xbrz" ? new XbrzFilter(options.Scale) : null;
        SdlScreen? created = null;
        if (!options.Headless)
        {
            created = Sdl.TryCreate("YT2 client", 320, 200, options.Scale, filter is not null,
                out string sdlError);
            if (created is not null)
            {
                Console.WriteLine($"client: video {created.VideoDriver}, renderer {created.RendererName}");
            }
            if (created is null)
            {
                Console.Error.WriteLine($"client: {sdlError}");
                Console.Error.WriteLine("client: the window needs SDL3.dll next to Yt2Client.exe " +
                                        "(the Windows bundle ships it)");
                return 1;
            }
            Console.WriteLine($"client: window {created.WindowSize} at {created.WindowPosition}" +
                              $" ({created.Backend})");
            if (!created.Visible)
            {
                Console.WriteLine("client: the window system still reports the window hidden");
            }
        }
        using var screen = created;
        if (created is not null)
        {
            // a defined first frame: the texture starts undefined and the link
            // may take a while to answer, and the window must stay paint-able
            created.Present(new byte[created.TextureBytes]);
        }
        SdlAudioStream? audio = null;
        if (!options.Headless && !options.NoAudio)
        {
            audio = SdlAudio.TryOpen(44100, out string audioError);
            if (audio is null)
            {
                Console.Error.WriteLine($"client: audio unavailable ({audioError}); " +
                                        "continuing without sound");
            }
            else
            {
                Console.WriteLine($"client: audio {audio.Rate} Hz stereo s16");
            }
        }
        using var audioScope = audio;
        var nextWaitLine = DateTime.UtcNow.AddSeconds(5);
        Func<bool>? pumpWhileConnecting = created is null ? null : () =>
        {
            bool alive = created!.Pump();
            if (DateTime.UtcNow >= nextWaitLine)
            {
                Console.WriteLine($"client: still waiting for {options.Host}:{options.Port}");
                nextWaitLine = DateTime.UtcNow.AddSeconds(5);
            }
            return alive;
        };

        FileStream? dump = options.Dump is null ? null : File.Create(options.Dump);
        using var dumpStop = new CancellationTokenSource();
        if (dump is not null)
        {
            _ = Task.Run(() =>
            {
                // keep the dump file current so a stuck link can be inspected
                while (!dumpStop.IsCancellationRequested)
                {
                    Thread.Sleep(500);
                    try
                    {
                        dump.Flush();
                    }
                    catch (ObjectDisposedException)
                    {
                        return;
                    }
                    catch (IOException)
                    {
                        return;
                    }
                }
            });
        }

        int screenWidth = 320;
        int screenHeight = 200;
        bool shownOnce = false;
        byte[]? last = null;
        Array? lastPixels = null;
        ClientState? finalState = null;

        while (true)
        {
            var state = new ClientState(320, 200);
            state.Audio.SampleSource = samples;
            state.Notice += message => Console.WriteLine($"client: {message}");
            bool quit = false;
            FrameSource source;
            try
            {
                source = FrameSource.Open(options, pumpWhileConnecting);
            }
            catch (OperationCanceledException)
            {
                break;                      // the window was closed while connecting
            }
            using (source)
            {
                string? inputPath = options.Follow is null
                    ? null
                    : options.Input ?? Path.Combine(
                        Path.GetDirectoryName(Path.GetFullPath(options.Follow)) ?? ".", "INPUT.BIN");
                using var input = inputPath is null
                    ? null
                    : new FrameSource.RingFileStream(inputPath, 64 * 1024);
                var tx = input is not null
                    ? new FrameWriter(input)
                    : options.File is null ? new FrameWriter(source.Stream) : null;
                var scripted = new List<(double At, string What, Action<FrameWriter> Send)>();
                foreach (var key in options.Keys)
                {
                    scripted.Add((key.At, $"key scan=0x{key.Scan:X2} ascii=0x{key.Ascii:X2}",
                        w => w.Key(key.Scan, key.Ascii)));
                }
                foreach (var move in options.Moves)
                {
                    scripted.Add((move.At, $"mouse move {move.Dx},{move.Dy}",
                        w => w.MouseMove(move.Dx, move.Dy)));
                }
                foreach (var click in options.Clicks)
                {
                    scripted.Add((click.At, $"mouse button {click.Button} " +
                                            (click.Down ? "down" : "up"),
                        w => w.MouseButton(click.Button, click.Down)));
                }
                scripted.Sort((a, b) => a.At.CompareTo(b.At));
                int nextScripted = 0;

                if (options.File is not null)
                {
                    Console.WriteLine($"client: replaying {options.File}");
                }
                else if (options.Follow is not null)
                {
                    Console.WriteLine($"client: following {options.Follow}");
                    Console.WriteLine($"client: input {inputPath}");
                    Handshake(tx!, options, out bool keyframe);
                    Console.WriteLine(keyframe
                        ? "client: keyframe + hello written to the input file"
                        : "client: hello written to the input file");
                }
                else
                {
                    Console.WriteLine($"client: connected to {options.Host}:{options.Port}");
                    // Keyframe first, then hello: the engine's UART FIFO is 16 bytes,
                    // the keyframe frame is 15 and the hello frame is 17, so a 32-byte
                    // burst keeps the keyframe and drops the hello tail.  The render
                    // loop repeats both until each is confirmed (a set_mode for the
                    // keyframe, the engine hello for ours).
                    Handshake(tx!, options, out bool keyframe);
                    Console.WriteLine(keyframe
                        ? "client: keyframe + hello requested"
                        : "client: hello sent");
                }

                using var session = new CancellationTokenSource();
                var stream = dump is null ? source.Stream : new DumpStream(source.Stream, dump);
                var reader = new Thread(() => Read(state, stream, session, options.File is null))
                {
                    IsBackground = true,
                    Name = "reader",
                };
                reader.Start();

                MouseRelay? mouse = null;
                if (screen is not null && tx is not null)
                {
                    var host = screen;
                    mouse = new MouseRelay(tx, () => (state.Width, state.Height,
                        host.WindowPixels.Width, host.WindowPixels.Height));
                }

                var start = DateTime.UtcNow;
                var nextStats = options.StatsSeconds > 0
                    ? start.AddSeconds(options.StatsSeconds)
                    : DateTime.MaxValue;
                var lastStatsAt = start;
                long lastStatsFrames = 0;
                var nextHandshake = start.AddSeconds(1);
                var nextKeyframe = start;
                var nextForcedKeyframe = start.AddSeconds(options.KeyframeEvery);
                int handshakeTries = 0;

                try
                {
                    while (!quit && !session.IsCancellationRequested)
                    {
                        audio?.Pump(state.Audio);
                        if (screen is not null &&
                            (state.Width != screenWidth || state.Height != screenHeight))
                        {
                            if (screen.Resize(state.Width, state.Height))
                            {
                                screenWidth = state.Width;
                                screenHeight = state.Height;
                                Console.WriteLine($"client: mode {screenWidth}x{screenHeight}x8");
                            }
                        }

                        bool present = state.NeedsPresent();
                        if (present || screen is null)
                        {
                            if (present)
                            {
                                last = state.SnapshotFrame();
                                if (filter is not null)
                                {
                                    long filtering = System.Diagnostics.Stopwatch.GetTimestamp();
                                    (List<int> rows, bool all) = state.TakeDirty();
                                    uint[] scaled = filter.Apply(last, state.Width, state.Height, rows, all);
                                    state.Stats.FilterMs +=
                                        System.Diagnostics.Stopwatch.GetElapsedTime(filtering)
                                            .TotalMilliseconds;
                                    state.Stats.Filtered++;
                                    lastPixels = scaled;
                                }
                                else
                                {
                                    lastPixels = last;
                                }
                                screen?.Present(lastPixels);
                                state.Stats.Presented++;
                                if (screen is not null)
                                {
                                    state.Stats.PresentMs += screen.PresentMilliseconds;
                                }
                                if (!shownOnce && screen is not null)
                                {
                                    shownOnce = true;
                                    Console.WriteLine($"client: first frame shown " +
                                                      $"{state.Width}x{state.Height}");
                                }
                            }
                        }
                        else
                        {
                            // keep the window responsive between frames
                            Sdl.Delay(4);
                        }

                        if (screen is not null && !screen.Pump(
                                tx is null ? null : (scan, ascii) => tx.Key(scan, ascii),
                                mouse is null ? null : mouse.Abs,
                                mouse is null ? null : mouse.Button))
                        {
                            quit = true;
                            break;
                        }
                        if (screen is not null && screen.NeedsRepaint && lastPixels is not null)
                        {
                            // a resize or expose invalidates the back buffer;
                            // a static screen must still show its last frame
                            screen.Present(lastPixels);
                            state.Stats.Presented++;
                            state.Stats.PresentMs += screen.PresentMilliseconds;
                            screen.NeedsRepaint = false;
                        }
                        if (screen is null && options.File is null && !present)
                        {
                            Sdl.Delay(4);
                        }

                        var now = DateTime.UtcNow;
                        if (tx is not null && options.KeyframeEvery > 0 && now >= nextForcedKeyframe)
                        {
                            // diagnostic: ask for a full frame every N seconds
                            tx.KeyframeRequest();
                            nextForcedKeyframe = now.AddSeconds(options.KeyframeEvery);
                        }
                        bool needHello = state.EngineVersion < 0;
                        bool needKey = options.Keyframe && !state.SawMode;
                        if (tx is not null && now >= nextHandshake && (needHello || needKey))
                        {
                            // Repeat until confirmed: the first keyframe of a session
                            // is regularly swallowed by the engine's FIFO, and the
                            // engine only volunteers set_mode when the game changes
                            // mode, so a missed one used to strand the client on the
                            // old screen size for the whole session.
                            if (needKey)
                            {
                                tx.KeyframeRequest();
                            }
                            if (needHello)
                            {
                                tx.Hello();
                            }
                            nextHandshake = now.AddSeconds(1);
                            if (++handshakeTries == 4 && needKey)
                            {
                                Console.WriteLine("client: no set_mode yet; repeating the " +
                                                  "keyframe request until the engine answers");
                            }
                        }
                        if (tx is not null && options.Keyframe && state.NeedsKeyframe &&
                            now >= nextKeyframe)
                        {
                            // a rect that cannot exist in our mode: the engine is
                            // drawing something else, so ask for a fresh mode message
                            tx.KeyframeRequest();
                            state.ClearKeyframeRequest();
                            nextKeyframe = now.AddSeconds(1);
                            Console.WriteLine("client: screen and engine disagree; " +
                                              "keyframe requested");
                        }
                        if (tx is not null)
                        {
                            double elapsedScript = (now - start).TotalSeconds;
                            while (nextScripted < scripted.Count &&
                                   scripted[nextScripted].At <= elapsedScript)
                            {
                                var step = scripted[nextScripted++];
                                step.Send(tx);
                                Console.WriteLine($"client: {step.What} at {elapsedScript:F1}s");
                            }
                        }
                        if (now >= nextStats)
                        {
                            double elapsed = Math.Max(0.001, (now - start).TotalSeconds);
                            long frames = state.Stats.Frames;
                            double interval = Math.Max(0.001, (now - lastStatsAt).TotalSeconds);
                            double fps = (frames - lastStatsFrames) / interval;
                            Console.WriteLine($"client: [{elapsed,6:F1}s] {state.Stats} " +
                                              $"({state.Stats.Bytes / elapsed / 1024:F1} KiB/s, " +
                                              $"fps={fps:F1})");
                            lastStatsFrames = frames;
                            lastStatsAt = now;
                            nextStats = now.AddSeconds(options.StatsSeconds);
                        }
                    }
                }
                finally
                {
                    session.Cancel();
                    if (quit)
                    {
                        try
                        {
                            tx?.Bye();
                        }
                        catch (IOException)
                        {
                            // the link is already gone; nothing to tell the engine
                        }
                    }
                }
            }
            finalState = state;
            if (options.File is not null || quit || screen is null)
            {
                // a headless run has no window to keep alive
                break;
            }
            Console.WriteLine("client: link lost; waiting for the server");
        }

        if (options.Ppm is not null && finalState is not null)
        {
            // one last snapshot: the reader may have applied messages after
            // the render loop's final pass.  The PPM is the engine's frame,
            // not the filtered one, so it does not go through the filter.
            byte[] frame = finalState.SnapshotFrame();
            WritePpm(options.Ppm, finalState.Width, finalState.Height, frame);
            Console.WriteLine($"client: wrote {options.Ppm}");
        }

        if (options.DumpFrame is not null && filter is not null && finalState is not null)
        {
            // the filtered final frame, for the xBRZ gate: the words are the
            // texture's own layout (ARGB8888 in memory), so the dump is the
            // little endian RGBA the gate expects.  Scale it here instead of
            // reading filter.Last: the reader can reach end of stream before
            // the render loop's first pass, and the dump must not depend on
            // that race.
            byte[] lastFrame = finalState.SnapshotFrame();
            uint[] scaled = filter.Apply(lastFrame, finalState.Width, finalState.Height, [], true);
            byte[] frame = new byte[scaled.Length * 4];
            Buffer.BlockCopy(scaled, 0, frame, 0, frame.Length);
            File.WriteAllBytes(options.DumpFrame, frame);
            Console.WriteLine($"client: wrote {options.DumpFrame}");
        }

        if (options.Hold && screen is not null)
        {
            // --hold: the recording is over but the window stays, so a smoke
            // test can be looked at (and closed) instead of flashing past.
            Console.WriteLine("client: window held open; close it to finish");
            while (screen.Pump())
            {
                if (finalState is not null)
                {
                    audio?.Pump(finalState.Audio);
                }
                if (last is not null)
                {
                    screen.Present(last);
                }
                Sdl.SDL_Delay(50);
            }
        }

        if (audio is not null)
        {
            Console.WriteLine($"client: audio {audio.Pushed} frames, {audio.Underruns} underruns");
            if (finalState is not null && finalState.Audio.Scheduled > 0)
            {
                long scheduledWrites = Math.Max(0,
                    finalState.Audio.Scheduled - finalState.Audio.SpeakerMessages -
                    finalState.Audio.SampleMessages);
                Console.WriteLine($"client: audio schedule: " +
                                  $"{scheduledWrites} writes, " +
                                  $"{finalState.Audio.SpeakerMessages} speaker edges, " +
                                  $"{finalState.Audio.SampleMessages} samples, " +
                                  $"{finalState.Audio.LateWrites} played late");
            }
        }
        Console.WriteLine($"client: done  {finalState?.Stats}");
        dumpStop.Cancel();
        dump?.Dispose();
        return 0;
    }

    /// <summary>Sends hello plus (optionally) a keyframe request.</summary>
    /// <summary>
    /// Turns SDL pointer positions into absolute INPUT messages: the window
    /// is an integer multiple of the engine mode, so dividing by the scale
    /// maps the host pointer proportionally into game pixels (upper-left to
    /// upper-left, centre to centre).  The engine moves its software cursor
    /// to exactly that point, so no delta can drift.
    /// </summary>
    private sealed class MouseRelay(
        FrameWriter tx, Func<(int GameW, int GameH, int WinW, int WinH)> viewport)
    {
        private bool _left;
        private bool _right;

        public void Abs(int px, int py)
        {
            // The renderer presents the game with an integer scale, centred
            // in the window.  Work out that viewport from the live window
            // size, so maximising or resizing keeps the mapping exact.
            var (gameW, gameH, winW, winH) = viewport();
            gameW = Math.Max(1, gameW);
            gameH = Math.Max(1, gameH);
            int factor = Math.Max(1, Math.Min(winW / gameW, winH / gameH));
            int offsetX = (winW - gameW * factor) / 2;
            int offsetY = (winH - gameH * factor) / 2;
            int gx = (px - offsetX) / factor;
            int gy = (py - offsetY) / factor;
            gx = Math.Clamp(gx, 0, gameW - 1);
            gy = Math.Clamp(gy, 0, gameH - 1);
            tx.MouseAbs((ushort)gx, (ushort)gy);
        }

        public void Button(byte button, bool down)
        {
            byte index = button switch
            {
                Sdl.ButtonLeft => 0,
                Sdl.ButtonMiddle => 1,
                Sdl.ButtonRight => 2,
                _ => 0xFF,
            };
            if (index == 0xFF)
            {
                return;
            }
            if (index == 0 && down == _left)
            {
                return;
            }
            if (index == 2 && down == _right)
            {
                return;
            }
            if (index == 0)
            {
                _left = down;
            }
            else if (index == 2)
            {
                _right = down;
            }
            tx.MouseButton(index, down);
        }
    }

    private static void Handshake(FrameWriter tx, Options options, out bool keyframe)
    {
        keyframe = options.Keyframe;
        if (keyframe)
        {
            tx.KeyframeRequest();
        }
        tx.Hello();
    }

    private static void Read(ClientState state, Stream stream, CancellationTokenSource session,
                             bool live)
    {
        var reader = new FrameReader(stream);
        int errors = 0;
        try
        {
            while (!session.IsCancellationRequested)
            {
                if (!reader.TryRead(out var header, out var payload, out int length))
                {
                    break;
                }
                state.Stats.Resyncs = reader.Resyncs;
                state.Stats.ResyncBytes = reader.ResyncBytes;
                state.Stats.SequenceGaps = reader.SequenceGaps;
                if (!state.Apply(header, payload, length, reader.BytesRead, out string? error))
                {
                    if (error is not null)
                    {
                        // a false boundary: log it, then look for the next one
                        Console.Error.WriteLine($"client: {error} (resyncing)");
                        if (++errors >= 64)
                        {
                            Console.Error.WriteLine("client: too many decode errors, giving up");
                            break;
                        }
                        reader.RequestResync();
                        continue;
                    }
                }
                errors = 0;
            }
        }
        catch (Exception ex)
        {
            if (!session.IsCancellationRequested)
            {
                Console.Error.WriteLine($"client: link error: {ex.Message}");
            }
        }
        if (reader.TruncatedBytes > 0)
        {
            // the engine was killed mid-frame: the stream ended inside a
            // record, which is the end of the session, not a broken link
            Console.WriteLine($"client: stream ended mid-frame ({reader.TruncatedBytes} bytes dropped)");
        }
        if (!session.IsCancellationRequested)
        {
            if (!live)
            {
                Console.WriteLine("client: end of stream");
            }
            session.Cancel();
        }
    }

    private static void WritePpm(string path, int width, int height, byte[] rgba)
    {
        using var file = File.Create(path);
        using var writer = new StreamWriter(file);
        writer.Write($"P6\n{width} {height}\n255\n");
        writer.Flush();
        var rgb = new byte[width * height * 3];
        for (int i = 0; i < width * height; i++)
        {
            rgb[i * 3 + 0] = rgba[i * 4 + 2];
            rgb[i * 3 + 1] = rgba[i * 4 + 1];
            rgb[i * 3 + 2] = rgba[i * 4 + 0];
        }
        file.Write(rgb);
    }
}
