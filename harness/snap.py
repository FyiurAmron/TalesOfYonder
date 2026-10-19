#!/usr/bin/env python3
"""Snapshot an X display to PNG via XGetImage (ctypes; needs libX11 + Pillow).

Usage:
    snap.py [--display :N] [--window] OUT.png

Without --window the whole screen is captured; with it, the largest direct child
of the root window (the DOSBox-X window under Xvfb, where there is no window
manager to resize it).

Used by harness/baseline.sh: DOSBox-X 2026.01.02's own screenshot (F12+P) aborts
the process on the game's VESA mode, so the harness grabs the X screen instead.
"""
import ctypes
import sys

from PIL import Image

X11 = ctypes.CDLL("libX11.so.6")

X11.XOpenDisplay.restype = ctypes.c_void_p
X11.XOpenDisplay.argtypes = [ctypes.c_char_p]
X11.XDefaultRootWindow.restype = ctypes.c_ulong
X11.XDefaultRootWindow.argtypes = [ctypes.c_void_p]
X11.XDefaultScreen.argtypes = [ctypes.c_void_p]
X11.XDefaultScreen.restype = ctypes.c_int
X11.XDisplayWidth.argtypes = [ctypes.c_void_p, ctypes.c_int]
X11.XDisplayWidth.restype = ctypes.c_int
X11.XDisplayHeight.argtypes = [ctypes.c_void_p, ctypes.c_int]
X11.XDisplayHeight.restype = ctypes.c_int
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


class XImage(ctypes.Structure):
    _fields_ = [
        ("width", ctypes.c_int),
        ("height", ctypes.c_int),
        ("xoffset", ctypes.c_int),
        ("format", ctypes.c_int),
        ("data", ctypes.c_void_p),
        ("byte_order", ctypes.c_int),
        ("bitmap_unit", ctypes.c_int),
        ("bitmap_bit_order", ctypes.c_int),
        ("bitmap_pad", ctypes.c_int),
        ("depth", ctypes.c_int),
        ("bytes_per_line", ctypes.c_int),
        ("bits_per_pixel", ctypes.c_int),
        ("red_mask", ctypes.c_ulong),
        ("green_mask", ctypes.c_ulong),
        ("blue_mask", ctypes.c_ulong),
    ]


X11.XGetImage.restype = ctypes.POINTER(XImage)
X11.XGetImage.argtypes = [
    ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int, ctypes.c_int,
    ctypes.c_uint, ctypes.c_uint, ctypes.c_ulong, ctypes.c_int,
]
X11.XDestroyImage.argtypes = [ctypes.POINTER(XImage)]
X11.XCloseDisplay.argtypes = [ctypes.c_void_p]

ZPIXMAP = 2
ALL_PLANES = 0xFFFFFFFF


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
                best, best_area = (x.value, y.value, ww.value, hh.value), area
    return best


def capture(dpy, x, y, w, h):
    img = X11.XGetImage(dpy, X11.XDefaultRootWindow(dpy), x, y, w, h, ALL_PLANES, ZPIXMAP)
    if not img:
        raise SystemExit("snap: XGetImage failed")
    im = img.contents
    buf = ctypes.string_at(im.data, im.bytes_per_line * im.height)
    if im.bits_per_pixel == 32 and im.depth == 24:
        r = Image.frombytes("L", (im.width, im.height), buf[2::4])
        g = Image.frombytes("L", (im.width, im.height), buf[1::4])
        b = Image.frombytes("L", (im.width, im.height), buf[0::4])
        out = Image.merge("RGB", (r, g, b))
    else:
        raise SystemExit("snap: unsupported depth/bpp: %d/%d" % (im.depth, im.bits_per_pixel))
    X11.XDestroyImage(img)
    return out


def main(argv):
    dsp = ":0"
    window = False
    outs = []
    i = 0
    while i < len(argv):
        if argv[i] == "--display" and i + 1 < len(argv):
            dsp = argv[i + 1]
            i += 2
        elif argv[i] == "--window":
            window = True
            i += 1
        else:
            outs.append(argv[i])
            i += 1
    if len(outs) != 1:
        raise SystemExit(__doc__)

    dpy = X11.XOpenDisplay(dsp.encode())
    if not dpy:
        raise SystemExit("snap: cannot open display " + dsp)
    scr = X11.XDefaultScreen(dpy)
    x, y = 0, 0
    w = X11.XDisplayWidth(dpy, scr)
    h = X11.XDisplayHeight(dpy, scr)
    if window:
        win = pick_window(dpy)
        if win is None:
            raise SystemExit("snap: no window found")
        x, y, w, h = win
    im = capture(dpy, x, y, w, h)
    im.save(outs[0])
    X11.XCloseDisplay(dpy)
    print("snap: %s (%dx%d at %d,%d)" % (outs[0], w, h, x, y))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
