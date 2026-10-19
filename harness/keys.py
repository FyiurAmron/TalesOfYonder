#!/usr/bin/env python3
"""Send keystrokes to an X server via XTEST (ctypes; only needs libX11/libXtst).

Usage:
    keys.py [--display :N] CHORD [CHORD ...]

CHORD is "key" or "mod+mod+key", e.g. "f12+p", "ctrl+f9". Keys are pressed and
released in order with a small inter-event delay. Intended for driving DOSBox-X
on a headless Xvfb display (hotkeys: F12+S save state, F12+P screenshot,
Ctrl+F9 exit).

The target is the largest direct child of the root window: the pointer is moved
inside it and input focus is assigned explicitly, so it works without a window
manager.
"""
import ctypes
import sys
import time

X11 = ctypes.CDLL("libX11.so.6")
XTST = ctypes.CDLL("libXtst.so.6")

X11.XOpenDisplay.restype = ctypes.c_void_p
X11.XOpenDisplay.argtypes = [ctypes.c_char_p]
X11.XDefaultRootWindow.restype = ctypes.c_ulong
X11.XDefaultRootWindow.argtypes = [ctypes.c_void_p]
X11.XQueryTree.argtypes = [
    ctypes.c_void_p, ctypes.c_ulong, ctypes.POINTER(ctypes.c_ulong),
    ctypes.POINTER(ctypes.c_ulong), ctypes.POINTER(ctypes.POINTER(ctypes.c_ulong)),
    ctypes.POINTER(ctypes.c_uint),
]
X11.XGetGeometry.argtypes = [
    ctypes.c_void_p, ctypes.c_ulong, ctypes.POINTER(ctypes.c_ulong),
    ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int),
    ctypes.POINTER(ctypes.c_uint), ctypes.POINTER(ctypes.c_uint),
    ctypes.POINTER(ctypes.c_uint), ctypes.POINTER(ctypes.c_uint),
]
X11.XKeysymToKeycode.restype = ctypes.c_ubyte
X11.XKeysymToKeycode.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
X11.XStringToKeysym.restype = ctypes.c_ulong
X11.XStringToKeysym.argtypes = [ctypes.c_char_p]
X11.XSetInputFocus.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int, ctypes.c_ulong]
X11.XFlush.argtypes = [ctypes.c_void_p]
X11.XCloseDisplay.argtypes = [ctypes.c_void_p]
XTST.XTestFakeKeyEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_int, ctypes.c_ulong]
XTST.XTestFakeKeyEvent.restype = ctypes.c_int
XTST.XTestFakeMotionEvent.argtypes = [
    ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_ulong,
]
XTST.XTestFakeMotionEvent.restype = ctypes.c_int

MODS = {"ctrl": "Control_L", "alt": "Alt_L", "shift": "Shift_L", "super": "Super_L"}


def pick_window(dpy):
    root = X11.XDefaultRootWindow(dpy)
    rt = ctypes.c_ulong()
    pt = ctypes.c_ulong()
    kids = ctypes.POINTER(ctypes.c_ulong)()
    n = ctypes.c_uint()
    if not X11.XQueryTree(dpy, root, ctypes.byref(rt), ctypes.byref(pt),
                          ctypes.byref(kids), ctypes.byref(n)):
        return None
    best = None
    best_area = 0
    for i in range(n.value):
        w = kids[i]
        rr = ctypes.c_ulong()
        x = ctypes.c_int()
        y = ctypes.c_int()
        ww = ctypes.c_uint()
        hh = ctypes.c_uint()
        bw = ctypes.c_uint()
        dp = ctypes.c_uint()
        if X11.XGetGeometry(dpy, w, ctypes.byref(rr), ctypes.byref(x), ctypes.byref(y),
                            ctypes.byref(ww), ctypes.byref(hh), ctypes.byref(bw),
                            ctypes.byref(dp)):
            area = ww.value * hh.value
            if area > best_area:
                best, best_area = (w, x.value, y.value, ww.value, hh.value), area
    return best


def keycode(dpy, name):
    # keysym names are case-sensitive ("F12" vs "p"); try the obvious variants
    ks = 0
    for cand in (name, name.lower(), name.upper(), name.capitalize()):
        ks = X11.XStringToKeysym(cand.encode())
        if ks != 0:
            break
    if ks == 0:
        raise SystemExit("keys: unknown key name: " + name)
    kc = X11.XKeysymToKeycode(dpy, ks)
    if kc == 0:
        raise SystemExit("keys: no keycode for: " + name)
    return kc


def main(argv):
    dsp = ":0"
    chords = []
    i = 0
    while i < len(argv):
        if argv[i] == "--display" and i + 1 < len(argv):
            dsp = argv[i + 1]
            i += 2
        else:
            chords.append(argv[i])
            i += 1

    dpy = X11.XOpenDisplay(dsp.encode())
    if not dpy:
        raise SystemExit("keys: cannot open display " + dsp)
    win = pick_window(dpy)
    if win:
        w, x, y, ww, hh = win                     # noqa: F841 (w unused, geometry parts are)
        X11.XSetInputFocus(dpy, w, 2, 0)          # RevertToParent
        XTST.XTestFakeMotionEvent(dpy, 0, x + ww // 2, y + hh // 2, 0)
        X11.XFlush(dpy)
        time.sleep(0.1)

    for chord in chords:
        parts = [p.strip() for p in chord.split("+") if p.strip()]
        codes = [keycode(dpy, MODS.get(p.lower(), p)) for p in parts]
        for c in codes:
            XTST.XTestFakeKeyEvent(dpy, c, 1, 20)
        for c in reversed(codes):
            XTST.XTestFakeKeyEvent(dpy, c, 0, 20)
        X11.XFlush(dpy)
        time.sleep(0.15)

    X11.XCloseDisplay(dpy)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
