#!/usr/bin/env python
"""Drives the knob's UI over USB serial: sends the debug keys, prints the log
and takes screenshots. Needs a build with SK_UI_DEBUG (seedlabs_devkit has it)
and pyserial and Pillow (pip install pyserial pillow).

Steps run in order and can be chained:

  knob_ui.py [--port /dev/cu.usbmodem101] STEP...

  keys ".,P"        send keys one at a time (see docs/flashing.md for the keys)
  gap 0.3           seconds between keys (default 0.35)
  wait 1            sleep
  snap out.png      screenshot of the display, saved at 2x
  log 5             print the log for 5 seconds
  grep 5 PATTERN    print matching log lines for 5 seconds

  knob_ui.py keys ".." wait 1 snap home.png keys P log 3
"""
import re
import sys
import time

import serial
from PIL import Image

ANSI = re.compile(r"\x1b\[[0-9;]*m")


def lines(ser, seconds):
    end = time.time() + seconds
    buf = b""
    while time.time() < end:
        chunk = ser.read(4096)
        if not chunk:
            continue
        buf += chunk
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            yield ANSI.sub("", line.decode("utf-8", "replace")).rstrip("\r")


def snap(ser, path):
    ser.reset_input_buffer()
    ser.write(b"S")
    rows = {}
    w = h = None
    for line in lines(ser, 40):
        m = re.search(r"SNAP begin (\d+) (\d+)", line)
        if m:
            w, h = int(m.group(1)), int(m.group(2))
            continue
        m = re.search(r"SNAP (\d+) (\d) ([0-9a-f]+)", line)
        if m:
            rows[(int(m.group(1)), int(m.group(2)))] = m.group(3)
            continue
        if "SNAP end" in line:
            break
    if w is None or h is None:
        print("no snapshot received")
        return
    img = Image.new("RGB", (w, h))
    px = img.load()
    half = w // 2
    missing = 0
    for y in range(h):
        for part in range(2):
            data = rows.get((y, part))
            if data is None or len(data) != half * 4:
                missing += 1
                continue
            for x in range(half):
                v = int(data[x * 4:x * 4 + 4], 16)
                r = (v >> 11) & 0x1F
                g = (v >> 5) & 0x3F
                b = v & 0x1F
                px[part * half + x, y] = (r * 255 // 31, g * 255 // 63, b * 255 // 31)
    img = img.resize((w * 2, h * 2), Image.NEAREST)
    img.save(path)
    print(f"saved {path} ({missing} half-rows missing)")


def main(argv):
    port = "/dev/cu.usbmodem101"
    if argv and argv[0] == "--port":
        port = argv[1]
        argv = argv[2:]
    ser = serial.Serial(port, 115200, timeout=0.1)
    gap = 0.35
    i = 0
    while i < len(argv):
        cmd = argv[i]
        if cmd == "keys":
            for k in argv[i + 1]:
                ser.write(k.encode())
                ser.flush()
                time.sleep(gap)
            i += 2
        elif cmd == "gap":
            gap = float(argv[i + 1])
            i += 2
        elif cmd == "wait":
            time.sleep(float(argv[i + 1]))
            i += 2
        elif cmd == "snap":
            snap(ser, argv[i + 1])
            i += 2
        elif cmd == "log":
            for line in lines(ser, float(argv[i + 1])):
                if line.strip() and not line.startswith("[INFO] SNAP"):
                    print(line)
            i += 2
        elif cmd == "grep":
            pat = re.compile(argv[i + 2])
            for line in lines(ser, float(argv[i + 1])):
                if pat.search(line):
                    print(line)
            i += 3
        else:
            print("unknown step", cmd)
            return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
