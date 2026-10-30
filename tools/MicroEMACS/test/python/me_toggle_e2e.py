#!/usr/bin/env python3
"""toggle-mouse switches reporting at runtime: the startup modes only
appear with -m, the flip/force states match the echo line, and the
enable/disable byte sequences go out on the wire on every change."""

import os

from common import check, drain, finish, fixture, kill, spawn

ENABLE = b"\x1b[?1000h\x1b[?1002h\x1b[?1006h"
DISABLE = b"\x1b[?1006l\x1b[?1002l\x1b[?1000l"


def mxcmd(name):
    return b"\x1bx" + name.encode() + b"\r"


path = fixture("toggle.txt", b"x\n")

# --- A: -m enables at startup; the command flips both ways -------------
p, fd = spawn(path)
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
kill(p)

# --- B: without -m nothing is enabled until the command says so --------
# (a bare flip would turn reporting on, so poke a harmless key first)
p, fd = spawn(path, mouse=False)
out = drain(fd)
os.write(fd, b"\x1b[B")                 # Down arrow: no command
out += drain(fd)
check(ENABLE not in out, "without -m nothing enables reporting at startup")
kill(p)

# --- C: a nonzero prefix forces the state on ---------------------------
p, fd = spawn(path, mouse=False)
drain(fd)
os.write(fd, b"\x15" + mxcmd("toggle-mouse"))   # C-U prefix -> on
out = drain(fd)
check(ENABLE in out and b"[Mouse now ON]" in out,
      "C-U prefix forces reporting on")
kill(p)

finish()
