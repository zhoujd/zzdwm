#!/usr/bin/env python3
"""Editing a binary file through hexl, as GNU's hexl mode does: the
buffer holds the dump itself, C-X C-H reads a file into it (and "me
-H" reads one on startup), two hex digits typed in the hex area write
a byte, a character typed in the ASCII area writes its own value, and
C-X C-S parses the hex back out and writes the file.  A line that is
no longer a dump line stops the save with the file untouched."""


import os
import tempfile

from common import drain, kill, save_quit, spawn

HEXL = b"\x18\x08"                   # C-X C-H hexl-find-file
SAVE = b"\x18\x13"                   # C-X C-S file-save
GOTO = b"\x1bxhexl-goto-address\r"
BOL = b"\x01"                        # C-A
RIGHT = b"\x1bOC"                    # application-mode arrow
BACKSPACE = b"\x7f"
UNDO = b"\x1f"                       # C-_

DATA = (b"Hello, World!\n\x00\x01"   # line 0: text, newline, two unprintables
        + b"\xfe\xffABCDEFGHIJKLMN"  # line 1: high bytes, then text
        + b"M\x7f\x1f Z")            # line 2: short, padded with blanks


def expect(condition, description):
    print(("PASS " if condition else "FAIL ") + description)
    if not condition:
        raise AssertionError(description)


def send(fd, keys, timeout=1.0):
    os.write(fd, keys)
    return drain(fd, timeout)


def content(path):
    with open(path, "rb") as fixture_file:
        return fixture_file.read()


def main():
    with tempfile.TemporaryDirectory(prefix="me-hexl-") as directory:
        start = os.path.join(directory, "start.txt")
        with open(start, "wb") as output_file:
            output_file.write(b"text\n")
        path = os.path.join(directory, "blob.bin")
        with open(path, "wb") as output_file:
            output_file.write(DATA)
        model = bytearray(DATA)

        process, fd = spawn(start)
        try:
            out = send(fd, b"", 1.5)
            out += send(fd, HEXL)
            out += send(fd, path.encode() + b"\r", 1.5)
            expect(b"[Read 37 bytes]" in out, "the file reads as 37 bytes")
            expect(b"00000000: 48 65 6c 6c 6f 2c 20 57"
                   b"  6f 72 6c 64 21 0a 00 01" in out,
                   "line 0 dumps in the GNU layout")
            expect(b"Hello, World!..." in out,
                   "the ASCII area dots the unprintables")
            expect(b"00000010: fe ff 41 42 43 44 45 46  47 48 49 4a 4b 4c"
                   b" 4d 4e" in out, "line 1 dumps in two groups of eight")
            expect(b"00000020: 4d 7f 1f 20 5a" in out,
                   "the short last line dumps left aligned")
            expect(b"(hexl)" in out, "the mode line shows the hexl mode")

            # Two hex digits at dot write the byte, and the save
            # writes 37 raw bytes with only that byte changed.
            out = send(fd, b"41")
            out += send(fd, SAVE)
            expect(b"[Wrote 37 bytes]" in out, "the save writes the bytes")
            model[0] = 0x41
            expect(content(path) == bytes(model),
                   "byte 0 was written through the hex area")

            # A character in the ASCII area writes its own value.
            send(fd, GOTO + b"1\r")
            send(fd, BOL + RIGHT * 61)       # the ASCII column of byte 1
            out = send(fd, b"Z")
            out += send(fd, SAVE)
            model[1] = 0x5a
            expect(content(path) == bytes(model),
                   "byte 1 was written through the ASCII area")

            # TAB steps to the next byte before anything is typed.
            send(fd, GOTO + b"3\r")
            send(fd, b"\t")
            out = send(fd, b"59")
            out += send(fd, SAVE)
            model[4] = 0x59
            expect(content(path) == bytes(model),
                   "TAB stepped the edit to the next byte")

            # An address goes straight to the byte, hex with 0x.
            send(fd, GOTO + b"0x20\r")
            out = send(fd, b"ff")
            out += send(fd, SAVE)
            model[32] = 0xff
            expect(content(path) == bytes(model),
                   "the address landed on byte 0x20")

            # The blank slots past the file's end take no bytes.
            send(fd, GOTO + b"0x25\r")
            out = send(fd, b"41")
            expect(b"[That byte is past the end of the file]" in out,
                   "a blank slot refuses a byte")
            expect(len(content(path)) == 37, "the file did not grow")

            # A non-hex character is refused in the hex area.
            send(fd, GOTO + b"0x10\r")
            out = send(fd, b"z")
            expect(b"[Hex digits here" in out,
                   "the hex area refuses a plain character")

            # A line damaged by a command that is not hexl aware
            # stops the save, and undo puts the dump back.
            send(fd, BACKSPACE)
            out = send(fd, SAVE)
            expect(b"is not a hexl dump line" in out,
                   "a damaged line stops the save")
            expect(content(path) == bytes(model),
                   "the damaged save wrote nothing")
            out = send(fd, UNDO)
            expect(b"fe ff 41 42" in out, "undo restored the dump line")
            send(fd, SAVE)               # a no-op: the buffer is
                                         # back at its saved state
            expect(content(path) == bytes(model),
                   "the undo left the file as the model")

            # The file already dumped is revisited, not re-read.
            send(fd, b"\x18\x06")            # C-X C-F the dump away
            out = send(fd, start.encode() + b"\r", 1.5)
            out += send(fd, HEXL)
            out += send(fd, path.encode() + b"\r", 1.5)
            expect(b"[Old buffer]" in out, "the dump is revisited")
        finally:
            save_quit(process, fd)

        # A second copy, dumped on startup by the -H option, edits and
        # saves as a dump buffer too.
        copy = os.path.join(directory, "copy.bin")
        with open(copy, "wb") as output_file:
            output_file.write(DATA)
        model = bytearray(DATA)
        process, fd = spawn(copy, args=["-H"])
        try:
            out = send(fd, b"", 1.5)
            expect(b"[Read 37 bytes]" in out, "-H read the file on startup")
            expect(b"(hexl)" in out, "-H shows the hexl mode line")
            out = send(fd, b"41")
            out += send(fd, SAVE)
            model[0] = 0x41
            expect(b"[Wrote 37 bytes]" in out, "the -H buffer saves bytes")
            expect(content(copy) == bytes(model),
                   "the -H buffer wrote the model bytes")
        finally:
            save_quit(process, fd)

    print("RESULT: all passed")


if __name__ == "__main__":
    main()
