#!/usr/bin/env python3
"""The search keys.  C-S and C-R run the incremental search, and M-S
mirrors C-S for terminals that use ^S/^Q for XON/XOFF.  A RET with
nothing typed yet drops out of the incremental search into the plain
non-incremental forw-search or back-search, as in GNU Emacs — the only
binding those two have left now that C-X S and C-X R belong to the
rectangle prefix.  Each case searches for a word that names one line,
then kills that line, so the file left behind tells where the search
landed."""

import os
import tempfile

from common import drain, kill, save_quit, spawn

GOTOEOB = b"\x1b>"
BOL = b"\x01"
FORWI = b"\x13"                   # C-S
BACKI = b"\x12"                   # C-R
MFORS = b"\x1bS"                  # M-S
DONE = b"\x1b"
KILLLINE = b"\x0b"
UNDO = b"\x18U"

CONTENT = b"alpha beta\ngamma delta\nalpha omega\n"


def expect(condition, description):
    print(("PASS " if condition else "FAIL ") + description)
    if not condition:
        raise AssertionError(description)


def run(directory, name, keys):
    """Run me over the three-line fixture, feed KEYS, save, and return
    what the file holds plus the terminal output."""
    path = os.path.join(directory, name)
    with open(path, "wb") as output_file:
        output_file.write(CONTENT)
    process, fd = spawn(path)
    try:
        terminal = drain(fd, 1.5)
        for chunk in keys:
            os.write(fd, chunk)
            terminal += drain(fd, 0.5)
    except BaseException:
        kill(process, fd)
        raise
    save_quit(process, fd)
    with open(path, "rb") as fixture_file:
        return fixture_file.read(), terminal


def main():
    with tempfile.TemporaryDirectory(prefix="me-search-") as directory:
        # Incremental forward: "delta" puts dot after it, on line 2.
        content, terminal = run(
            directory, "search_a1.txt",
            [FORWI, b"delta", DONE, BOL, KILLLINE, KILLLINE])
        expect(b"i-search forward" in terminal,
               "C-S starts the incremental search")
        expect(content == b"alpha beta\nalpha omega\n",
               "incremental forward search moves dot (%r)" % content)

        # Incremental backward from the end: "gamma" is on line 2.
        content, _ = run(
            directory, "search_a2.txt",
            [GOTOEOB, BACKI, b"gamma", DONE, BOL, KILLLINE, KILLLINE])
        expect(content == b"alpha beta\nalpha omega\n",
               "incremental backward search moves dot (%r)" % content)

        # C-S with a bare RET: the plain non-incremental search prompt,
        # and forw-search leaves dot after the match.
        content, terminal = run(
            directory, "search_b1.txt",
            [FORWI, b"\r", b"beta\r", BOL, KILLLINE, KILLLINE])
        expect(b"Search [" in terminal,
               "C-S RET drops into the non-incremental search")
        expect(content == b"gamma delta\nalpha omega\n",
               "non-incremental forward search moves dot (%r)" % content)

        # C-R with a bare RET, backward from the end: back-search leaves
        # dot at the first character of the match.
        content, terminal = run(
            directory, "search_b2.txt",
            [GOTOEOB, BACKI, b"\r", b"gamma\r", BOL, KILLLINE, KILLLINE])
        expect(b"Reverse search [" in terminal,
               "C-R RET drops into the non-incremental reverse search")
        expect(content == b"alpha beta\nalpha omega\n",
               "non-incremental backward search moves dot (%r)" % content)

        # M-S runs the incremental search too, as the ^S-free spelling.
        content, _ = run(
            directory, "search_c.txt",
            [MFORS, b"delta", DONE, BOL, KILLLINE, KILLLINE])
        expect(content == b"alpha beta\nalpha omega\n",
               "M-S searches like C-S (%r)" % content)

        # A RET after the pattern was deleted back to nothing still
        # exits the incremental search; only a virgin RET drops out.
        content, terminal = run(
            directory, "search_d.txt",
            [FORWI, b"delta", b"\x7f" * 5, b"\r", UNDO])
        expect(b"[Done]" in terminal and b"Search [" not in terminal,
               "RET after deleting the pattern just exits")
        expect(content == CONTENT,
               "the aborted search left the file alone (%r)" % content)

    print("RESULT: all passed")


if __name__ == "__main__":
    main()
