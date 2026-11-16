#!/usr/bin/env python3
"""The deck layouts: doubledeck "[D]" piles the first nmaster and the
remaining windows on top of each other in a left/right split, zetadeck
"[Z]" in a top/bottom split - the ports of dwm's doubledeck and
zetadeck.  Only the top of each pile keeps its border, so the second
window's border vanishes once nmaster grows to cover it.

Assertions run on whole lines: MOD+CTRL('L'), dvtm's redraw, makes the
terminal repaint completely, so a border's full text streams - ncurses
otherwise writes only the changed cells.  Geometry is read off the
cursor positioning ncurses emits on that repaint: a 100 column screen
split at mfact 0.50 puts the stack window at column 51 (1-based, the
ACS divider owns column 50), a 30 row one split the same way puts the
zetadeck stack at row 16."""


import fcntl
import os
import pty
import re
import select
import struct
import termios
import time

HERE = os.path.dirname(os.path.abspath(__file__))
DVTM = os.path.join(HERE, "..", "..", "dvtm")
MOD = b"\x1c"                          # CTRL('\') — dvtm's MOD key
REDRAW = MOD + b"\x0c"                 # MOD+CTRL('L'): full repaint
DOUBLEDECK = MOD + b"\x04"             # MOD+CTRL('d')
ZETADECK = MOD + b"\x1a"               # MOD+CTRL('z')
VSTACK = MOD + b"\x02"                 # MOD+CTRL('b')


def drain(fd, timeout=1.0):
    out = b""
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        r, _, _ = select.select([fd], [], [], 0.1)
        if r:
            try:
                chunk = os.read(fd, 65536)
            except OSError:
                break
            if not chunk:
                break
            out += chunk
    return out


def repaint(fd):
    """Force dvtm's complete repaint and return its output."""
    os.write(fd, REDRAW)
    return drain(fd, 1.0)


def check(cond, what):
    print(("PASS " if cond else "FAIL ") + what)
    if not cond:
        raise SystemExit(1)


if not os.access(DVTM, os.X_OK):
    print("SKIP: no dvtm binary; run make in tools/dvtm")
    raise SystemExit(0)


pid, fd = pty.fork()
if pid == 0:
    os.environ["TERM"] = "st-256color"
    os.environ["LANG"] = "en_US.UTF-8"
    os.execv(DVTM, [DVTM])
fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", 30, 100, 0, 0))

try:
    drain(fd, 2.0)
    os.write(fd, MOD + b"c")           # second window
    drain(fd, 1.0)
    os.write(fd, DOUBLEDECK)           # MOD+CTRL('d') selects [D]
    drain(fd, 1.0)

    out = repaint(fd)
    i1, i2 = out.find(b"[1/2]"), out.find(b"[2/2]")
    check(i1 != -1 and i2 != -1, "[D] draws both decks' borders")
    check(b"\r" not in out[i1:i2] and b"\n" not in out[i1:i2],
          "[D] keeps both decks' borders on one row")
    check(b"\x1b[2;1H" in out and b"\x1b[2;51H" in out,
          "[D] splits at mfact 0.50: masters at column 1, stack at 51")

    os.write(fd, MOD + b"=")           # mfact +0.05
    drain(fd, 0.5)
    out = repaint(fd)
    check(b"\x1b[2;56H" in out and b"\x1b[2;51H" not in out,
          "[D] follows mfact 0.55: stack moves to column 56")

    os.write(fd, MOD + b"i")           # nmaster 2: both windows decked as one
    drain(fd, 0.5)
    out = repaint(fd)
    check(b"[2/2]" not in out,
          "[D] with nmaster covering all piles the second border under")

    os.write(fd, MOD + b"d")           # nmaster 1
    os.write(fd, MOD + b"Z")           # mfact back to 0.50
    drain(fd, 0.5)
    os.write(fd, ZETADECK)             # MOD+CTRL('z') selects [Z]
    drain(fd, 1.0)

    out = repaint(fd)
    i1, i2 = out.find(b"[1/2]"), out.find(b"[2/2]")
    check(i1 != -1 and i2 != -1, "[Z] draws both decks' borders")
    check(b"\r" in out[i1:i2] or b"\n" in out[i1:i2],
          "[Z] stacks its decks' borders on separate rows")
    check(re.search(rb"\x1b\[16d", out) and b"\x1b[17;1H" in out,
          "[Z] splits at mfact 0.50: stack border at row 16, content at 17")

    os.write(fd, MOD + b"I")           # border info on: it shows the symbol
    drain(fd, 0.5)
    os.write(fd, VSTACK)               # MOD+CTRL('b') selects ====
    out = repaint(fd)
    check(b" === " in out, "MOD+CTRL('b') selects the vstack layout")
finally:
    os.kill(pid, 15)
    os.waitpid(pid, 0)

print("PASS deck layouts")
