#!/usr/bin/env python3
"""lps_model.py -- a Python copy of the C driver, kept honest by a byte diff.

The music baker has to decide whether a song fits: whether its notes reach the
wire in time, whether the queue backs up, what gets dropped. To answer that it has
to model the driver. And a model that drifts from the thing it models is worse
than no model at all -- it would approve content the console cannot play, and it
would do so confidently.

So this is deliberately a **transliteration, not a reimplementation**. Every
function below names the C function it mirrors, and the shapes are kept the same
even where Python would prefer otherwise: the same ring buffer arithmetic, the
same free-space thresholds, the same running-status rule, the same duplicate-note
handling, the same bitmap-clear on a program change. Where the C is awkward, this
is awkward in the same way on purpose.

The check is a byte diff: the same input run through this and through the C
driver built for the desktop (LPS_HOST) must give wire logs that are **byte
identical, timestamps included**. Any change to the C driver needs the matching
change here, or the two quietly diverge.

Licence: GPL-2.0-or-later. See COPYING.md.
"""

# --- lps_midi.c ------------------------------------------------------------

QUEUE_SIZE = 64            # LPS_QUEUE_SIZE
Q_MIN_FOR_NOTEON = 8       # LPS_Q_MIN_FOR_NOTEON
Q_MIN_FOR_NOTEOFF = 3      # LPS_Q_MIN_FOR_NOTEOFF
CHANNELS = 4               # LPS_CHANNELS
PROG_UNKNOWN = 0xFF        # LPS_PROG_UNKNOWN
PROG_ALL_OFF = 0x7F        # PROG_ALL_OFF
QMASK = QUEUE_SIZE - 1


class Midi:
    """Mirrors lps_midi.c in full."""

    def __init__(self, tx):
        # `tx` receives (t_ms, byte) as each byte leaves, mirroring LPS_PORT_TX.
        self._tx = tx
        self.q = [0] * QUEUE_SIZE
        self.head = 0
        self.tail = 0
        self.running_status = 0
        self.sounding = [set() for _ in range(CHANNELS)]
        self.cur_prog = [PROG_UNKNOWN] * CHANNELS
        self.high_water = 0
        self.dropped_on = 0
        self.dropped_off = 0
        self.ready = 1

    # queue_free()
    def free(self):
        used = (self.head - self.tail) & QMASK
        return QUEUE_SIZE - 1 - used

    # put()
    def _put(self, b):
        nxt = (self.head + 1) & QMASK
        if nxt == self.tail:
            return
        self.q[self.head] = b & 0xFF
        self.head = nxt

    # note_used()
    def _note_used(self):
        used = (self.head - self.tail) & QMASK
        if used > self.high_water:
            self.high_water = used

    # status()
    def _status(self, s):
        if s != self.running_status:
            self._put(s)
            self.running_status = s

    # emit_off()
    def _emit_off(self, ch, note):
        self._status(0x90 | ch)
        self._put(note)
        self._put(0x00)
        self.sounding[ch].discard(note)

    # LPS_NoteOn()
    def note_on(self, ch, note):
        if not self.ready or ch >= CHANNELS:
            return 0
        note &= 0x7F
        if self.free() < Q_MIN_FOR_NOTEON:
            self.dropped_on += 1
            return 0
        # The duplicate-note rule: release first, or the second copy can never
        # be turned off. See lps_midi.c.
        if note in self.sounding[ch]:
            self._emit_off(ch, note)
        self._status(0x90 | ch)
        self._put(note)
        self._put(0x40)
        self.sounding[ch].add(note)
        self._note_used()
        return 1

    # LPS_NoteOff()
    def note_off(self, ch, note):
        if not self.ready or ch >= CHANNELS:
            return 0
        note &= 0x7F
        if note not in self.sounding[ch]:
            return 1
        if self.free() < Q_MIN_FOR_NOTEOFF:
            self.dropped_off += 1
            return 0
        self._emit_off(ch, note)
        self._note_used()
        return 1

    # LPS_Program()
    def program(self, ch, prog):
        if not self.ready or ch >= CHANNELS:
            return 0
        prog &= 0x7F
        if self.cur_prog[ch] == prog:
            return 1
        if self.free() < Q_MIN_FOR_NOTEON:
            return 0
        self._put(0xC0 | ch)
        self._put(prog)
        self.running_status = 0
        self.sounding[ch].clear()
        self.cur_prog[ch] = PROG_UNKNOWN if prog >= PROG_ALL_OFF else prog
        self._note_used()
        return 1

    # LPS_Bend()
    def bend(self, ch, cents):
        if not self.ready or ch >= CHANNELS:
            return 0
        cents = max(-200, min(200, cents))
        # Integer division matching C's truncation toward zero, which Python's //
        # does not do for negatives.
        scaled = int(cents * 64 / 200)
        msb = max(0, min(127, 64 + scaled))
        if self.free() < Q_MIN_FOR_NOTEON:
            return 0
        self._status(0xE0 | ch)
        self._put(0x00)
        self._put(msb)
        self._note_used()
        return 1

    # LPS_SilenceChannel()
    def silence_channel(self, ch):
        if not self.ready or ch >= CHANNELS:
            return
        # The C walks the bitmap low bit first, which is ascending note order.
        for note in sorted(self.sounding[ch]):
            if self.free() < Q_MIN_FOR_NOTEOFF:
                self.dropped_off += 1
                return
            self._emit_off(ch, note)
        self._note_used()

    # LPS_MidiPanic()
    def panic(self):
        if not self.ready:
            return
        for ch in range(CHANNELS):
            self._put(0xC0 | ch)
            self._put(PROG_ALL_OFF)
            self.sounding[ch].clear()
            self.cur_prog[ch] = PROG_UNKNOWN
        self.running_status = 0
        self._note_used()

    def poly_now(self, ch):
        return len(self.sounding[ch])

    # LPS_MidiTxTick()
    def tx_tick(self, t_ms):
        if not self.ready or self.tail == self.head:
            return
        self._tx(t_ms, self.q[self.tail])
        self.tail = (self.tail + 1) & QMASK


# --- lps_seq.c -------------------------------------------------------------

SEQ_MAX_EV_PER_TICK = 8    # LPS_SEQ_MAX_EV_PER_TICK
FADE_STAGE1_PCT = 40
FADE_STAGE2_PCT = 80

OP_NOTE, OP_PROG, OP_CTRL, OP_META = 0, 1, 2, 3
CTRL_BEND = 0
META_END, META_LOOP, META_MARKER, META_NOP = 0, 1, 2, 3
SONG_LOOPS = 0x01


def ev(dt, opcode, ch, low, arg):
    """Build an event the way LPS_EV_MAKE does."""
    return (dt, ((opcode << 6) | ((ch & 3) << 4) | (low & 0x0F)) & 0xFF, arg & 0xFF)


class Seq:
    """Mirrors lps_seq.c."""

    def __init__(self, midi):
        self.m = midi
        self.song = None
        self.playing = 0
        self.paused = 0
        self.idx = 0
        self.pos_ms = 0
        self.next_ms = 0
        self.fade_total = 0
        self.fade_left = 0
        self.marker = 0

    def _silence_song_channels(self):
        if not self.song:
            return
        for ch in range(CHANNELS):
            if self.song["chmask"] & (1 << ch):
                self.m.silence_channel(ch)

    # LPS_MusicPlay()
    def play(self, song):
        if not song or not song["evs"]:
            return
        self.playing = 0
        self._silence_song_channels()
        self.song = song
        self.paused = 0
        self.pos_ms = 0
        self.idx = 0
        self.next_ms = song["evs"][0][0]
        self.fade_total = self.fade_left = 0
        self.marker = 0
        for ch in range(CHANNELS):
            if (song["chmask"] & (1 << ch)) and song["prog"][ch] != 0xFF:
                self.m.program(ch, song["prog"][ch])
        self.playing = 1

    # LPS_MusicStop()
    def stop(self):
        self.playing = 0
        self._silence_song_channels()
        self.song = None
        self.fade_left = 0

    # LPS_MusicPause()
    def pause(self, p):
        if not self.song:
            return
        if p and not self.paused:
            self.paused = 1
            self._silence_song_channels()
        elif not p and self.paused:
            self.paused = 0

    # LPS_MusicFadeOut()
    def fade_out(self, ms):
        if not self.song or not self.playing:
            return
        if not ms:
            self.stop()
            return
        self.fade_total = ms
        self.fade_left = ms

    def _fade_pct(self):
        if not self.fade_total or not self.fade_left:
            return 0
        return 100 - (100 * self.fade_left) // self.fade_total

    def _fade_drops_note(self, ch):
        if not self.fade_total:
            return False
        pct = self._fade_pct()
        if pct >= FADE_STAGE2_PCT:
            return True
        if pct >= FADE_STAGE1_PCT:
            return self.m.poly_now(ch) > 0
        return False

    def _rewind_to(self, e):
        self.idx = e
        n = self.song["n"]
        self.next_ms = self.pos_ms + (self.song["evs"][e][0] if e < n else 0)

    # do_event()
    def _do_event(self, e):
        _, op_byte, arg = e
        op = op_byte >> 6
        ch = (op_byte >> 4) & 3
        low = op_byte & 0x0F

        if not (self.song["chmask"] & (1 << ch)):
            return

        if op == OP_NOTE:
            if low & 1:
                if not self._fade_drops_note(ch):
                    self.m.note_on(ch, arg)
            else:
                self.m.note_off(ch, arg)
        elif op == OP_PROG:
            self.m.program(ch, arg)
        elif op == OP_CTRL:
            if low == CTRL_BEND:
                self.m.bend(ch, int((arg - 64) * 200 / 64))
        elif op == OP_META:
            if low == META_MARKER:
                self.marker = arg
            elif low == META_END:
                self._silence_song_channels()
                if (self.song["flags"] & SONG_LOOPS) and \
                        self.song["loop"] < self.song["n"]:
                    for ch2 in range(CHANNELS):
                        if (self.song["chmask"] & (1 << ch2)) and \
                                self.song["prog"][ch2] != 0xFF:
                            self.m.program(ch2, self.song["prog"][ch2])
                    self._rewind_to(self.song["loop"])
                else:
                    self.playing = 0

    # LPS_SeqTick()
    def tick(self, elapsed_ms):
        budget = SEQ_MAX_EV_PER_TICK
        if not self.playing or self.paused or not self.song:
            return
        self.pos_ms += elapsed_ms

        if self.fade_left:
            if self.fade_left <= elapsed_ms:
                self.stop()
                return
            self.fade_left -= elapsed_ms

        while self.playing and self.idx < self.song["n"] and \
                self.pos_ms >= self.next_ms:
            before = self.idx
            e = self.song["evs"][self.idx]
            if budget <= 0:
                break
            budget -= 1
            self._do_event(e)
            if not self.playing:
                return
            if self.idx != before:
                continue
            self.idx += 1
            if self.idx < self.song["n"]:
                self.next_ms += self.song["evs"][self.idx][0]

        if self.playing and self.idx >= self.song["n"]:
            self._silence_song_channels()
            if (self.song["flags"] & SONG_LOOPS) and \
                    self.song["loop"] < self.song["n"]:
                self._rewind_to(self.song["loop"])
            else:
                self.playing = 0


# --- lps.c -----------------------------------------------------------------

class Driver:
    """Mirrors LPS_Tick's ordering: claims expire, sequencer runs, one byte out."""

    def __init__(self):
        self.wire = []
        self.midi = Midi(lambda t, b: self.wire.append((t, b)))
        self.seq = Seq(self.midi)
        self.millis = 0

    def tick(self, now_ms, elapsed_ms):
        """`now_ms` is the caller's virtual clock, which is what stamps a byte.

        Not the same as the library's own millisecond counter: the caller runs
        LPS_Tick at now_ms and the counter has already advanced by a full period
        by the time a byte goes out. Timestamping with the wrong one puts every
        byte one tick early and makes the byte diff fail for a reason that has
        nothing to do with the driver."""
        self.millis += elapsed_ms
        self.seq.tick(elapsed_ms)
        self.midi.tx_tick(now_ms)

    def wirelog(self):
        """One "t_ms hex" line per byte, for diffing against the C driver."""
        out = ["# t_ms hex -- MIDI bytes as the console would have sent them"]
        out += ["%u %02X" % (t, b) for t, b in self.wire]
        return "\n".join(out) + "\n"
