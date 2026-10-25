// Client-side OPL (FM) state.
//
// The engine streams OPL register writes over the AUDIO family (docs/protocol.md, family 2);
// this class is the front door: it keeps one 256-byte register file per FM port (what the
// hardware would have), feeds each port's chip, and renders the mix.  The synthesiser itself
// lives in Opl2.cs so the register file stays inspectable and the tests can reason about it.
//
// Timed writes (`opl_batch_timed`) are placed on the render timeline by their PIT stamp, so a
// batch that carries several driver ticks keeps its spacing instead of collapsing into one
// instant.  The timeline is anchored when the first timed write arrives and follows the stamps
// from there; a write that maps behind the render front is played at the front (late), which is
// what the old untimed path always did.
//
// The PC speaker (`speaker` messages) rides the same timeline: the game's beep fallback gates
// port 61h for a few BIOS ticks, and the client renders a square wave between the on and off
// events, mixed with the FM output.

namespace Yt2.Client;

public sealed class Opl
{
    private const int RegistersPerPort = 256;
    private const long PitHz = 1_193_182;               // 8253 input clock
    private const int MaxEvents = 8192;                 // ~80 s of music at 96 Hz
    private const int SpeakerAmplitude = 9830;          // 30% fs; the beep must cut through
    private readonly Dictionary<int, byte[]> _banks = [];
    private readonly Dictionary<int, Opl2> _chips = [];
    private readonly Queue<OplEvent> _events = new();
    private readonly object _sync = new();
    private readonly int[] _mix = new int[8192];
    private int _rate = 44100;
    private long _frame;                                // next frame to render
    private long _nextFrame;                            // frame of the last timed write
    private long _stampRem;                             // fractional frame carry
    private uint _lastStamp;
    private bool _haveStamp;
    private int _speakerDivisor;                        // PIT ch2 reload, 0 = off
    private double _speakerPhase;
    private double _speakerStep;                        // cycles per sample
    private readonly List<ActiveSample> _activeSamples = [];
    private SampleBank? _sampleSource;
    private long _sampleEndFrame;                       // for the offline WAV tail
    private int _suppressBeep;                          // 1 queued, 2 on edge skipped
    private const int SampleAmplitude = 96;             // 37.5% fs per sample
    private const int MaxActiveSamples = 8;

    /// <summary>The port of the last write (the FM base from the BLASTER setting).</summary>
    public int Port { get; private set; }
    /// <summary>False while the stream says sound is off (audio_ctrl on=0).</summary>
    public bool Enabled { get; private set; } = true;
    /// <summary>Rate the client synthesises at, from audio_ctrl.</summary>
    public int Rate => _rate;
    /// <summary>Format word from audio_ctrl (0 = signed 16-bit stereo).</summary>
    public int Format { get; private set; }

    public long Writes { get; private set; }
    public long SpeakerMessages { get; private set; }
    public long SampleMessages { get; private set; }
    public long SamplePlays { get; private set; }
    public long UnknownSamples { get; private set; }
    public long Scheduled { get; private set; }
    public long LateWrites { get; private set; }
    public long Batches { get; private set; }
    public long Controls { get; private set; }
    public long Samples { get; private set; }

    /// <summary>
    /// WORLD.DAT sample bank, set once by the client before the stream starts.
    /// Null means the file was not found; sample messages then stay silent and
    /// the PC-speaker fallback (effect 3) still plays.
    /// </summary>
    public SampleBank? SampleSource
    {
        get => _sampleSource;
        set
        {
            lock (_sync)
            {
                _sampleSource = value;
            }
        }
    }

    /// <summary>Absolute frame where the last sample ends (offline WAV tail).</summary>
    public long SampleEndFrame
    {
        get
        {
            lock (_sync)
            {
                return _sampleEndFrame;
            }
        }
    }

    /// <summary>
    /// Offline mode: the stamps define an absolute timeline from the first
    /// write instead of being anchored to the live render front, and nothing
    /// is clamped or dropped.  The offline stream renderer sets this before it
    /// applies the recording.
    /// </summary>
    public bool FreeRun { get; set; }

    /// <summary>Absolute frame of the last scheduled write (the stream's length).</summary>
    public long NextFrame => _nextFrame;

    /// <summary>The register file of one FM port, allocated on first use.</summary>
    public byte[] Bank(int port)
    {
        if (!_banks.TryGetValue(port, out byte[]? bank))
        {
            bank = new byte[RegistersPerPort];
            _banks[port] = bank;
        }
        return bank;
    }

    /// <summary>Applies one OPL register write (already bounded to a u8 by the caller).</summary>
    public void Write(int port, int reg, int val)
    {
        lock (_sync)
        {
            ApplyLocked(port, reg, val);
        }
    }

    /// <summary>
    /// Applies one timed write.  The stamp is a 32-bit PIT count (docs/protocol.md, AUDIO);
    /// only its distance from the previous stamp matters.  A backwards step means the driver
    /// reset its tick phase at a music start, so it counts as "now".
    /// </summary>
    public void WriteTimed(int port, int reg, int val, uint stamp)
    {
        lock (_sync)
        {
            long frame = Advance(stamp);
            if (!FreeRun && _events.Count >= MaxEvents)
            {
                ApplyLocked(port, reg, val);            // the render is not draining
                return;
            }
            _events.Enqueue(new OplEvent(frame, EventKind.Write, port,
                ((reg & 0xFF) << 8) | (val & 0xFF)));
            Scheduled++;
        }
    }

    /// <summary>
    /// Applies one PC-speaker edge (AUDIO type 4).  `on` gates a square wave at
    /// 1,193,182/divisor Hz; `off` stops it.  The edge is placed at its stamp,
    /// like a register write.
    /// </summary>
    public void Speaker(bool on, int divisor, uint stamp)
    {
        lock (_sync)
        {
            long frame = Advance(stamp);
            SpeakerMessages++;
            if (!FreeRun && _events.Count >= MaxEvents)
            {
                ApplySpeaker(on, divisor);
                return;
            }
            _events.Enqueue(new OplEvent(frame, EventKind.Speaker, on ? 1 : 0,
                divisor & 0xFFFF));
            Scheduled++;
        }
    }

    /// <summary>
    /// Applies one sound-effect message (AUDIO type 5).  The seam sends the
    /// effect id, the WORLD.DAT VOC offset from the game's own SFX table and a
    /// PIT stamp; the record starts on the same timeline as the OPL writes.
    /// Effect 3 replaces its PC-speaker fallback: the sample is queued first,
    /// so the next speaker on/off pair is dropped by <see cref="ApplyEvent"/>.
    /// </summary>
    public void Sample(int id, uint offset, uint stamp)
    {
        lock (_sync)
        {
            long frame = Advance(stamp);
            SampleMessages++;
            Sample? sample = _sampleSource?.Get(offset);
            if (sample is null)
            {
                UnknownSamples++;
                return;
            }
            if (id == 3 && _suppressBeep == 0)
            {
                _suppressBeep = 1;                      // the sample replaces the beep
            }
            long end = frame + (long)Math.Ceiling(sample.Duration * _rate);
            if (end > _sampleEndFrame)
            {
                _sampleEndFrame = end;
            }
            SamplePlays++;
            if (!FreeRun && _events.Count >= MaxEvents)
            {
                StartSample(sample);                    // the render is not draining
                return;
            }
            _events.Enqueue(new OplEvent(frame, EventKind.Sample, id, (int)offset));
            Scheduled++;
        }
    }

    /// <summary>
    /// Maps one stamp onto the render timeline: the first stamp anchors at the
    /// render front, later ones advance it by their modular difference.  In
    /// FreeRun mode the first anchor is frame 0 and nothing is clamped.
    /// </summary>
    private long Advance(uint stamp)
    {
        if (!_haveStamp)
        {
            _haveStamp = true;
            _lastStamp = stamp;
            _nextFrame = FreeRun ? 0 : _frame;
            return _nextFrame;
        }
        uint delta = stamp - _lastStamp;
        _lastStamp = stamp;
        if (delta <= 0x8000_0000u)                      // a larger step is a reset
        {
            _stampRem += (long)delta * _rate;
            _nextFrame += _stampRem / PitHz;
            _stampRem %= PitHz;
        }
        if (!FreeRun)
        {
            if (_nextFrame < _frame)
            {
                _nextFrame = _frame;                    // late: play it at the front
                LateWrites++;
            }
            else if (_nextFrame > _frame + (long)_rate * 10)
            {
                _nextFrame = _frame;                    // replay skip: forget the gap
                _stampRem = 0;
            }
        }
        return _nextFrame;
    }

    /// <summary>Applies an audio_ctrl message.</summary>
    public void Control(int rate, int format, bool on)
    {
        lock (_sync)
        {
            Format = format;
            Enabled = on;
            Controls++;
            if (rate >= 8000 && rate != _rate)
            {
                // Rebuild each chip at the new rate and replay the register
                // files so the state (including silent registers) survives.
                _rate = rate;
                _chips.Clear();
                _speakerStep = _speakerDivisor == 0
                    ? 0.0
                    : (PitHz / (double)_speakerDivisor) / _rate;
                foreach (var active in _activeSamples)
                {
                    active.Step = active.Sample.Rate / _rate;
                }
                foreach (var (port, bank) in _banks)
                {
                    var chip = new Opl2(_rate);
                    for (int reg = 0; reg < bank.Length; reg++)
                    {
                        if (bank[reg] != 0)
                        {
                            chip.Write(reg, bank[reg]);
                        }
                    }
                    _chips[port] = chip;
                }
            }
        }
    }

    public void Batch() => Batches++;

    /// <summary>
    /// Renders interleaved signed 16-bit stereo frames from every port.  The
    /// caller owns the buffer; the mix is scaled and clamped here.  Timed
    /// writes are applied at their frame, so the render is split around them.
    /// </summary>
    public void Render(short[] output, int frames)
    {
        if (frames <= 0)
        {
            return;
        }
        if (frames * 2 > output.Length)
        {
            frames = output.Length / 2;
        }
        frames = Math.Min(frames, _mix.Length);
        lock (_sync)
        {
            int done = 0;
            while (done < frames)
            {
                while (_events.Count > 0 && _events.Peek().Frame <= _frame)
                {
                    ApplyEvent(_events.Dequeue());
                }
                int chunk = frames - done;
                if (_events.Count > 0)
                {
                    long until = _events.Peek().Frame - _frame;
                    if (until < chunk)
                    {
                        chunk = (int)Math.Max(1, until);
                    }
                }
                Array.Clear(_mix, 0, chunk);
                foreach (var chip in _chips.Values)
                {
                    chip.RenderMono(_mix, chunk);
                }
                MixSamples(_mix, chunk);
                for (int i = 0; i < chunk; i++)
                {
                    int s = _mix[i];
                    if (_speakerDivisor != 0)
                    {
                        _speakerPhase += _speakerStep;
                        _speakerPhase -= Math.Floor(_speakerPhase);
                        s += _speakerPhase < 0.5 ? SpeakerAmplitude : -SpeakerAmplitude;
                    }
                    if (s > short.MaxValue)
                    {
                        s = short.MaxValue;
                    }
                    else if (s < short.MinValue)
                    {
                        s = short.MinValue;
                    }
                    int at = done + i;
                    output[at * 2] = (short)s;
                    output[at * 2 + 1] = (short)s;
                }
                _frame += chunk;
                done += chunk;
            }
            Samples += frames;
        }
    }

    private void ApplyEvent(in OplEvent e)
    {
        switch (e.Kind)
        {
            case EventKind.Speaker:
                if (e.A != 0)
                {
                    if (_suppressBeep == 1)
                    {
                        _suppressBeep = 2;          // effect 3's sample is playing
                        return;
                    }
                    if (_suppressBeep == 2)
                    {
                        return;
                    }
                }
                else
                {
                    if (_suppressBeep == 2)
                    {
                        _suppressBeep = 0;
                        return;
                    }
                    _suppressBeep = 0;
                }
                ApplySpeaker(e.A != 0, e.B);
                break;

            case EventKind.Sample:
                if (_sampleSource?.Get((uint)e.B) is { } sample)
                {
                    StartSample(sample);
                }
                break;

            default:
                ApplyLocked(e.A, (e.B >> 8) & 0xFF, e.B & 0xFF);
                break;
        }
    }

    private void StartSample(Sample sample)
    {
        if (_activeSamples.Count >= MaxActiveSamples)
        {
            _activeSamples.RemoveAt(0);
        }
        _activeSamples.Add(new ActiveSample(sample, _rate));
    }

    /// <summary>Adds every active sample to the current FM mix (linear resample).</summary>
    private void MixSamples(int[] mix, int frames)
    {
        for (int n = _activeSamples.Count - 1; n >= 0; n--)
        {
            ActiveSample active = _activeSamples[n];
            byte[] pcm = active.Sample.Pcm;
            int last = pcm.Length - 1;
            if (last < 1)
            {
                _activeSamples.RemoveAt(n);
                continue;
            }
            double position = active.Position;
            for (int i = 0; i < frames; i++)
            {
                if (position >= last)
                {
                    break;
                }
                int index = (int)position;
                double fraction = position - index;
                int a = pcm[index] - 128;
                int b = pcm[index + 1] - 128;
                mix[i] += (int)((a + (b - a) * fraction) * SampleAmplitude);
                position += active.Step;
            }
            active.Position = position;
            if (position >= last)
            {
                _activeSamples.RemoveAt(n);
            }
        }
    }

    private void ApplySpeaker(bool on, int divisor)
    {
        _speakerDivisor = on && divisor != 0 ? divisor & 0xFFFF : 0;
        _speakerPhase = 0.0;
        _speakerStep = _speakerDivisor == 0
            ? 0.0
            : (PitHz / (double)_speakerDivisor) / _rate;
    }

    private void ApplyLocked(int port, int reg, int val)
    {
        reg &= 0xFF;
        val &= 0xFF;
        Port = port;
        Bank(port)[reg] = (byte)val;
        if (!_chips.TryGetValue(port, out Opl2? chip))
        {
            chip = new Opl2(_rate);
            _chips[port] = chip;
        }
        chip.Write(reg, val);
        Writes++;
    }

    /// <summary>
    /// One scheduled event: a register write (A = port, B = register &lt;&lt; 8 |
    /// value), a PC-speaker edge (A = 1 for on, B = PIT channel 2 divisor) or a
    /// sample start (A = effect id, B = WORLD.DAT VOC offset).
    /// </summary>
    private readonly record struct OplEvent(long Frame, EventKind Kind, int A, int B);

    private enum EventKind : byte
    {
        Write,
        Speaker,
        Sample,
    }

    private sealed class ActiveSample
    {
        public ActiveSample(Sample sample, int rate)
        {
            Sample = sample;
            Step = sample.Rate / rate;
        }

        public Sample Sample { get; }
        public double Position;
        public double Step { get; set; }
    }
}
