namespace TalesOfYonder.LegacyEngine {

using System.Drawing;
using System.IO;
using Vax.Reversing.Utils;

public class Palette {
    public const int ENTRIES_TOTAL = 256;
    public const int ENTRY_SIZE = 4;
    public const int BYTES_TOTAL = ENTRIES_TOTAL * ENTRY_SIZE;
    public const int RIFF_OFFSET = 24;

    public readonly Color[] entries = new Color[ENTRIES_TOTAL];

    public Palette( string fileName ) {
        using FileStream fs = new( fileName, FileMode.Open );
        byte[] rawEntries = new byte[BYTES_TOTAL];
        fs.Read( rawEntries, 0, BYTES_TOTAL );
        for ( int i = 0, j = 0; i < ENTRIES_TOTAL; i++, j += ENTRY_SIZE ) {
            entries[i] = Color.FromArgb(
                rawEntries[j + 3],
                rawEntries[j + 0],
                rawEntries[j + 1],
                rawEntries[j + 2]
            );
        }
    }

    public static void convert( string fileName ) {
        using BinaryReader reader = new( new FileStream( fileName, FileMode.Open ) );
        using BinaryWriter writer = new( new FileStream( fileName + ".pal", FileMode.Create ) );

        reader.skip( RIFF_OFFSET );
        byte[] buf = reader.ReadBytes( BYTES_TOTAL );

        for ( int i = 3; i < BYTES_TOTAL; i += ENTRY_SIZE ) {
            buf[i] = 0xFF;
        }

        writer.Write( buf );
    }
}

}
