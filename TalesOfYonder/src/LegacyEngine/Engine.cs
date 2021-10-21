namespace TalesOfYonder.LegacyEngine {

using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text.Json;
using Gfx;
using Map;
using Vax.Reversing.Utils;

public class Engine : IDisposable {
    public readonly PictureManager pictureManager;

    protected readonly string assetPath;
    protected readonly Config config;
    protected readonly PictureGroupDescriptor[] pictureGroupDescriptors;

    protected readonly List<MapDescriptor> mapDescriptors = new();
    protected readonly List<string> mapNames = new();

    protected Dictionary<byte, int> terrainToIconMap;
    protected Dictionary<byte, int> objectToIconMap;

    protected TerrainDescriptor[] terrainDescriptors;
    protected ObjectDescriptor[] objectDescriptors;

    protected TileContainer tiles;

    public Engine( string rootJsonFilename, string assetPath = App.ASSET_PATH ) {
        this.assetPath = assetPath;
        config = readJson<Config>( rootJsonFilename );
        pictureGroupDescriptors = readJson<PictureGroupDescriptor[]>( config.pictureGroupDescriptorsJsonFilename );

        List<string> paletteFilenames = new() {
            config.mainPaletteFilename,
        };

        IEnumerable<string> paletteFilenamesStr
            = pictureGroupDescriptors
              .Select(
                  pictureGroupDescriptor => pictureGroupDescriptor.paletteSelector?.Values.ToList() )
              .Where( list => list != null )
              .SelectMany( list => list );

        paletteFilenames.AddRange( paletteFilenamesStr );

        pictureManager = new( assetPath, config.picturesVgaFilename, paletteFilenames );
    }

    private T readJson<T>( string filename ) =>
        JsonSerializer.Deserialize<T>(
            File.ReadAllText( Path.Combine( assetPath, filename ) ),
            new() {
                IncludeFields = true,
            } );

    private static Dictionary<byte, int> createIconMap( IEnumerable<IMappable> mappables ) {
        Dictionary<byte, int> iconMap = new();

        byte i = 0;
        foreach ( IMappable mappable in mappables ) {
            if ( mappable != null ) {
                if ( !Enum.TryParse( mappable.mapIcon, out Icon8x8Enum icon8X8Enum ) ) {
                    throw new InvalidDataException();
                }

                iconMap[i] = (int) icon8X8Enum;
            }

            i++;
        }

        return iconMap;
    }

    public void loadWorldData() {
        terrainDescriptors = readJson<TerrainDescriptor[]>( config.terrainDescriptorsJsonFilename );
        objectDescriptors = readJson<ObjectDescriptor[]>( config.objectDescriptorsJsonFilename );

        using FileStream fs = new( Path.Combine( App.ASSET_PATH, config.worldDatFilename ), FileMode.Open );

        using BinaryReader reader = new( fs );

        tiles = new( config.tilesX, config.tilesY );

        int[] terrainIndexBuckets = new int[256];
        int[] passthroughFlagBucket = new int[256];
        int[] objectIndexBuckets = new int[256];
        int[] unusedFlagBucket = new int[256];

        foreach ( int y in ..config.tilesY ) {
            foreach ( int x in ..config.tilesX ) {
                byte terrainIndex = reader.ReadByte();
                terrainIndexBuckets[terrainIndex]++;
                byte passthroughFlag = reader.ReadByte(); // == 0 in YT2, 0 (block) or 1 (passthrough) in YT3
                passthroughFlagBucket[passthroughFlag]++;
                byte objectIndex = reader.ReadByte(); // <0,67>
                objectIndexBuckets[objectIndex]++;
                byte unusedFlag = reader.ReadByte(); // == 0 (both YT2 & YT3)
                unusedFlagBucket[unusedFlag]++;

                tiles[x, y] = new( terrainIndex, passthroughFlag, objectIndex, unusedFlag );
            }
        }

        /*
        "secret" (unused, but semi-complete) levels:
        - THIEF'S DEN LEVEL 3
        - ANCIENT RUINS-PRISON
        */

        foreach ( int _ in ..config.mapsTotal ) {
            MapDescriptor mapDescriptor = new() {
                suffix = reader.readString( 4 ),
                nameIndex = reader.ReadByte(),
                type = reader.ReadByte(),
            };
            mapDescriptors.Add( mapDescriptor );
        }

        foreach ( int _ in ..config.mapNameCount ) {
            mapNames.Add( reader.readString( config.mapNameLength ).TrimEnd() );
        }

        List<MapDescriptor> validMapDescriptors = new();

        bool hadPortHope = false; // note: fake maps at the borders/edges
        foreach ( MapDescriptor md in mapDescriptors ) {
            switch ( md.type ) {
                case 0:
                    md.fullName = "- filler - [limbo]";
                    break;
                case 1:
                case 2:
                    md.fullName = mapNames[md.nameIndex];
                    if ( md.suffix != "0   " ) {
                        md.fullName +=
                            ( md.suffix[0] == '0' )
                                ? " MAP " + md.suffix[1]
                                : " LEVEL " + md.suffix[0];
                    } else {
                        if ( md.nameIndex == 0 ) { // fake map OR real Port Hope
                            if ( md.type == 1 ) {
                                md.fullName = "- filler -";
                            } else {
                                if ( !hadPortHope ) {
                                    hadPortHope = true;
                                } else {
                                    md.fullName = "- unnamed -";
                                }
                            }
                        }
                    }

                    string typeSuffix = md.type == 1 ? "world" : "dungeon";
                    md.fullName += $" [{typeSuffix}]";
                    break;
            }

            validMapDescriptors.add( md );
        }
    }

    public IReadOnlyList<Bitmap> createOverheadMaps() {
        terrainToIconMap = createIconMap( terrainDescriptors );
        objectToIconMap = createIconMap( objectDescriptors );

        Size mapTileSize = new( 8, 8 );
        TiledBitmap.Config tbc = new(
            pictureManager.getBitmaps( 9 ).ToList(),
            mapTileSize,
            config.tilesX,
            config.tilesY
        );
        TiledBitmap terrainBitmap = new( tbc );
        TiledBitmap objectBitmap = new( tbc );
        List<Bitmap> maps = new() {
            terrainBitmap.bitmap,
            objectBitmap.bitmap,
        };

        foreach ( int y in ..config.tilesY ) {
            foreach ( int x in ..config.tilesX ) {
                Tile tile = tiles[x, y];
                terrainBitmap.setTile( x, y, terrainToIconMap[tile.terrainType] );
                // objectBitmap.setTile( x, y, objectToIconMap[tile.objectType] );
            }
        }

        return maps;
    }

    public IReadOnlyList<PictureGroup> processAllPictureGroups() =>
        pictureManager.processAllPictureGroups( pictureGroupDescriptors );

    public void Dispose() {
        pictureManager.Dispose();
        GC.SuppressFinalize( this );
    }
}

}
