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
`C-c` clear, `C-o` open, and the anchor on `C-@`.

Covered by `test/python/me_rect_e2e.py` (the operators, undo and
redo, the read-only guard, the lost-anchor case, prefix errors) and
`test/python/me_search_e2e.py` (the search merge).

## Remaining

* `string-rectangle` (replace the rectangle with a string on every
  line) and a prefix argument as the width of `open-rectangle`.
* Rectangle highlight rendering while the anchor is set, as a
  rectangular variant of the mouse selection highlight; the anchor
  currently has no visual.
* Mouse selection of the rectangle: a shifted drag anchoring a
  column-locked region in mouse.c, reusing the anchor plumbing.
* Region corners: taking the corners from mark-to-dot, the Emacs
  rule, instead of a dedicated anchor.
* `bin/win64/me.exe` has not been rebuilt since the feature landed;
  `Make.Mingw` already lists `rectangle.o`.
