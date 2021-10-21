namespace TalesOfYonder.LegacyEngine.Map {

using System.Collections.Generic;

public record TileContainer( int xCount, int yCount, IList<Tile> tiles ) {
    // private WorldTile[] tiles;

    public Tile this[ int x, int y ] {
        get => tiles[y * yCount + x];
        set => tiles[y * yCount + x] = value;
    }

    public TileContainer( int xCount, int yCount ) : this( xCount, yCount, new Tile[xCount * yCount] ) {
    }
}

}
