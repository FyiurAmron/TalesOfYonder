namespace Vax.Reversing.Utils {

using System.Drawing;
using System.Drawing.Imaging;
using static Const;

public static class PixelFormatExtensions {
    public static uint getBitsPerPixel( this PixelFormat pixelFormat )
        => ( (uint) pixelFormat << 16 ) >> 24;

    public static int calcStride( this PixelFormat pixelFormat, int width )
        => (int) (
            ( (uint) width * pixelFormat.getBitsPerPixel() )
            .roundUp( STRIDE_BYTES_ROUNDING * BITS_PER_BYTE )
            / BITS_PER_BYTE
        );
}

public static class BitmapExtensions {
    public static int fill( this Bitmap bitmap, Rectangle rectangle, byte? filler = null ) {
        BitmapData bitmapData = bitmap.LockBits(
            rectangle,
            ImageLockMode.WriteOnly,
            bitmap.PixelFormat
        );

        if ( filler != null ) {
            ExternHelper.RtlFillMemory( bitmapData.Scan0, (uint) bitmapData.length(), filler.Value );
        }

        bitmap.UnlockBits( bitmapData );

        return bitmapData.Stride;
    }
}

public static class BitmapDataExtensions {
    public static void copyTo( this BitmapData src, BitmapData dst, uint? count = null ) {
        ExternHelper.RtlMoveMemory(
            dst.Scan0,
            src.Scan0,
            count ?? (uint) src.length()
        );
    }

    public static int length( this BitmapData bitmapData )
        => bitmapData.Stride * bitmapData.Height;
}

}
