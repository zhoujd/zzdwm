#!/usr/bin/env python3
"""Saving several buffers at once through C-X S, as GNU Emacs's C-x s
does: each changed buffer that is editing a file is offered with a
yes-or-no prompt, y saving it and n leaving it changed, and C-U C-X S
saves them all without asking.  The buffer list runs newest first, so
a file visited after the one the editor started on is asked first."""


import os
import tempfile

from common import drain, kill, save_quit, spawn

VISIT = b"\x18\x06"                 # C-X C-F visit-file
SAVE_ALL = b"\x18S"                 # C-X S file-save-all
SAVE_ALL_ARG = b"\x15\x18S"         # C-U C-X S
SAVE = b"\x18\x13"                  # C-X C-S file-save


def expect(condition, description):
    print(("PASS " if condition else "FAIL ") + description)
    if not condition:
        raise AssertionError(description)


def send(fd, keys, timeout=1.0):
    os.write(fd, keys)
    return drain(fd, timeout)


def two_files(directory):
    """Start on a.txt, dirty both it and a freshly visited b.txt, and
    return the editor with b.txt as the current buffer."""
    path_a = os.path.join(directory, "a.txt")
    path_b = os.path.join(directory, "b.txt")
    for path in (path_a, path_b):
        with open(path, "wb") as output_file:
            output_file.write(b"one\ntwo\n")
    process, fd = spawn(path_a)
    try:
        startup = send(fd, b"", 1.5)
        send(fd, b"A")                          # dirty a.txt
        send(fd, VISIT)
        send(fd, path_b.encode() + b"\r", 1.5)  # visit b.txt
        send(fd, b"B")                          # dirty b.txt
    except BaseException:
        kill(process, fd)
        raise
    return (process, fd), path_a, path_b


def main():
    with tempfile.TemporaryDirectory(prefix="me-file-") as directory:
        # With an argument, every changed buffer is saved without a
        # prompt, and both files carry their edits.
        (process, fd), path_a, path_b = two_files(directory)
        try:
            saved = send(fd, SAVE_ALL_ARG)
            expect(b"[2 files saved]" in saved,
                   "an argument saves both buffers")
        finally:
            save_quit(process, fd)
        for path, edit in ((path_a, b"Aone"), (path_b, b"Bone")):
            with open(path, "rb") as fixture_file:
                content = fixture_file.read()
            expect(content == edit + b"\ntwo\n",
                   "%s was written (%r)" % (os.path.basename(path), content))

        # Without one, each buffer is offered: b.txt first, as the
        # buffer list runs newest first.  Save it, decline a.txt, and
        # only the answered-for file is written.
        (process, fd), path_a, path_b = two_files(directory)
        try:
            tour = send(fd, SAVE_ALL)
            expect(b"Save file" in tour and b"b.txt [y/n]" in tour,
                   "the newest buffer is offered first")
            tour += send(fd, b"y\r")
            expect(b"a.txt [y/n]" in tour,
                   "the older buffer is offered next")
            done = send(fd, b"n\r")
            expect(b"[1 file saved]" in done,
                   "the tour reports what it wrote")
        finally:
            kill(process, fd)
        with open(path_b, "rb") as fixture_file:
            expect(fixture_file.read() == b"Bone\ntwo\n",
                   "the answered-for file was written")
        with open(path_a, "rb") as fixture_file:
            expect(fixture_file.read() == b"one\ntwo\n",
                   "the declined file was left alone")

        # C-G stops the tour with the buffers left as answered so
        # far; the editor carries on, and the ordinary save still
        # works on the buffer the tour stopped at.
        (process, fd), path_a, path_b = two_files(directory)
        try:
            send(fd, SAVE_ALL)
            tour = send(fd, b"y\r")              # save b.txt; a.txt asked
            expect(b"a.txt [y/n]" in tour,
                   "the tour stopped at the older buffer")
            aborted = send(fd, b"\x07")          # C-G ends it
            expect(b"[Wrote" not in aborted,
                   "the aborted tour wrote nothing more")
            send(fd, VISIT)                      # back to a.txt
            send(fd, path_a.encode() + b"\r", 1.5)
            saved = send(fd, SAVE)               # C-X C-S still works
            expect(b"[y/n]" not in saved,
                   "the editor carries on after the abort")
        finally:
            save_quit(process, fd)
        with open(path_b, "rb") as fixture_file:
            expect(fixture_file.read() == b"Bone\ntwo\n",
                   "the buffer saved before the abort was written")
        with open(path_a, "rb") as fixture_file:
            expect(fixture_file.read() == b"Aone\ntwo\n",
                   "the buffer stopped at was saved by hand")

        # With nothing changed, there is nothing to ask.
        path = os.path.join(directory, "clean.txt")
        with open(path, "wb") as output_file:
            output_file.write(b"one\ntwo\n")
        process, fd = spawn(path)
        try:
            out = send(fd, b"", 1.5)
            out += send(fd, SAVE_ALL)
            expect(b"[No files need saving]" in out,
                   "a clean editor has nothing to save")
        finally:
            kill(process, fd)

    print("RESULT: all passed")


if __name__ == "__main__":
    main()
