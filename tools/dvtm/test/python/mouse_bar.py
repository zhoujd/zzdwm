#!/usr/bin/env python3
"""A mouse report must not leak into the bar's key echo.  The wheel
arrives as getch()'s KEY_MOUSE, and the KEY_MOUSE branch used to reset
key_index without clearing the keys buffer, so drawbar printed the
KEY_MOUSE code through printw's %c - the two raw bytes "~Y" on this
ncurses - in the bar, where it stayed until the next key press.  The
bar keeps showing the window title instead.  Needs me in
../../../MicroEMACS for its toggle-mouse, and the wheel has to land
where the relayed X10 bytes are 33+93 ('~') and 33+56 ('Y'), as on a
screen the size below.  me starts at the buffer top, so the check
scrolls down first: the wheel-up relay itself must keep working."""


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
ROWS, COLS = 60, 120                   # wheel lands at (94, 57)


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


def check(cond, what):
    print(("PASS " if cond else "FAIL ") + what)
    if not cond:
        raise SystemExit(1)


if not os.access(DVTM, os.X_OK) or not os.access(ME, os.X_OK):
    print("SKIP: no dvtm or me binary; run make in tools/dvtm and "
          "build.sh in tools/MicroEMACS")
    raise SystemExit(0)


pid, fd = pty.fork()
if pid == 0:
    os.environ["TERM"] = "st-256color"
    os.environ["LANG"] = "en_US.UTF-8"
    os.execv(DVTM, [DVTM])
fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", ROWS, COLS, 0, 0))
drain(fd, 2.0)

with tempfile.TemporaryDirectory(prefix="dvtm-mouse-") as directory:
    path = os.path.join(directory, "lines.txt")
    with open(path, "w") as output_file:
        output_file.write("".join("line%03d wxyz\n" % i for i in range(400)))

    try:
        os.write(fd, MOD + b"s")       # bar on: the key echo's home
        drain(fd, 0.5)
        os.write(fd, ME.encode() + b" " + path.encode() + b"\r")
        drain(fd, 3.0)
        os.write(fd, MX)               # me: mouse reporting on
        drain(fd, 1.5)

        # X10 over the text, 1-based cell (94, 57): the relay re-encodes
        # the coordinates as 33-based bytes, '~' and 'Y'
        wheel_up = b"\x1b[M" + bytes([32 + 64, 32 + 93, 32 + 56])
        wheel_down = b"\x1b[M" + bytes([32 + 65, 32 + 93, 32 + 56])

        os.write(fd, wheel_down)       # off the buffer top, so the
        out = drain(fd, 1.0)           # later wheel-up has work to do
        check(b"~Y" not in out, "a wheel report leaves the bar alone")

        os.write(fd, wheel_up)
        out = drain(fd, 1.0)
        check(b"~Y" not in out, "the wheel-up that spells ~Y does too")

        os.write(fd, REDRAW)
        out = drain(fd, 1.0)
        check(b"~Y" not in out, "and the repaint shows no trace of it")

        # the relay still scrolls: down moved view off line000, up is
        # back — the topmost streamed line tells which
        def top(out):
            first = None
            i = 0
            while i < len(out):
                if out[i:i + 4] == b"line":
                    j = i + 4
                    while j < len(out) and out[j:j + 1].isdigit():
                        j += 1
                    # "lines.txt" in the mode line matches "line"
                    # alone; only numbered rows count
                    if j > i + 4 and (first is None or out[i:j] < first):
                        first = out[i:j]
                    i = j
                else:
                    i += 1
            return first

        check(top(out) == b"line000", "the wheel-up relay still scrolls me")
    finally:
        os.kill(pid, 15)
        os.waitpid(pid, 0)

print("RESULT: all passed")
