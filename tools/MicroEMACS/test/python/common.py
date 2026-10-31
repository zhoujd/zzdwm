"""Shared PTY harness for MicroEMACS end-to-end tests."""

import os
import pty
import select
import subprocess
import time


ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ME = os.path.join(ROOT, "me")


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


def spawn(path):
    """Start the editor on path with a fresh pseudo-terminal."""
    master, slave = pty.openpty()
    environment = dict(os.environ, TERM="xterm-256color", LANG="C.UTF-8")
    process = subprocess.Popen(
        [ME, path],
        stdin=slave,
        stdout=slave,
        stderr=slave,
        env=environment,
        close_fds=True,
    )
    os.close(slave)
    return process, master


def kill(process, fd):
    """Terminate a test editor process and close its PTY."""
    process.terminate()
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=5)
    os.close(fd)
