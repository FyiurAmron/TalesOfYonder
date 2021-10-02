namespace TalesOfYonder.LegacyEngine {

using System;
using System.Drawing;
using Vax.Reversing.Utils;

public sealed class Picture : IDisposable {
    private readonly ByteBackedBitmap byteBackedBitmap;
    public readonly string description;

    public Image bitmap => byteBackedBitmap.bitmap;

    public Picture( ByteBackedBitmap byteBackedBitmap, string description )
        => ( this.byteBackedBitmap, this.description )
            = ( byteBackedBitmap, description );

    public void Dispose() {
        byteBackedBitmap?.Dispose();
    }
}

}
