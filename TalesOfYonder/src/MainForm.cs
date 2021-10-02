namespace TalesOfYonder {

using System;
using System.Collections.Generic;
using System.Drawing.Imaging;
using System.IO;
using System.Linq;
using System.Windows.Forms;
using LegacyEngine;
using Vax.FormUtils;
using Vax.Reversing.Utils;

public partial class MainForm : AutoForm {
    private readonly List<IDisposable> disposables = new();

    private readonly PaletteManager paletteManager = new( App.ASSET_PATH );
    private Palette defaultPalette;

    public MainForm() : base( layoutPanel:
                              new TableLayoutPanel() {
                                  // BackColor = System.Drawing.Color.Black,
                                  AutoSize = true,
                                  Dock = DockStyle.Fill,
                                  AutoScroll = true
                              } ) {
        InitializeComponent();

        loadAllPictures( Const.YT2_PICTURE_FILENAME );
    }

    private Control[] loadPicturePack(
        FileStream fileStream, PictureGroupDescriptor pictureGroupDescriptor
    ) {
        int width = pictureGroupDescriptor.picWidth;
        int height = pictureGroupDescriptor.picHeight;
        List<Control> controls = new();
        int actualWidth = Misc.roundUp( width, 4 );
        foreach ( int i in ..pictureGroupDescriptor.picCount ) {
            byte[] imgBuf = new byte[actualWidth * height];
            string tooltipCaption = $"{fileStream.Position}";
            foreach ( int y in ..height ) {
                if ( fileStream.Read( imgBuf, y * actualWidth, width ) != width ) {
                    throw new EndOfStreamException();
                }
            }

            ByteBackedBitmap bmp = new( width, height, PixelFormat.Format8bppIndexed, imgBuf );
            disposables.Add( bmp );

            bmp.setPalette( ( paletteManager[pictureGroupDescriptor.getPaletteName( i )] ?? defaultPalette ).entries );

            PictureBox pb = new() {
                Image = bmp.bitmap,
                SizeMode = PictureBoxSizeMode.AutoSize
            };
            new ToolTip().SetToolTip( pb, tooltipCaption );

            controls.Add( pb );
        }

        return controls.ToArray();
    }

    private void loadAllPictures( string fileName ) {
        using FileStream assetFileStream = new( App.ASSET_PATH + fileName, FileMode.Open );

        paletteManager.add( Const.paletteFilenames );
        defaultPalette = paletteManager[Const.DEFAULT_PALETTE_NAME];

        // [InstantHandle]
        Const.yt2PictureGroupDescriptors
             .Select( pgd => loadPicturePack( assetFileStream, pgd ) )
             .forEach( controls => {
                 FlowLayoutPanel panel = new() {
                     AutoSize = true,
                     AutoSizeMode = AutoSizeMode.GrowAndShrink,
                 };
                 panel.Controls.AddRange( controls );
                 add( panel );
             } );

        if ( assetFileStream.Position != assetFileStream.Length ) {
            MessageBox.Show( "not all data has been read:"
                             + $" pos {assetFileStream.Position}"
                             + $" vs len {assetFileStream.Length}" );
        }
    }

    /// <summary>
    ///     Clean up any resources being used.
    /// </summary>
    /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
    protected override void Dispose( bool disposing ) {
        if ( disposing && components != null ) {
            components.Dispose();
        }

        disposables.forEach( d => d.Dispose() );

        base.Dispose( disposing );
    }
}

}
