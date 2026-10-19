namespace Vax.FormUtils {

using System;
using System.Windows.Forms;

public class ToolStripMenuItemEx : ToolStripMenuItem {
    public ToolStripMenuItemEx() {
        ( (ToolStripDropDownMenu) DropDown ).ShowImageMargin = false;
    }

    public ToolStripMenuItemEx( EventHandler onClick ) {
        Click += onClick;
    }
}

}
