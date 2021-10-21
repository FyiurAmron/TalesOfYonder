// ReSharper disable UnassignedField.Global

namespace TalesOfYonder.LegacyEngine.Map {

/// <summary>
///
/// </summary>
public class ObjectDescriptor : IMappable {
    public string[][] objectTextures; // open/closed, N/E/S/W
    public string mapIcon { get; set; }
}

}
