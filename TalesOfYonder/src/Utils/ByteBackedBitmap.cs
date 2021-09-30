namespace Vax.Reversing.Utils {

using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public sealed class ByteBackedBitmap : IDisposable {
    private GCHandle pinnedBytes;

    public ByteBackedBitmap( int width, int height, PixelFormat pixelFormat, byte[] bytes = null ) {
        bitsPerPixel = ( (uint) pixelFormat << 16 ) >> 24;
        stride = Misc.roundUp( (uint) width * bitsPerPixel, 4 * 8 ) / 8;
        // stride = 318;
        this.bytes = bytes ?? new byte[height * stride];

        pinnedBytes = GCHandle.Alloc( bytes, GCHandleType.Pinned );

        bitmap = new( width, height, (int) stride, pixelFormat, pinnedBytes.AddrOfPinnedObject() );
    }

    public Bitmap bitmap { get; }
    public byte[] bytes { get; }

    public uint stride { get; }
    public uint bitsPerPixel { get; }

    public void Dispose() {
        bitmap?.Dispose();
        // pinnedBytes.Free();
    }

    public void setPalette( Color[] palette ) {
        setPalette( i => palette[i] );
    }

    public void setPalette( /* [InstantHandle] */ Func<byte, Color> setter ) {
        ColorPalette cp = bitmap.Palette;
        Color[] cpEntries = cp.Entries;

        for ( short i = 0; i < cpEntries.Length; i++ ) { // max palette size is 1 << 8 (256) per def
            cpEntries[i] = setter( (byte) i );
        }

        bitmap.Palette = cp;
    }
}

}
