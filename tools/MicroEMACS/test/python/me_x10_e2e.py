#!/usr/bin/env python3
"""Legacy X10 mouse reports (\\033[M plus three raw bytes) still work:
on a terminal whose terminfo has kmous=\\E[M (st) and on one with no
kmous at all (linux console), clicks land and nothing leaks."""

import os

from common import check, drain, finish, fixture, kill, save_quit, spawn


def x10_click(col, row, button=0):
    return b"\x1b[M" + bytes((32 + button, 33 + col, 33 + row))


def x10_wheel(down=True, col=0, row=0):
    return b"\x1b[M" + bytes((32 + 64 + (1 if down else 0),
                              33 + col, 33 + row))


good = b"alpha beta gamma\nsecond\n"

# --- A: X10 click on a terminal whose terminfo has kmous=\E[M (st) --------
# The define_key must claim the prefix ahead of kmous, or ncurses eats
# the three data bytes and returns KEY_MOUSE.
path = fixture("x10_a.txt", good)
p, fd = spawn(path, term="st-256color")
drain(fd)
os.write(fd, x10_click(6, 0))           # start of "beta"
drain(fd)
os.write(fd, b"XY")
drain(fd)
rc = save_quit(p, fd)
check(rc == 0, "A: clean quit (st-256color)")
content = open(path, "rb").read()
check(content == b"alpha XYbeta gamma\nsecond\n",
      "A: X10 click placed dot, no leaked bytes (got %r)" % content)

# --- B: X10 wheel scrolls vertically ---------------------------------------
path = fixture("x10_b.txt",
               b"".join(b"line%02d\n" % i for i in range(60)))
p, fd = spawn(path, term="st-256color")
first = drain(fd)
os.write(fd, x10_wheel(True))
out = drain(fd)
kill(p)
check(b"line24" in out and b"line24" not in first,
      "B: X10 wheel scrolled 3 lines")

# --- C: same click on the Linux console terminfo (no kmous at all) --------
path = fixture("x10_c.txt", good)
p, fd = spawn(path, term="linux")
drain(fd)
os.write(fd, x10_click(6, 0))
drain(fd)
os.write(fd, b"XY")
drain(fd)
rc = save_quit(p, fd)
check(rc == 0, "C: clean quit (linux)")
content = open(path, "rb").read()
check(content == b"alpha XYbeta gamma\nsecond\n",
      "C: X10 click placed dot on console terminfo (got %r)" % content)

finish()
