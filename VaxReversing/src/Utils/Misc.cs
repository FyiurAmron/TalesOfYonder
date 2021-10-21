namespace Vax.Reversing.Utils {

using System.Text;

public static class Misc {
    public const uint BITS_PER_BYTE = 8;
    public const uint STRIDE_BYTES_ROUNDING = 4; // as per the definition from BitmapData.Stride
    public static Encoding defaultEncoding { get; set; } = Encoding.ASCII;

    public static uint roundUp( uint n, uint multiple ) => ( n + ( multiple - 1 ) ) / multiple * multiple;

    public static int roundUp( int n, int multiple ) => ( n + ( multiple - 1 ) ) / multiple * multiple;
}

}
