// Minimal SDL3 binding for the client window (video only).  Kept to the calls
// the frame sink needs; the first call installs a resolver so the plain
// "SDL3" name also finds a versioned library such as libSDL3.so.0, which is
// what a machine without the -dev package has.

using System.Runtime.InteropServices;

namespace Yt2.Client;

internal static partial class Sdl
{
    private const string Library = "SDL3";

    // SDL3 flag values from SDL_init.h / SDL_video.h.  SDL_WINDOW_HIDDEN is
    // 0x8 and SDL_WINDOW_RESIZABLE is 0x20: passing the former creates a
    // window that is never mapped (and never in the taskbar), which looks
    // exactly like "the client has no window".
    public const uint InitVideo = 0x00000020;
    public const uint InitEvents = 0x00004000;
    public const uint WindowResizable = 0x00000020;
    public const uint TextureAccessStreaming = 1;
    public const uint PixelFormatArgb8888 = 0x16362004;
    public const int LogicalPresentationIntegerScale = 4;
    public const int ScaleModeNearest = 0;
    public const uint EventQuit = 0x100;
    public const uint EventWindowExposed = 0x204;
    public const uint EventWindowResized = 0x206;
    public const uint EventWindowPixelSizeChanged = 0x207;
    public const uint EventWindowMouseEnter = 0x20C;
    public const uint EventWindowMouseLeave = 0x20D;
    public const uint EventWindowFocusGained = 0x20E;
    public const uint EventWindowFocusLost = 0x20F;
    public const uint EventWindowCloseRequested = 0x210;
    public const uint EventKeyDown = 0x300;
    public const uint EventKeyUp = 0x301;
    public const uint EventMouseMotion = 0x400;
    public const uint EventMouseButtonDown = 0x401;
    public const uint EventMouseButtonUp = 0x402;
    public const byte ButtonLeft = 1;   // SDL3 numbers: left, middle, right
    public const byte ButtonMiddle = 2;
    public const byte ButtonRight = 3;
    public const ushort KmodCtrl = 0x00C0;
    public const int ScancodeQ = 20;

    static Sdl()
    {
        NativeLibrary.SetDllImportResolver(typeof(Sdl).Assembly, (name, assembly, path) =>
        {
            if (name != Library)
            {
                return IntPtr.Zero;
            }
            foreach (var candidate in OperatingSystem.IsWindows()
                         ? new[] { "SDL3.dll" }
                         : OperatingSystem.IsMacOS()
                             ? new[] { "libSDL3.dylib" }
                             : new[] { "libSDL3.so.0", "libSDL3.so" })
            {
                if (NativeLibrary.TryLoad(candidate, assembly, path, out var handle))
                {
                    return handle;
                }
                // single-file bundles have no assembly location; the runtime
                // library sits next to the executable, so try that explicitly
                var beside = Path.Combine(AppContext.BaseDirectory, candidate);
                if (File.Exists(beside) && NativeLibrary.TryLoad(beside, out handle))
                {
                    return handle;
                }
            }
            return IntPtr.Zero;
        });
    }

    [LibraryImport(Library)]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool SDL_Init(uint flags);

    [LibraryImport(Library)]
    internal static partial void SDL_Quit();

    [LibraryImport(Library, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial IntPtr SDL_CreateWindow(string title, int w, int h, uint flags);

    [LibraryImport(Library, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial IntPtr SDL_CreateRenderer(IntPtr window, string? name);

    [LibraryImport(Library)]
    internal static partial IntPtr SDL_CreateTexture(IntPtr renderer, uint format, uint access, int w, int h);

    [LibraryImport(Library)]
    internal static partial void SDL_DestroyTexture(IntPtr texture);

    [LibraryImport(Library)]
    internal static partial void SDL_DestroyRenderer(IntPtr renderer);

    [LibraryImport(Library)]
    internal static partial void SDL_DestroyWindow(IntPtr window);

    [LibraryImport(Library)]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool SDL_UpdateTexture(IntPtr texture, IntPtr rect, IntPtr pixels, int pitch);

    [LibraryImport(Library)]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool SDL_RenderClear(IntPtr renderer);

    [LibraryImport(Library)]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool SDL_RenderTexture(IntPtr renderer, IntPtr texture, IntPtr srcrect, IntPtr dstrect);

    [LibraryImport(Library)]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool SDL_RenderPresent(IntPtr renderer);

    [LibraryImport(Library)]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool SDL_SetRenderLogicalPresentation(IntPtr renderer, int w, int h, int mode);

    [LibraryImport(Library)]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool SDL_SetTextureScaleMode(IntPtr texture, int scaleMode);

    [LibraryImport(Library)]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool SDL_PollEvent(IntPtr @event);

    [LibraryImport(Library)]
    internal static partial IntPtr SDL_GetKeyboardState(out int numkeys);

    [LibraryImport(Library)]
    internal static partial void SDL_Delay(uint ms);

    [LibraryImport(Library)]
    internal static partial IntPtr SDL_GetError();

    [LibraryImport(Library)]
    internal static partial IntPtr SDL_GetCurrentVideoDriver();

    [LibraryImport(Library)]
    internal static partial IntPtr SDL_GetRendererName(IntPtr renderer);

    [LibraryImport(Library)]
    internal static partial void SDL_RaiseWindow(IntPtr window);

    [LibraryImport(Library)]
    internal static partial void SDL_ShowWindow(IntPtr window);

    [LibraryImport(Library)]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool SDL_GetWindowSize(IntPtr window, out int w, out int h);

    [LibraryImport(Library)]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool SDL_GetWindowSizeInPixels(IntPtr window, out int w, out int h);

    [LibraryImport(Library)]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool SDL_GetWindowPosition(IntPtr window, out int x, out int y);

    [LibraryImport(Library)]
    internal static partial ulong SDL_GetWindowFlags(IntPtr window);

    [LibraryImport(Library)]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool SDL_SetWindowPosition(IntPtr window, int x, int y);

    [LibraryImport(Library)]
    internal static partial IntPtr SDL_IOFromMem(IntPtr mem, nuint size);

    [LibraryImport(Library)]
    internal static partial IntPtr SDL_LoadBMP_IO(IntPtr src,
        [MarshalAs(UnmanagedType.I1)] bool closeio);

    [LibraryImport(Library)]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool SDL_SetWindowIcon(IntPtr window, IntPtr icon);

    [LibraryImport(Library)]
    internal static partial void SDL_DestroySurface(IntPtr surface);

    /// <summary>SDL's "centred on this display" window position.</summary>
    internal const int WindowPosCentered = unchecked((int)0x2FFF0000);

    /// <summary>SDL_WINDOW_HIDDEN.</summary>
    internal const ulong WindowHidden = 0x8;

    public static string LastError() => Marshal.PtrToStringUTF8(SDL_GetError()) ?? "unknown";

    private static string? Text(IntPtr ptr) => Marshal.PtrToStringUTF8(ptr);

    /// <summary>SDL names are lower case ("direct3d11"); show them as products.</summary>
    internal static string Pretty(string? name) => name switch
    {
        null or "" => "unknown",
        "windows" => "Windows",
        "x11" => "X11",
        "wayland" => "Wayland",
        "cocoa" => "Cocoa",
        "kmsdrm" => "KMSDRM",
        "android" => "Android",
        "offscreen" => "Offscreen",
        "dummy" => "Dummy",
        "direct3d" => "Direct3D",
        "direct3d11" => "Direct3D 11",
        "direct3d12" => "Direct3D 12",
        "opengl" => "OpenGL",
        "opengles2" => "OpenGL ES 2",
        "vulkan" => "Vulkan",
        "metal" => "Metal",
        "software" => "Software",
        var other => other,
    };

    /// <summary>
    /// Sets the window and taskbar icon from the Yendor2 bitmap embedded in
    /// the assembly (<c>client/Assets/yendor2.bmp</c>, a lossless copy of the
    /// game's <c>YENDOR2.BMP</c>).  Cosmetic: a missing or unreadable icon
    /// leaves the window as it is.
    /// </summary>
    private static bool TrySetWindowIcon(IntPtr window)
    {
        using var resource = typeof(Sdl).Assembly.GetManifestResourceStream("yendor2.bmp");
        if (resource is null)
        {
            return false;
        }
        var bytes = new byte[resource.Length];
        resource.ReadExactly(bytes);
        // SDL_IOFromMem does not copy: keep the buffer pinned until the BMP
        // has been decoded (the surface owns its own pixels afterwards).
        var pin = GCHandle.Alloc(bytes, GCHandleType.Pinned);
        try
        {
            var io = SDL_IOFromMem(pin.AddrOfPinnedObject(), (nuint)bytes.Length);
            if (io == IntPtr.Zero)
            {
                return false;
            }
            var icon = SDL_LoadBMP_IO(io, true);
            if (icon == IntPtr.Zero)
            {
                return false;
            }
            try
            {
                return SDL_SetWindowIcon(window, icon);
            }
            finally
            {
                SDL_DestroySurface(icon);
            }
        }
        finally
        {
            pin.Free();
        }
    }

    /// <summary>Creates the window, renderer and streaming texture.</summary>
    public static SdlScreen? TryCreate(string title, int width, int height, int scale,
                                       bool scaled, out string error)
    {
        error = string.Empty;
        if (!SDL_Init(InitVideo | InitEvents))
        {
            error = $"SDL_Init failed: {LastError()}";
            return null;
        }

        var window = SDL_CreateWindow(title, width * scale, height * scale, WindowResizable);
        if (window == IntPtr.Zero)
        {
            error = $"SDL_CreateWindow failed: {LastError()}";
            SDL_Quit();
            return null;
        }
        _ = TrySetWindowIcon(window);

        var renderer = SDL_CreateRenderer(window, null);
        if (renderer == IntPtr.Zero)
        {
            // a machine without a usable GPU backend still has the software
            // renderer; a window is more useful than a console-only client
            renderer = SDL_CreateRenderer(window, "software");
        }
        if (renderer == IntPtr.Zero)
        {
            error = $"SDL_CreateRenderer failed: {LastError()}";
            SDL_DestroyWindow(window);
            SDL_Quit();
            return null;
        }
        // SDL normally maps the window itself; a hidden window here means it
        // did not (a hidden owner, a remote session), so show it explicitly
        // and put it back in the middle of the screen if it landed off-screen.
        if ((SDL_GetWindowFlags(window) & WindowHidden) != 0)
        {
            SDL_ShowWindow(window);
        }
        SDL_SetWindowPosition(window, WindowPosCentered, WindowPosCentered);
        SDL_RaiseWindow(window);

        // A scaled image (xBRZ) is already the size of the window, so the
        // texture holds it as it is; otherwise the texture is the engine
        // resolution and the presentation scales it.
        int textureWidth = scaled ? width * scale : width;
        int textureHeight = scaled ? height * scale : height;
        _ = SDL_SetRenderLogicalPresentation(renderer, textureWidth, textureHeight,
            scaled ? 1 : LogicalPresentationIntegerScale);
        var texture = SDL_CreateTexture(renderer, PixelFormatArgb8888, TextureAccessStreaming,
            textureWidth, textureHeight);
        if (texture != IntPtr.Zero)
        {
            // nearest neighbour: the game is 320x200 (or 640x480) scaled by an
            // integer factor and must stay crisp; with xBRZ the pixels are
            // scaled by the client itself
            _ = SDL_SetTextureScaleMode(texture, ScaleModeNearest);
        }
        if (texture == IntPtr.Zero)
        {
            error = $"SDL_CreateTexture failed: {LastError()}";
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return null;
        }

        _ = SDL_GetWindowSize(window, out int realW, out int realH);
        _ = SDL_GetWindowPosition(window, out int realX, out int realY);
        return new SdlScreen(window, renderer, texture, textureWidth, textureHeight, scale, scaled)
        {
            Backend = $"{Pretty(Text(SDL_GetCurrentVideoDriver()))} / " +
                      $"{Pretty(Text(SDL_GetRendererName(renderer)))}",
            WindowSize = $"{realW}x{realH}",
            WindowPosition = $"{realX},{realY}",
            Visible = (SDL_GetWindowFlags(window) & WindowHidden) == 0,
        };
    }

    internal static bool PollEvent(IntPtr buffer) => SDL_PollEvent(buffer);

    internal static void Delay(uint ms) => SDL_Delay(ms);
}

/// <summary>A window that shows an 8-bit framebuffer through an ARGB texture.</summary>
public sealed class SdlScreen : IDisposable
{
    private readonly IntPtr _window;
    private readonly IntPtr _renderer;
    private IntPtr _texture;
    private readonly IntPtr _eventBuffer = Marshal.AllocHGlobal(128);
    private int _width;
    private int _height;
    private Array _pixels = Array.Empty<byte>();
    private IntPtr _pinned = IntPtr.Zero;
    private GCHandle _handle;

    private bool _focused = true;
    private readonly int _scale;
    private readonly bool _scaled;

    internal SdlScreen(IntPtr window, IntPtr renderer, IntPtr texture, int width, int height,
                       int scale, bool scaled)
    {
        _window = window;
        _renderer = renderer;
        _texture = texture;
        _width = width;
        _height = height;
        _scale = scale;
        _scaled = scaled;
    }

    /// <summary>Video driver and renderer SDL picked, for the startup line.</summary>
    public string Backend { get; init; } = "?";

    /// <summary>Geometry and visibility as the window system reports them.</summary>
    public string WindowSize { get; init; } = "?";

    /// <summary>
    /// Current window size in pixels.  The renderer uses integer scaling with
    /// letterboxing, so the pointer mapping has to follow the live window (a
    /// maximise changes it), not the scale the window was created with.
    /// </summary>
    public (int Width, int Height) WindowPixels
    {
        get
        {
            if (Sdl.SDL_GetWindowSizeInPixels(_window, out int w, out int h) && w > 0 && h > 0)
            {
                return (w, h);
            }
            _ = Sdl.SDL_GetWindowSize(_window, out w, out h);
            return (Math.Max(1, w), Math.Max(1, h));
        }
    }

    public string WindowPosition { get; init; } = "?";

    public bool Visible { get; init; } = true;

    /// <summary>SDL video driver and renderer actually in use (diagnostics).</summary>
    public string VideoDriver => Marshal.PtrToStringUTF8(Sdl.SDL_GetCurrentVideoDriver()) ?? "?";
    public string RendererName => Marshal.PtrToStringUTF8(Sdl.SDL_GetRendererName(_renderer)) ?? "?";

    /// <summary>
    /// Set when the window needs the last frame again (resize, expose,
    /// pixel-size change): the renderer's back buffer is invalid after those
    /// events, and a static game would otherwise leave a black window.
    /// </summary>
    public bool NeedsRepaint { get; set; }

    public int Width => _width;

    public int Height => _height;

    /// <summary>
    /// Poll SDL; returns false when the user closed the window (or pressed
    /// Ctrl+Q).  Key presses are forwarded as (scancode, ASCII) pairs.
    /// </summary>
    /// <remarks>
    /// Esc is deliberately *not* a local quit: the game needs it (cancel a
    /// dialog).  SDL_KeyboardEvent layout (SDL3): type 0, reserved 4,
    /// timestamp 8, windowID 16, which 20, scancode 24, key 28, mod 32 (u16),
    /// raw 34, down 36, repeat 37.
    /// </remarks>
    public bool Pump(Action<byte, byte>? onKey = null,
        Action<int, int>? onMousePos = null,
        Action<byte, bool>? onMouseButton = null)
    {
        while (Sdl.PollEvent(_eventBuffer))
        {
            uint type = (uint)Marshal.ReadInt32(_eventBuffer);
            if (type is Sdl.EventQuit or Sdl.EventWindowCloseRequested)
            {
                return false;
            }
            if (type is Sdl.EventWindowExposed or Sdl.EventWindowResized
                or Sdl.EventWindowPixelSizeChanged)
            {
                NeedsRepaint = true;
                continue;
            }
            if (type == Sdl.EventWindowFocusLost)
            {
                _focused = false;
                continue;
            }
            if (type == Sdl.EventWindowFocusGained)
            {
                _focused = true;
                continue;
            }
            if (type is Sdl.EventWindowMouseEnter or Sdl.EventWindowMouseLeave)
            {
                continue;
            }
            if (type == Sdl.EventMouseMotion)
            {
                // The window never captures the pointer.  SDL reports the
                // absolute position over the window; the relay maps it
                // proportionally into game pixels, so the game cursor sits
                // exactly where the host pointer sits.
                if (!_focused)
                {
                    continue;
                }
                float x = Marshal.PtrToStructure<float>(_eventBuffer + 28);
                float y = Marshal.PtrToStructure<float>(_eventBuffer + 32);
                onMousePos?.Invoke((int)MathF.Round(x), (int)MathF.Round(y));
                continue;
            }
            if (type is Sdl.EventMouseButtonDown or Sdl.EventMouseButtonUp)
            {
                if (onMouseButton is null)
                {
                    continue;
                }
                byte button = Marshal.ReadByte(_eventBuffer, 24);
                bool pressed = Marshal.ReadByte(_eventBuffer, 25) != 0;
                onMouseButton(button, pressed);
                continue;
            }
            if (type is not (Sdl.EventKeyDown or Sdl.EventKeyUp))
            {
                continue;
            }
            bool down = Marshal.ReadByte(_eventBuffer, 36) != 0;
            bool repeat = Marshal.ReadByte(_eventBuffer, 37) != 0;
            ushort mod = (ushort)Marshal.ReadInt16(_eventBuffer, 32);
            int scancode = Marshal.ReadInt32(_eventBuffer, 24);
            int key = Marshal.ReadInt32(_eventBuffer, 28);
            if ((mod & Sdl.KmodCtrl) != 0 && scancode == Sdl.ScancodeQ)
            {
                return false;
            }
            if (!down || repeat || onKey is null)
            {
                continue;
            }
            if (DosKeys.TryMap(scancode, key, out byte scan, out byte ascii))
            {
                onKey(scan, ascii);
            }
        }
        return true;
    }

    /// <summary>
    /// Rebuild the texture for a new engine mode (the intro runs at 640x480,
    /// the game at 320x200).  The window keeps its size.
    /// </summary>
    public bool Resize(int width, int height)
    {
        int textureWidth = _scaled ? width * _scale : width;
        int textureHeight = _scaled ? height * _scale : height;
        IntPtr texture = Sdl.SDL_CreateTexture(_renderer, Sdl.PixelFormatArgb8888,
            Sdl.TextureAccessStreaming, textureWidth, textureHeight);
        if (texture != IntPtr.Zero)
        {
            _ = Sdl.SDL_SetTextureScaleMode(texture, Sdl.ScaleModeNearest);
        }
        if (texture == IntPtr.Zero)
        {
            return false;
        }
        Sdl.SDL_DestroyTexture(_texture);
        _texture = texture;
        _width = textureWidth;
        _height = textureHeight;
        _ = Sdl.SDL_SetRenderLogicalPresentation(_renderer, textureWidth, textureHeight,
            _scaled ? 1 : Sdl.LogicalPresentationIntegerScale);
        if (_handle.IsAllocated)
        {
            _handle.Free();
            _handle = default;
            _pinned = IntPtr.Zero;
        }
        return true;
    }

    /// <summary>Milliseconds spent inside the last Present call.</summary>
    public double PresentMilliseconds { get; private set; }

    /// <summary>The size of the pixel buffer the texture expects.</summary>
    public int TextureBytes => _width * _height * 4;

    public void Present(byte[] pixels) => Present(pixels, pixels.Length);

    /// <summary>Presents either the raw frame or the scaled one.</summary>
    public void Present(Array pixels) =>
        Present(pixels, pixels is uint[] words ? words.Length * 4 : pixels.Length);

    /// <summary>
    /// Presents 0xAARRGGBB words, which in memory are the ARGB8888 layout the
    /// texture wants, so the scaled pixels need no conversion.
    /// </summary>
    public void Present(uint[] pixels)
    {
        Present(pixels, pixels.Length * 4);
    }

    private void Present(Array pixels, int bytes)
    {
        long started = System.Diagnostics.Stopwatch.GetTimestamp();
        if (!ReferenceEquals(_pixels, pixels) || _pinned == IntPtr.Zero)
        {
            if (_handle.IsAllocated)
            {
                _handle.Free();
            }
            _pixels = pixels;
            _handle = GCHandle.Alloc(pixels, GCHandleType.Pinned);
            _pinned = _handle.AddrOfPinnedObject();
        }

        _ = Sdl.SDL_UpdateTexture(_texture, IntPtr.Zero, _pinned, _width * 4);
        _ = Sdl.SDL_RenderClear(_renderer);
        _ = Sdl.SDL_RenderTexture(_renderer, _texture, IntPtr.Zero, IntPtr.Zero);
        _ = Sdl.SDL_RenderPresent(_renderer);
        PresentMilliseconds = System.Diagnostics.Stopwatch.GetElapsedTime(started).TotalMilliseconds;
    }

    public void Dispose()
    {
        if (_handle.IsAllocated)
        {
            _handle.Free();
        }
        Marshal.FreeHGlobal(_eventBuffer);
        Sdl.SDL_DestroyTexture(_texture);
        Sdl.SDL_DestroyRenderer(_renderer);
        Sdl.SDL_DestroyWindow(_window);
        Sdl.SDL_Quit();
    }
}
