"""Shared PTY harness for MicroEMACS end-to-end tests.

Each test drives the real ./me binary in a pseudo-terminal: raw bytes
(mouse reports, key sequences) go in through the master side, everything
the editor writes comes back out, and the tests assert on both.  The
scripts are not part of the C suite (build.sh -b test); they need a
built Linux ./me and run with plain python3.
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
    """Read terminal output until it stays quiet for timeout seconds."""
    output = b""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        readable, _, _ = select.select([fd], [], [], settle)
        if not readable:
            continue
        try:
            chunk = os.read(fd, 65536)
        except OSError:
            break
        if not chunk:
            break
        output += chunk
        deadline = time.monotonic() + timeout
    return output


def spawn(path, mouse=False, term="xterm-256color"):
    """Start ./me on a file with a fresh pseudo-terminal."""
    master, slave = pty.openpty()
    environment = dict(os.environ, TERM=term, LANG="C.UTF-8")
    argv = [ME] + (["-m"] if mouse else []) + [path]
    process = subprocess.Popen(
        argv,
        stdin=slave,
        stdout=slave,
        stderr=slave,
        env=environment,
        close_fds=True,
    )
    os.close(slave)
    return process, master


def save_quit(process, fd):
    """C-X C-S then C-X C-C; kill if the editor hangs.  Returns rc."""
    os.write(fd, b"\x18\x13")            # C-X C-S file-save
    drain(fd)
    os.write(fd, b"\x18\x03")            # C-X C-C quit
    drain(fd, 2.0)
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=5)
    os.close(fd)
    return process.returncode


def kill(process, fd):
    """Terminate a test editor process and close its PTY."""
    process.terminate()
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=5)
    os.close(fd)


def fixture(name, content=b""):
    """Write a scratch file in a fresh temp directory; returns its path."""
    path = os.path.join(tempfile.mkdtemp(prefix="me_e2e_"), name)
    with open(path, "wb") as f:
        f.write(content)
    return path


def finish():
    print("RESULT:", "all passed" if not fails else "%d FAILURES" % len(fails))
    sys.exit(1 if fails else 0)
