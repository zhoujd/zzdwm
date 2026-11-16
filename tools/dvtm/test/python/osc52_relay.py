#!/usr/bin/env python3
"""OSC 52 relay: a client sets the host's clipboard - me's M-w does -
by writing ESC]52;c;<base64>BEL to its vt, and vt used to swallow the
sequence as an unknown OSC, so the selection never reached the outer
terminal.  dvtm now writes sets through to the host, and keeps queries
(a '?' payload, which would have the answer arrive as input) to itself.

The sequence rides a buffer of its own, sized to me's OSC52_MAX_BYTES:
a whole-file selection past ebuf's 8KB still relays, while base64
beyond me's 1MB cap is dropped.

Driven through a pty, where the "host terminal" is the test itself:
the relay shows as dvtm emitting the sequence.  A shell printf builds
its ESC at runtime, so the tty's echo of the command line carries
backslashes, not the escape the assertions look for."""


import base64
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
MARK = b"\x1b "                        # M-space: me's set-mark alias
DOWN = b"\x1bOB"                       # application-mode arrow, what me's
                                       # st-256color terminfo expects
MW = b"\x1bw"                          # M-w: me's copy region
QUITEME = b"\x18\x03"                  # C-x C-c


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


if not os.access(DVTM, os.X_OK):
    print("SKIP: no dvtm binary; run make in tools/dvtm")
    raise SystemExit(0)
if not os.access(ME, os.X_OK):
    print("SKIP: no me binary; build it in tools/MicroEMACS")
    raise SystemExit(0)


def copy_region(out, what):
    """Pull the first OSC 52 out of dvtm's output, decode, check."""
    i = out.find(b"\x1b]52;c;")
    check(i != -1, what)
    j = out.find(b"\a", i)
    check(j != -1, what + " (terminated)")
    try:
        return base64.b64decode(out[i + len(b"\x1b]52;c;"):j])
    except Exception:
        check(False, what + " (valid base64)")
        return b""


os.environ["DVTM_EDITOR"] = ME         # MOD+e edits the window with me
os.environ["PATH"] = os.path.dirname(DVTM) + os.pathsep + os.environ["PATH"]

with tempfile.TemporaryDirectory(prefix="dvtm-osc52-") as directory:
    path = os.path.join(directory, "lines.txt")
    with open(path, "w") as output_file:
        output_file.write("".join("line%02d wxyz\n" % i for i in range(60)))

    pid, fd = pty.fork()
    if pid == 0:
        os.environ["TERM"] = "st-256color"
        os.environ["LANG"] = "en_US.UTF-8"
        os.execv(DVTM, [DVTM])
    fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", 30, 100, 0, 0))

    try:
        drain(fd, 2.0)
        os.write(fd, b"printf '\\033]52;c;dGVzdA==\\a'\r")
        out = drain(fd, 1.5)
        check(b"\x1b]52;c;dGVzdA==\a" in out,
              "a shell's OSC 52 set goes through to the host")

        os.write(fd, b"printf '\\033]52;c;?\\a'\r")
        out = drain(fd, 1.5)
        check(b"\x1b]52;c;?" not in out,
              "an OSC 52 clipboard query is not relayed")

        os.write(fd, MOD + b"c")       # a fresh window with known content
        drain(fd, 1.0)
        os.write(fd, b"clear; printf 'ZZ1\\nZZ2\\nZZ3\\n'\r")
        drain(fd, 1.0)
        os.write(fd, MOD + b"e")       # the editor window: me, as MOD+e runs it
        drain(fd, 3.0)
        os.write(fd, MARK + DOWN + DOWN + MW)
        out = drain(fd, 1.5)
        text = copy_region(out, "me in the MOD+e editor sets the clipboard")
        check(any(x in text for x in (b"ZZ1", b"ZZ2", b"ZZ3")),
              "the relayed clipboard holds the window's selected lines")
        os.write(fd, QUITEME)
        drain(fd, 1.0)

        os.write(fd, MOD + b"c")       # me as a plain client window
        drain(fd, 1.0)
        os.write(fd, ME.encode() + b" " + path.encode() + b"\r")
        drain(fd, 3.0)
        os.write(fd, MARK + DOWN + DOWN + MW)
        out = drain(fd, 1.5)
        text = copy_region(out, "me as a client sets the clipboard")
        check(b"line00" in text and b"line01" in text,
              "the relayed clipboard holds the file's selected lines")

        os.write(fd, QUITEME)
        drain(fd, 1.0)
        os.write(fd, MOD + b"c")       # a file big enough to pass ebuf
        drain(fd, 1.0)
        path = os.path.join(directory, "big.txt")
        with open(path, "w") as output_file:
            output_file.write("".join("bigline%04d wxyz\n" % i for i in range(2000)))
        os.write(fd, ME.encode() + b" " + path.encode() + b"\r")
        drain(fd, 3.0)
        os.write(fd, b"\x1b<")         # top of the buffer
        drain(fd, 0.3)
        os.write(fd, MARK)
        drain(fd, 0.3)
        os.write(fd, b"\x1b>")         # bottom: mark to end selects all
        drain(fd, 0.3)
        os.write(fd, MW)
        out = drain(fd, 3.0)
        i = out.find(b"\x1b]52;c;")
        check(i != -1, "a selection past ebuf's size still relays")
        j = out.find(b"\a", i)
        check(j != -1 and j - i > 8192, "the base64 crossed the old limit")
        text = base64.b64decode(out[i + len(b"\x1b]52;c;"):j])
        check(b"bigline0000" in text and b"bigline1999" in text,
              "the relayed clipboard holds the whole file")
        os.write(fd, QUITEME)
        drain(fd, 1.0)

        # 1,333,400 base64 chars: past me's 1MB cap, dropped not relayed
        os.write(fd, b"python3 -c \"import sys;"
                      b"sys.stdout.write('\\033]52;c;'+('A'*1333400)+'\\a')\"\r")
        out = drain(fd, 4.0)
        check(b"\x1b]52;c;" not in out,
              "base64 past me's cap is dropped, not relayed")
    finally:
        os.kill(pid, 15)
        os.waitpid(pid, 0)

print("PASS OSC 52 relay")
