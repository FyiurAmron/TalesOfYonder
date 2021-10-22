// ReSharper disable InconsistentNaming
// ReSharper disable UnusedType.Global

namespace Vax.Reversing.Utils {

using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Linq;
using System.Numerics;
using System.Text;
using static Misc;

public static class NumericExtensions {
    public static uint roundUp( this uint n, uint multiple )
        => ( n + ( multiple - 1 ) ) / multiple * multiple;

    public static int roundUp( this int n, int multiple )
        => ( n + ( multiple - 1 ) ) / multiple * multiple;
}

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

public static class RangeExtensions {
    public static Enumerator GetEnumerator( this Range range ) => new( range );

    public struct Enumerator {
        private readonly int start;
        private readonly int end;

        public Enumerator( Range range ) {
            start = Current = range.Start.Value - 1;
            end = range.End.Value - 1;
        }

        public bool MoveNext() {
            if ( Current == end ) {
                return false;
            }

            Current++;
            return true;
        }

        public void Reset() {
            Current = start;
        }

        public int Current { get; private set; }

        public void Dispose() {
        }
    }
}

public static class IntExtensions {
    public static (byte, byte) toBytes( this short s )
        => ( (byte) s, (byte) ( s >> 8 ) );
}

public static class StringExtensions {
    public static byte[] toBytes( this string s, Encoding encoding = null )
        => ( encoding ?? defaultEncoding ).GetBytes( s );

    public static string join( this IEnumerable<string> strings, string separator )
        => string.Join( separator, strings );

    public static StringBuilder appendAll( this StringBuilder sb, IEnumerable<string> strings ) {
        strings.forEach( s => sb.Append( s ) );

        return sb;
    }
}

public static class IEnumerableExtensions {
    public static void forEach<T>( this IEnumerable<T> iEnumerable, Action<T> action ) {
        foreach ( T t in iEnumerable ) {
            action( t );
        }
    }

    public static string toString<T>( this IEnumerable<T> iEnumerable )
        => "[" + string.Join( "; ", iEnumerable ) + "]";

    public static object[] toObjectArray<T>( this IEnumerable<T> iEnumerable ) {
        T[] tArr = iEnumerable.ToArray();
        object[] arr = new object[tArr.Length];

        Array.Copy( tArr, arr, tArr.Length );

        return arr;
    }
}

public static class ArrayExtensions {
    public static string toString( this byte[] bytes, Encoding encoding = null )
        => ( encoding ?? defaultEncoding ).GetString( bytes );

    public static short getShort( this byte[] bytes, int pos )
        => (short) ( bytes[pos] | ( bytes[pos + 1] << 8 ) );
}

public static class ListExtensions {
    public static void add<T>( this List<T> list, params T[] elems ) =>
        list.AddRange( elems );
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

public static class SizeExtensions {
    public static int area( this Size size )
        => size.Width * size.Height;
}

public static class BinaryReaderExtensions {
    public static void read( this BinaryReader binaryReader, byte[] buffer, int offset, int count ) {
        binaryReader.BaseStream.Read( buffer, offset, count );
    }

    public static void read( this BinaryReader binaryReader, byte[] buffer, int count ) {
        binaryReader.read( buffer, 0, count );
    }

    public static void skip( this BinaryReader binaryReader, int offset ) {
        binaryReader.seek( offset, SeekOrigin.Current );
    }

    public static void seek( this BinaryReader binaryReader, int offset, SeekOrigin origin = SeekOrigin.Begin ) {
        binaryReader.BaseStream.Seek( offset, origin );
    }

    public static bool isAvailable( this BinaryReader binaryReader ) {
        Stream stream = binaryReader.BaseStream;
        return stream.Position < stream.Length;
    }

    public static string readString( this BinaryReader binaryReader, int length, Encoding encoding = null )
        => ( encoding ?? Encoding.Default ).GetString( binaryReader.ReadBytes( length ) );

    public static string readStringZ( this BinaryReader binaryReader, int length, Encoding encoding = null )
        => binaryReader.readString( length, encoding ).TrimEnd( (char) 0 );

    public static Color readColorRGBA( this BinaryReader binaryReader ) {
        byte[] rgba = binaryReader.ReadBytes( 4 );
        return Color.FromArgb( rgba[3], rgba[0], rgba[1], rgba[2] );
    }

    public static Color readColorRGB( this BinaryReader binaryReader ) {
        byte[] rgb = binaryReader.ReadBytes( 3 );
        return Color.FromArgb( rgb[0], rgb[1], rgb[2] );
    }

    public static Vector3 readVector3f( this BinaryReader binaryReader ) =>
        new(
            binaryReader.ReadSingle(),
            binaryReader.ReadSingle(),
            binaryReader.ReadSingle()
        );

    public static Vector2 readVector2f( this BinaryReader binaryReader ) =>
        new(
            binaryReader.ReadSingle(),
            binaryReader.ReadSingle()
        );
}

}
