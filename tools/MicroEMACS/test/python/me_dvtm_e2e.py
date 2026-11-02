#!/usr/bin/env python3
"""me inside dvtm: dvtm's ncurses decodes the outer reports and
vt_mouse re-encodes them as X10 for the client under the mouse, with
the coordinates of the client's content (its title line is not part of
what the client draws).  Two modes: dvtm's own mouse handling on
(MOD+M), and the default forwarding-only relay that arms the outer
mask whenever a client enables mouse tracking.  The wheel must scroll
me, and clicks must land on the line under the pointer.  Reports are
injected where st would send them, on st-256color terminfo."""

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


def start_dvtm():
    pid, fd = pty.fork()
    if pid == 0:
        os.environ["TERM"] = "st-256color"
        os.environ["LANG"] = "en_US.UTF-8"
        os.execv(DVTM, [DVTM])
    fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", 30, 100, 0, 0))
    drain(fd, 2.0)
    return pid, fd


if not os.access(DVTM, os.X_OK):
    print("SKIP: no dvtm binary at", DVTM)
    sys.exit(0)

# --- A: dvtm's own mouse handling on (MOD+M) -------------------------------
pid, fd = start_dvtm()
os.write(fd, MOD + b"M")
out = drain(fd, 0.8)
check(b"\x1b[?1000h" in out, "A: MOD+M enabled reporting on the outer terminal")

path = fixture("dvtm_a.txt", b"".join(b"line%02d wxyz\n" % i for i in range(60)))
os.write(fd, ME.encode() + b" -m " + path.encode() + b"\r")
drain(fd, 3.0)

# Click on screen row 2, column 6 (0-based): the title line is screen
# row 0 and me's first text row is screen row 1, so the click is on
# line 1, column 6.  dvtm forwards the CLICKED gesture as an X10 press
# plus the mode-1000 release byte (button 3).
os.write(fd, x10(0, 6, 2))             # press
time.sleep(0.05)
os.write(fd, x10(3, 6, 2))             # release -> dvtm forwards CLICKED
drain(fd)
os.write(fd, b"QQ")
drain(fd)

# Wheel down twice: the top moves 2 * mouse-wheel-step (6) lines; the
# wheel must reach me, not dvtm's own scrollback.
os.write(fd, x10(65, 6, 1))
drain(fd)
os.write(fd, x10(65, 6, 1))
out = drain(fd)
check(b"line32 wxyz" in out, "A: wheel-down scrolled me (line32 scrolled in)")

# Wheel up once: the top moves back 3, line03 redrawn at the top.
os.write(fd, x10(64, 6, 1))
out = drain(fd)
check(b"line03 wxyz" in out, "A: wheel-up scrolled me back (line03 at top)")

os.write(fd, b"\x18\x13")              # C-X C-S
drain(fd)
os.write(fd, b"\x18\x03")              # C-X C-C: quit me, back to the shell
drain(fd, 2.0)
os.kill(pid, 9)
os.waitpid(pid, 0)
content = open(path, "rb").read().splitlines()
check(content[1].startswith(b"line01QQ"),
      "A: click placed dot on the clicked line through dvtm (line 1 = %r)"
      % content[1][:12])

# --- B: forwarding-only relay (dvtm's mouse handling off) ------------------
# me enabling mouse tracking must arm the outer mask on dvtm's behalf,
# and quitting must disarm it again.
pid, fd = start_dvtm()
path = fixture("dvtm_b.txt", b"".join(b"line%02d wxyz\n" % i for i in range(60)))
os.write(fd, ME.encode() + b" -m " + path.encode() + b"\r")
out = drain(fd, 3.0)
check(b"\x1b[?1000h" in out,
      "B: me's -m armed reporting on the outer terminal")

os.write(fd, x10(0, 6, 2))
time.sleep(0.05)
os.write(fd, x10(3, 6, 2))
drain(fd)
os.write(fd, b"QQ")
drain(fd)

os.write(fd, x10(65, 6, 1))
out = drain(fd)
# The scroll is optimized: only the new bottom rows are rewritten.
check(b"line29 wxyz" in out,
      "B: wheel reached me without dvtm eating it for scrollback")

os.write(fd, b"\x18\x13")
drain(fd)
os.write(fd, b"\x18\x03")
out = drain(fd, 2.0)
check(b"\x1b[?1000l" in out,
      "B: quitting me disarmed reporting on the outer terminal")
os.kill(pid, 9)
os.waitpid(pid, 0)
content = open(path, "rb").read().splitlines()
check(content[1].startswith(b"line01QQ"),
      "B: click placed dot on the clicked line through the relay (line 1 = %r)"
      % content[1][:12])

# --- C: drag selection through the relay -----------------------------------
# The relay must forward the motion of a held button 1 (the X10 motion
# bit), and me must anchor on the press and complete the region on the
# release, so a drag selects the dragged-over text and highlights it.
pid, fd = start_dvtm()
path = fixture("dvtm_c.txt", b"".join(b"line%02d wxyz\n" % i for i in range(60)))
os.write(fd, ME.encode() + b" -m " + path.encode() + b"\r")
out = drain(fd, 3.0)
check(b"\x1b[?1002h" in out,
      "C: button-event tracking armed for drag motion")

os.write(fd, x10(0, 2, 2))             # press on the "n" of line01
time.sleep(0.15)
os.write(fd, x10(32, 4, 2))            # drag through "e0"
time.sleep(0.15)
os.write(fd, x10(32, 6, 2))            # drag to the blank after "1"
time.sleep(0.15)
os.write(fd, x10(3, 6, 2))             # release: ne01 selected
out = drain(fd)
check(b"48;5;24" in out,
      "C: drag painted the selection highlight")

os.write(fd, b"\x17")                  # C-W: kill-region cuts the selection
drain(fd)
os.write(fd, b"\x18\x13")
drain(fd)
os.write(fd, b"\x18\x03")
drain(fd, 2.0)
os.kill(pid, 9)
os.waitpid(pid, 0)
content = open(path, "rb").read().splitlines()
check(content[1] == b"li wxyz",
      "C: drag selected cols 2..5, kill-region cut them (line 1 = %r)"
      % content[1])

finish()
