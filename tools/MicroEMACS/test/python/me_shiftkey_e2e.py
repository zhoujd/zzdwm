#!/usr/bin/env python3
"""Shift+PageUp/PageDown byte sequences — what terminals that bypass
mouse reporting while Shift is held (st) send for Shift+wheel — scroll
horizontally both with and without -m, decode without emitting garbage
glyphs, and leave the buffer untouched."""

import os

from common import check, drain, finish, fixture, kill, save_quit, spawn

SPGUP = b"\x1b[5;2~"                    # st: shift+wheel-up
SPGDN = b"\x1b[6;2~"                    # st: shift+wheel-down
GARBAGE = (b"\xc6\x8d", b"\xc6\x8e")    # ƌ (397), Ǝ (398) as UTF-8

# Long line wider than the screen; each chunk is unique and repeated
# nowhere else, so a shifted repaint can be asserted unambiguously.
content = b"".join(b"[%03d]" % i for i in range(60)) + b"\nend\n"
path = fixture("shiftkey.txt", content)


def run_case(mouse):
    tag = "with -m" if mouse else "without -m"
    p, fd = spawn(path, mouse=mouse)
    drain(fd)
    os.write(fd, SPGDN); out1 = drain(fd)        # leftcol 0 -> 4
    os.write(fd, SPGDN); out2 = drain(fd)        # leftcol 4 -> 8
    os.write(fd, SPGUP); out3 = drain(fd)        # leftcol 8 -> 4
    kill(p)

    ok = all(g not in out1 + out2 + out3 for g in GARBAGE)
    check(ok, "%s: no garbage glyphs" % tag)
    # After the second scroll the visible line starts at column 8.
    check(content[8:48] in out2 and content[0:8] not in out2,
          "%s: right scroll by 8 columns" % tag)
    # Scrolling back left restores the column-4 view.
    check(content[4:44] in out3 and content[0:4] not in out3,
          "%s: left scroll back to column 4" % tag)


run_case(mouse=True)
run_case(mouse=False)

# Dot and buffer untouched: scroll, save, quit; file must be identical.
p, fd = spawn(path)
drain(fd)
os.write(fd, SPGDN + SPGDN + SPGUP)
drain(fd)
rc = save_quit(p, fd)
check(rc == 0, "clean quit after scrolling")
check(open(path, "rb").read() == content,
      "buffer unchanged by shift-page scrolling")

finish()
