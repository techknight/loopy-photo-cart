#!/usr/bin/env python3
"""lps_bake.py -- turn a MIDI file into a song the Loopy can actually play.

    python3 tools/sound/lps_bake.py song.mid song.toml -o src/song_data.h
    python3 tools/sound/lps_bake.py song.mid song.toml --check-poly --simulate

The gap between a MIDI file and this console is wide: sixteen channels become
four, a hundred-odd instruments become presets chosen by ear, dynamics vanish
entirely, and the whole thing has to fit through a 500-byte-per-second pipe. This
does that reduction, and -- more importantly -- **refuses** to do it silently when
the result will not play.

Two checks are what make it worth having:

  --check-poly   Walks a program timeline per channel and compares the peak
                 simultaneous notes against the real budget, which is not a
                 constant: twelve of the 110 presets are layered and cost double,
                 so channel 2 running one of them is monophonic. The costs come
                 from tools/sound/programs.json, which was read out of the sound
                 ROM rather than assumed.

  --simulate     Runs the baked events through tools/sound/lps_model.py, the
                 byte-exact model of the driver, and reports what actually
                 reaches the wire and what gets dropped -- so a song is checked
                 against the MIDI link's real bandwidth, not just by ear.

Configuration is TOML:

    chan_map = { 0 = [0, 100], 1 = [1, 108], 9 = [3, 39] }
    drum_map = { 36 = 36, 38 = 39, 42 = 43 }
    transpose = -12
    pdrop = 40
    loop = true
    name = "title"

Licence: GPL-2.0-or-later. See COPYING.md.

The knob set is adapted from LoopyDOOM's tools/bake_music.py
(GPL-2.0-or-later). See NOTICE.md.
"""

import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lps_model as M      # noqa: E402
import lps_smf             # noqa: E402

try:
    import tomllib
except ImportError:  # Python < 3.11
    tomllib = None

NOTE_LO, NOTE_HI = 36, 96
CHANNELS = 4


def load_config(path):
    if not path:
        return {}
    if tomllib is None:
        raise SystemExit("TOML config needs Python 3.11+; pass --chan-map instead")
    with open(path, "rb") as f:
        return tomllib.load(f)


def load_programs():
    """Layered flags and poly caps, as read out of the sound ROM."""
    p = os.path.join(os.path.dirname(os.path.abspath(__file__)), "programs.json")
    try:
        with open(p, "r", encoding="utf-8") as f:
            data = json.load(f)
    except (OSError, ValueError):
        return None
    return {q["prog"]: q for q in data["programs"]}


RAW_VOICES = (12, 8, 4, 8)  # 4-channel mode


def poly_cap(ch, prog, progs):
    """Notes this channel can sound with this program in force."""
    vpn = 2
    if progs and prog in progs and progs[prog]["layered"]:
        vpn = 4
    return RAW_VOICES[ch] // vpn


def fold(note):
    """The synth folds out-of-range notes by octaves rather than clamping. Doing
    it here means the baked stream says what will actually sound."""
    n = note
    while n < NOTE_LO:
        n += 12
    while n > NOTE_HI:
        n -= 12
    return n


def bake(events, cfg, warn):
    """MIDI events -> (t_ms, rank, ch, kind, arg) tuples on the four channels."""
    chan_map = {int(k): v for k, v in (cfg.get("chan_map") or {}).items()}
    drum_map = {int(k): v for k, v in (cfg.get("drum_map") or {}).items()}
    transpose = int(cfg.get("transpose", 0))
    pdrop = int(cfg.get("pdrop", 0))

    if not chan_map:
        raise SystemExit("config has no chan_map: nothing would be audible")

    out = []
    cur_prog = {}
    dropped_src = set()
    folded = 0
    drum_unmapped = set()

    for e in events:
        t = e[0]
        kind = e[1]
        if kind not in ("on", "off", "program", "bend"):
            continue
        src = e[2]

        if src not in chan_map:
            dropped_src.add(src)
            continue
        dst, prog = chan_map[src]

        if kind == "program":
            # The source file's own program changes are ignored: its instrument
            # numbers refer to General MIDI, and this synth's presets are Casio's
            # own set with no relationship to them. The mapping in chan_map is the
            # authority.
            continue

        if kind == "bend":
            out.append((t, 1, dst, "bend", e[3] >> 7))
            continue

        note, vel = e[3], e[4]

        # Percussion. MIDI channel 9 is the GM drum channel; a drum note is a
        # sample index, not a pitch, so it maps through drum_map and is never
        # transposed or folded.
        if src == 9:
            if kind == "on" and vel < pdrop:
                continue
            if note not in drum_map:
                drum_unmapped.add(note)
                continue
            dnote = drum_map[note]
        else:
            dnote = note + transpose
            f = fold(dnote)
            if f != dnote and kind == "on":
                # Count notes, not events: a folded note produces both an on and
                # an off, and reporting four when the composer wrote two is the
                # kind of small wrongness that makes a tool untrustworthy.
                folded += 1
            dnote = f

        # A program change must precede the first note that needs it, and it is
        # destructive, so it is emitted lazily and only once.
        if cur_prog.get(dst) != prog:
            out.append((t, 0, dst, "program", prog))
            cur_prog[dst] = prog

        out.append((t, 1, dst, kind, dnote))

    if dropped_src:
        warn("source channels %s have no chan_map entry and were dropped"
             % sorted(dropped_src))
    if drum_unmapped:
        warn("drum notes %s have no drum_map entry and were dropped"
             % sorted(drum_unmapped))
    if folded:
        warn("%d note(s) fell outside %d-%d and were octave-folded; the synth "
             "would have done this anyway, but the pitch is not what was written"
             % (folded, NOTE_LO, NOTE_HI))

    out.sort(key=lambda x: (x[0], x[1]))
    return out


def check_poly(baked, progs, warn):
    """Peak simultaneous notes per channel, against the program in force."""
    sounding = {c: set() for c in range(CHANNELS)}
    prog = {c: None for c in range(CHANNELS)}
    peak = {c: (0, None, 0) for c in range(CHANNELS)}
    bad = 0

    for t, _rank, ch, kind, arg in baked:
        if kind == "program":
            prog[ch] = arg
            sounding[ch].clear()
        elif kind == "on":
            sounding[ch].add(arg)
            n = len(sounding[ch])
            if n > peak[ch][0]:
                peak[ch] = (n, prog[ch], t)
        elif kind == "off":
            sounding[ch].discard(arg)

    print("\nPolyphony:")
    for ch in range(CHANNELS):
        n, p, t = peak[ch]
        if not n:
            continue
        cap = poly_cap(ch, p, progs)
        lay = progs and p in progs and progs[p]["layered"]
        note = " [LAYERED, so the cap is halved]" if lay else ""
        flag = ""
        if n > cap:
            flag = "   <-- OVER BUDGET"
            bad += 1
        print("  ch%d: peak %d of %d with program %s%s at t=%d ms%s"
              % (ch, n, cap, p, note, t, flag))
        if n > cap:
            warn("ch%d wants %d simultaneous notes but program %s allows %d -- "
                 "the synth will steal voices, audibly" % (ch, n, p, cap))
    return bad == 0


def simulate(baked, tick_hz, warn):
    """Play the baked stream through the driver model and report the wire cost."""
    d = M.Driver()
    period = 1000 // tick_hz
    now = 0
    next_tick = 0
    i = 0
    end = baked[-1][0] + 2000 if baked else 0

    while now < end:
        while i < len(baked) and baked[i][0] <= now:
            _t, _r, ch, kind, arg = baked[i]
            if kind == "on":
                d.midi.note_on(ch, arg)
            elif kind == "off":
                d.midi.note_off(ch, arg)
            elif kind == "program":
                d.midi.program(ch, arg)
            elif kind == "bend":
                d.midi.bend(ch, int((arg - 64) * 200 / 64))
            i += 1
        if now >= next_tick:
            d.tick(now, period)
            next_tick = now + period
        now += 1

    span = max(1, now) / 1000.0
    print("\nWire simulation at %d Hz (%d bytes/s available):" % (tick_hz, tick_hz))
    print("  bytes sent        : %d over %.1f s (%.0f/s average)"
          % (len(d.wire), span, len(d.wire) / span))
    print("  queue high-water  : %d of %d" % (d.midi.high_water, M.QUEUE_SIZE - 1))
    print("  note-ons dropped  : %d" % d.midi.dropped_on)
    print("  note-offs dropped : %d" % d.midi.dropped_off)

    ok = True
    if d.midi.dropped_off:
        warn("%d note-off(s) were dropped -- those notes drone forever. The song "
             "is too dense to play." % d.midi.dropped_off)
        ok = False
    if d.midi.dropped_on:
        warn("%d note-on(s) were dropped. The song will play, with gaps."
             % d.midi.dropped_on)
    if d.midi.high_water > (M.QUEUE_SIZE - 1) * 3 // 4:
        warn("queue reached %d of %d -- little headroom left for sound effects"
             % (d.midi.high_water, M.QUEUE_SIZE - 1))
    return ok


def emit(baked, cfg, name, warn):
    """Delta-encode and write the C table."""
    evs = []
    prev_t = 0
    for t, _rank, ch, kind, arg in baked:
        dt = t - prev_t
        # dt is a uint16. A gap longer than that is split with no-op spacers
        # rather than truncated, which would shift everything after it.
        while dt > 65535:
            evs.append((65535, M.OP_META, 0, M.META_NOP, 0))
            dt -= 65535
        prev_t = t
        if kind == "on":
            evs.append((dt, M.OP_NOTE, ch, 1, arg))
        elif kind == "off":
            evs.append((dt, M.OP_NOTE, ch, 0, arg))
        elif kind == "program":
            evs.append((dt, M.OP_PROG, ch, 0, arg))
        elif kind == "bend":
            evs.append((dt, M.OP_CTRL, ch, M.CTRL_BEND, arg))
    evs.append((0, M.OP_META, 0, M.META_END, 0))

    chmask = 0
    progs = [0xFF] * CHANNELS
    for _t, _r, ch, kind, arg in baked:
        chmask |= 1 << ch
        if kind == "program" and progs[ch] == 0xFF:
            progs[ch] = arg

    loops = bool(cfg.get("loop", True))
    ident = "lps_song_" + name

    L = []
    w = L.append
    w("/* %s -- GENERATED by tools/lps_bake.py. Do not edit. */" % (ident + ".h"))
    w("")
    w("#ifndef %s_H" % ident.upper())
    w("#define %s_H" % ident.upper())
    w("")
    w('#include "lps_seq.h"')
    w("")
    w("static const lps_ev_t %s_evs[] = {" % ident)
    for i in range(0, len(evs), 4):
        row = "".join(
            "{%d,LPS_EV_MAKE(%d,%d,%d),%d}," % (dt, op, ch, low, arg)
            for dt, op, ch, low, arg in evs[i:i + 4])
        w("\t" + row)
    w("};")
    w("")
    w("static const lps_song_t %s = {" % ident)
    w("\t%s_evs," % ident)
    w("\t%d, /* events */" % len(evs))
    w("\t%s, /* loop point */" % ("0" if loops else "0xFFFF"))
    w("\t{ %s }," % ", ".join(str(p) for p in progs))
    w("\t0x%02X, /* channels */" % chmask)
    w("\t%s," % ("LPS_SONG_LOOPS" if loops else "0"))
    w("\t0")
    w("};")
    w("")
    w("#endif")
    return "\n".join(L) + "\n", len(evs)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("midi")
    ap.add_argument("config", nargs="?")
    ap.add_argument("-o", "--out")
    ap.add_argument("--name", default=None)
    ap.add_argument("--check-poly", action="store_true")
    ap.add_argument("--simulate", action="store_true")
    ap.add_argument("--tick-hz", type=int, default=500)
    ap.add_argument("--strict", action="store_true",
                    help="treat warnings as errors")
    args = ap.parse_args()

    warnings = []

    def warn(msg):
        warnings.append(msg)

    cfg = load_config(args.config)
    progs = load_programs()
    if progs is None:
        warn("tools/sound/programs.json not found -- layered presets cannot be "
             "costed, so the polyphony check is optimistic.")

    events, tpb, fmt = lps_smf.read(args.midi)
    print("%s: format %d, %d ticks/beat, %d events"
          % (os.path.basename(args.midi), fmt, tpb, len(events)))

    baked = bake(events, cfg, warn)
    if not baked:
        raise SystemExit("nothing survived the mapping -- check chan_map")

    span = baked[-1][0] / 1000.0
    notes = sum(1 for e in baked if e[3] == "on")
    print("baked: %d events, %d notes, %.1f s" % (len(baked), notes, span))

    ok = True
    if args.check_poly:
        ok = check_poly(baked, progs, warn) and ok
    if args.simulate:
        ok = simulate(baked, args.tick_hz, warn) and ok

    name = args.name or os.path.splitext(os.path.basename(args.midi))[0]
    name = "".join(c if c.isalnum() else "_" for c in name).lower()
    text, n = emit(baked, cfg, name, warn)

    if warnings:
        print("\nWarnings:")
        for m in warnings:
            print("  ! %s" % m)

    if args.out:
        if args.strict and (warnings or not ok):
            raise SystemExit("\nrefusing to write %s: --strict and there are "
                             "warnings above" % args.out)
        with open(args.out, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
        print("\nwrote %s (%d events, %d bytes of ROM)"
              % (args.out, n, n * 4))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
