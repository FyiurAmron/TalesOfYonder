namespace TalesOfYonder {

using System;
using System.Windows.Forms;

public static class App {
    public const string NAME = "Tales of Yonder";

    public const string ASSET_PATH = "asset/";

    /// <summary>
    ///     The main entry point for the application.
    /// </summary>
    [STAThread]
    private static void Main() {
        // Palette.convert( ASSET_PATH + "webfoot.pal" );

        Application.SetHighDpiMode( HighDpiMode.SystemAware );
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault( false );
        Application.Run( new MainForm() );
    }
}

}
