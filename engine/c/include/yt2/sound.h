#ifndef YT2_SOUND_H
#define YT2_SOUND_H

#include <stdio.h>

#include "yt2/core.h"

/*
 * The resident SBFMDRV 1.34 emulation (M6).
 *
 * The game finds the driver through the INT 21h 35h vector scan and calls its
 * resident API through the self-patched `int 0`.  The driver is not executed
 * as 16-bit code: this module implements the documented resident state machine,
 * music sequencer and OPL writes (docs/sbfmdrv.md) and installs the signature
 * and data tables into a spare guest paragraph so the game's scanner sees a
 * normal resident driver.  The timer ISR is driven from the emulated guest
 * clock at the driver's PIT divisor; writes are timestamped in 8253 counts and
 * leave through the AUDIO family (opl_batch_timed, audio_ctrl, speaker).
 */

#define YT2_SOUND_VECTOR_SEG 0xF100u
#define YT2_SOUND_VECTOR_NUMBER 0x80u
#define YT2_SOUND_API_OFFSET 0x12E6u
#define YT2_SOUND_WRITE_CAPACITY 2048u
#define YT2_SOUND_WRITE_BYTES 6u

typedef struct yt2_sound {
    int installed;

    /* Resident API state (docs/sbfmdrv.md section 3). */
    uint32_t status_ptr;          /* command 1: one-byte status block */
    uint32_t music_pos;           /* current stream byte, flat */
    uint32_t restart_pos;         /* command 6 start pointer */
    uint32_t callback_ptr;        /* command B far callback, flat */
    uint32_t instrument_table;    /* command 2: flat CS:table */
    uint8_t instrument_count;
    uint8_t state;                /* 0 idle, 1 playing, 2 paused */
    int16_t repeat_count;         /* command F: -1 infinite */
    uint8_t voice_count;          /* 9 melodic / 6 rhythm */
    uint8_t rhythm_mode;
    uint8_t rhythm_shadow;        /* BDh shadow */
    uint8_t last_note;
    uint8_t last_velocity;
    uint8_t command;              /* current stream main command 0..7 */
    uint8_t channel;
    int32_t delay;                /* 32-bit timer countdown, signed */
    int delay_active;
    uint16_t sequencer_calls;     /* [0151h] diagnostics */

    /* Voice tables (offsets 0259h..02AC in the resident image). */
    uint8_t voice_state[11];
    uint8_t voice_note[11];
    uint8_t voice_map[11];
    uint8_t voice_ksl[11];
    uint8_t voice_volume[11];     /* carrier attenuation base 0285h */
    uint16_t voice_freq[11];
    uint16_t voice_age[11];

    /* Per-channel state. */
    uint8_t program[16];
    int16_t bend[16];
    uint8_t gate[16];             /* 032Ah; 1 = note-ons accepted */

    /* Volume / tempo. */
    uint8_t volume_current;
    uint8_t volume_target;
    uint8_t volume_step;
    uint8_t volume_interval;
    uint8_t volume_countdown;
    uint8_t volume_input[11];

    /* Hardware and timing. */
    uint16_t fm_port;             /* FM register base (0228h) */
    uint16_t pit_divisor;         /* [0143h] */
    uint16_t bios_reload;         /* [0141h] */
    uint16_t last_divisor;        /* [0145h] */
    uint16_t transpose;           /* [0153h] */
    uint8_t status_byte;
    uint64_t tick_at;             /* next driver tick in guest cycles */
    int tick_active;

    /* AUDIO output: 6-byte (u32 pit, u8 register, u8 value) records. */
    uint8_t pending[YT2_SOUND_WRITE_CAPACITY * YT2_SOUND_WRITE_BYTES];
    uint32_t pending_count;
    int ctrl_pending;
    int install_pending;          /* install-time reg 1/8 snapshot not yet sent */

    /* Diagnostics. */
    FILE *log;
    uint32_t logged_writes;
} yt2_sound;

struct yt2_machine;

void yt2_sound_init(struct yt2_machine *machine);
void yt2_sound_shutdown(struct yt2_machine *machine);

/* INT 80h (the resident driver vector) with BX = command. */
void yt2_sound_int(struct yt2_machine *machine);

/* INT 21h AH=35h answer for the resident FMDRV vector. */
int yt2_sound_vector(const struct yt2_machine *machine, uint8_t vector,
                     uint16_t *segment, uint16_t *offset);

/* Guest-clock hooks. */
void yt2_sound_poll(struct yt2_machine *machine);
void yt2_sound_flush(struct yt2_machine *machine);

/* PC speaker gate edges (PIT channel 2 divisor in the message). */
void yt2_sound_speaker(struct yt2_machine *machine, uint8_t gate,
                       uint16_t divisor);

/* Diagnostics: write one line per OPL/speaker/control event to `stream`. */
void yt2_sound_set_log(struct yt2_machine *machine, FILE *stream);

#endif /* YT2_SOUND_H */
