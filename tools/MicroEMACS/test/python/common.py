"""Shared pty harness for the me_* end-to-end scripts.

Each test drives the real ./me binary in a pseudo-terminal: bytes go in
through the master side, everything the editor writes comes back out,
and the tests assert on both.  The scripts are not part of the C test
suite (build.sh -b test); they need a built Linux ./me and run with
plain python3.
"""

import os
import pty
import select
import subprocess
import sys
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ME = os.path.join(ROOT, "me")

fails = []


def check(cond, what):
    print(("PASS " if cond else "FAIL ") + what)
    if not cond:
        fails.append(what)


def drain(fd, timeout=1.5, settle=0.4):
    """Read from the pty until it goes quiet for `timeout` seconds."""
    out = b""
    end = time.time() + timeout
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], settle)
        if not r:
            continue
        try:
            chunk = os.read(fd, 65536)
        except OSError:
            break
        if not chunk:
            break
        out += chunk
        end = time.time() + timeout
    return out


def spawn(path, mouse=True, term="xterm-256color"):
    """Start ./me on a file in a fresh pty.  Returns (proc, master_fd)."""
    master, slave = pty.openpty()
    env = dict(os.environ, TERM=term, LANG="en_US.UTF-8")
    argv = [ME] + (["-m"] if mouse else []) + [path]
    p = subprocess.Popen(argv, stdin=slave, stdout=slave,
                         stderr=slave, env=env, close_fds=True)
    os.close(slave)
    return p, master


def save_quit(p, fd):
    """C-X C-S then C-X C-C; kill if the editor hangs.  Returns rc."""
    os.write(fd, b"\x18\x13")            # C-X C-S file-save
    drain(fd)
    os.write(fd, b"\x18\x03")            # C-X C-C quit
    drain(fd, 2.0)
    try:
        p.wait(timeout=5)
    except subprocess.TimeoutExpired:
        p.kill()
    return p.returncode


def kill(p):
    p.terminate()
    try:
        p.wait(timeout=5)
    except subprocess.TimeoutExpired:
        p.kill()


def fixture(name, content=b""):
    """Write a scratch file next to this script; returns its path."""
    path = os.path.join(tempfile.mkdtemp(prefix="me_e2e_"), name)
    with open(path, "wb") as f:
        f.write(content)
    return path


def finish():
    print("RESULT:", "all passed" if not fails else "%d FAILURES" % len(fails))
    sys.exit(1 if fails else 0)
