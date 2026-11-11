#!/usr/bin/env python3
"""The border's mouse indicators: (M) while dvtm's own mouse grabbing
is on - the global MOD+M toggle - and (m) on a window whose application
enables mouse tracking (DECSET 1000), taking the events itself.  Round
brackets, as the layout symbols - fullscreen's is "[M]" - own the square
ones.  Driven through a pty with me inside, whose toggle-mouse command
flips the tracking at run time.

Assertions run on whole lines: MOD+CTRL('L'), dvtm's redraw, makes the
terminal repaint completely, so a border's full text streams - ncurses
otherwise writes only the changed cells, and a shifted indicator can
lose even its closing bracket to an unchanged cell."""


import fcntl
import os
import pty
import select
import struct
import termios
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
DVTM = os.path.join(HERE, "..", "..", "dvtm")
ME = os.path.join(HERE, "..", "..", "..", "MicroEMACS", "me")
MOD = b"\x1c"                          # CTRL('\') — dvtm's MOD key
REDRAW = MOD + b"\x0c"                 # MOD+CTRL('L'): full repaint
MX = b"\x1bxtoggle-mouse\r"            # me: toggle-mouse


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


def repaint(fd):
    """Force dvtm's complete repaint and return its output."""
    os.write(fd, REDRAW)
    return drain(fd, 1.0)


def check(cond, what):
    print(("PASS " if cond else "FAIL ") + what)
    if not cond:
        raise SystemExit(1)


if not os.access(DVTM, os.X_OK) or not os.access(ME, os.X_OK):
    print("SKIP: no dvtm or me binary; run make in tools/dvtm and "
          "build.sh in tools/MicroEMACS")
    raise SystemExit(0)


def start_dvtm():
    pid, fd = pty.fork()
    if pid == 0:
        os.environ["TERM"] = "st-256color"
        os.environ["LANG"] = "en_US.UTF-8"
        os.execv(DVTM, [DVTM])
    fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", 30, 100, 0, 0))
    drain(fd, 2.0)
    return pid, fd


with tempfile.TemporaryDirectory(prefix="dvtm-mouse-") as directory:
    path = os.path.join(directory, "lines.txt")
    with open(path, "w") as output_file:
        output_file.write("".join("line%02d wxyz\n" % i for i in range(60)))

    pid, fd = start_dvtm()
    try:
        os.write(fd, ME.encode() + b" " + path.encode() + b"\r")
        drain(fd, 3.0)
        out = repaint(fd)
        check(b"(M)" not in out and b"(m)" not in out,
              "no indicators while the mouse is off and untracked")

        os.write(fd, MOD + b"t")       # tile: tag 1 starts fullscreen
        drain(fd, 0.8)
        os.write(fd, MOD + b"I")       # showinfo on: layout symbol joins
        out = repaint(fd)
        check(b" []= " in out,
              "the layout symbol shows once showinfo is on")
        check(b"(M)" not in out and b"(m)" not in out,
              "and no mouse indicators yet")

        os.write(fd, MOD + b"M")       # dvtm: mouse grabbing on
        out = repaint(fd)
        check(b"(M) []= " in out,
              "MOD+M shows the global (M) indicator beside the layout")
        check(b"(m)" not in out, "the window is not marked as tracking")

        os.write(fd, MX)               # me: mouse reporting on
        out = drain(fd, 1.5)
        check(b"[Mouse now ON]" in out, "me turned its reporting on")
        out = repaint(fd)
        check(b"(M) (m) []= " in out,
              "the tracking window gains (m) beside the global (M)")

        os.write(fd, MOD + b"M")       # dvtm: mouse grabbing off
        out = repaint(fd)
        check(b"(M)" not in out, "the global (M) leaves with the toggle")
        check(b"(m) []= " in out,
              "the tracking (m) holds with dvtm's grabbing off")

        os.write(fd, MX)               # me: mouse reporting off
        out = drain(fd, 1.5)
        check(b"[Mouse now OFF]" in out, "me turned its reporting off")
        out = repaint(fd)
        check(b"(m)" not in out and b"(M)" not in out,
              "both indicators are gone once the mouse is fully off")
    finally:
        os.kill(pid, 15)
        os.waitpid(pid, 0)

print("RESULT: all passed")
