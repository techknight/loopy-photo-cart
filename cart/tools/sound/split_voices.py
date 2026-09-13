#!/usr/bin/env python3
#
# Copyright (C) 2026 Derek Quenneville
#
# Licensed under the GNU General Public License, version 2 or later; see
# COPYING.md.
#
# Split a MIDI channel's notes across two channels so neither exceeds a voice
# budget. The Loopy's synth gives console channels 1 and 3 four notes each;
# Gymnopedie's left hand peaks at five, so its overflow moves to a second
# channel instead of stealing voices.
#
# Each note-on goes to the first channel with a free voice; its note-off
# follows it. Everything else is copied through. Output is a format-0 SMF.
#
# Usage:
#   split_voices.py in.mid out.mid --channel 1 --overflow 2 --voices 4

import argparse
import struct
import sys


def vlq_read(b, i):
    v = 0
    while True:
        c = b[i]
        i += 1
        v = (v << 7) | (c & 0x7F)
        if not c & 0x80:
            return v, i


def vlq_write(v):
    out = [v & 0x7F]
    v >>= 7
    while v:
        out.append((v & 0x7F) | 0x80)
        v >>= 7
    return bytes(reversed(out))


def read_events(path):
    """-> (division, [(abs_tick, order, raw_event_bytes)])"""
    b = open(path, "rb").read()
    if b[:4] != b"MThd":
        sys.exit(f"{path}: not a MIDI file")
    _fmt, ntrk, div = struct.unpack(">HHH", b[8:14])
    i = 8 + struct.unpack(">I", b[4:8])[0]
    events = []
    order = 0
    for _ in range(ntrk):
        if b[i:i + 4] != b"MTrk":
            sys.exit(f"{path}: bad track chunk")
        end = i + 8 + struct.unpack(">I", b[i + 4:i + 8])[0]
        j, tick, status = i + 8, 0, 0
        while j < end:
            d, j = vlq_read(b, j)
            tick += d
            c = b[j]
            if c == 0xFF:
                typ = b[j + 1]
                length, k = vlq_read(b, j + 2)
                if typ != 0x2F:  # end-of-track is re-added once
                    events.append((tick, order, b[j:k + length]))
                j = k + length
            elif c in (0xF0, 0xF7):
                length, k = vlq_read(b, j + 1)
                events.append((tick, order, b[j:k + length]))
                j = k + length
            else:
                if c & 0x80:
                    status = c
                    j += 1
                n = 1 if status >> 4 in (0xC, 0xD) else 2
                events.append((tick, order, bytes([status]) + b[j:j + n]))
                j += n
            order += 1
        i = end
    events.sort(key=lambda e: (e[0], e[1]))
    return div, events


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("src")
    ap.add_argument("dst")
    ap.add_argument("--channel", type=int, required=True)
    ap.add_argument("--overflow", type=int, required=True)
    ap.add_argument("--voices", type=int, required=True)
    args = ap.parse_args()

    div, events = read_events(args.src)
    active = {args.channel: 0, args.overflow: 0}
    sounding = {}  # note -> [assigned channels, oldest first]
    moved = 0
    out = bytearray()
    last = 0

    for tick, _order, ev in events:
        status = ev[0]
        if status < 0xF0 and status & 0x0F == args.channel and status >> 4 in (0x8, 0x9):
            note, vel = ev[1], ev[2]
            if status >> 4 == 0x9 and vel > 0:
                ch = args.channel if active[args.channel] < args.voices else args.overflow
                if active[ch] >= args.voices:
                    sys.exit(f"tick {tick}: more than {2 * args.voices} notes at once")
                active[ch] += 1
                moved += ch == args.overflow
                sounding.setdefault(note, []).append(ch)
                ev = bytes([0x90 | ch, note, vel])
            else:
                chans = sounding.get(note)
                if not chans:
                    continue  # stray note-off
                ch = chans.pop(0)
                active[ch] -= 1
                ev = bytes([0x80 | ch, note, 0])
        out += vlq_write(tick - last) + ev
        last = tick
    out += vlq_write(0) + b"\xFF\x2F\x00"

    with open(args.dst, "wb") as f:
        f.write(b"MThd" + struct.pack(">IHHH", 6, 0, 1, div))
        f.write(b"MTrk" + struct.pack(">I", len(out)) + out)
    print(f"{args.dst}: {moved} note(s) moved from channel {args.channel} "
          f"to channel {args.overflow}")


if __name__ == "__main__":
    main()
