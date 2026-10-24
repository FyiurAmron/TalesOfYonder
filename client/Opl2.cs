// YM3812 (OPL2) core for the companion client.
//
// The seam streams the resident SBFMDRV's register writes (docs/protocol.md,
// family 2); this class is the chip those writes land in.  It renders at the
// stream's requested rate and scales the phase/envelope clocks from the
// chip's native 49716 Hz, so pitch and note timing match the hardware at any
// output rate.
//
// Coverage: 9 two-operator channels, FM or additive connection, feedback,
// the four OPL2 waveforms, the attack/decay/sustain/release envelope with key
// scaling, total level and KSL, tremolo/vibrato, and the rhythm mode gates
// (the shipped music does not use rhythm, but the driver's SFX path may).
// Timers and the status port are not modelled: the game polls neither.

namespace Yt2.Client;

public sealed class Opl2
{
    private const double ChipRate = 49716.0;   // 3.579545 MHz / 72
    private const int Points = 1024;            // phase points per cycle
    private const double MaxDb = 96.0;

    private readonly object _lock = new();
    private readonly double[] _sine = new double[Points];
    private readonly double[] _gainDb = new double[(int)(MaxDb * 10) + 1];
    private readonly byte[] _reg = new byte[256];
    private readonly Operator[] _ops = new Operator[18];
    private readonly Channel[] _channels = new Channel[9];
    private readonly int _rate;
    private readonly double _rateScale;
    private readonly double _dt;

    private bool _waveSelect;
    private double _amPhase;
    private double _vibPhase;
    private double _amDepthDb = 1.0;
    private double _vibDepthCents = 7.0;

    // Scales a full-scale operator output into carrier phase points.  The
    // chip adds the 13-bit operator output directly to the 10-bit phase, so
    // full scale is four cycles (8*pi); one point is 2*pi/1024.
    private const double ModIndexPoints = 4.0 * Points;
    // Feedback 0..7: the average of the last two modulator outputs, in phase
    // points (the chip adds the sum of the last two samples, full scale 4*pi
    // at depth 7).
    private static readonly double[] FeedbackPoints =
        [0.0, 32.0, 64.0, 128.0, 256.0, 512.0, 1024.0, 2048.0];

    // OPL2 envelope tables (0-100% time in milliseconds, from the YMF278B
    // application manual, indexed by the 6-bit effective rate).  Rates 0..3
    // never move; attack rates 60..63 are instantaneous.
    private static readonly double[] AttackTimeMs =
    [
        0, 0, 0, 0,
        2826.24, 2252.80, 1884.16, 1597.44, 1413.12, 1126.40, 942.08, 798.72,
        706.56, 563.20, 471.04, 399.36, 353.28, 281.60, 235.52, 199.68,
        176.76, 140.80, 117.76, 99.84, 88.32, 70.40, 58.88, 49.92,
        44.16, 35.20, 29.44, 24.96, 22.08, 17.60, 14.72, 12.48,
        11.04, 8.80, 7.36, 6.24, 5.52, 4.40, 3.68, 3.12,
        2.76, 2.20, 1.84, 1.56, 1.40, 1.12, 0.92, 0.80,
        0.70, 0.56, 0.46, 0.42, 0.38, 0.30, 0.24, 0.20,
        0.00, 0.00, 0.00, 0.00,
    ];

    private static readonly double[] DecayTimeMs =
    [
        0, 0, 0, 0,
        39280.64, 31416.32, 26173.44, 22446.08, 19640.32, 15708.16, 13086.72, 11223.04,
        9820.16, 7854.08, 6543.36, 5611.52, 4910.08, 3927.04, 3271.68, 2805.76,
        2455.04, 1936.52, 1635.84, 1402.88, 1227.52, 981.76, 817.92, 701.44,
        613.76, 490.88, 488.96, 350.72, 306.88, 245.44, 204.48, 175.36,
        153.44, 122.72, 102.24, 87.68, 76.72, 61.36, 51.12, 43.84,
        38.36, 30.68, 25.56, 21.92, 19.20, 15.36, 12.80, 10.96,
        9.60, 7.68, 6.40, 5.48, 4.80, 3.84, 3.20, 2.74,
        2.40, 2.40, 2.40, 2.40,
    ];

    // Key scale level: the chip's attenuation curve, in 0.375 dB units for
    // KSL=1.  Indexed by the top four frequency-number bits.
    private static readonly int[] KslRom =
        [0, 32, 40, 45, 48, 51, 53, 55, 56, 58, 59, 60, 61, 62, 63, 64];

    private enum EnvState
    {
        Off,
        Attack,
        Decay,
        Sustain,
        DecayToEnd,
        Release,
    }

    private sealed class Operator
    {
        public int Offset;
        public Channel Channel = null!;
        public bool Carrier;
        public double Phase;                    // 0..Points
        public double Increment;                // phase points per sample
        public int Fnum;
        public int Block;
        public int Multiple = 1;
        public int TotalLevel;
        public int Ksl;
        public int Attack = 15;
        public int Decay;
        public int SustainLevel;
        public int Release = 15;
        public int KeyScaleRate;
        public bool Tremolo;
        public bool Vibrato;
        public bool SustainFlag;
        public int Waveform;
        public EnvState State = EnvState.Off;
        public double EnvDb = MaxDb;
        public double FixedGain = 1.0;
        public double AttackDbPerSec;
        public double DecayDbPerSec;
        public double ReleaseDbPerSec;
        public double SustainDb;
        public double Last;
        public double Prev;
    }

    private sealed class Channel
    {
        public int Fnum;
        public int Block;
        public bool KeyOn;
        public int Feedback;
        public int Connection;
        public Operator Mod = null!;
        public Operator Car = null!;
    }

    public Opl2(int sampleRate)
    {
        if (sampleRate < 8000)
        {
            sampleRate = 8000;
        }
        _rate = sampleRate;
        _rateScale = ChipRate / sampleRate;
        _dt = 1.0 / sampleRate;
        for (int i = 0; i < Points; i++)
        {
            _sine[i] = Math.Sin(2.0 * Math.PI * i / Points);
        }
        for (int i = 0; i < _gainDb.Length; i++)
        {
            _gainDb[i] = Math.Pow(10.0, -(i / 10.0) / 20.0);
        }
        for (int ch = 0; ch < 9; ch++)
        {
            // Operator register offsets: three groups of three channels, each
            // group eight bytes apart, carriers three bytes after modulators.
            int baseOffset = (ch / 3) * 8 + (ch % 3);
            var channel = new Channel();
            channel.Mod = new Operator
            {
                Offset = baseOffset,
                Channel = channel,
                Carrier = false,
            };
            channel.Car = new Operator
            {
                Offset = baseOffset + 3,
                Channel = channel,
                Carrier = true,
            };
            _channels[ch] = channel;
            _ops[ch * 2] = channel.Mod;
            _ops[ch * 2 + 1] = channel.Car;
        }
    }

    public int SampleRate => _rate;

    /// <summary>Applies one register write (register and value are already u8).</summary>
    public void Write(int reg, int val)
    {
        reg &= 0xFF;
        val &= 0xFF;
        lock (_lock)
        {
            _reg[reg] = (byte)val;
            switch (reg)
            {
                case 0x01:
                    _waveSelect = (val & 0x20) != 0;
                    foreach (var op in _ops)
                    {
                        SetWaveform(op, op.Waveform);
                    }
                    return;
                case 0xBD:
                    _amDepthDb = (val & 0x80) != 0 ? 4.8 : 1.0;
                    _vibDepthCents = (val & 0x40) != 0 ? 14.0 : 7.0;
                    // Rhythm gates: BD/SD/TOM/TC/HH are bits 4..0.  Only key
                    // them when rhythm mode itself is on; the game's music
                    // keeps it off, but the driver's SFX path may use it.
                    if ((val & 0x20) != 0)
                    {
                        RhythmGate(0, _channels[6].Mod, (val & 0x10) != 0);
                        RhythmGate(1, _channels[6].Car, (val & 0x10) != 0);
                        RhythmGate(2, _channels[7].Car, (val & 0x08) != 0);
                        RhythmGate(3, _channels[8].Mod, (val & 0x04) != 0);
                        RhythmGate(4, _channels[8].Car, (val & 0x02) != 0);
                        RhythmGate(5, _channels[7].Mod, (val & 0x01) != 0);
                    }
                    return;
            }

            if (reg >= 0x20 && reg <= 0x35)
            {
                var op = OperatorAt(reg - 0x20);
                if (op is not null)
                {
                    op.Tremolo = (val & 0x80) != 0;
                    op.Vibrato = (val & 0x40) != 0;
                    op.SustainFlag = (val & 0x20) != 0;
                    op.KeyScaleRate = (val >> 4) & 1;
                    op.Multiple = val & 0x0F;   // 0 = half frequency
                    Recalculate(op);
                }
                return;
            }
            if (reg >= 0x40 && reg <= 0x55)
            {
                var op = OperatorAt(reg - 0x40);
                if (op is not null)
                {
                    op.Ksl = (val >> 6) & 3;
                    op.TotalLevel = val & 0x3F;
                    Recalculate(op);
                }
                return;
            }
            if (reg >= 0x60 && reg <= 0x75)
            {
                var op = OperatorAt(reg - 0x60);
                if (op is not null)
                {
                    op.Attack = (val >> 4) & 0x0F;
                    op.Decay = val & 0x0F;
                    Recalculate(op);
                }
                return;
            }
            if (reg >= 0x80 && reg <= 0x95)
            {
                var op = OperatorAt(reg - 0x80);
                if (op is not null)
                {
                    op.SustainLevel = (val >> 4) & 0x0F;
                    op.Release = val & 0x0F;
                    Recalculate(op);
                }
                return;
            }
            if (reg >= 0xA0 && reg <= 0xA8)
            {
                var ch = _channels[reg - 0xA0];
                ch.Fnum = (ch.Fnum & 0x300) | val;
                RecalculateChannel(ch);
                return;
            }
            if (reg >= 0xB0 && reg <= 0xB8)
            {
                var ch = _channels[reg - 0xB0];
                ch.Fnum = (ch.Fnum & 0xFF) | ((val & 3) << 8);
                ch.Block = (val >> 2) & 7;
                RecalculateChannel(ch);
                bool key = (val & 0x20) != 0;
                if (key != ch.KeyOn)
                {
                    ch.KeyOn = key;
                    Key(ch, key);
                }
                return;
            }
            if (reg >= 0xC0 && reg <= 0xC8)
            {
                var ch = _channels[reg - 0xC0];
                ch.Feedback = (val >> 1) & 7;
                ch.Connection = val & 1;
                return;
            }
            if (reg >= 0xE0 && reg <= 0xF5)
            {
                var op = OperatorAt(reg - 0xE0);
                if (op is not null)
                {
                    SetWaveform(op, val & 3);
                }
                return;
            }
        }
    }

    /// <summary>Adds <paramref name="frames"/> mono samples to the mix.</summary>
    public void RenderMono(int[] mix, int frames)
    {
        lock (_lock)
        {
            for (int n = 0; n < frames; n++)
            {
                mix[n] += (int)(RenderSample() * 32767.0);
            }
        }
    }

    private double RenderSample()
    {
        // The OPL2's LFOs: tremolo ~3.7 Hz, vibrato ~6.1 Hz.
        _amPhase += 3.7 * _dt;
        if (_amPhase >= 1.0)
        {
            _amPhase -= 1.0;
        }
        _vibPhase += 6.1 * _dt;
        if (_vibPhase >= 1.0)
        {
            _vibPhase -= 1.0;
        }
        double amGain = _gainDb[Math.Clamp((int)(_amDepthDb * 0.5 * (1.0 - Math.Cos(2.0 * Math.PI * _amPhase)) * 10.0), 0, _gainDb.Length - 1)];
        double vibFactor = 1.0;
        if (_vibDepthCents != 0)
        {
            vibFactor = Math.Pow(2.0, _vibDepthCents * Math.Sin(2.0 * Math.PI * _vibPhase) / 1200.0);
        }

        double sum = 0.0;
        foreach (var ch in _channels)
        {
            var mod = ch.Mod;
            var car = ch.Car;

            double modPhase = mod.Phase;
            if (ch.Feedback != 0)
            {
                double fb = (mod.Last + mod.Prev) * 0.5 * FeedbackPoints[ch.Feedback];
                modPhase += fb;
            }
            double modOut = Wave(mod, modPhase) * Gain(mod, amGain);
            mod.Prev = mod.Last;
            mod.Last = modOut;

            double carPhase = car.Phase;
            double carOut;
            if (ch.Connection == 0)
            {
                carOut = Wave(car, carPhase + modOut * ModIndexPoints) * Gain(car, amGain);
            }
            else
            {
                carOut = Wave(car, carPhase) * Gain(car, amGain) + modOut;
            }

            Advance(mod, vibFactor);
            Advance(car, vibFactor);
            sum += carOut;
        }
        // The chip's DAC sums the channels; one full-scale carrier is about
        // an eighth of the 16-bit range, and the reference DOSBox capture
        // peaks near 42% for a nine-channel arrangement.
        return sum * 0.17;
    }

    private double Gain(Operator op, double amGain)
    {
        int idx = (int)(op.EnvDb * 10.0);
        if (idx >= _gainDb.Length)
        {
            return 0.0;
        }
        double g = _gainDb[idx] * op.FixedGain;
        if (op.Tremolo)
        {
            g *= amGain;
        }
        return g;
    }

    private double Wave(Operator op, double phase)
    {
        int p = (int)phase & (Points - 1);
        int wave = _waveSelect ? op.Waveform : 0;
        return wave switch
        {
            1 => p < Points / 2 ? _sine[p] : 0.0,
            2 => Math.Abs(_sine[p]),
            3 => p < Points / 4 ? _sine[p] : 0.0,
            _ => _sine[p],
        };
    }

    private void Advance(Operator op, double vibFactor)
    {
        double inc = op.Increment;
        if (op.Vibrato)
        {
            inc *= vibFactor;
        }
        op.Phase += inc;
        if (op.Phase >= Points)
        {
            op.Phase %= Points;
        }
        if (op.State == EnvState.Off)
        {
            return;
        }
        switch (op.State)
        {
            case EnvState.Attack:
                if (op.AttackDbPerSec <= 0.0)
                {
                    break;
                }
                op.EnvDb -= op.AttackDbPerSec * _dt;
                if (op.EnvDb <= 0.0)
                {
                    op.EnvDb = 0.0;
                    op.State = EnvState.Decay;
                }
                break;
            case EnvState.Decay:
                op.EnvDb += op.DecayDbPerSec * _dt;
                if (op.EnvDb >= op.SustainDb)
                {
                    op.EnvDb = op.SustainDb;
                    op.State = op.SustainFlag ? EnvState.Sustain : EnvState.DecayToEnd;
                }
                break;
            case EnvState.Sustain:
                // The envelope type can change while a note is held: EGT=0
                // leaves the sustain level and continues to silence.
                if (!op.SustainFlag)
                {
                    op.State = EnvState.DecayToEnd;
                }
                break;
            case EnvState.DecayToEnd:
                if (op.SustainFlag && op.Channel.KeyOn)
                {
                    op.State = EnvState.Sustain;
                    break;
                }
                // EGT=0: the sustain stage is skipped for the release rate.
                op.EnvDb += op.ReleaseDbPerSec * _dt;
                if (op.EnvDb >= MaxDb)
                {
                    op.EnvDb = MaxDb;
                    op.State = EnvState.Off;
                }
                break;
            case EnvState.Release:
                op.EnvDb += op.ReleaseDbPerSec * _dt;
                if (op.EnvDb >= MaxDb)
                {
                    op.EnvDb = MaxDb;
                    op.State = EnvState.Off;
                }
                break;
        }
    }

    private void Key(Channel ch, bool on)
    {
        foreach (var op in new[] { ch.Mod, ch.Car })
        {
            if (!on)
            {
                if (op.State != EnvState.Off)
                {
                    op.State = EnvState.Release;
                }
                continue;
            }
            op.Phase = 0.0;
            op.Last = 0.0;
            op.Prev = 0.0;
            if (op.State == EnvState.Off)
            {
                op.EnvDb = MaxDb;
            }
            op.State = EnvState.Attack;
        }
    }

    private readonly bool[] _rhythmGate = new bool[6];

    private void RhythmGate(int gate, Operator op, bool on)
    {
        if (on == _rhythmGate[gate])
        {
            return;
        }
        _rhythmGate[gate] = on;
        if (on)
        {
            op.Phase = 0.0;
            op.Last = 0.0;
            op.Prev = 0.0;
            op.State = EnvState.Attack;
        }
        else if (op.State != EnvState.Off)
        {
            op.State = op.Release == 0 ? EnvState.Sustain : EnvState.Release;
        }
    }

    private void SetWaveform(Operator op, int wave)
    {
        op.Waveform = wave & 3;
    }

    private void RecalculateChannel(Channel ch)
    {
        foreach (var op in new[] { ch.Mod, ch.Car })
        {
            op.Fnum = ch.Fnum;
            op.Block = ch.Block;
            Recalculate(op);
        }
    }

    private void Recalculate(Operator op)
    {
        double multiple = op.Multiple == 0 ? 0.5 : op.Multiple;
        op.Increment = op.Fnum * (1 << op.Block) * multiple * _rateScale / Points;

        // Key scale value (NTS=0, which is what the game and driver use).
        int ksv = (op.Block << 1) | ((op.Fnum >> 9) & 1);
        int ks = op.KeyScaleRate != 0 ? ksv : (ksv >> 2);
        int ar6 = Math.Min(63, op.Attack * 4 + ks);
        int dr6 = Math.Min(63, op.Decay * 4 + ks);
        int rr6 = Math.Min(63, op.Release * 4 + ks);
        op.AttackDbPerSec = DbPerSec(ar6, AttackTimeMs);
        op.DecayDbPerSec = DbPerSec(dr6, DecayTimeMs);
        op.ReleaseDbPerSec = DbPerSec(rr6, DecayTimeMs);
        op.SustainDb = op.SustainLevel == 15 ? 93.0 : op.SustainLevel * 3.0;

        int kslRaw = KslRom[(op.Fnum >> 6) & 0x0F] - ((8 - op.Block) << 3);
        if (kslRaw < 0)
        {
            kslRaw = 0;
        }
        double kslScale = op.Ksl switch
        {
            1 => 1.0,
            2 => 0.5,
            3 => 2.0,
            _ => 0.0,
        };
        double fixedDb = op.TotalLevel * 0.75 + kslRaw * 0.375 * kslScale;
        int idx = Math.Clamp((int)(fixedDb * 10.0), 0, _gainDb.Length - 1);
        op.FixedGain = _gainDb[idx];
    }

    private static double DbPerSec(int rate6, double[] timesMs)
    {
        if (rate6 <= 3)
        {
            return 0.0;                     // rates 0..3 never move
        }
        double ms = timesMs[Math.Clamp(rate6, 0, 63)];
        return ms <= 0.0 ? MaxDb / 0.0001 : MaxDb / (ms / 1000.0);
    }

    private Operator? OperatorAt(int offset)
    {
        foreach (var op in _ops)
        {
            if (op.Offset == offset)
            {
                return op;
            }
        }
        return null;
    }
}
