namespace TalesOfYonder {

using System.Drawing.Imaging;
using System.IO;
using System.Windows.Forms;
using Vax.Reversing.Utils;

public partial class MainForm : Form {
    private const int IMG_WIDTH = 318;
    private const int IMG_HEIGHT = 198;

    // private const int IMG_SIZE = IMG_WIDTH * IMG_HEIGHT; // 62964 XD

    private const string ASSET_PATH = "asset/";

    public MainForm() {
        InitializeComponent();

        openPictures( "YT2_PICTURES.VGA" );
    }

    private void openPictures( string fileName ) {
        int actualWidth = Misc.roundUp( IMG_WIDTH, 4 );
        byte[] imgBuf = new byte[actualWidth * IMG_HEIGHT];
        using FileStream fileStream = new( ASSET_PATH + fileName, FileMode.Open );
        for ( int y = 0; y < IMG_HEIGHT; y++ ) {
            fileStream.Read( imgBuf, y * actualWidth, IMG_WIDTH );
        }

        ByteBackedBitmap bmp = new( IMG_WIDTH, IMG_HEIGHT, PixelFormat.Format8bppIndexed, imgBuf );
        // note: don't dispose this ^ before disposing the form! 

        PictureBox pb = new() {
            Image = bmp.bitmap,
            SizeMode = PictureBoxSizeMode.AutoSize
        };

        Controls.Add( pb );
    }
}

}
