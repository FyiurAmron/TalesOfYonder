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

namespace Yt2.Client;

public sealed class Opl
{
    private const int RegistersPerPort = 256;
    private const long PitHz = 1_193_182;               // 8253 input clock
    private const int MaxEvents = 8192;                 // ~80 s of music at 96 Hz
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

    /// <summary>The port of the last write (the FM base from the BLASTER setting).</summary>
    public int Port { get; private set; }
    /// <summary>False while the stream says sound is off (audio_ctrl on=0).</summary>
    public bool Enabled { get; private set; } = true;
    /// <summary>Rate the client synthesises at, from audio_ctrl.</summary>
    public int Rate => _rate;
    /// <summary>Format word from audio_ctrl (0 = signed 16-bit stereo).</summary>
    public int Format { get; private set; }

    public long Writes { get; private set; }
    public long Scheduled { get; private set; }
    public long LateWrites { get; private set; }
    public long Batches { get; private set; }
    public long Controls { get; private set; }
    public long Samples { get; private set; }

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
            if (!_haveStamp)
            {
                _haveStamp = true;
                _lastStamp = stamp;
                _nextFrame = FreeRun ? 0 : _frame;
            }
            else
            {
                uint delta = stamp - _lastStamp;
                _lastStamp = stamp;
                if (delta <= 0x8000_0000u)              // a larger step is a reset
                {
                    _stampRem += (long)delta * _rate;
                    _nextFrame += _stampRem / PitHz;
                    _stampRem %= PitHz;
                }
            }

            if (!FreeRun)
            {
                if (_nextFrame < _frame)
                {
                    _nextFrame = _frame;                // late: play it at the front
                    LateWrites++;
                }
                else if (_nextFrame > _frame + (long)_rate * 10)
                {
                    _nextFrame = _frame;                // replay skip: forget the gap
                    _stampRem = 0;
                }
                if (_events.Count >= MaxEvents)
                {
                    ApplyLocked(port, reg, val);        // the render is not draining
                    return;
                }
            }
            _events.Enqueue(new OplEvent(_nextFrame, port, reg, val));
            Scheduled++;
        }
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
                    var e = _events.Dequeue();
                    ApplyLocked(e.Port, e.Reg, e.Val);
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
                for (int i = 0; i < chunk; i++)
                {
                    int s = _mix[i];
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

    private readonly record struct OplEvent(long Frame, int Port, int Reg, int Val);
}
