namespace TalesOfYonder.LegacyEngine {

using System.Collections.Generic;
using System.IO;

public class PaletteManager {
    private readonly Dictionary<string, Palette> paletteMap = new();
    private readonly string assetPath;

    public PaletteManager( string assetPath )
        => this.assetPath = assetPath;

    public Palette this[ string fileName ]
        => ( fileName == null ) ? null : paletteMap[fileName];

    public void add( params string[] fileNames ) {
        foreach ( string fileName in fileNames ) {
            paletteMap[fileName] = new( Path.Combine( assetPath, fileName ) );
        }
    }
}

}
