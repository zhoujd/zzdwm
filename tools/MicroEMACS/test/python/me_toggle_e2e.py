#!/usr/bin/env python3
"""toggle-mouse switches reporting at runtime: the startup modes only
appear with -m, the flip/force states match the echo line, and the
enable/disable byte sequences go out on the wire on every change.
A prefix argument sets a toggle's state rather than flipping it, for
toggle-mouse and toggle-readonly alike."""

import os

from common import check, drain, finish, fixture, kill, spawn

ENABLE = b"\x1b[?1000h\x1b[?1002h\x1b[?1006h"
DISABLE = b"\x1b[?1006l\x1b[?1002l\x1b[?1000l"


def mxcmd(name):
    return b"\x1bx" + name.encode() + b"\r"


path = fixture("toggle.txt", b"x\n")

# --- A: -m enables at startup; the command flips both ways -------------
p, fd = spawn(path, mouse=True)
out = drain(fd)
check(ENABLE in out, "-m startup enables reporting")
os.write(fd, mxcmd("toggle-mouse"))
out += drain(fd)
check(DISABLE in out and b"[Mouse now OFF]" in out,
      "toggle off sends the disable trio and echoes OFF")
os.write(fd, mxcmd("toggle-mouse"))
out += drain(fd)
check(out.count(ENABLE) >= 2 and b"[Mouse now ON]" in out,
      "toggle back on sends the enable trio and echoes ON")
kill(p, fd)

# --- B: without -m nothing is enabled until the command says so --------
# (a bare flip would turn reporting on, so poke a harmless key first)
p, fd = spawn(path, mouse=False)
out = drain(fd)
os.write(fd, b"\x1b[B")                 # Down arrow: no command
out += drain(fd)
check(ENABLE not in out, "without -m nothing enables reporting at startup")
kill(p, fd)

# --- C: a prefix sets the state rather than toggling it ----------------
p, fd = spawn(path, mouse=False)
drain(fd)
os.write(fd, b"\x150" + mxcmd("toggle-mouse"))  # C-U 0 -> stays off
out = drain(fd)
check(ENABLE not in out and b"[Mouse now OFF]" in out,
      "C-U 0 forces reporting off, not a flip")
os.write(fd, b"\x151" + mxcmd("toggle-mouse"))  # C-U 1 -> on
out = drain(fd)
check(ENABLE in out and b"[Mouse now ON]" in out,
      "C-U 1 forces reporting on")
kill(p, fd)

# --- D: the same rule holds for toggle-readonly ------------------------
p, fd = spawn(path)
drain(fd)
os.write(fd, b"\x151" + b"\x18\x11")            # C-U 1 C-X C-Q -> on
out = drain(fd)
check(b"Buffer is now read-only" in out, "C-U 1 makes the buffer read-only")
os.write(fd, b"\x151" + b"\x18\x11")            # again -> stays on
out = drain(fd)
check(b"Buffer is now read-only" in out, "C-U 1 twice does not toggle it off")
os.write(fd, b"\x18\x11")                       # bare -> off
out = drain(fd)
check(b"Buffer is now read-write" in out, "a bare C-X C-Q toggles it off")
kill(p, fd)

finish()
