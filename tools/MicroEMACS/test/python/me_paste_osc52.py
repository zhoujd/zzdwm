#!/usr/bin/env python3
"""Test grouped bracketed paste and OSC 52 clipboard output."""

import os
import tempfile

from common import drain, kill, spawn


def expect(condition, description):
    print(("PASS " if condition else "FAIL ") + description)
    if not condition:
        raise AssertionError(description)


def fixture(directory, name, content=b""):
    path = os.path.join(directory, name)
    with open(path, "wb") as output:
        output.write(content)
    return path


def save_quit(process, fd):
    output = b""
    os.write(fd, b"\x18\x13")
    output += drain(fd)
    os.write(fd, b"\x18\x03")
    output += drain(fd, 2.0)
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=5)
    os.close(fd)
    return process.returncode, output


def test_bracketed_paste(directory):
    path = fixture(directory, "paste.txt")
    process, fd = spawn(path)
    try:
        output = drain(fd)
        expect(b"\x1b[?2004h" in output,
               "startup enables bracketed paste mode")

        payload = b"hello\tworld\rsecond line"
        os.write(fd, b"\x1b[200~" + payload + b"\x1b[201~")
        drain(fd)
    except BaseException:
        kill(process, fd)
        raise

    returncode, output = save_quit(process, fd)
    with open(path, "rb") as pasted_file:
        content = pasted_file.read()

    expect(returncode == 0, "editor exits cleanly after paste")
    expect(b"\x1b[?2004l" in output,
           "shutdown disables bracketed paste mode")
    expect(content == b"hello\tworld\nsecond line\n",
           "paste inserts text, tabs, and newlines as one unit")


def test_osc52_copy(directory):
    path = fixture(directory, "osc52.txt", b"abcdef\n")
    process, fd = spawn(path)
    try:
        output = drain(fd)
        os.write(fd, b"\x1b ")
        drain(fd, 0.3)
        for _ in range(3):
            os.write(fd, b"\x1bOC")
            drain(fd, 0.3)
        os.write(fd, b"\x1bW")
        output += drain(fd)
        sequences = [part for part in output.split(b"\x1b")
                     if b"]52;c;" in part]
        expect(b"\x1b]52;c;YWJj\x07" in output,
               "copy-region sends the OSC 52 clipboard sequence")
        expect(sequences != [], "OSC 52 output is terminal wire bytes")
    finally:
        kill(process, fd)


def main():
    with tempfile.TemporaryDirectory(prefix="me-paste-osc52-") as directory:
        test_bracketed_paste(directory)
        test_osc52_copy(directory)
    print("RESULT: all passed")


if __name__ == "__main__":
    main()
