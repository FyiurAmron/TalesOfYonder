namespace TalesOfYonder.LegacyEngine.Gfx {

using System.Collections.Generic;
using System.IO;
using Vax.Reversing.Utils;

public class PaletteManager {
    private readonly Dictionary<string, Palette> paletteMap = new();
    private readonly string assetPath;

    public Palette this[ string fileName ]
        => ( fileName == null ) ? null : paletteMap[fileName];

    public PaletteManager( string assetPath )
        => this.assetPath = assetPath;

    public void add( params string[] fileNames ) {
        add( (IEnumerable<string>) fileNames );
    }

    public void add( IEnumerable<string> fileNames ) {
        fileNames.forEach(
            fileName => paletteMap[fileName] = new( Path.Combine( assetPath, fileName ) )
        );
    }
}

}
