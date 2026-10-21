// xBRZ - "scale by rules", Zenju's high quality pixel-art scaler, in its
// advanced form: a 4x4 kernel decides for every corner of a pixel whether it
// has to be blended and how strongly, and only those corners are blended.
// This is an independent implementation of the published algorithm, written
// for this client; its arithmetic follows the reference implementation step
// for step, so the output matches the reference bit for bit.
//
// The game is an indexed framebuffer with a palette, so every pixel is
// opaque and the alpha handling of the reference reduces to a straight
// interpolation of the colour channels.
//
// Pixels are 0xAARRGGBB (the reference's layout), not the client's
// little-endian RGBA byte order: the colour distance is computed in YCbCr and
// the channel order matters for it.

namespace Yt2.Client;

internal static class Xbrz
{
    // The reference's tuning (Zenju's advanced xBRZ).
    private const double DominantDirectionThreshold = 3.6;
    private const double SteepDirectionThreshold = 2.2;
    private const double EqualColorTolerance = 30.0;

    // Blend types, two bits each.
    private const int BlendNone = 0;
    private const int BlendNormal = 1;
    private const int BlendDominant = 2;

    private enum Rot { R0, R90, R180, R270 }

    /// <summary>Scales an ARGB image by an integer factor (2 to 6).</summary>
    public static void Scale(ReadOnlySpan<uint> src, int width, int height, Span<uint> dst, int scale) =>
        Scale(src, width, height, dst, scale, 0, height);

    /// <summary>
    /// Scales the source rows [firstRow, lastRow) of an ARGB image by an
    /// integer factor (2 to 6).  The rows around the slice are read, so the
    /// caller has to scale a row that changed together with its neighbours:
    /// a blend looks two rows up and down, so a changed row changes the
    /// output of the rows above and below it as well.
    /// </summary>
    public static void Scale(ReadOnlySpan<uint> src, int width, int height, Span<uint> dst, int scale,
        int firstRow, int lastRow)
    {
        if (scale < 2 || scale > 6)
        {
            throw new ArgumentOutOfRangeException(nameof(scale));
        }
        firstRow = Math.Max(firstRow, 0);
        lastRow = Math.Min(lastRow, height);
        if (firstRow >= lastRow || width <= 0)
        {
            return;
        }
        int trgWidth = width * scale;
        var policy = Policy.For(scale);
        // The corner blend info of the previous row, one byte per column.
        var preProc = new byte[width];
        // The 3x3 kernel around the current pixel, reused for every pixel.
        var ker = new uint[9];

        if (firstRow > 0)
        {
            // The first row of a slice needs the corner blends of the row
            // above it, which the previous slice would have left in the
            // buffer; they are computed again here.
            int y = firstRow - 1;
            int ym1 = Math.Max(y - 1, 0);
            int yp1 = Math.Min(y + 1, height - 1);
            int yp2 = Math.Min(y + 2, height - 1);
            for (int x = 0; x < width; x++)
            {
                int xm1 = Math.Max(x - 1, 0);
                int xp1 = Math.Min(x + 1, width - 1);
                int xp2 = Math.Min(x + 2, width - 1);

                uint a = src[ym1 * width + xm1], b = src[ym1 * width + x];
                uint c = src[ym1 * width + xp1], d = src[ym1 * width + xp2];
                uint e = src[y * width + xm1], f = src[y * width + x];
                uint g = src[y * width + xp1], h = src[y * width + xp2];
                uint i = src[yp1 * width + xm1], j = src[yp1 * width + x];
                uint k = src[yp1 * width + xp1], l = src[yp1 * width + xp2];
                uint m = src[yp2 * width + xm1], n = src[yp2 * width + x];
                uint o = src[yp2 * width + xp1], p = src[yp2 * width + xp2];

                int blendF, blendG, blendJ, blendK;
                PreProcessCorners(e, f, g, h, i, j, k, l, m, n, o, p, b, c,
                    out blendF, out blendG, out blendJ, out blendK);

                // The top right corner of the pixel below and the top left
                // corner of the pixel to the right of it.
                preProc[x] = (byte)SetTopR(preProc[x], blendJ);
                if (x + 1 < width)
                {
                    preProc[x + 1] = (byte)SetTopL(preProc[x + 1], blendK);
                }
            }
        }

        for (int y = firstRow; y < lastRow; y++)
        {
            int ym1 = Math.Max(y - 1, 0);
            int yp1 = Math.Min(y + 1, height - 1);
            int yp2 = Math.Min(y + 2, height - 1);
            int rowBase = y * scale * trgWidth;
            // The blend info of the pixel below the current one.
            int blendNext = 0;

            for (int x = 0; x < width; x++)
            {
                int xm1 = Math.Max(x - 1, 0);
                int xp1 = Math.Min(x + 1, width - 1);
                int xp2 = Math.Min(x + 2, width - 1);

                uint a = src[ym1 * width + xm1], b = src[ym1 * width + x];
                uint c = src[ym1 * width + xp1], d = src[ym1 * width + xp2];
                uint e = src[y * width + xm1], f = src[y * width + x];
                uint g = src[y * width + xp1], h = src[y * width + xp2];
                uint i = src[yp1 * width + xm1], j = src[yp1 * width + x];
                uint k = src[yp1 * width + xp1], l = src[yp1 * width + xp2];
                uint m = src[yp2 * width + xm1], n = src[yp2 * width + x];
                uint o = src[yp2 * width + xp1], p = src[yp2 * width + xp2];

                // The four corners around the pixel: F, G (top) and J, K
                // (bottom).  The 4x4 kernel decides for each of them whether
                // it has to be blended at all.
                int blendF, blendG, blendJ, blendK;
                PreProcessCorners(e, f, g, h, i, j, k, l, m, n, o, p, b, c,
                    out blendF, out blendG, out blendJ, out blendK);

                int blend = preProc[x];
                blend = SetBottomR(blend, blendF);
                blendNext = SetTopR(blendNext, blendJ);
                preProc[x] = (byte)blendNext;
                blendNext = SetTopL(0, blendK);
                if (x + 1 < width)
                {
                    preProc[x + 1] = (byte)SetBottomL(preProc[x + 1], blendG);
                }

                // The block of the output this pixel owns, filled with the
                // pixel itself; the blends below paint over it.
                int block = rowBase + x * scale;
                for (int by = 0; by < scale; by++)
                {
                    dst.Slice(block + by * trgWidth, scale).Fill(f);
                }

                if (blend != BlendNone)
                {
                    // The 3x3 kernel around the pixel.
                    ker[0] = a; ker[1] = b; ker[2] = c;
                    ker[3] = e; ker[4] = f; ker[5] = g;
                    ker[6] = i; ker[7] = j; ker[8] = k;
                    BlendPixel(ker, Rot.R0, (byte)blend, dst, block, trgWidth, policy);
                    BlendPixel(ker, Rot.R90, (byte)blend, dst, block, trgWidth, policy);
                    BlendPixel(ker, Rot.R180, (byte)blend, dst, block, trgWidth, policy);
                    BlendPixel(ker, Rot.R270, (byte)blend, dst, block, trgWidth, policy);
                }
            }
        }
    }

    /// <summary>
    /// Decides for the four corners of a pixel whether they have to be
    /// blended, and how strongly: the two gradients around the pixel are
    /// compared, and the corner gets a normal or a dominant blend.
    /// </summary>
    private static void PreProcessCorners(
        uint e, uint f, uint g, uint h, uint i, uint j, uint k, uint l,
        uint m, uint n, uint o, uint p, uint b, uint c,
        out int blendF, out int blendG, out int blendJ, out int blendK)
    {
        blendF = blendG = blendJ = blendK = BlendNone;
        if ((f == g && j == k) || (f == j && g == k))
        {
            return;
        }

        const int weight = 4;
        double jg = Distance(i, f) + Distance(f, c) + Distance(n, k) + Distance(k, h) +
                     weight * Distance(j, g);
        double fk = Distance(e, j) + Distance(j, o) + Distance(b, g) + Distance(g, l) +
                     weight * Distance(f, k);

        if (jg < fk)
        {
            bool dominant = DominantDirectionThreshold * jg < fk;
            if (f != g && f != j)
            {
                blendF = dominant ? BlendDominant : BlendNormal;
            }
            if (k != j && k != g)
            {
                blendK = dominant ? BlendDominant : BlendNormal;
            }
        }
        else if (fk < jg)
        {
            bool dominant = DominantDirectionThreshold * fk < jg;
            if (j != f && j != k)
            {
                blendJ = dominant ? BlendDominant : BlendNormal;
            }
            if (g != f && g != k)
            {
                blendG = dominant ? BlendDominant : BlendNormal;
            }
        }
    }

    /// <summary>
    /// Blends one corner of a pixel into the output block, in the given
    /// rotation of the kernel: the four rotations write the four corners of
    /// the block around the pixel.
    /// </summary>
    private static void BlendPixel(uint[] k, Rot rot, byte blendInfo, Span<uint> dst,
        int block, int trgWidth, Policy policy)
    {
        byte blend = Rotate(blendInfo, rot);
        if (BottomR(blend) < BlendNormal)
        {
            return;
        }

        uint e = k[KernelIndex(rot, 4)];
        uint f = k[KernelIndex(rot, 5)];
        uint g = k[KernelIndex(rot, 6)];
        uint h = k[KernelIndex(rot, 7)];
        uint b = k[KernelIndex(rot, 1)];
        uint c = k[KernelIndex(rot, 2)];
        uint d = k[KernelIndex(rot, 3)];
        uint i = k[KernelIndex(rot, 8)];

        bool doLineBlend;
        if (BottomR(blend) >= BlendDominant)
        {
            doLineBlend = true;
        }
        else
        {
            // No second blending in an adjacent rotation for this pixel.
            if (TopR(blend) != BlendNone && !Equal(e, g))
            {
                doLineBlend = false;
            }
            else if (BottomL(blend) != BlendNone && !Equal(e, c))
            {
                doLineBlend = false;
            }
            // No full blending for L-shapes; the corner is blended only.
            else if (!Equal(e, i) && Equal(g, h) && Equal(h, i) && Equal(i, f) && Equal(f, c))
            {
                doLineBlend = false;
            }
            else
            {
                doLineBlend = true;
            }
        }

        // The most similar colour is the one that is blended in.
        uint px = Distance(e, f) <= Distance(e, h) ? f : h;

        if (doLineBlend)
        {
            double fg = Distance(f, g);
            double hc = Distance(h, c);
            bool haveShallow = SteepDirectionThreshold * fg <= hc && e != g && d != g;
            bool haveSteep = SteepDirectionThreshold * hc <= fg && e != c && b != c;
            if (haveShallow)
            {
                Apply(haveSteep ? policy.SteepAndShallow : policy.Shallow,
                    px, rot, dst, block, trgWidth, policy.Scale);
            }
            else
            {
                Apply(haveSteep ? policy.Steep : policy.Diagonal,
                    px, rot, dst, block, trgWidth, policy.Scale);
            }
        }
        else
        {
            Apply(policy.Corner, px, rot, dst, block, trgWidth, policy.Scale);
        }
    }

    /// <summary>Applies the blend steps of one policy to the output block.</summary>
    private static void Apply(Step[] steps, uint colour, Rot rot, Span<uint> dst,
        int block, int trgWidth, int scale)
    {
        foreach (Step step in steps)
        {
            (int row, int column) = Rotate(step.Row, step.Column, scale, rot);
            int index = block + column + row * trgWidth;
            if (step.M == 0)
            {
                dst[index] = colour;
            }
            else
            {
                dst[index] = Gradient(step.M, step.N, dst[index], colour);
            }
        }
    }

    /// <summary>
    /// The reference's alpha weighted interpolation: the front colour over
    /// the back one with the opacity M/N.  Opaque pixels reduce to
    /// (front * M + back * (N - M)) / N.
    /// </summary>
    private static uint Gradient(int m, int n, uint back, uint front)
    {
        int weightFront = (int)(front >> 24) * m;
        int weightBack = (int)(back >> 24) * (n - m);
        int sum = weightFront + weightBack;
        if (sum == 0)
        {
            return 0;
        }
        uint r = (uint)(((int)((front >> 16) & 0xff) * weightFront +
                             (int)((back >> 16) & 0xff) * weightBack) / sum);
        uint g = (uint)(((int)((front >> 8) & 0xff) * weightFront +
                             (int)((back >> 8) & 0xff) * weightBack) / sum);
        uint b = (uint)(((int)(front & 0xff) * weightFront +
                             (int)(back & 0xff) * weightBack) / sum);
        return ((uint)(sum / n) << 24) | (r << 16) | (g << 8) | b;
    }

    /// <summary>
    /// The colour distance of the reference: the YCbCr distance of the
    /// halved channel differences.  The reference keeps every pair in a 64 MB
    /// table; the pairs that matter - the neighbours of a pixel - differ in
    /// a few steps at most, so a small table around the middle covers them
    /// and the rare rest is computed.  The values are the reference's, down
    /// to the float rounding.
    /// </summary>
    private static double Distance(uint pix1, uint pix2)
    {
        int ir = ((int)((pix1 >> 16) & 0xff) - (int)((pix2 >> 16) & 0xff) + 255) / 2;
        int ig = ((int)((pix1 >> 8) & 0xff) - (int)((pix2 >> 8) & 0xff) + 255) / 2;
        int ib = ((int)(pix1 & 0xff) - (int)(pix2 & 0xff) + 255) / 2;
        if (ir >= TableLow && ir <= TableHigh &&
            ig >= TableLow && ig <= TableHigh &&
            ib >= TableLow && ib <= TableHigh)
        {
            return Table[((ir - TableLow) * TableSide + (ig - TableLow)) * TableSide + (ib - TableLow)];
        }
        return Compute(ir, ig, ib);
    }

    /// <summary>The YCbCr distance of one pair of halved differences.</summary>
    private static float Compute(int ir, int ig, int ib)
    {
        int r = ir * 2 - 255;
        int g = ig * 2 - 255;
        int b = ib * 2 - 255;

        const double kb = 0.0593;         // ITU-R BT.2020 conversion
        const double kr = 0.2627;
        const double kg = 1 - kb - kr;
        const double scaleB = 0.5 / (1 - kb);
        const double scaleR = 0.5 / (1 - kr);

        double y = kr * r + kg * g + kb * b;
        double cb = scaleB * (b - y);
        double cr = scaleR * (r - y);
        return (float)Math.Sqrt(y * y + cb * cb + cr * cr);
    }

    // The table covers the halved differences around the middle, that is the
    // channel values that are close to each other.
    private const int TableRadius = 8;
    private const int TableSide = 2 * TableRadius + 1;
    private const int TableLow = 127 - TableRadius;
    private const int TableHigh = 127 + TableRadius;
    private static readonly float[] Table = BuildTable();

    private static float[] BuildTable()
    {
        var table = new float[TableSide * TableSide * TableSide];
        for (int ir = TableLow; ir <= TableHigh; ir++)
        {
            for (int ig = TableLow; ig <= TableHigh; ig++)
            {
                for (int ib = TableLow; ib <= TableHigh; ib++)
                {
                    table[((ir - TableLow) * TableSide + (ig - TableLow)) * TableSide + (ib - TableLow)] =
                        Compute(ir, ig, ib);
                }
            }
        }
        return table;
    }

    private static bool Equal(uint pix1, uint pix2) =>
        Distance(pix1, pix2) < EqualColorTolerance;

    // The blend info byte: two bits per corner, top-left, top-right,
    // bottom-right, bottom-left.
    private static int TopL(byte b) => b & 3;
    private static int TopR(byte b) => (b >> 2) & 3;
    private static int BottomR(byte b) => (b >> 4) & 3;
    private static int BottomL(byte b) => (b >> 6) & 3;
    private static int SetTopL(int b, int blend) => b | (blend << 0);
    private static int SetTopR(int b, int blend) => b | (blend << 2);
    private static int SetBottomR(int b, int blend) => b | (blend << 4);
    private static int SetBottomL(int b, int blend) => b | (blend << 6);

    /// <summary>Rotates the blend info with the kernel.</summary>
    private static byte Rotate(byte b, Rot rot) => rot switch
    {
        Rot.R90 => (byte)((b << 2) | (b >> 6)),
        Rot.R180 => (byte)((b << 4) | (b >> 4)),
        Rot.R270 => (byte)((b << 6) | (b >> 2)),
        _ => b,
    };

    /// <summary>The kernel index of a rotated kernel position.</summary>
    private static int KernelIndex(Rot rot, int index) => rot switch
    {
        // a b c / d e f / g h i, rotated clockwise
        Rot.R90 => index switch
        {
            0 => 6, 1 => 3, 2 => 0, 3 => 7, 4 => 4, 5 => 1, 6 => 8, 7 => 5, _ => 2,
        },
        Rot.R180 => index switch
        {
            0 => 8, 1 => 7, 2 => 6, 3 => 5, 4 => 4, 5 => 3, 6 => 2, 7 => 1, _ => 0,
        },
        Rot.R270 => index switch
        {
            0 => 2, 1 => 5, 2 => 8, 3 => 1, 4 => 4, 5 => 7, 6 => 0, 7 => 3, _ => 6,
        },
        _ => index,
    };

    /// <summary>The output position of a rotated blend step.</summary>
    private static (int Row, int Column) Rotate(int row, int column, int scale, Rot rot) =>
        rot switch
        {
            Rot.R90 => (scale - 1 - column, row),
            Rot.R180 => (scale - 1 - row, scale - 1 - column),
            Rot.R270 => (column, scale - 1 - row),
            _ => (row, column),
        };

    /// <summary>One blend step: the colour with the opacity M/N at a position.</summary>
    private readonly record struct Step(int M, int N, int Row, int Column);

    /// <summary>The blend steps of one scale factor.</summary>
    private sealed class Policy
    {
        public required int Scale { get; init; }
        public required Step[] Shallow { get; init; }
        public required Step[] Steep { get; init; }
        public required Step[] SteepAndShallow { get; init; }
        public required Step[] Diagonal { get; init; }
        public required Step[] Corner { get; init; }

        private static readonly Dictionary<int, Policy> Policies = new()
        {
            [2] = new Policy
            {
                Scale = 2,
                Shallow = [new(1, 4, 1, 0), new(3, 4, 1, 1)],
                Steep = [new(1, 4, 0, 1), new(3, 4, 1, 1)],
                SteepAndShallow = [new(1, 4, 1, 0), new(1, 4, 0, 1), new(5, 6, 1, 1)],
                Diagonal = [new(1, 2, 1, 1)],
                Corner = [new(21, 100, 1, 1)],
            },
            [3] = new Policy
            {
                Scale = 3,
                Shallow = [new(1, 4, 2, 0), new(1, 4, 1, 2), new(3, 4, 2, 1), new(0, 0, 2, 2)],
                Steep = [new(1, 4, 0, 2), new(1, 4, 2, 1), new(3, 4, 1, 2), new(0, 0, 2, 2)],
                SteepAndShallow =
                [
                    new(1, 4, 2, 0), new(1, 4, 0, 2), new(3, 4, 2, 1), new(3, 4, 1, 2),
                    new(0, 0, 2, 2),
                ],
                Diagonal = [new(1, 8, 1, 2), new(1, 8, 2, 1), new(7, 8, 2, 2)],
                Corner = [new(45, 100, 2, 2)],
            },
            [4] = new Policy
            {
                Scale = 4,
                Shallow =
                [
                    new(1, 4, 3, 0), new(1, 4, 2, 2), new(3, 4, 3, 1), new(3, 4, 2, 3),
                    new(0, 0, 3, 2), new(0, 0, 3, 3),
                ],
                Steep =
                [
                    new(1, 4, 0, 3), new(1, 4, 2, 2), new(3, 4, 1, 3), new(3, 4, 3, 2),
                    new(0, 0, 2, 3), new(0, 0, 3, 3),
                ],
                SteepAndShallow =
                [
                    new(3, 4, 3, 1), new(3, 4, 1, 3), new(1, 4, 3, 0), new(1, 4, 0, 3),
                    new(1, 3, 2, 2), new(0, 0, 3, 3), new(0, 0, 3, 2), new(0, 0, 2, 3),
                ],
                Diagonal = [new(1, 2, 3, 2), new(1, 2, 2, 3), new(0, 0, 3, 3)],
                Corner = [new(68, 100, 3, 3), new(9, 100, 3, 2), new(9, 100, 2, 3)],
            },
            [5] = new Policy
            {
                Scale = 5,
                Shallow =
                [
                    new(1, 4, 4, 0), new(1, 4, 3, 2), new(1, 4, 2, 4), new(3, 4, 4, 1),
                    new(3, 4, 3, 3), new(0, 0, 4, 2), new(0, 0, 4, 3), new(0, 0, 4, 4),
                    new(0, 0, 3, 4),
                ],
                Steep =
                [
                    new(1, 4, 0, 4), new(1, 4, 2, 3), new(1, 4, 4, 2), new(3, 4, 1, 4),
                    new(3, 4, 3, 3), new(0, 0, 2, 4), new(0, 0, 3, 4), new(0, 0, 4, 4),
                    new(0, 0, 4, 3),
                ],
                SteepAndShallow =
                [
                    new(1, 4, 0, 4), new(1, 4, 2, 3), new(3, 4, 1, 4), new(1, 4, 4, 0),
                    new(1, 4, 3, 2), new(3, 4, 4, 1), new(2, 3, 3, 3), new(0, 0, 2, 4),
                    new(0, 0, 3, 4), new(0, 0, 4, 4), new(0, 0, 4, 2), new(0, 0, 4, 3),
                ],
                Diagonal =
                [
                    new(1, 8, 4, 2), new(1, 8, 3, 3), new(1, 8, 2, 4), new(7, 8, 4, 3),
                    new(7, 8, 3, 4), new(0, 0, 4, 4),
                ],
                Corner = [new(86, 100, 4, 4), new(23, 100, 4, 3), new(23, 100, 3, 4)],
            },
            [6] = new Policy
            {
                Scale = 6,
                Shallow =
                [
                    new(1, 4, 5, 0), new(1, 4, 4, 2), new(1, 4, 3, 4), new(3, 4, 5, 1),
                    new(3, 4, 4, 3), new(3, 4, 3, 5), new(0, 0, 5, 2), new(0, 0, 5, 3),
                    new(0, 0, 5, 4), new(0, 0, 5, 5), new(0, 0, 4, 4), new(0, 0, 4, 5),
                ],
                Steep =
                [
                    new(1, 4, 0, 5), new(1, 4, 2, 4), new(1, 4, 4, 3), new(3, 4, 1, 5),
                    new(3, 4, 3, 4), new(3, 4, 5, 3), new(0, 0, 2, 5), new(0, 0, 3, 5),
                    new(0, 0, 4, 5), new(0, 0, 5, 5), new(0, 0, 4, 4), new(0, 0, 5, 4),
                ],
                SteepAndShallow =
                [
                    new(1, 4, 0, 5), new(1, 4, 2, 4), new(3, 4, 1, 5), new(3, 4, 3, 4),
                    new(1, 4, 5, 0), new(1, 4, 4, 2), new(3, 4, 5, 1), new(3, 4, 4, 3),
                    new(0, 0, 2, 5), new(0, 0, 3, 5), new(0, 0, 4, 5), new(0, 0, 5, 5),
                    new(0, 0, 5, 2), new(0, 0, 5, 3),
                ],
                Diagonal =
                [
                    new(1, 2, 5, 3), new(1, 2, 4, 4), new(1, 2, 3, 5), new(0, 0, 4, 5),
                    new(0, 0, 5, 5), new(0, 0, 5, 4),
                ],
                Corner =
                [
                    new(97, 100, 5, 5), new(42, 100, 4, 5), new(42, 100, 5, 4),
                    new(6, 100, 5, 3), new(6, 100, 3, 5),
                ],
            },
        };

        public static Policy For(int scale) => Policies[scale];
    }
}
