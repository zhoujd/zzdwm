#!/usr/bin/env python3
"""The mouse modes mouse_setup asks of the outer terminal.  ncurses
arms what its XM string knows - press reporting, mode 1000, on simple
terminfo entries - and dvtm adds the 1002 drag-motion tracking relay
clients need on top.  The stream matters to terminals of st's kind,
which clear every mouse mode on any of 1000/1002/1003, not just their
own: a 1002l sent to drop motion tracking after the 1000h ncurses
armed would take the 1000 with it, and click and wheel would go dead
in the outer terminal even with MOD+M on - while the pty harness,
injecting reports straight in, would never notice.  So the test
asserts on the mode sequences themselves, at each toggle."""


import fcntl
import os
import pty
import re
import select
import struct
import termios
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
DVTM = os.path.join(HERE, "..", "..", "dvtm")
ME = os.path.join(HERE, "..", "..", "..", "MicroEMACS", "me")
MOD = b"\x1c"                          # CTRL('\') — dvtm's MOD key
MX = b"\x1bxtoggle-mouse\r"            # me: toggle-mouse

ON_1000 = b"\x1b[?1000h"
OFF_1000 = b"\x1b[?1000l"
ON_1002 = b"\x1b[?1002h"
OFF_1002 = b"\x1b[?1002l"


def drain(fd, timeout=1.2):
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
fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", 30, 100, 0, 0))
drain(fd, 2.0)

with tempfile.TemporaryDirectory(prefix="dvtm-modes-") as directory:
    path = os.path.join(directory, "lines.txt")
    with open(path, "w") as output_file:
        output_file.write("".join("line%02d wxyz\n" % i for i in range(60)))

    os.write(fd, ME.encode() + b" " + path.encode() + b"\r")
    drain(fd, 3.0)

    def modes(keys):
        """Toggle something and return the mode sequences it emitted."""
        os.write(fd, keys)
        return drain(fd, 1.2)

    try:
        out = modes(MOD + b"M")        # dvtm: mouse grabbing on
        check(ON_1000 in out and OFF_1002 not in out,
              "MOD+M arms press reporting without a mode-clearing 1002l")

        out = modes(MOD + b"M")        # dvtm: mouse grabbing off
        check(OFF_1000 in out and ON_1000 not in out and ON_1002 not in out,
              "nothing left armed once the mouse is fully off")

        out = modes(MX)                # me: mouse reporting on
        check(ON_1002 in out and OFF_1002 not in out,
              "a tracking client arms drag-motion tracking for the relay")

        out = modes(MOD + b"M")        # dvtm: grabbing on with tracking
        check(ON_1000 in out and OFF_1002 not in out,
              "local bindings re-assert press reporting, again no 1002l")

        out = modes(MOD + b"M")        # dvtm: grabbing off, relay returns
        check(ON_1002 in out and OFF_1002 not in out,
              "the relay's drag-motion tracking comes back")

        out = modes(MX)                # me: mouse reporting off
        check(OFF_1000 in out and ON_1000 not in out and ON_1002 not in out,
              "the last relay ends with the motion tracking undone")
    finally:
        os.kill(pid, 15)
        os.waitpid(pid, 0)

print("RESULT: all passed")
