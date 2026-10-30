#!/usr/bin/env python3
"""me inside dvtm: with dvtm's mouse handling on (MOD+M) its ncurses
decodes the outer reports and vt_mouse re-encodes them as X10 for the
client under the mouse.  The wheel must scroll me (dvtm maps BUTTON4/5
to X10 64/65 instead of eating them for its own scrollback) and clicks
must land exactly (X10 coordinates are 1-based).  Reports are injected
where st would send them, on st-256color terminfo."""

import fcntl
import os
import pty
import struct
import sys
import termios
import time

from common import ME, ROOT, check, drain, finish, fixture

DVTM = os.path.join(os.path.dirname(ROOT), "dvtm", "dvtm")
MOD = b"\x1c"                          # CTRL('\') — dvtm's MOD key


def x10(button, col, row):             # X10 report as st sends it (1-based)
    return b"\x1b[M" + bytes((32 + button, 33 + col, 33 + row))


if not os.access(DVTM, os.X_OK):
    print("SKIP: no dvtm binary at", DVTM)
    sys.exit(0)

pid, fd = pty.fork()
if pid == 0:
    os.environ["TERM"] = "st-256color"
    os.environ["LANG"] = "en_US.UTF-8"
    os.execv(DVTM, [DVTM])
fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", 30, 100, 0, 0))
drain(fd, 2.0)

# --- MOD+M turns dvtm's own mouse handling on ------------------------------
os.write(fd, MOD + b"M")
out = drain(fd, 0.8)
check(b"\x1b[?1000h" in out, "MOD+M enabled reporting on the outer terminal")

# --- me on 60 marked lines -------------------------------------------------
path = fixture("dvtm.txt", b"".join(b"line%02d wxyz\n" % i for i in range(60)))
os.write(fd, ME.encode() + b" -m " + path.encode() + b"\r")
drain(fd, 3.0)

# --- click on screen row 2, column 6 (0-based): line 2, column 6 -----------
os.write(fd, x10(0, 6, 2))             # press (X10 has no release)
time.sleep(0.05)
os.write(fd, x10(3, 6, 2))             # release -> dvtm forwards CLICKED
drain(fd)
os.write(fd, b"QQ")
drain(fd)

# --- wheel down twice: top moves 2 * mouse-wheel-step (6) lines ------------
os.write(fd, x10(65, 6, 1))
drain(fd)
os.write(fd, x10(65, 6, 1))
out = drain(fd)
check(b"line32 wxyz" in out, "wheel-down scrolled me (line32 scrolled in)")

# --- wheel up once: top moves back 3, line03 redrawn at the top ------------
os.write(fd, x10(64, 6, 1))
out = drain(fd)
check(b"line03 wxyz" in out, "wheel-up scrolled me back (line03 at top)")

os.write(fd, b"\x18\x13")              # C-X C-S
drain(fd)
os.write(fd, b"\x18\x03")              # C-X C-C: quit me, back to the shell
drain(fd, 2.0)
os.kill(pid, 9)
os.waitpid(pid, 0)
content = open(path, "rb").read().splitlines()
check(content[2].startswith(b"line02QQ"),
      "click placed dot through dvtm (line 2 = %r)" % content[2][:12])

finish()
