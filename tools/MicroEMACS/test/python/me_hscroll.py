#!/usr/bin/env python3
"""Shift+wheel (SGR, button 64/65 + shift bit) scrolls the view
horizontally in four-column steps, the pin survives idle updates,
dot motion drops it, and no mouse byte leaks into the buffer."""

import os

from common import check, drain, finish, fixture, save_quit, spawn

good = (b"0123456789" * 30) + b"\nshort\n"
path = fixture("hscroll.txt", good)

# SGR mouse encoding (terminfo kmous=\E[<): button 64/65 + 4 shift.
SH_WHEEL_DOWN = b"\x1b[<69;6;2M"        # wheel-down + shift
SH_WHEEL_UP = b"\x1b[<68;6;2M"          # wheel-up + shift

p, fd = spawn(path)
out = drain(fd)
check(b"\x1b[?1000h" in out, "mouse mode enabled (1000h)")

# Shift+wheel-down: view moves right four columns.
os.write(fd, SH_WHEEL_DOWN)
out = drain(fd)
check(b"4567890123456789" in out, "shift+wheel-down scrolls view to column 4")

# An update that does not move dot (the M-X prompt) must keep the view.
os.write(fd, b"\x1bx")                  # M-X: execute prompt on the echo line
out = drain(fd)
check(b"0123456789012345678" not in out,
      "idle update keeps the scrolled view (no snap-back)")
os.write(fd, b"\x07")                   # C-G: cancel the prompt
drain(fd)

# Moving dot (right arrow, application mode) drops the pin and
# recentres around dot.
os.write(fd, b"\x1bOC")
out = drain(fd)
check(b"0123456789012345678" in out,
      "dot movement drops the pin and recentres")

# Step right again, then all the way back to the left edge.
os.write(fd, SH_WHEEL_DOWN)
out = drain(fd)
check(b"8901234567890123" in out, "second shift+wheel-down reaches column 8")
os.write(fd, SH_WHEEL_UP)
os.write(fd, SH_WHEEL_UP)
out = drain(fd)
check(b"0123456789012345678" in out, "shift+wheel-up returns to column 0")

# Save and quit; the file on disk must be exactly what we wrote —
# any mouse byte that leaked into the buffer shows up here.
rc = save_quit(p, fd)
check(rc == 0, "quit after scroll exits cleanly (rc=%r)" % rc)
content = open(path, "rb").read()
check(content == good,
      "no mouse bytes leaked into the buffer (got %r)" % content[:60])

finish()
