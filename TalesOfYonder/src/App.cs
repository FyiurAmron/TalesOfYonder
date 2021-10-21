namespace TalesOfYonder {

using System;
using System.Windows.Forms;
using LegacyEngine;

public static class App {
    public const string NAME = "Tales of Yonder";

    public const string ASSET_PATH = "asset/";
    
    public static readonly Engine engine = new( "YT2.json" );

    /// <summary>
    ///     The main entry point for the application.
    /// </summary>
    [STAThread]
    private static void Main() {
        // ExternHelper.AllocConsole();

        runForm();
    }

    private static void runForm() {
        Application.SetHighDpiMode( HighDpiMode.SystemAware );
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault( false );
        Application.Run( new MainForm() );
    }
}

}
