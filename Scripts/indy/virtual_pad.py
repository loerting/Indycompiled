#!/usr/bin/env python3
"""Virtual Xbox 360 pad for headless controller tests (Linux uinput, no extra modules).

Creates a uinput device with the Xbox 360 pad's USB IDs, so SDL, and through it Wine's winebus/XInput, treat it as a
real controller. It then plays a timeline and removes the device again.
Needs write access to /dev/uinput (here: a user ACL). While it exists, every program that reads gamepads sees it.

Timeline: "<second>:<control>=<value>,...;..." with seconds counted from the start of this script.
  sticks lx ly rx ry: -1.0 .. 1.0 (ly = -1.0 is up/forward, as on a real pad)
  triggers lt rt: 0.0 .. 1.0      d-pad hatx haty: -1, 0, 1
  buttons a b x y lb rb back start guide ls rs: 1 (down) or 0 (up)
Example: virtual_pad.py 70 "40:ly=-1.0;44:ly=0;48:ly=-0.5;52:ly=0"
Usage: python3 -I virtual_pad.py <total seconds> "<timeline>"
"""
import fcntl
import os
import struct
import sys
import time

EV_SYN, EV_KEY, EV_ABS = 0x00, 0x01, 0x03
BUTTONS = {"a": 0x130, "b": 0x131, "x": 0x133, "y": 0x134, "lb": 0x136, "rb": 0x137, "back": 0x13a, "start": 0x13b,
           "guide": 0x13c, "ls": 0x13d, "rs": 0x13e}
# axis code, minimum, maximum, flat
AXES = {"lx": (0x00, -32768, 32767, 128), "ly": (0x01, -32768, 32767, 128), "lt": (0x02, 0, 255, 0),
        "rx": (0x03, -32768, 32767, 128), "ry": (0x04, -32768, 32767, 128), "rt": (0x05, 0, 255, 0),
        "hatx": (0x10, -1, 1, 0), "haty": (0x11, -1, 1, 0)}


def _iow(nr, size):
    return (1 << 30) | (size << 16) | (ord("U") << 8) | nr


UI_DEV_CREATE, UI_DEV_DESTROY = (ord("U") << 8) | 1, (ord("U") << 8) | 2
UI_DEV_SETUP, UI_ABS_SETUP = _iow(3, 92), _iow(4, 28)
UI_SET_EVBIT, UI_SET_KEYBIT, UI_SET_ABSBIT = _iow(100, 4), _iow(101, 4), _iow(103, 4)


def scaled(name, value):
    code, lo, hi, _ = AXES[name]
    if name in ("lt", "rt"):
        return round(float(value) * hi)
    if name in ("hatx", "haty"):
        return int(value)
    return max(lo, min(hi, round(float(value) * hi)))


def parse(timeline):
    steps = []
    for part in filter(None, timeline.split(";")):
        sec, _, assigns = part.partition(":")
        steps.append((float(sec), [a.split("=") for a in assigns.split(",") if a]))
    return sorted(steps, key=lambda s: s[0])


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    total, steps = float(sys.argv[1]), parse(sys.argv[2])
    fd = os.open("/dev/uinput", os.O_WRONLY | os.O_NONBLOCK)

    def emit(etype, code, value):
        os.write(fd, struct.pack("llHHi", 0, 0, etype, code, value))

    try:
        for ev in (EV_KEY, EV_ABS, EV_SYN):
            fcntl.ioctl(fd, UI_SET_EVBIT, ev)
        for code in BUTTONS.values():
            fcntl.ioctl(fd, UI_SET_KEYBIT, code)
        for code, lo, hi, flat in AXES.values():
            fcntl.ioctl(fd, UI_SET_ABSBIT, code)
            # struct uinput_abs_setup: code, padding, input_absinfo {value, minimum, maximum, fuzz, flat, resolution}
            fcntl.ioctl(fd, UI_ABS_SETUP, struct.pack("Hxxiiiiii", code, 0, lo, hi, 16 if hi > 255 else 0, flat, 0))
        name = b"Microsoft X-Box 360 pad"
        fcntl.ioctl(fd, UI_DEV_SETUP, struct.pack("HHHH80sI", 0x03, 0x045E, 0x028E, 0x0110, name, 0))
        fcntl.ioctl(fd, UI_DEV_CREATE)
        print(f"virtual_pad: created, playing {len(steps)} steps for {total:.0f}s", flush=True)

        start = time.monotonic()
        for sec, assigns in steps:
            time.sleep(max(0.0, start + sec - time.monotonic()))
            for name, value in assigns:
                if name in BUTTONS:
                    emit(EV_KEY, BUTTONS[name], int(value))
                else:
                    emit(EV_ABS, AXES[name][0], scaled(name, value))
            emit(EV_SYN, 0, 0)
            print(f"virtual_pad: {sec:5.1f}s {','.join('='.join(a) for a in assigns)}", flush=True)
        time.sleep(max(0.0, start + total - time.monotonic()))
    finally:
        try:
            fcntl.ioctl(fd, UI_DEV_DESTROY)
        finally:
            os.close(fd)
        print("virtual_pad: removed", flush=True)


if __name__ == "__main__":
    main()
