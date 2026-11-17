#!/usr/bin/env python3
"""The `send` fifo command: `dvtm-cmd send 'keys'` types keys straight
into the focused window, no focus change, no new window.  The fifo
parser's `case '\\n'` sits inside `if (!quote)` (dvtm.c), so a raw
newline ends the command even inside quotes and cannot be delivered —
Enter rides along as a raw CR instead, which the parser leaves in the
argument.  That is the command's main use, typing a command line and
running it, so both halves get asserted here."""


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

    def cmd_send(keys):
        env = dict(os.environ, DVTM_CMD_FIFO=fifo)
        return subprocess.run([DVTM_CMD, "send", keys], env=env,
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

        check(cmd_send("echo ZZT1") == 0, "dvtm-cmd send writes the fifo")
        check(b"ZZT1" in drain(fd, 1.0),
              "send types the keys into the focused window")

        check(cmd_send("\r") == 0, "dvtm-cmd send delivers a raw CR")
        out = drain(fd, 1.0)
        check(b"ZZT1" in out and b"echo ZZT1" not in out,
              "the raw CR presses Enter: the pending line runs, "
              "only its output streams")
    finally:
        os.kill(pid, 15)
        os.waitpid(pid, 0)

print("PASS send command")
