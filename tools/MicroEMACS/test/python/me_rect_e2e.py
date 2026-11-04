#!/usr/bin/env python3
"""Rectangle editing through the C-X r prefix: anchor one corner
with C-X r C-@, or mark it with C-Space as GNU Emacs does, put point
on the other, and copy/kill/yank/clear/open the column range between
them; a set anchor is the corner the commands take first.  The
rectangles are cut on the display-cell grid,
so a tab counts as the cells it renders as: clearing replaces it with
that many spaces, and a kill whose edge falls inside it expands it
first.  Short lines read as blanks when text is copied and are padded
out by yank.  One undo reverses a whole kill."""

import os
import re
import tempfile

from common import drain, kill, save_quit, spawn

UP, DOWN, RIGHT, LEFT = b"\x1bOA", b"\x1bOB", b"\x1bOC", b"\x1bOD"
SETMARK = b"\x00"                 # C-Space (C-@)
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


def sgr_mouse(code, col, row, final=b"M"):
    """One SGR mouse report for a 0-based COL/ROW cell."""
    return b"\x1b[<%d;%d;%d%s" % (code, col + 1, row + 1, final)


def x10_mouse(code, col, row):
    """One legacy X10 report for a 0-based COL/ROW cell."""
    return b"\x1b[M" + bytes((32 + code, 33 + col, 33 + row))


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

        # The anchor highlight: while the anchor is set, the
        # anchor-to-point rectangle is painted in the selection
        # color, following the point.  A drop changes nothing else,
        # so the refresh after it is what proves the repaint; the
        # clear at the mid-point repaints every row it blanked.
        # The painted runs after a full refresh name their rows, so
        # a rectangle off by a row is caught, not just its absence.
        path = os.path.join(directory, "rect_h1.txt")
        with open(path, "wb") as output_file:
            output_file.write(b"abcd\nwxyz\n")
        process, fd = spawn(path)
        try:
            startup = send(fd, b"", 1.5)
            expect(b"48;5;24" not in startup,
                   "no highlight before the anchor is set")
            send(fd, ANCHOR)
            moved = send(fd, DOWN + RIGHT * 2)       # point (1,2)
            expect(b"48;5;24" in moved,
                   "the anchor-to-point rectangle is highlighted")
            full = send(fd, b"\x0c")                 # C-L refresh
            runs = re.findall (rb"\x1b\[48;5;24m([^\x1b]*)", full)
            expect(any(r.startswith(b"ab") for r in runs)
                   and any(r.startswith(b"wx") for r in runs),
                   "the highlight paints its own rows' cells (%r)" % runs)
            cleared = send(fd, CLEAR)
            expect(b"48;5;24" not in cleared,
                   "the highlight is gone once the anchor is used")
            send(fd, ANCHOR)                         # anchor at (1,2)
            moved = send(fd, UP + RIGHT)             # point (0,3)
            expect(b"48;5;24" in moved,
                   "a fresh anchor highlights again")
            aborted = send(fd, b"\x07")              # C-G drops it
            redrawn = send(fd, b"\x0c")              # C-L refresh
            expect(b"48;5;24" not in redrawn,
                   "C-G drops the anchor and its highlight")
        finally:
            kill(process, fd)

        # A shifted button drag sweeps the rectangle: the press anchors
        # the corner under the mouse, the drag carries the point with
        # the highlight following it, and the release leaves the anchor
        # set, so the next operator keys act on the dragged rectangle.
        # SGR reports: press col 1 row 0 with shift, drag to col 4
        # row 1, release there -- cols [1,4) over both lines.
        content, terminal = run(
            directory, "rect_i1.txt", b"abcd\nwxyz\n",
            [sgr_mouse(4, 1, 0), sgr_mouse(36, 4, 1),
             sgr_mouse(4, 4, 1, b"m"), CLEAR], mouse=True)
        expect(b"48;5;24" in terminal,
               "a shifted drag highlights the rectangle")
        expect(b"[Rectangle of 2 lines selected]" in terminal,
               "the release reports the swept lines")
        expect(content == b"a   \nw   \n",
               "clear acts on the dragged rectangle (%r)" % content)

        # The same drag on the legacy X10 reports: the press arrives as
        # a whole-gesture click with shift riding along, and the
        # release is the three-button code with shift.  Kill takes the
        # swept cells: cols [1,4) of both lines.
        content, terminal = run(
            directory, "rect_i2.txt", b"abcd\nwxyz\n",
            [x10_mouse(4, 1, 0), x10_mouse(36, 4, 1),
             x10_mouse(7, 4, 1), KILL], mouse=True)
        expect(b"[Rectangle of 2 lines selected]" in terminal,
               "an X10 release also completes the drag")
        expect(content == b"a\nw\n",
               "kill acts on the X10-dragged rectangle (%r)" % content)

        # A control drag sweeps the same rectangle: st and xterm keep
        # a shifted button for their own text selection, so control
        # is the modifier that reaches the editor there.  Drag up and
        # left this time -- press col 4 row 1, release col 1 row 0,
        # the corners taken in the other order.
        content, terminal = run(
            directory, "rect_i3.txt", b"abcd\nwxyz\n",
            [sgr_mouse(16, 4, 1), sgr_mouse(48, 1, 0),
             sgr_mouse(16, 1, 0, b"m"), CLEAR], mouse=True)
        expect(b"[Rectangle of 2 lines selected]" in terminal,
               "a control drag also completes the rectangle")
        expect(content == b"a   \nw   \n",
               "clear acts on the control-dragged rectangle (%r)" % content)

        # A right click extends the anchored rectangle instead of the
        # region, so the two highlights never paint together: drag
        # rows 0-1, right click on row 3, and the clear takes all
        # four rows.
        content, terminal = run(
            directory, "rect_i4.txt", b"abcd\nwxyz\n1234\n7890\n",
            [sgr_mouse(16, 1, 0), sgr_mouse(48, 4, 1),
             sgr_mouse(16, 4, 1, b"m"),            # ctrl drag: rows 0-1
             sgr_mouse(2, 5, 3), sgr_mouse(2, 5, 3, b"m"),  # right click
             CLEAR], mouse=True)
        expect(b"[Mouse kind" not in terminal,
               "a right click with an anchor set is not a stray event")
        expect(content == b"a   \nw   \n1   \n7   \n",
               "a right click extends the anchored rectangle (%r)" % content)

        # A plain click supersedes the anchor, and its release puts
        # the mark ring back with the selection: the kill has no
        # corner to take, reports it, and changes nothing.
        content, terminal = run(
            directory, "rect_i5.txt", b"abcd\nwxyz\n",
            [sgr_mouse(16, 1, 0), sgr_mouse(16, 4, 1, b"m"),  # anchor set
             sgr_mouse(0, 2, 0), sgr_mouse(0, 2, 0, b"m"),    # plain click
             KILL], mouse=True)
        expect(b"No rectangle anchor or mark" in terminal,
               "a plain click leaves the kill no corner")
        expect(content == b"abcd\nwxyz\n",
               "the refused kill changed nothing (%r)" % content)

        # The GNU Emacs rule: with no anchor set, the corners come
        # from the mark and point.  C-Space marks (0,0), point moves
        # to (1,2), and the kill takes cols [0,2) of both lines.
        content, terminal = run(
            directory, "rect_j1.txt", b"abcd\nwxyz\n",
            [SETMARK, DOWN + RIGHT * 2, KILL])
        expect(b"[Rectangle of 2 lines killed]" in terminal,
               "the kill runs between mark and point")
        expect(content == b"cd\nyz\n",
               "the mark's rectangle is killed (%r)" % content)

        # A plain mouse drag marks its corners the same way, and the
        # keyboard kill takes them; the prefix shows no rectangle
        # preview while the linear selection is still showing, whose
        # highlight would otherwise repaint a longer run on the
        # marked row.
        path = os.path.join(directory, "rect_j2.txt")
        with open(path, "wb") as output_file:
            output_file.write(b"abcd\nwxyz\n")
        process, fd = spawn(path, mouse=True)
        try:
            startup = send(fd, b"", 1.5)
            send(fd, sgr_mouse(0, 0, 0))          # plain press (0,0)
            dragged = send(fd, sgr_mouse(32, 2, 1))    # drag to (2,1)
            dragged += send(fd, sgr_mouse(0, 2, 1, b"m"))
            expect(b"48;5;24" in dragged,
                   "the linear drag highlights the region")
            prefix = send(fd, b"\x18r")           # the prefix waits
            expect(b"48;5;24" not in prefix,
                   "no rectangle preview over the region highlight")
            killed = send(fd, b"\x17")            # C-w
            expect(b"[Rectangle of 2 lines killed]" in killed,
                   "the kill runs on the dragged corners")
            save_quit(process, fd)
        except BaseException:
            kill(process, fd)
            raise
        with open(path, "rb") as fixture_file:
            expect(fixture_file.read() == b"cd\nyz\n",
                   "the dragged rectangle is killed")

        # While the prefix waits for its key, the rectangle the
        # command would take is highlighted: the mark's, when no
        # anchor is set.  C-G aborts the prefix and drops the
        # highlight with it.
        path = os.path.join(directory, "rect_j3.txt")
        with open(path, "wb") as output_file:
            output_file.write(b"abcd\nwxyz\n")
        process, fd = spawn(path)
        try:
            startup = send(fd, b"", 1.5)
            send(fd, SETMARK)
            send(fd, DOWN + RIGHT * 2)              # mark (0,0), point (1,2)
            prefix = send(fd, b"\x18r")             # the prefix waits
            expect(b"48;5;24" in prefix,
                   "the prefix highlights the mark's rectangle")
            aborted = send(fd, b"\x07")
            redrawn = send(fd, b"\x0c")             # C-L refresh
            expect(b"48;5;24" not in redrawn,
                   "C-G drops the preview with the prefix")
        finally:
            kill(process, fd)

        # With neither an anchor nor a mark, the kill reports it and
        # changes nothing.
        content, terminal = run(
            directory, "rect_j4.txt", b"one\ntwo\n",
            [KILL])
        expect(b"No rectangle anchor or mark" in terminal,
               "no corners to take is reported")
        expect(content == b"one\ntwo\n",
               "the refused kill changed nothing (%r)" % content)

    print("RESULT: all passed")


if __name__ == "__main__":
    main()
