namespace TalesOfYonder {

using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Windows.Forms;
using LegacyEngine.Gfx;
using Vax.FormUtils;
using Vax.Reversing.Utils;

public partial class MainForm : AutoForm {

    public MainForm() : base( isMdiContainer: true ) {
        InitializeComponent();

        addMenus( menuStrip );

        loadAllPictures( /*true*/ );

        loadWorldData();
    }

    private void loadWorldData() {
        App.engine.loadWorldData();
        IReadOnlyList<Bitmap> maps = App.engine.createOverheadMaps();
        IEnumerable<PictureBox> pictureBoxes = maps.Select(
            ( bitmap ) => new PictureBox() {
                Image = bitmap,
                SizeMode = PictureBoxSizeMode.AutoSize,
            } );

        AutoForm mapMdiChildForm = new( mdiParent: this ) {
            Text = "MegaMap",
            // WindowState = FormWindowState.Minimized,
        };
        mapMdiChildForm.add( pictureBoxes );
        mapMdiChildForm.Show();
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

    private void loadAllPictures( bool createPreviewForms = false ) {
        SuspendLayout();
        // [InstantHandle]
        IReadOnlyList<PictureGroup> pictureGroups = App.engine.processAllPictureGroups();
        if ( createPreviewForms ) {
            pictureGroups.forEach( pictureGroup => {
                    AutoForm mdiChildForm = new( mdiParent: this ) {
                        Text = pictureGroup.pictureGroupDescriptor.description,
                        WindowState = FormWindowState.Minimized,
                    };
                    mdiChildForm.add( createPictureControls( pictureGroup.pictures ) );
                    mdiChildForm.Show();
                }
            );
        }
        ResumeLayout();
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

    /// <summary>
    ///     Clean up any resources being used.
    /// </summary>
    /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
    protected override void Dispose( bool disposing ) {
        if ( disposing && components != null ) {
            components.Dispose();
        }

        App.engine.Dispose();

        base.Dispose( disposing );
    }
}

}
