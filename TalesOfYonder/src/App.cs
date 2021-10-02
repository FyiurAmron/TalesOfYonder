namespace TalesOfYonder {

using System;
using System.IO;
using System.Windows.Forms;
using Vax.Reversing.Utils;

public static class App {
    public const string NAME = "Tales of Yonder";

    public const string ASSET_PATH = "asset/";

    /// <summary>
    ///     The main entry point for the application.
    /// </summary>
    [STAThread]
    private static void Main() {
        // convertGoat();
        // Palette.convert( ASSET_PATH + "webfoot.pal" );

        runForm();
    }

    private static void convertGoat() {
        using BinaryReader reader = new( new FileStream( ASSET_PATH + "goat.data", FileMode.Open ) );
        using BinaryWriter writer = new( new FileStream( ASSET_PATH + "goat.raw", FileMode.Create ) );
        foreach ( int _ in ..1024 ) {
            writer.Write( (byte) reader.Read() );
            reader.Read();
        }
    }

    private static void runForm() {
        Application.SetHighDpiMode( HighDpiMode.SystemAware );
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault( false );
        Application.Run( new MainForm() );
    }
}

}
