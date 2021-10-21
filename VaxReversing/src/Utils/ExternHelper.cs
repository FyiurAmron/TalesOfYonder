#pragma warning disable CA1401 // P/Invoke are public here
namespace Vax.Reversing.Utils {

using System;
using System.Runtime.InteropServices;

public class ExternHelper {
    [DllImport( "shell32.dll", CharSet = CharSet.Unicode )]
    public static extern int ShellAbout( IntPtr hWnd, string szApp, string szOtherStuff, IntPtr hIcon );

    [DllImport( "kernel32.dll" )]
    public static extern bool AllocConsole();

    [DllImport( "kernel32.dll" )]
    public static extern void RtlMoveMemory( IntPtr dest, IntPtr src, /*size_t*/ uint count );

    public static void showShellAboutDialog(
        string appName = "", string additionalDesc = "", IntPtr? hWnd = null, IntPtr? hIcon = null ) {
        if ( ShellAbout( hWnd ?? IntPtr.Zero, appName, additionalDesc, hIcon ?? IntPtr.Zero ) == 0 ) {
            throw new InvalidOperationException();
        }
    }
}

}
