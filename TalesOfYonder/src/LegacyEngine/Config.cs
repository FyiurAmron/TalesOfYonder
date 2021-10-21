// ReSharper disable UnassignedField.Global
namespace TalesOfYonder.LegacyEngine {

public class Config {
    public string picturesVgaFilename;
    public string mainPaletteFilename;
    public string worldDatFilename;
    public string terrainDescriptorsJsonFilename;
    public string objectDescriptorsJsonFilename;
    public string pictureGroupDescriptorsJsonFilename;
    public int mapsX;
    public int mapsY;
    public int tilesPerMapX;
    public int tilesPerMapY;
    public int mapNameCount;
    public int mapNameLength;

    public int tilesX => mapsX * tilesPerMapX;
    public int tilesY => mapsY * tilesPerMapY;
    public int tilesTotal => tilesX * tilesY;
    public int mapsTotal => mapsX * mapsY;
}

}
