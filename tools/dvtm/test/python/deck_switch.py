#!/usr/bin/env python3
"""Switching windows within a pile: the deck layouts stack the slave
windows on one rect, and focusing another card of the pile used to
leave the old card's pixels under the new card's border - focus only
redrew borders, and vt_draw touches dirty rows alone.  Now the top of
the pile - the window focused most recently - owns the rect: focusing
a piled window replays its whole buffer over it, the windows buried
below no longer draw their vt or border into it, and the repaint
refreshes the top alone.

Each slave echoes its marker while focused; switching back and forth
then replays a marker with no new shell output, which only a full
buffer replay produces."""


import fcntl
import os
import pty
import select
import struct
import termios
import time

HERE = os.path.dirname(os.path.abspath(__file__))
DVTM = os.path.join(HERE, "..", "..", "dvtm")
MOD = b"\x1c"                          # CTRL('\') — dvtm's MOD key
REDRAW = MOD + b"\x0c"                 # MOD+CTRL('L'): full repaint
DOUBLEDECK = MOD + b"\x04"             # MOD+CTRL('d')


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
    os.write(fd, MOD + b"c")           # third window: one master, two slaves
    drain(fd, 1.0)
    os.write(fd, DOUBLEDECK)           # MOD+CTRL('d') selects [D]
    drain(fd, 1.0)

    os.write(fd, MOD + b"j")           # first slave, the top of its pile
    drain(fd, 1.0)
    os.write(fd, b"echo ZZS1\r")       # its marker
    check(b"ZZS1" in drain(fd, 2.0), "the top slave echoes its marker")

    os.write(fd, MOD + b"j")           # the slave buried below: pile switch
    drain(fd, 1.0)
    os.write(fd, b"echo ZZS2\r")
    check(b"ZZS2" in drain(fd, 2.0), "the newly topped slave echoes its own")

    os.write(fd, MOD + b"k")           # back to the first slave
    check(b"ZZS1" in drain(fd, 1.0),
          "switching piles replays the new top's buffer, not just its border")

    os.write(fd, MOD + b"j")           # and back to the second
    check(b"ZZS2" in drain(fd, 1.0),
          "switching piles replays it the other way round")

    os.write(fd, REDRAW)
    out = drain(fd, 1.5)
    check(b"ZZS2" in out, "the repaint draws the top of the pile")
    check(b"ZZS1" not in out, "the repaint leaves the buried slave out")
finally:
    os.kill(pid, 15)
    os.waitpid(pid, 0)

print("PASS pile switching")
