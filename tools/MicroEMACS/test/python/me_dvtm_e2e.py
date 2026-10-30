#!/usr/bin/env python3
"""me inside dvtm: dvtm relays mouse reporting for clients that track
the mouse (DECSET 1000) even when its own mouse handling is off (the
default).  Scenario A: me -m with dvtm's mouse off — reporting turns
on by itself, the wheel scrolls me, clicks land exactly, and quitting
closes the relay.  Scenario B: me without -m, M-x toggle-mouse opens
and closes the same path at runtime.  Scenario C: dvtm mouse on
(MOD+M) with me -m — the wheel reaches me, not the local scrollback.
X10 reports are injected where st would send them, on st-256color."""

import fcntl
import os
import pty
import struct
import sys
import termios

from common import ME, ROOT, check, drain, finish, fixture

DVTM = os.path.join(os.path.dirname(ROOT), "dvtm", "dvtm")
MOD = b"\x1c"                          # CTRL('\') — dvtm's MOD key
LINES = b"".join(b"line%02d wxyz\n" % i for i in range(60))


def x10(button, col, row):             # X10 report as st sends it (1-based)
    return b"\x1b[M" + bytes((32 + button, 33 + col, 33 + row))


def wheel(down=True):
    return x10(64 + (1 if down else 0), 6, 1)


def mxcmd(name):                       # me's M-x prompt
    return b"\x1bx" + name.encode() + b"\r"


def start_dvtm():
    pid, fd = pty.fork()
    if pid == 0:
        os.environ["TERM"] = "st-256color"
        os.environ["LANG"] = "en_US.UTF-8"
        os.execv(DVTM, [DVTM])
    fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", 30, 100, 0, 0))
    drain(fd, 2.0)
    return pid, fd


def start_me(fd, path, args=b"-m"):
    os.write(fd, ME.encode() + b" " + args + b" " + path.encode() + b"\r")
    return drain(fd, 3.0)


if not os.access(DVTM, os.X_OK):
    print("SKIP: no dvtm binary at", DVTM)
    sys.exit(0)

# --- A: dvtm mouse off (default), me -m: the relay turns reporting on ------
pid, fd = start_dvtm()
path = fixture("dvtm_a.txt", LINES)
out = start_me(fd, path)
check(b"\x1b[?1000h" in out, "A: me -m enabled reporting on the outer terminal")
os.write(fd, wheel())                  # one step: line29 scrolls in
out = drain(fd)
check(b"line29 wxyz" in out, "A: wheel-down scrolled me")
os.write(fd, x10(0, 6, 2))             # click screen row 2 (top=line03 -> line05)
drain(fd)
os.write(fd, b"QQ")
drain(fd)
os.write(fd, b"\x18\x13")              # C-X C-S
drain(fd)
os.write(fd, b"\x18\x03")              # C-X C-C
out = drain(fd, 2.0)
check(b"\x1b[?1000l" in out, "A: relay closed when me quit")
content = open(path, "rb").read().splitlines()
check(content[5].startswith(b"line05QQ"),
      "A: click placed dot exactly (line 5 = %r)" % content[5][:12])
os.kill(pid, 9)
os.waitpid(pid, 0)

# --- B: dvtm mouse off, me without -m: M-x toggle-mouse at runtime ---------
pid, fd = start_dvtm()
path = fixture("dvtm_b.txt", LINES)
out = start_me(fd, path, args=b"")
check(b"?1000h" not in out, "B: no reporting without -m or a toggle")
os.write(fd, mxcmd("toggle-mouse"))
out = drain(fd)
check(b"[Mouse now ON]" in out and b"\x1b[?1000h" in out,
      "B: toggle-mouse echoed ON and enabled reporting")
os.write(fd, wheel())
out = drain(fd)
check(b"line29 wxyz" in out, "B: wheel scrolled me after the toggle")
os.write(fd, mxcmd("toggle-mouse"))
out = drain(fd)
check(b"[Mouse now OFF]" in out and b"\x1b[?1000l" in out,
      "B: toggle-mouse echoed OFF and closed the relay")
os.write(fd, wheel())
out = drain(fd)
check(out == b"", "B: wheel after OFF produces no output (no leak)")
os.kill(pid, 9)
os.waitpid(pid, 0)

# --- C: dvtm mouse on (MOD+M): the wheel reaches me, not the scrollback ----
pid, fd = start_dvtm()
os.write(fd, MOD + b"M")
out = drain(fd, 0.8)
check(b"\x1b[?1000h" in out, "C: MOD+M enabled reporting on the outer terminal")
path = fixture("dvtm_c.txt", LINES)
start_me(fd, path)
os.write(fd, wheel())
drain(fd)
os.write(fd, wheel())                  # top moves 6: line32 scrolls in
out = drain(fd)
check(b"line32 wxyz" in out, "C: wheel-down scrolled me past the scrollback")
os.write(fd, wheel(False))             # wheel up: line03 back at the top
out = drain(fd)
check(b"line03 wxyz" in out, "C: wheel-up scrolled me back")
os.kill(pid, 9)
os.waitpid(pid, 0)

finish()
