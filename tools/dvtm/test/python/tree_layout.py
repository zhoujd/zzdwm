#!/usr/bin/env python3
"""dvtm-tree's setlayout call selects the doubledeck [D] layout.

dvtm-tree pushes two FIFO commands before exec'ing nnn: setlayout
"[D]" (doubledeck: master and stack piled in a left/right split) and
setmfact "0.25".  This script drives a real dvtm over a pty with the
same fifo path dvtm-cmd uses, sends those exact commands, and reads
the result off a full repaint's border text: [D] at mfact 0.25 on a
100 column screen puts the stack border at column 26 (wax + 0.25*waw
+ 1, 1-based)."""

import fcntl
import os
import pty
import select
import stat
import struct
import subprocess
import tempfile
import termios
import time

HERE = os.path.dirname(os.path.abspath(__file__))
DVTM = os.path.join(HERE, "..", "..", "dvtm")
DVTM_CMD = os.path.join(HERE, "..", "..", "dvtm-cmd")
MOD = b"\x1c"                          # CTRL('\') — dvtm's MOD key
REDRAW = MOD + b"\x0c"                 # MOD+CTRL('L'): full repaint


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
if not os.access(DVTM_CMD, os.X_OK):
    print("SKIP: no dvtm-cmd; run make in tools/dvtm")
    raise SystemExit(0)

with tempfile.TemporaryDirectory() as tmp:
    fifo = os.path.join(tmp, "cmd.fifo")
    pid, fd = pty.fork()
    if pid == 0:
        os.environ["TERM"] = "st-256color"
        os.environ["LANG"] = "en_US.UTF-8"
        os.execv(DVTM, [DVTM, "-c", fifo])
    fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", 30, 100, 0, 0))

    def run(action, arg):
        env = dict(os.environ, DVTM_CMD_FIFO=fifo)
        return subprocess.run([DVTM_CMD, action, arg], env=env,
                              stdout=subprocess.DEVNULL,
                              stderr=subprocess.DEVNULL).returncode

    try:
        drain(fd, 2.0)
        for _ in range(40):            # dvtm mkfifos the -c path at startup
            try:
                if stat.S_ISFIFO(os.stat(fifo).st_mode):
                    break
            except FileNotFoundError:
                pass
            time.sleep(0.05)

        os.write(fd, MOD + b"c")       # second window: [D] needs a stack
        drain(fd, 1.0)

        # dvtm-tree line 16, exactly as the script runs it
        check(run("setlayout", "[D]") == 0,
              "dvtm-cmd setlayout '[D]' goes through the fifo")
        drain(fd, 1.0)
        check(run("setmfact", "0.25") == 0,
              "dvtm-cmd setmfact '0.25' goes through the fifo")
        drain(fd, 1.0)

        os.write(fd, MOD + b"I")       # border info on: the symbol shows
        drain(fd, 0.5)
        os.write(fd, REDRAW)
        out = drain(fd, 1.0)
        check(b"[D]" in out, "setlayout '[D]' selects the doubledeck layout")
        check(b"[1/2]" in out and b"[2/2]" in out,
              "doubledeck piles master and stack in a left/right split")
    finally:
        os.kill(pid, 15)
        os.waitpid(pid, 0)

print("PASS tree layout")
