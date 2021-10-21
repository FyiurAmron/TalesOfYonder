namespace TalesOfYonder.LegacyEngine.Gfx {

using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Linq;
using System.Windows.Forms;
using Vax.Reversing.Utils;

public sealed class PictureManager : IDisposable {
    private readonly List<List<Picture>> managedPictures = new();

    private readonly string assetPath;
    private readonly string fileName;
    private readonly IEnumerable<string> paletteFilenames;

    private readonly PaletteManager paletteManager;

    private Palette defaultPalette;
    private FileStream pictureFileStream;

    public PictureManager( string assetPath, string fileName, IEnumerable<string> paletteFilenames ) {
        this.assetPath = assetPath;
        this.fileName = fileName;
        this.paletteFilenames = paletteFilenames;
        paletteManager = new( assetPath );
    }

    public List<Picture> getPictureGroup( int groupNr )
        => managedPictures[groupNr];
    
    public IEnumerable<Bitmap> getBitmaps( int groupNr )
        => getPictureGroup( groupNr ).Select( picture => picture.bitmap );

    private void init() {
        paletteManager.add( paletteFilenames );
        defaultPalette = paletteManager[paletteFilenames.First()];

        pictureFileStream = new( Path.Combine( assetPath, fileName ), FileMode.Open );
    }

    private (List<Picture>, string) loadPictureGroup( PictureGroupDescriptor pictureGroupDescriptor ) {
        int width = pictureGroupDescriptor.picWidth;
        int height = pictureGroupDescriptor.picHeight;
        List<Picture> pictures = new();
        int actualWidth = width.roundUp( 4 );
        foreach ( int i in ..pictureGroupDescriptor.picCount ) {
            byte[] imgBuf = new byte[actualWidth * height];
            string description = $"{pictureFileStream.Position}";
            foreach ( int y in ..height ) {
                if ( pictureFileStream.Read( imgBuf, y * actualWidth, width ) != width ) {
                    throw new EndOfStreamException();
                }
            }

            ByteBackedBitmap bmp = new( width, height, PixelFormat.Format8bppIndexed, imgBuf );

            bmp.setPalette(
                ( paletteManager[pictureGroupDescriptor.getPaletteName( i )] ?? defaultPalette ).entries
            );

            pictures.Add( new( bmp, description ) );
        }

        managedPictures.Add( pictures );

        return ( pictures, pictureGroupDescriptor.description );
    }

    private void end() {
        if ( pictureFileStream.Position != pictureFileStream.Length ) {
            MessageBox.Show( "not all data has been read:"
                             + $" pos {pictureFileStream.Position}"
                             + $" vs len {pictureFileStream.Length}" );
        }

        pictureFileStream.Dispose();
    }

    public IEnumerable<(List<Picture>, string)> processAllPictureGroups(
        IEnumerable<PictureGroupDescriptor> pictureGroupDescriptors,
        Action<(List<Picture> pictures, string description)> action = null
    ) {
        init();

        // forced evaluation
        List<(List<Picture>, string)> ret = pictureGroupDescriptors.Select( loadPictureGroup ).ToList();

        if ( action != null ) {
            ret.forEach( action );
        }

        end();

        return ret;
    }

    public void Dispose() {
        managedPictures.forEach( list => list.forEach( picture => picture.Dispose() ) );
        pictureFileStream?.Dispose();
    }
}

}
