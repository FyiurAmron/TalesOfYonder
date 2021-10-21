// ReSharper disable UnassignedField.Global
namespace TalesOfYonder.LegacyEngine.Gfx {

using System.Collections.Generic;

public struct PictureGroupDescriptor {
    public string description;
    public int picWidth;
    public int picHeight;
    public int picCount;
    public Dictionary<int, string> paletteSelector;

    public string getPaletteName( int picNr ) {
        if ( paletteSelector == null ) {
            return null;
        }

        bool hasName = paletteSelector.TryGetValue( picNr, out string paletteFileName );
        return hasName ? paletteFileName : null;
    }
}

}
