#!/usr/bin/env python3
"""Move/click the DOSBox-X window through XTEST.

Usage:
    mouse.py [--display :N] (--move X Y | --click X Y | --press | --release)
    mouse.py [--display :N] (--relative DX DY)
    mouse.py [--display :N] (--host-move X Y | --host-click X Y)

Coordinates for --move/--click are guest pixels (320x200); the script maps
them onto the DOSBox-X window found on the display.  The window may include
the DOSBox-X menu bar, so the content area is taken as the bottom 320:200 part
of the window (the configured windowresolution is "original", i.e. an integer
scale).  --host-* takes root-window coordinates directly (menu-bar tests).
Used by the M5 oracle driver, which needs mouse input for the character-sheet
item clicks and the chest/loot interactions.
"""
import ctypes
import sys
import time

from keys import X11, XTST, pick_window

XTST.XTestFakeButtonEvent.argtypes = [
    ctypes.c_void_p, ctypes.c_uint, ctypes.c_int, ctypes.c_ulong,
]
XTST.XTestFakeButtonEvent.restype = ctypes.c_int
XTST.XTestFakeRelativeMotionEvent.argtypes = [
    ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_ulong,
]
XTST.XTestFakeRelativeMotionEvent.restype = ctypes.c_int


def content_rect(window):
    _w, x, y, ww, hh = window
    scale = ww / 320.0
    content_h = 200.0 * scale
    if content_h > hh:
        scale = min(ww / 320.0, hh / 200.0)
        content_h = 200.0 * scale
    top = hh - content_h
    return x, y + top, scale, (ww - 320.0 * scale) / 2.0


def to_host(rect, gx, gy):
    left, top, scale, xpad = rect
    return (int(round(left + xpad + (gx + 0.5) * scale)),
            int(round(top + (gy + 0.5) * scale)))


def main(argv):
    dsp = ":0"
    action = None
    args = []
    debug = False
    i = 0
    while i < len(argv):
        if argv[i] == "--display" and i + 1 < len(argv):
            dsp = argv[i + 1]
            i += 2
        elif argv[i] == "--debug":
            debug = True
            i += 1
        elif argv[i] in ("--move", "--click", "--host-move", "--host-click",
                         "--relative"):
            action = argv[i][2:]
            if i + 2 >= len(argv):
                raise SystemExit("mouse: %s needs X Y" % argv[i])
            args = [int(argv[i + 1]), int(argv[i + 2])]
            i += 3
        elif argv[i] in ("--press", "--release"):
            action = argv[i][2:]
            i += 1
        else:
            raise SystemExit("mouse: bad argument %r" % argv[i])

    dpy = X11.XOpenDisplay(dsp.encode())
    if not dpy:
        raise SystemExit("mouse: cannot open display " + dsp)
    win = pick_window(dpy)
    if win is None:
        raise SystemExit("mouse: no DOSBox-X window")
    w, x, y, ww, hh = win
    X11.XSetInputFocus(dpy, w, 2, 0)
    rect = content_rect(win)

    if action == "relative":
        XTST.XTestFakeRelativeMotionEvent(dpy, args[0], args[1], 0)
        X11.XFlush(dpy)
        time.sleep(0.08)
    elif action in ("move", "click", "host-move", "host-click"):
        if action.startswith("host-"):
            hx, hy = args
        else:
            hx, hy = to_host(rect, args[0], args[1])
        if debug:
            print("mouse: window %dx%d at %d,%d content scale %.3f pad %.1f -> host %d,%d"
                  % (ww, hh, x, y, rect[2], rect[3], hx, hy), file=sys.stderr)
        XTST.XTestFakeMotionEvent(dpy, 0, hx, hy, 0)
        X11.XFlush(dpy)
        time.sleep(0.08)
    if action in ("click", "host-click"):
        XTST.XTestFakeButtonEvent(dpy, 1, 1, 30)
        X11.XFlush(dpy)
        time.sleep(0.06)
        XTST.XTestFakeButtonEvent(dpy, 1, 0, 30)
        X11.XFlush(dpy)
        time.sleep(0.08)
    elif action == "press":
        XTST.XTestFakeButtonEvent(dpy, 1, 1, 30)
        X11.XFlush(dpy)
    elif action == "release":
        XTST.XTestFakeButtonEvent(dpy, 1, 0, 30)
        X11.XFlush(dpy)

    X11.XCloseDisplay(dpy)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
