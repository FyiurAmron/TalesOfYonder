namespace TalesOfYonder.LegacyEngine {

using System.IO;
using Vax.Reversing.Utils;

public static class Misc {
    private static void convertGoat() {
        using BinaryReader reader = new( new FileStream( App.ASSET_PATH + "goat.dat", FileMode.Open ) );
        using BinaryWriter writer = new( new FileStream( App.ASSET_PATH + "goat.raw", FileMode.Create ) );
        foreach ( int _ in ..1024 ) {
            writer.Write( (byte) reader.Read() );
            reader.Read();
        }
    }
}

}
