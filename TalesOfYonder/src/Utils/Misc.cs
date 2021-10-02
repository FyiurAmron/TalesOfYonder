namespace Vax.Reversing.Utils {

using System.Text;

public static class Misc {
    public static Encoding defaultEncoding { get; set; } = Encoding.ASCII;

    public static uint roundUp( uint n, uint multiple ) => ( n + ( multiple - 1 ) ) / multiple * multiple;

    public static int roundUp( int n, int multiple ) => ( n + ( multiple - 1 ) ) / multiple * multiple;
}

}
