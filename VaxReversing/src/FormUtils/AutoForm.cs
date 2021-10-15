namespace Vax.FormUtils {

using System.Collections.Generic;
using System.Drawing;
using System.Windows.Forms;
using Reversing.Utils;

public class AutoForm : Form {
    public sealed override bool AutoSize {
        get => base.AutoSize;
        set => base.AutoSize = value;
    }

    // ReSharper disable once InconsistentNaming
    /// <summary>
    ///     added to encapsulate the setter for consistency
    /// </summary>
    public new Form MdiParent {
        get => base.MdiParent;
        private init => base.MdiParent = value;
    }

    // ReSharper disable once InconsistentNaming
    /// <summary>
    ///     added to encapsulate the setter for consistency
    /// </summary>
    public new bool IsMdiContainer {
        get => base.IsMdiContainer;
        private init => base.IsMdiContainer = value;
    }
    
    public Control.ControlCollection autoControls => layoutPanel?.Controls;

    public MenuStrip menuStrip { get; }
    public Panel layoutPanel { get; protected set; }

    public AutoForm(
        bool hasMenu = true,
        bool isMdiContainer = false,
        Form mdiParent = null,
        Panel layoutPanel = null
    ) {
        SuspendLayout();

        // Font = new Font( new FontFamily( "Microsoft Sans Serif" ), 8f );

        MdiParent = mdiParent;
        IsMdiContainer = isMdiContainer;

        // MinimumSize = new( Width, Height );
        Rectangle rect = Screen.PrimaryScreen.Bounds;
        // not Screen.FromControl( this ).Bounds due to handle creation error (?)
        MaximumSize = new( rect.Width, rect.Height );

        this.layoutPanel = ( layoutPanel != null || IsMdiContainer )
            ? layoutPanel
            : new FlowLayoutPanel() {
                Dock = DockStyle.Fill,
                AutoScroll = true,
            };

        if ( this.layoutPanel != null ) {
            Controls.Add( this.layoutPanel );
        }

        if ( hasMenu ) {
            menuStrip = new() {
                RenderMode = ToolStripRenderMode.System,
                Dock = DockStyle.Top,
                Visible = ( mdiParent == null ),
            };
            MainMenuStrip = menuStrip;
            Controls.Add( MainMenuStrip ); // has to be added last to order the docks properly
        }

        ResumeLayout();
    }

    public void add( params Control[] items ) {
        add( (IEnumerable<Control>) items );
    }

    public void add( IEnumerable<Control> controls ) {
        Control.ControlCollection targetControls = ( layoutPanel != null ) ? layoutPanel.Controls : Controls;
        targetControls.Owner.SuspendLayout();
        try {
            controls.forEach( control => targetControls.Add( control ) );
        } finally {
            targetControls.Owner.ResumeLayout( true );
        }
    }
}

}
