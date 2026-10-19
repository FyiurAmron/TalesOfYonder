namespace TalesOfYonder.LegacyEngine.Map {

public record Tile(
    byte terrainType,
    byte passthroughFlag,
    byte objectType,
    byte unusedFlag
);

}
