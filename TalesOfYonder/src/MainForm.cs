namespace TalesOfYonder {

using System.Collections.Generic;
using System.Windows.Forms;
using LegacyEngine;
using Vax.FormUtils;
using Vax.Reversing.Utils;

public partial class MainForm : AutoForm {
    private readonly PictureManager pictureManager = new( Const.YT2_PICTURE_FILENAME );

    public MainForm() : base( isMdiContainer: true ) {
        InitializeComponent();

        addMenus( menuStrip );

        loadAllPictures();
    }

    private void addMenus( MenuStrip ms ) {
        SuspendLayout();

        /*
ToolStripMenuItemEx fileOpenMenuItem = new( fileOpenEventHandler ) {
    Text = @"&Open DEMISE file...",
    ShortcutKeys = Keys.Control | Keys.O
};
ToolStripMenuItemEx fileOpenModelMenuItem = new( fileOpenModelEventHandler ) {
    Text = @"Open 3D &model...",
    ShortcutKeys = Keys.Control | Keys.M
};
*/
        ToolStripMenuItemEx exitMenuItem = new( ( _, _ ) => Application.Exit() ) {
            Text = @"E&xit",
            ShortcutKeys = Keys.Alt | Keys.F4,
        };
        ToolStripMenuItemEx fileMenu = new() {
            Text = @"&File",
            DropDownItems = {
                // fileOpenMenuItem,
                // fileOpenModelMenuItem,
                // new ToolStripSeparator(),
                exitMenuItem,
            },
        };

        ToolStripMenuItemEx closeAllWindowsMenuItem = new(
            ( _, _ ) => MdiChildren.forEach( f => f.Close() )
        ) {
            Text = @"&Close all",
            ShortcutKeys = Keys.Control | Keys.X,
        };
        ToolStripMenuItemEx windowMenu = new() {
            Text = @"&Windows",
            DropDownItems = {
                closeAllWindowsMenuItem,
            },
        };
        ms.MdiWindowListItem = windowMenu;

        ToolStripMenuItemEx aboutMenuItem = new(
                ( _, _ ) => ExternHelper.showShellAboutDialog( App.NAME ) ) {
                Text = @"&About...",
                ShortcutKeys = Keys.Control | Keys.A,
            }
            ;
        ToolStripMenuItemEx helpMenu = new() {
            Text = @"&Help",
            DropDownItems = {
                aboutMenuItem,
            },
            Alignment = ToolStripItemAlignment.Right,
        };

        ms.Items.add( fileMenu, /*actionMenu,*/ windowMenu, helpMenu );

        ResumeLayout();
    }

    private static Control[] createPictureControls( List<Picture> pictures ) {
        Control[] controls = new Control[pictures.Count];
        foreach ( int i in ..pictures.Count ) {
            Picture picture = pictures[i];
            PictureBox pb = new() {
                Image = picture.bitmap,
                SizeMode = PictureBoxSizeMode.AutoSize,
            };
            new ToolTip().SetToolTip( pb, picture.description );

            controls[i] = pb;
        }

        return controls;
    }

    private void loadAllPictures() {
        SuspendLayout();
        // [InstantHandle]
        pictureManager.processAllPictureGroups(
            Const.yt2PictureGroupDescriptors, tuple => {
                AutoForm mdiChildForm = new( mdiParent: this ) {
                    Text = tuple.description,
                    WindowState = FormWindowState.Minimized,
                };
                mdiChildForm.add( createPictureControls( tuple.pictures ) );
                mdiChildForm.Show();
            }
        );
        ResumeLayout();
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
