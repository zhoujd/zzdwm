# Rectangle Editing

## Status: 2026-11-27 — enabled, keys aligned with GNU Emacs

`rectangle.c` holds the working feature: an anchor plus
`copy-rectangle`, `kill-rectangle`, `delete-rectangle`,
`yank-rectangle`, `clear-rectangle`, `open-rectangle`, and
`string-rectangle` (b01139c), cut on the display-cell
grid (tabs and wide characters expanded) with a dedicated column
clipboard.  Each operator undoes in one step: every edit goes
through `linsert`/`ldelete` behind a `UMOVE` undo record at its
position.  Wired in by 1d564d1 on a `C-X a` prefix; 550199a moved
the prefix to `C-X r`, as in GNU Emacs, by folding the plain
`forw-search`/`back-search` that held `C-X S`/`C-X R` into the
incremental search on `C-S`/`C-R` — a bare `RET` before any search
text drops into the plain command.  The keys are GNU Emacs's own
C-x r keys — `k` kill, `d` delete, `y` yank, `c` clear, `o` open,
`t` string, and `M-w` copy, the capital `M-W` the region verb is
written with taken too — after the verbs first sat one prefix
deeper (`C-X r C-w` kill, `C-X r M-W` copy, `C-X r C-y` yank, with
`C-c` clear and `C-o` open); `delete-rectangle` is GNU's `C-x r d`,
the cut without the clipboard save, which leaves the last copy or
kill in place.  The anchor sits on `C-@`, the set-mark key.  With no anchor
set the operators take the corners from mark-to-dot instead, the
Emacs rule, so plain `C-Space` and the mouse drag's mark feed them
as the anchor does.  While the anchor
is set, `rectangle.c` and `display.c` highlight the
anchor-to-dot rectangle in the selection color, the rectangular
counterpart of the mouse selection highlight, following dot out
past the end of a line that ends first; the mark's rectangle shows
the same way while the `C-X r` prefix waits for its key and no
anchor is set, which is the only time, as me's mark stands until
it is cleared and an always-on highlight would glow after every
move — and not while a mouse selection is showing either, whose
highlight would paint the same cells.  **C-G** drops the anchor
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

Covered by `test/python/me_rect_e2e.py` (the operators on their
GNU keys, delete leaving the clipboard alone, the capital `M-W`
copy, undo and
redo, the read-only guard, the lost-anchor case, prefix errors,
the pre-alignment chords among the refused, the anchor highlight,
the shifted and control mouse drag, the
mark-to-dot corners, and the prefix's preview of them) and
`test/python/me_search_e2e.py` (the
search merge).
