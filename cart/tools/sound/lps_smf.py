#!/usr/bin/env python3
"""lps_smf.py -- a Standard MIDI File reader with no dependencies.

Deliberately small. The baker needs note-ons, note-offs, program changes and a
tempo map, and nothing else in the format matters to a synth with four channels
and no controllers. Everything else is skipped rather than modelled.

No third-party library, because a build tool that needs `pip install` before a ROM
can be built is a build tool that will eventually stop working for someone.

Licence: GPL-2.0-or-later. See LICENSE.
"""

import struct


class MidiError(Exception):
    pass


def _vlq(data, i):
    """Variable-length quantity: 7 bits per byte, high bit continues."""
    n = 0
    for _ in range(4):
        b = data[i]
        i += 1
        n = (n << 7) | (b & 0x7F)
        if not (b & 0x80):
            return n, i
    raise MidiError("variable-length quantity longer than 4 bytes")


def _read_track(data, i, end):
    """Yield (abs_tick, kind, *args) for one track."""
    tick = 0
    status = 0
    out = []
    while i < end:
        dt, i = _vlq(data, i)
        tick += dt
        b = data[i]

        if b & 0x80:
            status = b
            i += 1
        elif not status:
            raise MidiError("data byte with no running status at %d" % i)
        # else: running status, `b` is already the first data byte

        hi = status & 0xF0
        ch = status & 0x0F

        if status == 0xFF:
            meta = data[i]
            i += 1
            length, i = _vlq(data, i)
            payload = data[i:i + length]
            i += length
            if meta == 0x51 and length == 3:  # set tempo
                out.append((tick, "tempo",
                            (payload[0] << 16) | (payload[1] << 8) | payload[2]))
            elif meta == 0x2F:  # end of track
                break
            # Everything else -- track names, key signatures, lyrics -- has no
            # bearing on what reaches the synth.
        elif status in (0xF0, 0xF7):
            length, i = _vlq(data, i)
            i += length
        elif hi in (0x80, 0x90, 0xA0, 0xB0, 0xE0):
            d1, d2 = data[i], data[i + 1]
            i += 2
            if hi == 0x90:
                # Velocity 0 is the conventional note-off, and most files use it.
                out.append((tick, "off" if d2 == 0 else "on", ch, d1, d2))
            elif hi == 0x80:
                out.append((tick, "off", ch, d1, d2))
            elif hi == 0xE0:
                out.append((tick, "bend", ch, (d2 << 7) | d1))
            # Aftertouch and controllers are dropped: this synth honours neither.
        elif hi in (0xC0, 0xD0):
            d1 = data[i]
            i += 1
            if hi == 0xC0:
                out.append((tick, "program", ch, d1))
        else:
            raise MidiError("unknown status %02X at %d" % (status, i))
    return out


def read(path):
    """Return (events, ticks_per_beat) with events in absolute milliseconds.

    Merges all tracks, applies the tempo map, and sorts. Program changes sort
    before notes at the same instant, because a note that arrives first would be
    played on the outgoing instrument.
    """
    with open(path, "rb") as f:
        data = f.read()

    if data[:4] != b"MThd":
        raise MidiError("%s is not a Standard MIDI File" % path)
    hlen = struct.unpack(">I", data[4:8])[0]
    fmt, ntracks, division = struct.unpack(">HHH", data[8:14])
    if division & 0x8000:
        raise MidiError("SMPTE time division is not supported")
    tpb = division
    i = 8 + hlen

    raw = []
    for _ in range(ntracks):
        if data[i:i + 4] != b"MTrk":
            raise MidiError("expected MTrk at offset %d" % i)
        tlen = struct.unpack(">I", data[i + 4:i + 8])[0]
        raw.extend(_read_track(data, i + 8, i + 8 + tlen))
        i += 8 + tlen

    raw.sort(key=lambda e: e[0])

    # Walk the tempo map once, converting ticks to milliseconds as we go. Doing it
    # in a single pass is what makes a mid-song tempo change come out right.
    out = []
    us_per_beat = 500000  # 120 BPM until told otherwise
    last_tick = 0
    ms = 0.0
    for e in raw:
        tick = e[0]
        ms += (tick - last_tick) * us_per_beat / tpb / 1000.0
        last_tick = tick
        if e[1] == "tempo":
            us_per_beat = e[2]
            continue
        out.append((int(round(ms)),) + e[1:])

    rank = {"program": 0, "bend": 1, "off": 2, "on": 3}
    out.sort(key=lambda e: (e[0], rank.get(e[1], 9)))
    return out, tpb, fmt
