namespace TalesOfYonder.LegacyEngine.Map {

public record WorldTile(
    byte terrainType,
    byte passthroughFlag,
    byte objectType,
    byte unusedFlag
);

}
