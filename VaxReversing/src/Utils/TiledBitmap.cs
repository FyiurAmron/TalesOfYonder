namespace Vax.Reversing.Utils {

using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;

public class TiledBitmap {
    private static readonly Point ZERO = new();

    public readonly Bitmap bitmap;

    private readonly Config config;
    private readonly PixelFormat pixelFormat;

    private readonly Rectangle entireTileRectangle;
    private readonly int stride;

    private Size tileSize => config.tileSize;

    public TiledBitmap( Config config, byte? filler = null, PixelFormat? pixelFormat = null ) {
        this.config = config;
        Bitmap firstTile = config.tiles[0];
        this.pixelFormat = pixelFormat ?? firstTile.PixelFormat;
        entireTileRectangle = new( ZERO, config.tileSize );

        bitmap = new( config.width, config.height, this.pixelFormat );
        bitmap.Palette = firstTile.Palette;

        stride = bitmap.fill( new( ZERO, config.totalSize ), filler );
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
        int srcStride = bitmapDataSrc.Stride;

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
        public Size totalSize => new( width, height );

        public Point xyFromRasterCoords( Point rasterCoords ) =>
            new( rasterCoords.X * tileSize.Width, rasterCoords.Y * tileSize.Height );
    }
}

}
