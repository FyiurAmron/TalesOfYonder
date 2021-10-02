namespace TalesOfYonder.LegacyEngine {

using System;
using System.Collections.Generic;
using System.Drawing.Imaging;
using System.IO;
using System.Linq;
using System.Windows.Forms;
using Vax.Reversing.Utils;

public sealed class PictureManager : IDisposable {
    public readonly string fileName;
    public readonly PaletteManager paletteManager = new( App.ASSET_PATH );
    public readonly List<List<Picture>> managedPictures = new();

    public Palette defaultPalette;
    public FileStream pictureFileStream;

    public PictureManager( string fileName ) => this.fileName = fileName;

    public void Dispose() {
        managedPictures.forEach( list
                                     => list.forEach( picture
                                                          => picture.Dispose() ) );
        pictureFileStream?.Dispose();
    }

    private void init() {
        paletteManager.add( Const.paletteFilenames );
        defaultPalette = paletteManager[Const.DEFAULT_PALETTE_NAME];

        pictureFileStream = new( App.ASSET_PATH + fileName, FileMode.Open );
    }

    private (List<Picture>,string) loadPictureGroup( PictureGroupDescriptor pictureGroupDescriptor ) {
        int width = pictureGroupDescriptor.picWidth;
        int height = pictureGroupDescriptor.picHeight;
        List<Picture> pictures = new();
        int actualWidth = Misc.roundUp( width, 4 );
        foreach ( int i in ..pictureGroupDescriptor.picCount ) {
            byte[] imgBuf = new byte[actualWidth * height];
            string description = $"{pictureFileStream.Position}";
            foreach ( int y in ..height ) {
                if ( pictureFileStream.Read( imgBuf, y * actualWidth, width ) != width ) {
                    throw new EndOfStreamException();
                }
            }

            ByteBackedBitmap bmp = new( width, height, PixelFormat.Format8bppIndexed, imgBuf );

            bmp.setPalette( ( paletteManager[pictureGroupDescriptor.getPaletteName( i )] ?? defaultPalette ).entries );

            pictures.Add( new( bmp, description ) );
        }

        managedPictures.Add( pictures );

        return (pictures, pictureGroupDescriptor.description);
    }

    private void end() {
        if ( pictureFileStream.Position != pictureFileStream.Length ) {
            MessageBox.Show( "not all data has been read:"
                             + $" pos {pictureFileStream.Position}"
                             + $" vs len {pictureFileStream.Length}" );
        }

        pictureFileStream.Dispose();
    }

    public void processAllPictureGroups(
        IEnumerable<PictureGroupDescriptor> pictureGroupDescriptors,
        Action<(List<Picture> pictures,string description)> action
    ) {
        init();
        
        pictureGroupDescriptors.Select( loadPictureGroup ).forEach( action );
        
        end();
    }
}

}
