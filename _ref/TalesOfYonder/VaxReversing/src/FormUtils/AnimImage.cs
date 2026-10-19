namespace Vax.Reversing.FormUtils {

using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Linq;
using System.Windows.Forms;

// simple animation code for WinForms courtesy of Hans Passant
/*
// usage:
pictureBox1.Image = CreateAnimation( pictureBox1,
    new Image[] { Properties.Resources.Frame1, Properties.Resources.Frame2, Properties.Resources.Frame3 },
    new int[] { 1000, 2000, 300 }
);
*/
public static class AnimImage {
    public static Image createAnimation( Control ctl, Image[] frames, int[] delays ) {
        MemoryStream ms = new();
        ImageCodecInfo codec = ImageCodecInfo.GetImageEncoders().First( i => i.MimeType == "image/tiff" );

        EncoderParameters encoderParameters = new( 2 ) {
            Param = {
                [0] = new( Encoder.SaveFlag, (long) EncoderValue.MultiFrame ),
                [1] = new( Encoder.Quality, (long) EncoderValue.CompressionLZW )
            }
        };
        frames[0].Save( ms, codec, encoderParameters );

        encoderParameters = new( 1 ) {
            Param = { [0] = new( Encoder.SaveFlag, (long) EncoderValue.FrameDimensionPage ) }
        };
        for ( int i = 1; i < frames.Length; i++ ) {
            frames[0].SaveAdd( frames[i], encoderParameters );
        }

        encoderParameters.Param[0] = new( Encoder.SaveFlag, (long) EncoderValue.Flush );
        frames[0].SaveAdd( encoderParameters );

        ms.Position = 0;
        Image img = Image.FromStream( ms );
        animate( ctl, img, delays );
        return img;
    }

    private static void animate( Control ctl, Image img, int[] delays ) {
        int frame = 0;
        Timer tmr = new() {
            Interval = delays[0],
            Enabled = true
        };
        tmr.Tick += ( _, _ ) => {
            frame++;
            if ( frame >= delays.Length ) {
                frame = 0;
            }

            img.SelectActiveFrame( FrameDimension.Page, frame );
            tmr.Interval = delays[frame];
            ctl.Invalidate();
        };
        ctl.Disposed += ( _, _ ) => tmr.Dispose();
    }
}

}
