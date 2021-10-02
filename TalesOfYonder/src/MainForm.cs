namespace TalesOfYonder {

using System.Collections.Generic;
using System.Windows.Forms;
using LegacyEngine;
using Vax.FormUtils;
using Vax.Reversing.Utils;

public partial class MainForm : AutoForm {
    private readonly PictureManager pictureManager = new( Const.YT2_PICTURE_FILENAME );

    public MainForm() : base( layoutPanel:
                              new TableLayoutPanel() {
                                  // BackColor = System.Drawing.Color.Black,
                                  AutoSize = true,
                                  Dock = DockStyle.Fill,
                                  AutoScroll = true
                              } ) {
        InitializeComponent();

        loadAllPictures();
    }

    private static Control[] createPictureControls( List<Picture> pictures ) {
        Control[] controls = new Control[pictures.Count];
        foreach ( int i in ..pictures.Count ) {
            Picture picture = pictures[i];
            PictureBox pb = new() {
                Image = picture.bitmap,
                SizeMode = PictureBoxSizeMode.AutoSize
            };
            new ToolTip().SetToolTip( pb, picture.description );

            controls[i] = pb;
        }

        return controls;
    }

    private void loadAllPictures() {
        // [InstantHandle]
        pictureManager.processAllPictureGroups(
            Const.yt2PictureGroupDescriptors,
            pictureList => {
                FlowLayoutPanel panel = new() {
                    AutoSize = true,
                    AutoSizeMode = AutoSizeMode.GrowAndShrink
                };
                panel.Controls.AddRange( createPictureControls( pictureList ) );
                add( panel );
            }
        );
    }

    /// <summary>
    ///     Clean up any resources being used.
    /// </summary>
    /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
    protected override void Dispose( bool disposing ) {
        if ( disposing && components != null ) {
            components.Dispose();
        }

        pictureManager.Dispose();

        base.Dispose( disposing );
    }
}

}
