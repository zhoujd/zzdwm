#!/usr/bin/env python3
"""The `send` fifo command: `dvtm-cmd send 'keys'` types keys straight
into the focused window, no focus change, no new window.  The fifo
parser once ended a command at any newline — its `case '\\n'` label
sat inside `if (!quote)` (dvtm.c) — so Enter could only ride along as
a raw CR; a quoted newline now stays in the argument, and unquoted
arguments join into one line with the spaces between them."""


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

        check(cmd_send("echo ZZT2\n") == 0, "a quoted newline goes through")
        out = drain(fd, 1.0)
        check(out.count(b"ZZT2") >= 2,
              "the quoted newline presses Enter too: "
              "the line types and then runs")

        wfd = os.open(fifo, os.O_WRONLY)
        os.write(wfd, b"send echo ZZT3\n")
        os.close(wfd)
        check(b"echo ZZT3" in drain(fd, 1.0),
              "unquoted arguments join into one typed line, space and all")
        cmd_send("\r")
        check(b"ZZT3" in drain(fd, 1.0),
              "the joined line runs once Enter follows")
    finally:
        os.kill(pid, 15)
        os.waitpid(pid, 0)

print("PASS send command")
