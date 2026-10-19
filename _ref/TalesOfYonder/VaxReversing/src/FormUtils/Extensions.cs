namespace Vax.FormUtils {

using System.Windows.Forms;

public static class MenuExtensions {
    public static void add( this ToolStripItemCollection toolStripItemCollection, params ToolStripItem[] items ) {
        toolStripItemCollection.AddRange( items );
    }
}

}
