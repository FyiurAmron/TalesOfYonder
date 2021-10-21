namespace Vax.Reversing.Utils {

using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;

public class TiledBitmap {
    public readonly Bitmap bitmap;

    private readonly Config config;
    private readonly PixelFormat pixelFormat;
    
    private readonly Rectangle entireTileRectangle;
    private readonly int stride;

    private Size tileSize => config.tileSize; 

    public TiledBitmap( Config config, PixelFormat? pixelFormat = null) {
        this.config = config;
        Bitmap firstTile = config.tiles[0];
        this.pixelFormat = pixelFormat ?? firstTile.PixelFormat;
        entireTileRectangle = new( new( 0, 0 ), config.tileSize );

        bitmap = new( config.width, config.height, this.pixelFormat );
        bitmap.Palette = firstTile.Palette;

        stride = this.pixelFormat.calcStride( config.width );
    }

    public void setTile( int rasterX, int rasterY, int tileIndex ) {
        setTile( new( rasterX, rasterY ), tileIndex );
    }

    public void setTile( Point rasterCoords, int tileIndex ) {
        Bitmap tile = config.tiles[tileIndex];

        BitmapData bitmapDataSrc = tile.LockBits(
            entireTileRectangle,
            ImageLockMode.ReadOnly,
            pixelFormat
        );
        BitmapData bitmapDataDst = bitmap.LockBits(
            new( config.xyFromRasterCoords( rasterCoords ), tileSize ),
            ImageLockMode.WriteOnly,
            pixelFormat
        );

        IntPtr pSrc = bitmapDataSrc.Scan0;
        IntPtr pDst = bitmapDataDst.Scan0;
        int srcStride = pixelFormat.calcStride( tileSize.Width );

        foreach ( int _ in ..tileSize.Height ) {
            ExternHelper.RtlMoveMemory( pDst, pSrc, (uint) tileSize.Width );
            pSrc += srcStride;
            pDst += stride;
        }

        bitmap.UnlockBits( bitmapDataDst );
        tile.UnlockBits( bitmapDataSrc );
    }

    public record Config(
        IList<Bitmap> tiles,
        Size tileSize,
        int tilesX,
        int tilesY
    ) {
        public int width => tilesX * tileSize.Width;
        public int height => tilesY * tileSize.Height;
        
        public Point xyFromRasterCoords( Point rasterCoords ) =>
            new( rasterCoords.X * tileSize.Width, rasterCoords.Y * tileSize.Height );
    }
}

}
