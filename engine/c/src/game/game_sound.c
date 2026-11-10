#include "yt2/game.h"
#include "yt2/sound.h"
#include "sound_tables.h"

#include <string.h>

/*
 * SBFMDRV 1.34 resident emulation.
 *
 * The table offsets and the routine semantics follow docs/sbfmdrv.md and the
 * annotated driver source.  The game only uses commands 1, 2, 3, 4, 6, 7 and
 * 8; the other entries are implemented only as far as the boundary needs them
 * (version, pause/resume, volume, repeat and the resident pointer queries).
 */

#define DRIVER_SEG YT2_SOUND_VECTOR_SEG
#define DRIVER_BASE ((uint32_t)DRIVER_SEG << 4)
#define DRIVER_PTR(machine, offset) \
    ((machine)->mem.bytes + DRIVER_BASE + (uint32_t)(offset))

static uint32_t yt2_flat32(uint16_t segment, uint16_t offset)
{
    return ((uint32_t)segment << 4) + (uint32_t)offset;
}

static uint32_t yt2_sound_pit_now(const yt2_machine *machine)
{
    return (uint32_t)((machine->cycles * YT2_PIT_HZ) /
                      ((uint64_t)machine->cycles_per_ms * 1000u));
}

static uint64_t yt2_sound_tick_interval(const yt2_machine *machine)
{
    uint64_t interval;

    if (machine->sound.pit_divisor == 0u) {
        return 1u;
    }
    interval = (uint64_t)machine->sound.pit_divisor *
               (uint64_t)machine->cycles_per_ms * 1000u / YT2_PIT_HZ;
    return interval == 0u ? 1u : interval;
}

static void yt2_sound_mirror(yt2_machine *machine)
{
    yt2_sound *sound = &machine->sound;
    uint8_t *driver = DRIVER_PTR(machine, 0);
    unsigned index;

    driver[0x0110] = 0x34u;
    driver[0x0111] = 0x01u;
    driver[0x0114] = (uint8_t)(sound->fm_port & 0xFFu);
    driver[0x0115] = (uint8_t)(sound->fm_port >> 8);
    driver[0x0116] = YT2_SOUND_VECTOR_NUMBER;
    driver[0x0117] = (uint8_t)sound->repeat_count;
    driver[0x0118] = (uint8_t)((uint16_t)sound->repeat_count >> 8);
    driver[0x0121] = (uint8_t)(sound->instrument_table & 0x0Fu);
    driver[0x0123] = (uint8_t)(sound->instrument_table >> 4);
    driver[0x0125] = (uint8_t)(sound->music_pos & 0x0Fu);
    driver[0x0127] = (uint8_t)(sound->music_pos >> 4);
    driver[0x0129] = (uint8_t)(sound->music_pos & 0x0Fu);
    driver[0x012B] = (uint8_t)(sound->music_pos >> 4);
    driver[0x012D] = (uint8_t)(sound->restart_pos & 0x0Fu);
    driver[0x012F] = (uint8_t)(sound->restart_pos >> 4);
    driver[0x0131] = (uint8_t)(sound->delay & 0xFF);
    driver[0x0132] = (uint8_t)((sound->delay >> 8) & 0xFF);
    driver[0x0133] = (uint8_t)((sound->delay >> 16) & 0xFF);
    driver[0x0134] = (uint8_t)((sound->delay >> 24) & 0xFF);
    driver[0x0135] = (uint8_t)(sound->status_ptr & 0x0Fu);
    driver[0x0137] = (uint8_t)(sound->status_ptr >> 4);
    driver[0x0139] = (uint8_t)(sound->callback_ptr & 0x0Fu);
    driver[0x013B] = (uint8_t)(sound->callback_ptr >> 4);
    driver[0x0141] = (uint8_t)(sound->bios_reload & 0xFF);
    driver[0x0142] = (uint8_t)(sound->bios_reload >> 8);
    driver[0x0143] = (uint8_t)(sound->pit_divisor & 0xFF);
    driver[0x0144] = (uint8_t)(sound->pit_divisor >> 8);
    driver[0x0145] = (uint8_t)(sound->last_divisor & 0xFF);
    driver[0x0146] = (uint8_t)(sound->last_divisor >> 8);
    driver[0x014B] = sound->voice_count;
    driver[0x014D] = sound->channel;
    driver[0x014F] = sound->command;
    driver[0x0151] = (uint8_t)(sound->sequencer_calls & 0xFF);
    driver[0x0152] = (uint8_t)(sound->sequencer_calls >> 8);
    driver[0x0153] = (uint8_t)(sound->transpose & 0xFF);
    driver[0x0154] = (uint8_t)(sound->transpose >> 8);
    driver[0x02C7] = sound->volume_current;
    driver[0x02C8] = sound->volume_target;
    driver[0x02C9] = sound->volume_countdown;
    driver[0x02CB] = sound->volume_interval;
    driver[0x02CD] = sound->volume_step;
    driver[0x02F1] = sound->instrument_count;
    driver[0x02F2] = sound->state;
    driver[0x02F3] = sound->last_note;
    driver[0x02F4] = sound->last_velocity;
    driver[0x02F5] = 0;
    driver[0x02F6] = sound->status_byte;
    driver[0x02F7] = sound->rhythm_shadow;
    driver[0x02F8] = sound->rhythm_mode;
    for (index = 0; index < 11u; index++) {
        driver[0x0259u + index] = sound->voice_state[index];
        driver[0x0264u + index] = sound->voice_map[index];
        driver[0x026Fu + index] = sound->voice_note[index];
        driver[0x027Au + index] = sound->voice_ksl[index];
        driver[0x0285u + index] = sound->voice_volume[index];
        driver[0x0290u + index] = sound->volume_input[index];
        driver[0x029Bu + index * 2u] = (uint8_t)(sound->voice_freq[index] & 0xFFu);
        driver[0x029Cu + index * 2u] = (uint8_t)(sound->voice_freq[index] >> 8);
        driver[0x02B1u + index * 2u] = (uint8_t)(sound->voice_age[index] & 0xFFu);
        driver[0x02B2u + index * 2u] = (uint8_t)(sound->voice_age[index] >> 8);
    }
    for (index = 0; index < 16u; index++) {
        driver[0x02FAu + index] = sound->program[index];
        driver[0x030Au + index * 2u] = (uint8_t)((uint16_t)sound->bend[index] & 0xFFu);
        driver[0x030Bu + index * 2u] = (uint8_t)((uint16_t)sound->bend[index] >> 8);
        driver[0x032Au + index] = sound->gate[index];
    }
}

static void yt2_sound_set_status(yt2_machine *machine, uint8_t value)
{
    yt2_sound *sound = &machine->sound;

    sound->status_byte = value;
    if (sound->status_ptr != 0u && sound->status_ptr < YT2_MEM_SIZE) {
        machine->mem.bytes[sound->status_ptr] = value;
    }
}

static void yt2_sound_schedule(yt2_machine *machine)
{
    yt2_sound *sound = &machine->sound;

    sound->tick_at = machine->cycles + yt2_sound_tick_interval(machine);
    sound->tick_active = 1;
}

static void yt2_sound_send_ctrl(yt2_machine *machine, uint8_t on)
{
    uint8_t payload[8];
    int length;

    if (!machine->link_ready) {
        return;
    }
    length = yt2_payload_audio_ctrl(payload, 44100u, 0u, on);
    if (length > 0) {
        (void)yt2_service_send_payload(&machine->service,
                                       (uint8_t)YT2_FAMILY_AUDIO, 0, payload,
                                       (uint32_t)length);
    }
    if (machine->sound.log != NULL) {
        fprintf(machine->sound.log, "C %08X %u\n",
                yt2_sound_pit_now(machine), (unsigned)on);
    }
}

static void yt2_sound_write(yt2_machine *machine, uint8_t reg, uint8_t value)
{
    yt2_sound *sound = &machine->sound;
    uint32_t pit = yt2_sound_pit_now(machine);
    uint8_t *record;

    if (sound->log != NULL) {
        fprintf(sound->log, "W %08X %02X %02X\n", pit, (unsigned)reg,
                (unsigned)value);
        sound->logged_writes++;
    }
    if (sound->pending_count >= YT2_SOUND_WRITE_CAPACITY) {
        yt2_sound_flush(machine);
        if (sound->pending_count >= YT2_SOUND_WRITE_CAPACITY) {
            return;
        }
    }
    record = sound->pending + sound->pending_count * YT2_SOUND_WRITE_BYTES;
    record[0] = (uint8_t)(pit & 0xFFu);
    record[1] = (uint8_t)((pit >> 8) & 0xFFu);
    record[2] = (uint8_t)((pit >> 16) & 0xFFu);
    record[3] = (uint8_t)((pit >> 24) & 0xFFu);
    record[4] = reg;
    record[5] = value;
    sound->pending_count++;
}

void yt2_sound_flush(yt2_machine *machine)
{
    yt2_sound *sound = &machine->sound;

    if (sound->install_pending) {
        /* SBFMDRV.COM's installer leaves the chip with waveform select on
           and CSM off (sub_19AF); the DRO snapshot shows reg 1 = 20h. */
        sound->install_pending = 0;
        if (machine->sound_enabled) {
            yt2_sound_write(machine, 0x01u, 0x20u);
            yt2_sound_write(machine, 0x08u, 0x00u);
        }
    }
    if (sound->ctrl_pending) {
        sound->ctrl_pending = 0;
        yt2_sound_send_ctrl(machine, sound->state == 1u ? 1u : 0u);
    }
    if (sound->pending_count == 0u) {
        return;
    }
    if (machine->link_ready) {
        uint32_t offset = 0u;

        while (offset < sound->pending_count) {
            uint32_t count = sound->pending_count - offset;
            uint8_t payload[YT2_MAX_PAYLOAD];
            int length;

            if (count > 128u) {
                count = 128u;
            }
            length = yt2_payload_audio_opl_batch_timed(
                payload, sound->fm_port, (uint8_t)count,
                sound->pending + offset * YT2_SOUND_WRITE_BYTES);
            if (length < 0 ||
                yt2_service_send_payload(&machine->service,
                                         (uint8_t)YT2_FAMILY_AUDIO, 0, payload,
                                         (uint32_t)length) != YT2_OK) {
                sound->pending_count = 0u;
                return;
            }
            offset += count;
        }
    }
    sound->pending_count = 0u;
}

void yt2_sound_speaker(yt2_machine *machine, uint8_t gate, uint16_t divisor)
{
    yt2_sound *sound = &machine->sound;

    if (sound->log != NULL) {
        fprintf(sound->log, "S %08X %u %u\n",
                yt2_sound_pit_now(machine), (unsigned)gate,
                (unsigned)divisor);
    }
    if (machine->link_ready) {
        uint8_t payload[8];
        int length = yt2_payload_audio_speaker(
            payload, gate, divisor, yt2_sound_pit_now(machine));

        if (length > 0) {
            (void)yt2_service_send_payload(&machine->service,
                                           (uint8_t)YT2_FAMILY_AUDIO, 0,
                                           payload, (uint32_t)length);
        }
    }
}

static void yt2_sound_reset_voices(yt2_machine *machine)
{
    yt2_sound *sound = &machine->sound;

    sound->voice_count = 9u;
    sound->rhythm_shadow = 0xC0u;
    yt2_sound_write(machine, 0xBDu, 0xC0u);
}

static void yt2_sound_load_default_patches(yt2_machine *machine)
{
    yt2_sound *sound = &machine->sound;
    unsigned voice;

    memset(sound->program, 0, sizeof(sound->program));
    memset(sound->voice_state, 0xFF, sizeof(sound->voice_state));
    for (voice = 0; voice < 9u; voice++) {
        unsigned base = yt2_sound_operator_base[voice];
        yt2_sound_write(machine, 0xBDu, 0xC0u);
        yt2_sound_write(machine, 0x08u, 0x00u);
        yt2_sound_write(machine, (uint8_t)(0x20u + base),
                        yt2_sound_default_patch[0]);
        yt2_sound_write(machine, (uint8_t)(0x23u + base),
                        yt2_sound_default_patch[1]);
        yt2_sound_write(machine, (uint8_t)(0x40u + base),
                        yt2_sound_default_patch[2]);
        yt2_sound_write(machine, (uint8_t)(0x43u + base),
                        yt2_sound_default_patch[3]);
        yt2_sound_write(machine, (uint8_t)(0x60u + base),
                        yt2_sound_default_patch[4]);
        yt2_sound_write(machine, (uint8_t)(0x63u + base),
                        yt2_sound_default_patch[5]);
        yt2_sound_write(machine, (uint8_t)(0x80u + base),
                        yt2_sound_default_patch[6]);
        yt2_sound_write(machine, (uint8_t)(0x83u + base),
                        yt2_sound_default_patch[7]);
        yt2_sound_write(machine, (uint8_t)(0xE0u + base),
                        yt2_sound_default_patch[8]);
        yt2_sound_write(machine, (uint8_t)(0xE3u + base),
                        yt2_sound_default_patch[9]);
        /* The shipped loader addresses the last byte as [02CEh+voice]+voice,
           not C0h+voice (the stream loader at 0C0Fh does add C0h); keep the
           quirk so the reset leaves the connection registers untouched. */
        yt2_sound_write(machine, (uint8_t)(base + voice),
                        yt2_sound_default_patch[10]);
    }
}

static void yt2_sound_load_patch(yt2_machine *machine, unsigned voice,
                                 unsigned program)
{
    yt2_sound *sound = &machine->sound;
    uint32_t record;
    uint8_t tl;
    uint8_t ksl;
    uint8_t attenuation;
    unsigned base;

    if (program >= sound->instrument_count) {
        return;
    }
    record = sound->instrument_table + program * 16u;
    if (record + 16u > YT2_MEM_SIZE) {
        return;
    }
    tl = machine->mem.bytes[record + 3u];
    if (sound->rhythm_mode != 0u && voice >= 7u) {
        tl = machine->mem.bytes[record + 2u];
    }
    ksl = (uint8_t)(tl & 0xC0u);
    sound->voice_ksl[voice] = ksl;
    attenuation = (uint8_t)(0x3Fu - (tl & 0x3Fu));
    sound->voice_volume[voice] = attenuation;
    sound->volume_input[voice] = attenuation;

    if (sound->rhythm_mode != 0u && voice > 6u) {
        return;                 /* percussion patches share the melodic load */
    }
    base = yt2_sound_operator_base[voice];
    yt2_sound_write(machine, (uint8_t)(0x20u + base),
                    machine->mem.bytes[record + 0u]);
    yt2_sound_write(machine, (uint8_t)(0x23u + base),
                    machine->mem.bytes[record + 1u]);
    yt2_sound_write(machine, (uint8_t)(0x40u + base),
                    machine->mem.bytes[record + 2u]);
    yt2_sound_write(machine, (uint8_t)(0x60u + base),
                    machine->mem.bytes[record + 4u]);
    yt2_sound_write(machine, (uint8_t)(0x63u + base),
                    machine->mem.bytes[record + 5u]);
    yt2_sound_write(machine, (uint8_t)(0x80u + base),
                    machine->mem.bytes[record + 6u]);
    yt2_sound_write(machine, (uint8_t)(0x83u + base),
                    machine->mem.bytes[record + 7u]);
    yt2_sound_write(machine, (uint8_t)(0xE0u + base),
                    machine->mem.bytes[record + 8u]);
    yt2_sound_write(machine, (uint8_t)(0xE3u + base),
                    machine->mem.bytes[record + 9u]);
    yt2_sound_write(machine, (uint8_t)(0xC0u + voice),
                    machine->mem.bytes[record + 10u]);
}

static uint16_t yt2_sound_frequency(yt2_machine *machine, uint8_t map)
{
    yt2_sound *sound = &machine->sound;
    int16_t octave = (int16_t)((map & 0x70u) >> 2);
    uint16_t semitone = (uint16_t)(map & 0x0Fu);
    int32_t index = (int32_t)semitone * 64 +
                    sound->bend[sound->channel];
    uint32_t table_index;
    uint16_t fnum;

    if (index < 0) {
        index += 0x300;
        octave = (int16_t)(octave - 4);
        if (octave < 0) {
            octave = 0;
            index = 0;
        }
    } else if (index >= 0x300) {
        index -= 0x300;
        octave = (int16_t)(octave + 4);
        if (octave > 0x1C) {
            index = 0x2FF;
            octave = 0x1C;
        }
    }
    table_index = (uint32_t)index;
    if (table_index >= 768u) {
        table_index = 767u;
    }
    fnum = yt2_sound_fnum[table_index];
    return (uint16_t)((fnum & 0x00FFu) |
                      (((fnum >> 8) | (uint16_t)octave) << 8));
}

static unsigned yt2_sound_allocate(yt2_machine *machine, uint8_t channel)
{
    yt2_sound *sound = &machine->sound;
    unsigned index;
    unsigned best = 0u;
    uint16_t oldest = 0u;

    for (index = 0; index < sound->voice_count; index++) {
        if (sound->voice_state[index] == (uint8_t)(0x80u | channel)) {
            return index;
        }
    }
    for (index = 0; index < sound->voice_count; index++) {
        if (sound->voice_state[index] == 0xFFu) {
            return index;
        }
    }
    for (index = 0; index < sound->voice_count; index++) {
        if ((sound->voice_state[index] & 0x80u) != 0u) {
            return index;
        }
    }
    for (index = 0; index < sound->voice_count; index++) {
        uint16_t age = (uint16_t)(sound->sequencer_calls -
                                  sound->voice_age[index]);

        if (age >= oldest) {
            oldest = age;
            best = index;
        }
    }
    yt2_sound_write(machine, (uint8_t)(0xA0u + best),
                    (uint8_t)(sound->voice_freq[best] & 0xFFu));
    yt2_sound_write(machine, (uint8_t)(0xB0u + best),
                    (uint8_t)(sound->voice_freq[best] >> 8));
    return best;
}

static void yt2_sound_note_off(yt2_machine *machine, uint8_t note)
{
    yt2_sound *sound = &machine->sound;
    unsigned index;

    if (sound->rhythm_mode != 0u && sound->channel >= 0x0Bu) {
        sound->rhythm_shadow = (uint8_t)(
            sound->rhythm_shadow & (uint8_t)~yt2_sound_drum_bits[
                sound->channel - 0x0Bu]);
        yt2_sound_write(machine, 0xBDu, sound->rhythm_shadow);
        return;
    }
    for (index = 0; index < sound->voice_count; index++) {
        if (sound->voice_note[index] == note &&
            (sound->voice_state[index] & 0x0Fu) == sound->channel) {
            sound->voice_state[index] |= 0x80u;
            yt2_sound_write(machine, (uint8_t)(0xA0u + index),
                            (uint8_t)(sound->voice_freq[index] & 0xFFu));
            yt2_sound_write(machine, (uint8_t)(0xB0u + index),
                            (uint8_t)(sound->voice_freq[index] >> 8));
            return;
        }
    }
}

static void yt2_sound_percussion(yt2_machine *machine, uint8_t note,
                                 uint8_t velocity)
{
    yt2_sound *sound = &machine->sound;
    unsigned drum = (unsigned)(sound->channel - 0x0Bu);
    unsigned slot = (unsigned)(sound->channel - 5u);
    int32_t volume;
    uint8_t value;
    uint8_t reg;

    if (drum >= 5u || slot >= 11u) {
        return;
    }
    sound->rhythm_shadow |= yt2_sound_drum_bits[drum];
    volume = (int16_t)yt2_sound_velocity[(velocity + 4u) >> 3];
    volume += sound->voice_volume[slot];
    if (volume < 0) {
        volume = 0;
    } else if (volume > 63) {
        volume = 63;
    }
    value = (uint8_t)((0x3Fu - volume) | sound->voice_ksl[slot]);
    reg = (uint8_t)(0x40u + yt2_sound_drum_levels[drum] +
                    (drum == 0u ? 3u : 0u));
    yt2_sound_write(machine, reg, value);
    sound->voice_note[slot] = note;
    sound->voice_freq[slot] = yt2_sound_frequency(machine,
                                                  yt2_sound_note_map[note]);
    yt2_sound_write(machine,
                    (uint8_t)(0xA0u + yt2_sound_drum_offsets[drum]),
                    (uint8_t)(sound->voice_freq[slot] & 0xFFu));
    yt2_sound_write(machine,
                    (uint8_t)(0xB0u + yt2_sound_drum_offsets[drum]),
                    (uint8_t)(sound->voice_freq[slot] >> 8));
    yt2_sound_write(machine, 0xBDu, sound->rhythm_shadow);
}

static void yt2_sound_note_on(yt2_machine *machine, uint8_t note,
                              uint8_t velocity)
{
    yt2_sound *sound = &machine->sound;
    unsigned voice;
    uint8_t old_state;
    int32_t volume;
    uint8_t value;
    uint8_t map;
    uint8_t transposed;

    sound->last_note = note;
    sound->last_velocity = velocity;
    if (velocity == 0u) {
        yt2_sound_note_off(machine, note);
        return;
    }
    if (sound->channel < 16u && sound->gate[sound->channel] == 0u) {
        return;
    }
    if (sound->rhythm_mode != 0u && sound->channel >= 0x0Bu) {
        yt2_sound_percussion(machine, note, velocity);
        return;
    }
    voice = yt2_sound_allocate(machine, sound->channel);
    old_state = sound->voice_state[voice];
    sound->voice_state[voice] = sound->channel;
    if ((old_state & 0x7Fu) != sound->channel) {
        yt2_sound_load_patch(machine, voice, sound->program[sound->channel]);
    }
    volume = (int16_t)yt2_sound_velocity[(velocity + 4u) >> 3];
    volume += sound->voice_volume[voice];
    if (volume < 0) {
        volume = 0;
    } else if (volume > 63) {
        volume = 63;
    }
    value = (uint8_t)((0x3Fu - volume) | sound->voice_ksl[voice]);
    yt2_sound_write(machine,
                    (uint8_t)(0x43u + yt2_sound_operator_base[voice]), value);
    transposed = note;
    if ((unsigned)transposed + sound->transpose < 128u) {
        transposed = (uint8_t)(transposed + sound->transpose);
    } else {
        transposed = 127u;
    }
    map = yt2_sound_note_map[transposed];
    sound->voice_map[voice] = map;
    sound->voice_note[voice] = note;
    sound->voice_freq[voice] = yt2_sound_frequency(machine, map);
    sound->voice_age[voice] = sound->sequencer_calls;
    yt2_sound_write(machine, (uint8_t)(0xA0u + voice),
                    (uint8_t)(sound->voice_freq[voice] & 0xFFu));
    yt2_sound_write(machine, (uint8_t)(0xB0u + voice),
                    (uint8_t)((sound->voice_freq[voice] >> 8) | 0x20u));
}

static void yt2_sound_control(yt2_machine *machine, uint8_t controller,
                              uint8_t value)
{
    yt2_sound *sound = &machine->sound;
    unsigned index;
    int16_t bend;

    switch (controller) {
    case 0x66u:
        yt2_sound_set_status(machine, value);
        break;
    case 0x67u:
        yt2_sound_load_default_patches(machine);
        sound->rhythm_mode = value != 0u ? 1u : 0u;
        sound->voice_count = value != 0u ? 6u : 9u;
        sound->rhythm_shadow = value != 0u ? 0xE0u : 0xC0u;
        yt2_sound_write(machine, 0xBDu, sound->rhythm_shadow);
        break;
    case 0x68u:
    case 0x69u:
        bend = (int16_t)(int8_t)value;
        if (controller == 0x69u) {
            bend = (int16_t)-bend;
        }
        bend = (int16_t)(bend / 4);
        sound->bend[sound->channel] = bend;
        for (index = 0; index < sound->voice_count; index++) {
            if ((sound->voice_state[index] & 0x0Fu) == sound->channel) {
                sound->voice_freq[index] = yt2_sound_frequency(
                    machine, sound->voice_map[index]);
                yt2_sound_write(machine, (uint8_t)(0xA0u + index),
                                (uint8_t)(sound->voice_freq[index] & 0xFFu));
                yt2_sound_write(machine, (uint8_t)(0xB0u + index),
                                (uint8_t)((sound->voice_freq[index] >> 8) |
                                         ((sound->voice_state[index] & 0x80u)
                                              ? 0u
                                              : 0x20u)));
            }
        }
        break;
    default:
        break;
    }
}

static void yt2_sound_program(yt2_machine *machine, uint8_t program)
{
    yt2_sound *sound = &machine->sound;
    unsigned index;

    if (sound->instrument_count == 0u) {
        return;
    }
    program = (uint8_t)(program % sound->instrument_count);
    sound->program[sound->channel] = program;
    for (index = 0; index < sound->voice_count; index++) {
        if ((sound->voice_state[index] & 0x0Fu) == sound->channel) {
            yt2_sound_load_patch(machine, index, program);
        }
    }
}

static void yt2_sound_silence(yt2_machine *machine)
{
    yt2_sound *sound = &machine->sound;
    int index;
    uint8_t reg;

    for (index = (int)sound->voice_count - 1; index >= 0; index--) {
        yt2_sound_write(machine, (uint8_t)(0x80u + index), 0x13u);
        if (sound->voice_state[index] <= 0x7Fu) {
            yt2_sound_write(machine, (uint8_t)(0xA0u + index),
                            (uint8_t)(sound->voice_freq[index] & 0xFFu));
            yt2_sound_write(machine, (uint8_t)(0xB0u + index),
                            (uint8_t)(sound->voice_freq[index] >> 8));
        }
    }
    sound->rhythm_shadow &= 0xE0u;
    yt2_sound_write(machine, 0xBDu, sound->rhythm_shadow);
    for (reg = 0x90u; reg <= 0x95u; reg++) {
        yt2_sound_write(machine, reg, 0xFFu);
    }
}

static uint32_t yt2_sound_read_vlq(yt2_machine *machine)
{
    yt2_sound *sound = &machine->sound;
    uint32_t value = 0u;
    uint8_t byte;

    do {
        byte = machine->mem.bytes[sound->music_pos++];
        value = (value << 7) | (uint32_t)(byte & 0x7Fu);
    } while ((byte & 0x80u) != 0u && sound->music_pos < YT2_MEM_SIZE);
    return value;
}

static uint8_t yt2_sound_read8(yt2_machine *machine)
{
    yt2_sound *sound = &machine->sound;

    return machine->mem.bytes[sound->music_pos++];
}

static void yt2_sound_jump(yt2_machine *machine, uint32_t offset)
{
    yt2_sound *sound = &machine->sound;

    sound->music_pos = (uint32_t)((int32_t)sound->music_pos +
                                  (int32_t)offset);
}

static void yt2_sound_sequencer(yt2_machine *machine)
{
    yt2_sound *sound = &machine->sound;
    int stopped;

    sound->sequencer_calls++;
    for (;;) {
        uint8_t first;
        uint8_t note;
        uint8_t velocity;
        uint32_t delay;
        uint32_t jump;

        if (sound->music_pos >= YT2_MEM_SIZE) {
            sound->delay_active = 0;
            sound->tick_active = 0;
            return;
        }
        first = machine->mem.bytes[sound->music_pos];
        if ((first & 0x80u) != 0u) {
            sound->music_pos++;
            sound->command = (uint8_t)((first >> 4) - 8u);
            sound->channel = (uint8_t)(first & 0x0Fu);
        }
        stopped = 0;
        switch (sound->command) {
        case 0:                 /* 8n note-off */
            note = yt2_sound_read8(machine);
            (void)yt2_sound_read8(machine);
            yt2_sound_note_off(machine, note);
            break;
        case 1:                 /* 9n note-on */
            note = yt2_sound_read8(machine);
            velocity = yt2_sound_read8(machine);
            yt2_sound_note_on(machine, note, velocity);
            break;
        case 2:                 /* An polyphonic aftertouch: ignored */
            (void)yt2_sound_read8(machine);
            (void)yt2_sound_read8(machine);
            break;
        case 3:                 /* Bn controller */
            note = yt2_sound_read8(machine);
            velocity = yt2_sound_read8(machine);
            yt2_sound_control(machine, note, velocity);
            break;
        case 4:                 /* Cn program */
            yt2_sound_program(machine, yt2_sound_read8(machine));
            break;
        case 5:                 /* Dn channel aftertouch: ignored */
            (void)yt2_sound_read8(machine);
            break;
        case 6:                 /* En pitch bend: ignored */
            (void)yt2_sound_read8(machine);
            (void)yt2_sound_read8(machine);
            break;
        default:                /* Fn */
            switch (sound->channel) {
            case 0x0u:          /* F0 callback + jump */
                sound->music_pos++;
                jump = yt2_sound_read_vlq(machine);
                yt2_sound_jump(machine, jump);
                break;
            case 0x0Cu:         /* FC stop */
                stopped = 1;
                break;
            case 0x0Fu:         /* FF marker + jump, or FF 2F stop */
                note = yt2_sound_read8(machine);
                if (note == 0x2Fu) {
                    stopped = 1;
                } else {
                    jump = yt2_sound_read_vlq(machine);
                    yt2_sound_jump(machine, jump);
                }
                break;
            case 0x07u:         /* F7 jump */
                jump = yt2_sound_read_vlq(machine);
                yt2_sound_jump(machine, jump);
                break;
            case 0x02u:         /* F2: 2-byte payload */
                (void)yt2_sound_read8(machine);
                (void)yt2_sound_read8(machine);
                break;
            case 0x03u:         /* F3: 1-byte payload */
                (void)yt2_sound_read8(machine);
                break;
            default:            /* F1, F4..F6, F8..FB, FD, FE */
                break;
            }
            break;
        }
        if (stopped != 0) {
            sound->state = 0u;
            sound->delay_active = 0;
            sound->tick_active = 0;
            /* The driver's stop (sub_1376) silences every voice and clears
               the rhythm shadow before it reports idle. */
            yt2_sound_silence(machine);
            yt2_sound_set_status(machine, 0u);
            if (sound->repeat_count != 0) {
                sound->music_pos = sound->restart_pos;
                sound->state = 1u;
                sound->delay = (int32_t)yt2_sound_read_vlq(machine);
                sound->delay_active = 1;
                yt2_sound_schedule(machine);
                if (sound->repeat_count != -1) {
                    sound->repeat_count--;
                }
            }
            sound->ctrl_pending = 1;
            return;
        }
        delay = yt2_sound_read_vlq(machine);
        if (delay == 0u) {
            continue;
        }
        sound->delay = (int32_t)(delay - 1u);
        sound->delay_active = 1;
        return;
    }
}

static void yt2_sound_tick(yt2_machine *machine)
{
    yt2_sound *sound = &machine->sound;

    if (sound->state != 1u || !sound->delay_active) {
        return;
    }
    if (sound->delay > 0) {
        sound->delay--;
        return;
    }
    yt2_sound_sequencer(machine);
}

void yt2_sound_poll(yt2_machine *machine)
{
    yt2_sound *sound = &machine->sound;
    unsigned guard = 0u;

    while (sound->tick_active && machine->cycles >= sound->tick_at) {
        uint64_t interval = yt2_sound_tick_interval(machine);

        sound->tick_at += interval;
        yt2_sound_tick(machine);
        if (++guard > 100000u) {
            break;
        }
    }
}

static void yt2_sound_api(yt2_machine *machine)
{
    yt2_sound *sound = &machine->sound;
    uint16_t command = machine->r.bx;
    uint16_t ax = machine->r.ax;
    uint16_t dx = machine->r.dx;
    uint16_t cx = machine->r.cx;
    uint32_t pointer;

    switch ((int16_t)command) {
    case 0:
        machine->r.ax = 0x0134u;
        break;
    case 1:
        pointer = sound->status_ptr;
        sound->status_ptr = yt2_flat32(dx, ax);
        yt2_sound_set_status(machine, 0u);
        machine->r.ax = (uint16_t)(pointer & 0xFFFFu);
        machine->r.dx = (uint16_t)(pointer >> 4);
        break;
    case 2:
        sound->instrument_count = (uint8_t)(cx & 0xFFu);
        sound->instrument_table = yt2_flat32(dx, ax);
        yt2_sound_reset_voices(machine);
        yt2_sound_load_default_patches(machine);
        machine->r.ax = 0u;
        break;
    case 3:
        sound->bios_reload = ax;
        machine->r.ax = ax;
        break;
    case 4:
        sound->pit_divisor = ax;
        sound->last_divisor = ax;
        if (sound->state == 1u) {
            yt2_sound_schedule(machine);
        }
        machine->r.ax = 0u;
        break;
    case 5:
        sound->transpose = ax;
        machine->r.ax = 0u;
        break;
    case 6:
        if (sound->state != 0u) {
            machine->r.ax = 0xFFFEu;
            break;
        }
        sound->music_pos = yt2_flat32(dx, ax);
        sound->restart_pos = sound->music_pos;
        memset(sound->bend, 0, sizeof(sound->bend));
        memset(sound->voice_state, 0xFF, 9u);
        sound->delay = (int32_t)yt2_sound_read_vlq(machine);
        sound->delay_active = 1;
        sound->sequencer_calls = 0u;
        yt2_sound_reset_voices(machine);
        sound->state = 1u;
        yt2_sound_set_status(machine, 0xFFu);
        yt2_sound_schedule(machine);
        sound->ctrl_pending = 1;
        machine->r.ax = 0u;
        break;
    case 7:
        if (sound->state == 0u) {
            machine->r.ax = 0xFFFDu;
            break;
        }
        sound->state = 0u;
        sound->delay_active = 0;
        sound->tick_active = 0;
        yt2_sound_silence(machine);
        yt2_sound_set_status(machine, 0u);
        sound->ctrl_pending = 1;
        machine->r.ax = 0u;
        break;
    case 8:
        sound->repeat_count = 0;
        sound->volume_current = 0xFFu;
        sound->volume_target = 0xFFu;
        if (sound->state != 0u) {
            sound->state = 0u;
            sound->delay_active = 0;
            sound->tick_active = 0;
            yt2_sound_silence(machine);
            yt2_sound_set_status(machine, 0u);
        }
        yt2_sound_reset_voices(machine);
        memset(sound->gate, 0x01, sizeof(sound->gate));
        sound->instrument_count = 0x10u;
        sound->instrument_table = yt2_flat32(DRIVER_SEG, 0x0159u);
        yt2_sound_load_default_patches(machine);
        sound->pit_divisor = 0x48D3u;
        sound->last_divisor = 0x48D3u;
        sound->transpose = 0u;
        sound->ctrl_pending = 1;
        machine->r.ax = 0u;
        break;
    case 9:
        if (sound->state != 1u) {
            machine->r.ax = 0xFFFDu;
            break;
        }
        sound->state = 2u;
        sound->tick_active = 0;
        yt2_sound_silence(machine);
        sound->ctrl_pending = 1;
        machine->r.ax = 0u;
        break;
    case 0x0A:
        if (sound->state != 2u) {
            machine->r.ax = 0xFFFCu;
            break;
        }
        sound->state = 1u;
        yt2_sound_schedule(machine);
        sound->ctrl_pending = 1;
        machine->r.ax = 0u;
        break;
    case 0x0B:
        sound->callback_ptr = yt2_flat32(dx, ax);
        machine->r.ax = 0u;
        break;
    case 0x0C:
        yt2_sound_mirror(machine);
        machine->r.ax = 0x032Au;
        machine->r.dx = DRIVER_SEG;
        break;
    case 0x0D:
        yt2_sound_mirror(machine);
        machine->r.ax = (uint16_t)(sound->music_pos & 0x0Fu);
        machine->r.dx = (uint16_t)(sound->music_pos >> 4);
        break;
    case 0x0E:
        sound->volume_target = (uint8_t)((ax >> 8) & 0xFFu);
        sound->volume_current = sound->volume_target;
        sound->volume_step = (uint8_t)(cx & 0xFFu);
        machine->r.ax = 0u;
        break;
    case 0x0F: {
        uint16_t previous = (uint16_t)sound->repeat_count;
        sound->repeat_count = (int16_t)ax;
        machine->r.ax = previous;
        break;
    }
    case -1:
        machine->r.ax = 0u;
        machine->r.dx = DRIVER_SEG;
        break;
    case -2:
        machine->r.ax = 0x0149u;
        machine->r.dx = DRIVER_SEG;
        break;
    case -3:
        machine->r.ax = 0x02F5u;
        machine->r.dx = DRIVER_SEG;
        break;
    case -4:
        machine->r.ax = 0u;
        break;
    case -5:
        machine->r.ax = 0x02F2u;
        machine->r.dx = DRIVER_SEG;
        break;
    default:
        machine->r.ax = 0xFFFFu;
        break;
    }
    yt2_sound_mirror(machine);
}

void yt2_sound_int(yt2_machine *machine)
{
    yt2_sound_api(machine);
}

int yt2_sound_vector(const yt2_machine *machine, uint8_t vector,
                     uint16_t *segment, uint16_t *offset)
{
    if (!machine->sound_enabled || vector != YT2_SOUND_VECTOR_NUMBER) {
        return 0;
    }
    *segment = YT2_SOUND_VECTOR_SEG;
    *offset = YT2_SOUND_API_OFFSET;
    return 1;
}

void yt2_sound_init(yt2_machine *machine)
{
    yt2_sound *sound = &machine->sound;
    uint8_t *driver = DRIVER_PTR(machine, 0);

    memset(sound, 0, sizeof(*sound));
    memset(driver, 0, 0x400u);
    memcpy(driver + 0x0103u, "FMDRV", 5u);
    memcpy(driver + 0x0159u, yt2_sound_default_instruments,
           sizeof(yt2_sound_default_instruments));
    memcpy(driver + 0x02CEu, yt2_sound_operator_base,
           sizeof(yt2_sound_operator_base));
    memcpy(driver + 0x02D7u, yt2_sound_default_patch,
           sizeof(yt2_sound_default_patch));
    memcpy(driver + 0x02E2u, yt2_sound_drum_levels,
           sizeof(yt2_sound_drum_levels));
    memcpy(driver + 0x02E7u, yt2_sound_drum_bits,
           sizeof(yt2_sound_drum_bits));
    memcpy(driver + 0x02ECu, yt2_sound_drum_offsets,
           sizeof(yt2_sound_drum_offsets));
    memcpy(driver + 0x039Cu, yt2_sound_note_map,
           sizeof(yt2_sound_note_map));
    memcpy(driver + 0x041Cu, yt2_sound_fnum, sizeof(yt2_sound_fnum));
    memcpy(driver + 0x0A1Cu, yt2_sound_velocity,
           sizeof(yt2_sound_velocity));

    sound->installed = 1;
    sound->fm_port = 0x0228u;
    sound->status_ptr = yt2_flat32(DRIVER_SEG, 0x02F6u);
    sound->instrument_table = yt2_flat32(DRIVER_SEG, 0x0159u);
    sound->instrument_count = 0x10u;
    sound->state = 0u;
    sound->voice_count = 9u;
    sound->rhythm_shadow = 0xC0u;
    sound->pit_divisor = 0x48D3u;
    sound->bios_reload = 0xFFFFu;
    sound->last_divisor = 0x48D3u;
    sound->volume_current = 0xFFu;
    sound->volume_target = 0xFFu;
    sound->ctrl_pending = 1;
    sound->install_pending = 1;
    memset(sound->gate, 0x01, sizeof(sound->gate));
    yt2_sound_mirror(machine);
}

void yt2_sound_shutdown(yt2_machine *machine)
{
    if (machine->sound.log != NULL) {
        fclose(machine->sound.log);
        machine->sound.log = NULL;
    }
}

void yt2_sound_set_log(yt2_machine *machine, FILE *stream)
{
    machine->sound.log = stream;
}
