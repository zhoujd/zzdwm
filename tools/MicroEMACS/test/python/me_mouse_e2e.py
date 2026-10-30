#!/usr/bin/env python3
"""SGR mouse basics: a click moves dot so typed text lands there, a
double click selects the word (and M-W OSC 52s it), and the vertical
wheel scrolls lines."""

import os
import time

from common import check, drain, finish, fixture, kill, save_quit, spawn


def sgr_click(x, y, button=0):          # 1-based x/y like the terminal sends
    return b"\x1b[<%d;%d;%dM\x1b[<%d;%d;%dm" % (button, x, y, button, x, y)


# --- A: click moves dot; typed text lands at the click ----------------
good_a = b"alpha beta gamma\nsecond\n"
path = fixture("e2e_a.txt", good_a)
p, fd = spawn(path)
drain(fd)
os.write(fd, sgr_click(7, 1))           # col 6 (0-based) = start of "beta"
drain(fd)
os.write(fd, b"XY")                     # insert at dot
drain(fd)
rc = save_quit(p, fd)
check(rc == 0, "A: clean quit")
content = open(path, "rb").read()
check(content == b"alpha XYbeta gamma\nsecond\n",
      "A: click placed dot, text inserted at click (got %r)" % content)

# --- B: double click selects the word; M-W OSC 52s it ------------------
path = fixture("e2e_b.txt", good_a)
p, fd = spawn(path)
drain(fd)
for _ in range(2):                      # two clicks = double click
    os.write(fd, sgr_click(8, 1))
    time.sleep(0.05)
out = drain(fd)
os.write(fd, b"\x1bW")                  # M-W copy-region
out += drain(fd)
check(b"\x1b]52;c;YmV0YQ==\x07" in out,
      'B: double click selected "beta" and M-W OSC 52\'d it')
kill(p)

# --- C: vertical wheel still scrolls lines -----------------------------
path = fixture("e2e_c.txt",
               b"".join(b"line%02d\n" % i for i in range(60)))
p, fd = spawn(path)
drain(fd)
for _ in range(3):                      # wheel-down 3x3 = 9 lines
    os.write(fd, b"\x1b[<65;5;2M")
    time.sleep(0.05)
out = drain(fd)
# ncurses optimizes the scroll; the new bottom rows prove it moved.
check(b"line28" in out, "C: vertical wheel scrolled 9 lines")
kill(p)

finish()
