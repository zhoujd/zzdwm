# Rectangle Editing

## Status: 2026-11-03 — enabled

`rectangle.c` holds the working feature: an anchor plus
`copy-rectangle`, `kill-rectangle`, `yank-rectangle`,
`clear-rectangle`, and `open-rectangle`, cut on the display-cell
grid (tabs and wide characters expanded) with a dedicated column
clipboard.  Each operator undoes in one step: every edit goes
through `linsert`/`ldelete` behind a `UMOVE` undo record at its
position.  Wired in by 1d564d1 on a `C-X a` prefix; 550199a moved
the prefix to `C-X r`, as in GNU Emacs, by folding the plain
`forw-search`/`back-search` that held `C-X S`/`C-X R` into the
incremental search on `C-S`/`C-R` — a bare `RET` before any search
text drops into the plain command.  The region verbs sit one prefix
deeper (`C-X r C-w` kill, `C-X r M-W` copy, `C-X r C-y` yank), with
`C-c` clear, `C-o` open, and the anchor on `C-@`.  While the anchor
is set, `rectangle.c` and `display.c` highlight the
anchor-to-dot rectangle in the selection color, the rectangular
counterpart of the mouse selection highlight, following dot out
past the end of a line that ends first; **C-G** drops the anchor
with the rest of the aborted state, as GNU's C-G deactivates the
mark. A shifted or control button drag sweeps the rectangle with
the mouse: the press anchors the cell under it (`mouse.c` calling
the same anchor plumbing through `rectstart`), the drag carries
dot with the highlight following, and the release reports the
line count and leaves the anchor set for the operators. Control
matters because st, xterm, and the like keep a shifted button for
their own text selection, so the shift variant only reaches the
editor on terminals that pass modifiers through. Both the SGR and
the legacy X10 reports drive it. While the anchor stands, a right
click moves the far corner and a plain click drops the anchor
(`rectset` tells the two gestures apart), so the rectangle and
region highlights never paint together.

Covered by `test/python/me_rect_e2e.py` (the operators, undo and
redo, the read-only guard, the lost-anchor case, prefix errors,
the anchor highlight, and the shifted and control mouse drag) and
`test/python/me_search_e2e.py` (the
search merge).

## Remaining

* Region corners: taking the corners from mark-to-dot, the Emacs
  rule, instead of a dedicated anchor.
