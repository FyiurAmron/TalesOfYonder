namespace TalesOfYonder {

using System;
using System.Drawing.Imaging;
using System.IO;
using System.Windows.Forms;
using Vax.Reversing.Utils;

internal static class Program {
    private const int IMG_WIDTH = 318;
    private const int IMG_HEIGHT = 198;
    private const int IMG_SIZE = IMG_WIDTH * IMG_HEIGHT;

    /// <summary>
    ///     The main entry point for the application.
    /// </summary>
    [STAThread]
    private static void Main() {
        Application.SetHighDpiMode( HighDpiMode.SystemAware );
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault( false );
        Application.Run( new MainForm() );
    }
}

}
