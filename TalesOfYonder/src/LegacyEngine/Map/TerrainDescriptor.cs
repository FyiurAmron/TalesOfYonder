// ReSharper disable UnassignedField.Global

namespace TalesOfYonder.LegacyEngine.Map {

/// <summary>
///     0x39 - max map layer 0 index (real max: 0x4D, further entries cause crashes)
/// </summary>
public class TerrainDescriptor : IMappable {
    public string topTexture;
    public string bottomTexture;
    public string wallTexture;
    public string mapIcon { get; set; }
}

}
