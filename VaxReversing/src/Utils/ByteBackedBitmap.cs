namespace Vax.Reversing.Utils {

using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public sealed class ByteBackedBitmap : IDisposable {
    private GCHandle pinnedBytes;

    public Bitmap bitmap { get; }
    public byte[] bytes { get; }

    public int stride { get; }

    public ByteBackedBitmap( Size size, PixelFormat pixelFormat, byte[] bytes = null )
        : this( size.Width, size.Height, pixelFormat, bytes ) {
    }

    public ByteBackedBitmap( int width, int height, PixelFormat pixelFormat, byte[] bytes = null ) {
        stride = pixelFormat.calcStride( width );

        this.bytes = bytes ?? new byte[height * stride];

        pinnedBytes = GCHandle.Alloc( bytes, GCHandleType.Pinned );

        bitmap = new( width, height, stride, pixelFormat, pinnedBytes.AddrOfPinnedObject() );
    }

    public void setPalette( Color[] palette ) {
        setPalette( i => palette[i] );
    }

    public void setPalette( /* [InstantHandle] */ Func<byte, Color> setter ) {
        ColorPalette cp = bitmap.Palette;
        Color[] cpEntries = cp.Entries;

        foreach ( int i in ..cpEntries.Length ) { // max palette size is 1 << 8 (256) per def
            cpEntries[i] = setter( (byte) i );
        }

        bitmap.Palette = cp;
    }

    public void Dispose() {
        bitmap?.Dispose();
        pinnedBytes.Free();
    }
}

}
