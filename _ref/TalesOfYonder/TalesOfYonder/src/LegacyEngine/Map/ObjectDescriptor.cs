// ReSharper disable UnassignedField.Global

namespace TalesOfYonder.LegacyEngine.Map {

/// <summary>
///
/// </summary>
public class ObjectDescriptor : IMappable {
    public string[][] objectTextures; // open/closed, N/E/S/W
    public string mapIcon { get; set; }
    public string facing { get; set; } // Direction
    public bool open { get; set; }
    // TODO blocking
    // TODO bind with obj gfx/logic
}

}
