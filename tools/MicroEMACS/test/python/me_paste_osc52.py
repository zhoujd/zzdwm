#!/usr/bin/env python3
"""Bracketed paste inserts as one unit; copy-region sends OSC 52."""

import os

from common import check, drain, finish, fixture, kill, spawn

# --- A: bracketed paste --------------------------------------------------
path = fixture("paste.txt")
p, fd = spawn(path, mouse=False)
out = drain(fd)
check(b"\x1b[?2004h" in out, "ttopen enables bracketed paste (2004h)")
payload = b"hello\tworld\rsecond line"
os.write(fd, b"\x1b[200~" + payload + b"\x1b[201~")
drain(fd)
os.write(fd, b"\x18\x13")              # C-X C-S file-save
drain(fd)
os.write(fd, b"\x18\x03")              # C-X C-C quit
out += drain(fd)
p.wait(timeout=5)
check(p.returncode == 0, "quit after paste exits cleanly")
out += drain(fd, 0.5)
check(b"\x1b[?2004l" in out, "ttclose disables bracketed paste (2004l)")
content = open(path, "rb").read()
check(content == b"hello\tworld\nsecond line\n",
      "pasted text inserted as a unit (got %r)" % content)

# --- B: OSC 52 copy-region -----------------------------------------------
path = fixture("osc52.txt", b"abcdef\n")
p, fd = spawn(path, mouse=False)
drain(fd)
os.write(fd, b"\x1b ")                  # M-Space set-mark at bob
drain(fd, 0.3)
for _ in range(3):
    os.write(fd, b"\x1bOC")             # right arrow x3
    drain(fd, 0.3)
os.write(fd, b"\x1bW")                  # M-W copy-region
out = drain(fd)
check(b"\x1b]52;c;YWJj\x07" in out,
      'copy-region sends OSC 52 (b64 abc); saw %r' %
      ([x for x in out.split(b"\x1b") if b"]52" in x] or b"nothing"))
kill(p)

finish()
