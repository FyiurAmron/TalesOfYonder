// SDL3 scancodes -> IBM PC XT scancodes and the ASCII a DOS program reads.
//
// The game polls DOS int 21h 06h/0Ch (fn_PollInput / fn_WaitKey), so what it
// consumes is exactly what the BIOS keyboard ring holds: an ASCII byte plus a
// scancode.  For extended keys (arrows, function keys, modifiers) DOS returns
// ASCII 0 and the scancode is what the game dispatches on, which is why the
// ASCII field is forced to 0 there.
//
// SDL scancodes are physical positions, so this table is layout-independent;
// the ASCII value comes from the SDL keycode, which already has Shift applied
// (SDL returns 'A' for Shift+A, '!' for Shift+1, and so on).

namespace Yt2.Client;

internal static class DosKeys
{
    /// <summary>Maps one SDL key event; false when the key has no DOS meaning.</summary>
    public static bool TryMap(int scancode, int keycode, out byte scan, out byte ascii)
    {
        scan = 0;
        ascii = 0;
        switch (scancode)
        {
            case >= 4 and <= 29:                       // A..Z -> 1Eh..2Ch
                scan = (byte)(0x1E + (scancode - 4));
                break;
            case >= 30 and <= 38:                      // 1..9 -> 02h..0Ah
                scan = (byte)(0x02 + (scancode - 30));
                break;
            case 39: scan = 0x0B; break;               // 0
            case 40: scan = 0x1C; break;               // Return
            case 41: scan = 0x01; break;               // Escape
            case 42: scan = 0x0E; break;               // Backspace
            case 43: scan = 0x0F; break;               // Tab
            case 44: scan = 0x39; break;               // Space
            case 45: scan = 0x0C; break;               // -
            case 46: scan = 0x0D; break;               // =
            case 47: scan = 0x1A; break;               // [
            case 48: scan = 0x1B; break;               // ]
            case 49: scan = 0x2B; break;               // backslash
            case 51: scan = 0x27; break;               // ;
            case 52: scan = 0x28; break;               // '
            case 53: scan = 0x29; break;               // `
            case 54: scan = 0x33; break;               // ,
            case 55: scan = 0x34; break;               // .
            case 56: scan = 0x35; break;               // /
            case 57: scan = 0x3A; break;               // CapsLock
            case 58: scan = 0x3B; break;               // F1
            case 59: scan = 0x3C; break;
            case 60: scan = 0x3D; break;
            case 61: scan = 0x3E; break;
            case 62: scan = 0x3F; break;
            case 63: scan = 0x40; break;
            case 64: scan = 0x41; break;
            case 65: scan = 0x42; break;
            case 66: scan = 0x43; break;
            case 67: scan = 0x44; break;               // F10
            case 68: scan = 0x57; break;               // F11
            case 69: scan = 0x58; break;               // F12
            case 70: scan = 0x46; break;               // ScrollLock
            case 73: scan = 0x52; break;               // Insert
            case 74: scan = 0x47; break;               // Home
            case 75: scan = 0x49; break;               // PageUp
            case 76: scan = 0x53; break;               // Delete
            case 77: scan = 0x4F; break;               // End
            case 78: scan = 0x51; break;               // PageDown
            case 79: scan = 0x4D; break;               // Right
            case 80: scan = 0x4B; break;               // Left
            case 81: scan = 0x50; break;               // Down
            case 82: scan = 0x48; break;               // Up
            case 224: scan = 0x1D; break;              // Left Ctrl
            case 225: scan = 0x2A; break;              // Left Shift
            case 226: scan = 0x38; break;              // Left Alt
            case 227: scan = 0x5B; break;              // Left GUI (no XT key)
            case 228: scan = 0x1D; break;              // Right Ctrl
            case 229: scan = 0x36; break;              // Right Shift
            case 230: scan = 0x38; break;              // Right Alt
            case 231: scan = 0x5C; break;              // Right GUI (no XT key)
            default:
                return false;                          // keypad and the rest
        }

        // Function keys, arrows and modifiers are "extended": DOS hands the
        // program ASCII 0 and the scancode.  Everything below is a character.
        if (scan < 0x3B && keycode is >= 0x08 and <= 0x7E)
        {
            ascii = (byte)keycode;
        }
        return true;
    }
}
