namespace Vax.Reversing.Utils {

using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;

public class TiledBitmap {
    public readonly Bitmap bitmap;
    public readonly IList<Bitmap> tiles;

    private readonly Size tileSize;
    private readonly PixelFormat pixelFormat;
    private readonly Rectangle entireTileRectangle;
    private readonly int stride;

    public TiledBitmap( IList<Bitmap> tiles,
                        Size tileSize, int tilesX, int tilesY, PixelFormat? pixelFormat = null ) {
        this.tiles = tiles;
        this.tileSize = tileSize;
        this.pixelFormat = pixelFormat ?? tiles[0].PixelFormat;
        entireTileRectangle = new( new( 0, 0 ), tileSize );

        int width = tilesX * tileSize.Width;
        int height = tilesY * tileSize.Height;
        bitmap = new( width, height, this.pixelFormat );
        bitmap.Palette = tiles[0].Palette;

        stride = this.pixelFormat.calcStride( width );
    }

    public void setTile( int rasterX, int rasterY, int tileIndex ) {
        setTile( new( rasterX, rasterY ), tileIndex );
    }

    public void setTile( Point rasterCoords, int tileIndex ) {
        Bitmap tile = tiles[tileIndex];

        BitmapData bitmapDataSrc = tile.LockBits(
            entireTileRectangle,
            ImageLockMode.ReadOnly,
            pixelFormat
        );
        BitmapData bitmapDataDst = bitmap.LockBits(
            new( xyFromRasterCoords( rasterCoords ), tileSize ),
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

    protected Point xyFromRasterCoords( Point rasterCoords ) =>
        new( rasterCoords.X * tileSize.Width, rasterCoords.Y * tileSize.Height );
}

}
