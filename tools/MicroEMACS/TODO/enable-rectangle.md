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
mark.

Covered by `test/python/me_rect_e2e.py` (the operators, undo and
redo, the read-only guard, the lost-anchor case, prefix errors,
the anchor highlight) and `test/python/me_search_e2e.py` (the
search merge).

## Remaining

* Mouse selection of the rectangle: a shifted drag anchoring a
  column-locked region in mouse.c, reusing the anchor plumbing.
* Region corners: taking the corners from mark-to-dot, the Emacs
  rule, instead of a dedicated anchor.
* `bin/win64/me.exe` has not been rebuilt since the feature landed;
  `Make.Mingw` already lists `rectangle.o`.
