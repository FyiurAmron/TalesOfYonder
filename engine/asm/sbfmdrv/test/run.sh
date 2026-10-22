#!/bin/sh
# Black-box run: install the reference SBFMDRV.COM and exercise it under
# DOSBox-X (SB16 profile).
#   api        - TEST.COM INT API regression + crafted stream trace
#   uninstall  - install, SBFMDRV.COM /U, verify with TEST.COM
#   uninstall-late - install, load LATE.COM (a TSR), then SBFMDRV.COM /U: the
#                driver must refuse with error 0005h and leave its hooks, its
#                API vector and the TSR intact (TEST.COM L checks all four)
#   music      - play a real CTMF block from WORLD.DAT (MUSIC.BLK) with
#                MUSIC.COM and cross-check the sequencer positions
#                (MUSIC_TRACK selects the id, MUSIC_SONG=1 slices the song
#                instead of the game's record window)
#
# Music options:
#   MUSIC_PLAY=1     playback mode: the driver's timer ISR plays the song and
#                    the log reports the [0151h] delta and the elapsed BIOS
#                    ticks; the check compares both with the static walk
#   MUSIC_CAP=wav|dro|both  wrap the run in DX-CAPTURE (audio and/or OPL log);
#                    the check also reports the audio and/or register writes
#   MUSIC_TIMEOUT=N  DOSBox-X -time-limit (default 30 s, 90 s for playback)
#
# API option:
#   API_CAP=wav|dro|both  wrap TEST.COM in DX-CAPTURE; with dro the run's OPL
#                    writes are checked against the percussion path (perc.py)
# Outputs under build/runs/sbfmdrv/<timestamp>/ (RESULT.TXT / MUSIC.LOG).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../../.." && pwd)"
MODE="${1:-api}"
TRACK="${MUSIC_TRACK:-1}"
SONGARG=""
[ -n "$MUSIC_SONG" ] && SONGARG="--song"
TAIL=""
[ -n "$MUSIC_PLAY" ] && TAIL="P"
CAPFLAG=""
case "${MUSIC_CAP:-}" in
    1|wav|WAV) CAPFLAG="/A" ;;
    dro|DRO)   CAPFLAG="/O" ;;
    both|BOTH) CAPFLAG="/A /O" ;;
    "")        ;;
    *)         echo "MUSIC_CAP must be wav, dro or both" >&2; exit 2 ;;
esac
CAP=""
[ -n "$CAPFLAG" ] && CAP="DX-CAPTURE $CAPFLAG "
APIFLAG=""
case "${API_CAP:-}" in
    wav|WAV)   APIFLAG="/A" ;;
    dro|DRO)   APIFLAG="/O" ;;
    both|BOTH) APIFLAG="/A /O" ;;
    "")        ;;
    *)         echo "API_CAP must be wav, dro or both" >&2; exit 2 ;;
esac
if [ -n "$MUSIC_TIMEOUT" ]; then
    TIMEOUT="$MUSIC_TIMEOUT"
elif [ -n "$MUSIC_PLAY" ]; then
    TIMEOUT=90
else
    TIMEOUT=30
fi
case "$MODE" in
    api)       CONF="$HERE/dosbox-x.conf" ;;
    uninstall) CONF="$HERE/dosbox-x-uninstall.conf" ;;
    uninstall-late) CONF="$HERE/dosbox-x-uninstall.conf" ;;
    music)     CONF="$HERE/dosbox-x-music.conf" ;;
    *)         echo "usage: $0 [api|uninstall|uninstall-late|music]" >&2; exit 2 ;;
esac
STAMP="$(date +%Y%m%d-%H%M%S)-$MODE"
RUN="$ROOT/build/runs/sbfmdrv/$STAMP"
REFBIN="$ROOT/_ref/orig-bin"
DISP=":98"

sh "$ROOT/engine/asm/sbfmdrv/tools/build.sh" >/dev/null
sh "$HERE/build.sh" >/dev/null

mkdir -p "$RUN"
cp -f "$REFBIN/SBFMDRV.COM" "$RUN/SBFMDRV.COM"
if [ "$MODE" = music ]; then
    python3 "$HERE/music.py" extract --track "$TRACK" $SONGARG --out "$RUN/MUSIC.BLK"
    cp -f "$ROOT/build/sbfmdrv/test/MUSIC.COM" "$RUN/MUSIC.COM"
    # patch the autoexec for the mode (command tail / audio capture); the
    # capture directory is relative to the DOS working directory (the run dir)
    sed "s|^MUSIC\.COM\$|${CAP}MUSIC.COM ${TAIL}|" "$CONF" > "$RUN/dosbox-x.conf"
    CONF="$RUN/dosbox-x.conf"
else
    cp -f "$ROOT/build/sbfmdrv/test/TEST.COM" "$RUN/TEST.COM"
    if [ "$MODE" = uninstall-late ]; then
        cp -f "$ROOT/build/sbfmdrv/test/LATE.COM" "$RUN/LATE.COM"
        # the TSR goes in after the driver, and the driver's output is
        # redirected so the refusal message can be checked
        sed -e "s|^SBFMDRV.COM /U$|LATE.COM\\nSBFMDRV.COM /U > U.TXT|" \
            -e "s|^TEST.COM$|TEST.COM L|" \
            "$CONF" > "$RUN/dosbox-x.conf"
        CONF="$RUN/dosbox-x.conf"
    elif [ -n "$APIFLAG" ]; then
        sed "s|^TEST\\.COM\$|DX-CAPTURE $APIFLAG TEST.COM|" "$CONF" > "$RUN/dosbox-x.conf"
        CONF="$RUN/dosbox-x.conf"
    fi
fi

Xvfb "$DISP" -screen 0 1024x768x24 >"$RUN/xvfb.log" 2>&1 &
XPID=$!
trap 'kill "$XPID" 2>/dev/null || true' EXIT

(
    cd "$RUN"
    DISPLAY="$DISP" SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy \
        dosbox-x -conf "$CONF" -fastlaunch -time-limit "$TIMEOUT" \
        >"$RUN/stdout.log" 2>&1 || true
)

echo "run dir: $RUN"
if [ "$MODE" = music ]; then
    if [ -f "$RUN/MUSIC.LOG" ]; then
        echo "--- MUSIC.LOG (head) ---"
        head -8 "$RUN/MUSIC.LOG"
        echo "--- MUSIC.LOG (tail) ---"
        tail -6 "$RUN/MUSIC.LOG"
        echo "--- cross-check ---"
        CHECKARGS="--track $TRACK $SONGARG"
        [ -n "$MUSIC_PLAY" ] && CHECKARGS="$CHECKARGS --play"
        WAV="$(ls -t "$RUN"/capture/*.wav 2>/dev/null | head -1 || true)"
        [ -n "$WAV" ] && CHECKARGS="$CHECKARGS --wav $WAV"
        DRO="$(ls -t "$RUN"/capture/*.dro 2>/dev/null | head -1 || true)"
        [ -n "$DRO" ] && CHECKARGS="$CHECKARGS --dro $DRO"
        python3 "$HERE/music.py" check "$RUN/MUSIC.LOG" $CHECKARGS
    else
        echo "no MUSIC.LOG (see $RUN/stdout.log)"
        tail -30 "$RUN/stdout.log" || true
        exit 1
    fi
elif [ "$MODE" = uninstall-late ]; then
    echo "--- RESULT.TXT ---"
    cat "$RUN/RESULT.TXT" 2>/dev/null || true
    echo "--- SBFMDRV.COM /U output ---"
    cat "$RUN/U.TXT" 2>/dev/null || true
    RC=0
    for want in "VEC8 OK" "VEC9 OK" "APIV OK" "LATE OK"; do
        grep -qF "$want" "$RUN/RESULT.TXT" 2>/dev/null || {
            echo "uninstall-late: FAIL - RESULT.TXT does not report '$want'" >&2
            RC=1
        }
    done
    if grep -qE "BAD|NONE" "$RUN/RESULT.TXT" 2>/dev/null; then
        echo "uninstall-late: FAIL - a late check failed (see RESULT.TXT)" >&2
        RC=1
    fi
    if ! grep -qF "Error 0005" "$RUN/U.TXT" 2>/dev/null; then
        echo "uninstall-late: FAIL - the driver did not refuse (no error 0005)" >&2
        RC=1
    fi
    if [ "$RC" -eq 0 ]; then
        echo "uninstall-late: OK (refused with error 0005; hooks, API vector and the later TSR intact)"
    fi
    exit "$RC"
elif [ -f "$RUN/RESULT.TXT" ]; then
    echo "--- RESULT.TXT ---"
    cat "$RUN/RESULT.TXT"
    DRO="$(ls -t "$RUN"/capture/*.dro 2>/dev/null | head -1 || true)"
    if [ -n "$DRO" ]; then
        echo "--- OPL cross-check ---"
        python3 "$HERE/perc.py" "$DRO" "$RUN/RESULT.TXT"
    fi
else
    echo "no RESULT.TXT (see $RUN/stdout.log)"
    tail -30 "$RUN/stdout.log" || true
fi
