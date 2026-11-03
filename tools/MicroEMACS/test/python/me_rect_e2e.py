#!/usr/bin/env python3
"""Rectangle editing through the C-X r prefix: anchor one corner,
put point on the other, and copy/kill/yank/clear/open the column
range between them.  The rectangles are cut on the display-cell grid,
so a tab counts as the cells it renders as: clearing replaces it with
that many spaces, and a kill whose edge falls inside it expands it
first.  Short lines read as blanks when text is copied and are padded
out by yank.  One undo reverses a whole kill."""

import os
import tempfile

from common import drain, kill, save_quit, spawn

UP, DOWN, RIGHT, LEFT = b"\x1bOA", b"\x1bOB", b"\x1bOC", b"\x1bOD"
ANCHOR = b"\x18r\x00"             # C-X r C-@ (C-Space sends the same NUL)
COPY = b"\x18r\x1bW"             # C-X r M-W
KILL = b"\x18r\x17"              # C-X r C-w
CLEAR = b"\x18r\x03"             # C-X r C-c
OPEN = b"\x18r\x0f"              # C-X r C-o
STRING = b"\x18r\x14"            # C-X r C-t
YANK = b"\x18r\x19"              # C-X r C-y
UNDO = b"\x18U"
REDO = b"\x15\x18U"                 # C-U C-X U
READONLY = b"\x18\x11"
KILLLINE = b"\x0b"


def expect(condition, description):
    print(("PASS " if condition else "FAIL ") + description)
    if not condition:
        raise AssertionError(description)


def send(fd, keys, timeout=1.0):
    os.write(fd, keys)
    return drain(fd, timeout)


def run(directory, name, content, keys, mouse=False, undo=False):
    """Run me over a fixture, feed KEYS, save, and return what the
    file holds plus the terminal output.  With UNDO, the buffer is
    written out under a new name with C-X C-W instead: undo restores
    the change flag too, so a fully undone buffer reads as unmodified
    and C-X C-S would write nothing while the file keeps the fixture
    and the check would pass on the wrong thing."""
    path = os.path.join(directory, name)
    with open(path, "wb") as output_file:
        output_file.write(content)
    process, fd = spawn(path, mouse=mouse)
    try:
        terminal = send(fd, b"", 1.5)
        for chunk in keys:
            terminal += send(fd, chunk)
        if undo:
            written = os.path.join(directory, name + ".out")
            terminal += send(fd, b"\x18\x17")    # C-X C-W write-as
            terminal += send(fd, written.encode() + b"\r")
    except BaseException:
        kill(process, fd)
        raise
    save_quit(process, fd)
    with open(path if not undo else path + ".out", "rb") as fixture_file:
        return fixture_file.read(), terminal


def main():
    with tempfile.TemporaryDirectory(prefix="me-rect-") as directory:
        # Kill anchored below and right of point, then yank at a
        # column past the end of a short line: the killed text comes
        # back column-aligned as an insert, the short line padded out
        # with spaces.
        content, _ = run(
            directory, "rect_a.txt", b"abcdef ghij\nxy\n123456 zzz\n",
            [RIGHT * 6, DOWN, DOWN, ANCHOR,           # anchor (2,6)
             LEFT * 4, UP, UP, KILL,                  # point (0,2): kill
             RIGHT * 4, YANK])                        # yank at (0,6)
        expect(content == b"ab ghicdefj\nxy        \n12 zzz3456\n",
               "kill and yank move the rectangle (%r)" % content)

        # Clear blanks in place on the cell grid: the tab becomes the
        # eight spaces it renders as, and nothing after it moves.
        content, _ = run(
            directory, "rect_b.txt", b"\tabbed text\nplain line\n",
            [ANCHOR, RIGHT, DOWN, CLEAR])             # cols [0,8)
        expect(content == b"        abbed text\n        ne\n",
               "clear replaces cells with spaces (%r)" % content)

        # Open shifts text right by the rectangle's width; the edge
        # column inside the tab opens before it.
        content, _ = run(
            directory, "rect_c.txt", b"\tabc\nplain\n",
            [RIGHT, ANCHOR, DOWN, LEFT, OPEN])        # cols [4,8)
        expect(content == b"    \tabc\nplai    n\n",
               "open inserts a column of blanks (%r)" % content)

        # Open shifts text right by the rectangle's width; a C-U
        # argument gives the width in place of the rectangle's own.
        # C-N preserves the character offset, so the moves here keep
        # track of it: anchor (0,1), dot (1,3).
        content, _ = run(
            directory, "rect_c2.txt", b"abcd\nwxyz\n",
            [RIGHT, ANCHOR, DOWN, RIGHT * 2,
             b"\x15", b"3",                        # C-U 3
             OPEN])                                # cols [1,3), width 3
        expect(content == b"a   bcd\nw   xyz\n",
               "open takes the width from the argument (%r)" % content)

        # String-rectangle: each line's slice goes and the string is
        # inserted at the left edge; a line ending at the left edge
        # ("x") is not padded and just gets the string.  Anchor (0,1),
        # dot (2,3); C-N from column 1 keeps offset 1, so two more
        # rights reach offset 3.
        content, _ = run(
            directory, "rect_g1.txt", b"abcd\nx\nlonger line\n",
            [RIGHT, ANCHOR, DOWN * 2, RIGHT * 2, STRING, b"XX\r"])
        expect(content == b"aXXd\nxXX\nlXXger line\n",
               "string replaces each line's slice (%r)" % content)
        content, _ = run(
            directory, "rect_g2.txt", b"abcd\nx\nlonger line\n",
            [RIGHT, ANCHOR, DOWN * 2, RIGHT * 2, STRING, b"XX\r", UNDO],
            undo=True)
        expect(content == b"abcd\nx\nlonger line\n",
               "one undo restores the whole string (%r)" % content)

        # A kill whose right edge falls inside a tab expands the tab
        # to spaces first, so only the part of it inside the
        # rectangle goes.
        content, _ = run(
            directory, "rect_d1.txt", b"\tabc\nplain\n",
            [ANCHOR, DOWN, RIGHT * 4, KILL])          # cols [0,4)
        expect(content == b"    abc\nn\n",
               "kill splits a straddling tab (%r)" % content)

        # One undo undoes the whole kill, tab and all; the same holds
        # for the other operators, and undos peel back one rectangle
        # command at a time.
        content, _ = run(
            directory, "rect_d2.txt", b"\tabc\nplain\n",
            [ANCHOR, DOWN, RIGHT * 4, KILL, UNDO], undo=True)
        expect(content == b"\tabc\nplain\n",
               "one undo restores the whole kill (%r)" % content)
        # A taller rectangle: undo restores every line, each at its
        # own column, where replay once chained all the restored text
        # onto the rectangle's last line; redo kills it again.
        content, _ = run(
            directory, "rect_d3.txt", b"abcdef\nghijkl\nmnopqr\nstuvwx\n",
            [RIGHT * 2, ANCHOR, DOWN * 3, RIGHT * 2, KILL, UNDO, REDO],
            undo=True)
        expect(content == b"abef\nghkl\nmnqr\nstwx\n",
               "undo and redo a tall rectangle (%r)" % content)
        content, _ = run(
            directory, "rect_d3.txt", b"abcdef\nghijkl\nmnopqr\nstuvwx\n",
            [RIGHT * 2, ANCHOR, DOWN * 3, RIGHT * 2, KILL, UNDO, REDO,
             UNDO],
            undo=True)
        expect(content == b"abcdef\nghijkl\nmnopqr\nstuvwx\n",
               "undo again after a redo (%r)" % content)
        content, _ = run(
            directory, "rect_d2.txt", b"\tabc def\nplain\n",
            [ANCHOR, RIGHT, DOWN, CLEAR, UNDO], undo=True)
        expect(content == b"\tabc def\nplain\n",
               "one undo restores a clear (%r)" % content)
        content, _ = run(
            directory, "rect_d2.txt", b"\tabc\nplain\n",
            [RIGHT, ANCHOR, DOWN, LEFT, OPEN, UNDO], undo=True)
        expect(content == b"\tabc\nplain\n",
               "one undo restores an open (%r)" % content)
        content, _ = run(
            directory, "rect_d2.txt", b"abcdef ghij\nxy\n123456 zzz\n",
            [RIGHT * 6, DOWN * 2, ANCHOR, LEFT * 4, UP * 2, KILL,
             RIGHT * 4, YANK, UNDO, UNDO], undo=True)
        expect(content == b"abcdef ghij\nxy\n123456 zzz\n",
               "two undos peel back yank then kill (%r)" % content)

        # A read-only buffer refuses the kill; the file is unchanged.
        content, terminal = run(
            directory, "rect_e1.txt", b"hello world\nsecond line\n",
            [READONLY, ANCHOR, DOWN, RIGHT * 4, KILL])
        expect(b"Buffer is read-only" in terminal,
               "kill on a read-only buffer is refused")
        expect(content == b"hello world\nsecond line\n",
               "read-only kill changed nothing (%r)" % content)

        # Deleting the anchored line itself drops the anchor, and the
        # next rectangle command says so instead of acting.  The
        # anchor goes on the second line; two C-K from the first line
        # (the first only empties it, the second merges the two and
        # frees the anchored one's storage).
        content, terminal = run(
            directory, "rect_e2.txt", b"hello world\nsecond line\n",
            [DOWN, ANCHOR, UP, KILLLINE, KILLLINE, RIGHT * 4, KILL])
        expect(b"Rectangle anchor lost" in terminal,
               "a killed anchor line is reported")
        expect(content == b"second line\n",
               "the lost anchor kills nothing more (%r)" % content)

        # The prefix takes exactly one key: an unknown one aborts with
        # a message the way C-X 4 does, and C-G aborts cleanly.
        content, terminal = run(
            directory, "rect_f.txt", b"one\ntwo\n",
            [b"\x18r\x0e",                            # C-X r C-N
             b"\x18r\x07"])                           # C-X r C-G
        expect(terminal.count (b"Unknown C-X r command") == 1,
               "an unknown key after the prefix is refused")
        expect(b"[Aborted]" in terminal,
               "C-G aborts out of the prefix cleanly")
        expect(content == b"one\ntwo\n",
               "the refused prefix keys changed nothing (%r)" % content)

    print("RESULT: all passed")


if __name__ == "__main__":
    main()
