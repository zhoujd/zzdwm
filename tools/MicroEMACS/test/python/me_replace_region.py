#!/usr/bin/env python3
"""Test query replacement within a Unicode region."""

import os
import subprocess
import tempfile

from common import drain, kill, spawn


def expect(condition, description):
    print(("PASS " if condition else "FAIL ") + description)
    if not condition:
        raise AssertionError(description)


def send(fd, keys, timeout=1.5):
    os.write(fd, keys)
    return drain(fd, timeout)


def save_quit(process, fd):
    output = b""
    output += send(fd, b"\x13")
    output += send(fd, b"\x18\x13")
    output += send(fd, b"\x18\x03")
    if process.poll() is None:
        output += send(fd, b"y\r")
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=5)
    os.close(fd)
    return process.returncode


def test_replace_before_dot(directory):
    path = os.path.join(directory, "region.txt")
    with open(path, "wb") as output:
        output.write(b"alpha beta alpha\n")

    process, fd = spawn(path)
    try:
        send(fd, b"")
        send(fd, b"\x1b ", 0.3)
        for _ in range(5):
            send(fd, b"\x1bOC", 0.1)
        send(fd, b"\x1bOB", 0.3)
        send(fd, b"\x18%")
        send(fd, b"alpha\r")
        send(fd, b"\xc3\xa9\r")
        send(fd, b"y", 0.3)
    except BaseException:
        kill(process, fd)
        raise

    returncode = save_quit(process, fd)
    with open(path, "rb") as region_file:
        content = region_file.read()

    expect(returncode == 0, "editor exits cleanly after region replacement")
    expect(content == b"\xc3\xa9 beta alpha\n",
           "Unicode replacement stays within the selected region")


def main():
    with tempfile.TemporaryDirectory(prefix="me-replace-region-") as directory:
        test_replace_before_dot(directory)
    print("RESULT: all passed")


if __name__ == "__main__":
    main()
