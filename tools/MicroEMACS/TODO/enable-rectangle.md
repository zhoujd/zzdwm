# Rectangle Editing Enable

## Status: 2026-11-03

`rectangle.c` holds a first sketch of column (rectangle) editing: an
anchor plus `copy-rectangle`, `kill-rectangle`, `yank-rectangle`,
`clear-rectangle`, and `open-rectangle`, with a dedicated column
clipboard.  The sketch is not compiled — it is absent from the
`Make.Linux` object list — so the tree builds without it.  This note
is the plan for turning the sketch into a working feature; it records
what the sketch already gets right, what must change, and the
semantics decisions.

What the sketch gets right and stays:

* A dedicated column clipboard, separate from the linear kill buffer,
  so rectangle kills do not clobber ordinary kills.  It persists until
  the next rectangle copy or kill.
* Virtual space: a line shorter than the rectangle reads as blanks in
  the copy and is padded on yank, open, and delete.
* Normalized bounds, so the anchor may be below or right of dot.
* The five-operation set with kill saving the text it removes.

## Semantics decisions

1. Columns are display columns, `getcolpos () - 1` (origin 0), so tabs
   and wide characters expand the same way they do on screen and in
   the mouse cell mapping.  A character belongs to the rectangle when
   its display cells intersect the column range `[start, end)`.  A
   boundary falling inside a wide character or a tab therefore
   includes that character; splitting a tab at an exact cell is not
   attempted (a pad or delete that lands inside a tab first expands
   the tab, by the ordinary insert/delete machinery doing the work).
2. The anchor is a line pointer plus a column, not a row number.  The
   operators walk `lforw`/`lback` between the anchor line and dot;
   no row indices are computed.  If the anchor line has been deleted
   (the walk from `b_linep` does not find it), the operator fails with
   "rectangle anchor lost" rather than acting on a dangling pointer.
3. Undo is one step per operator: each is wrapped in
   `startsaveundo`/`endsaveundo`, and the per-line edits go through
   `linsert`/`ldelete`, which save undo and check read-only
   themselves.  No direct `l_text[]` writes (the sketch's
   `clear_rectangle` bypassed undo, the read-only check, and the
   change flags that way).
4. Dot is restored after `copy-rectangle` and `clear-rectangle`;
   `kill-rectangle` and `open-rectangle` leave dot at the rectangle's
   top-left corner.  `yank-rectangle` inserts at dot's line and
   column and walks down, stopping with a message at the end of the
   buffer.
5. The clipboard has no fixed line cap; it is a growable `malloc`ed
   array of byte strings, freed on replacement.

## Work items

1. Fix the API mismatches with this code base (all compile blockers):
   `w_dotp`/`w_doto` do not exist — dot is the `POS` `curwp->w_dot`
   with `.p` and `.o` (def.h); `getccol (FALSE)` does not exist — use
   `getcolpos ()` (random.c), origin 1 and tab-aware, so subtract 1;
   `linsert` here takes `(int n, int c, char *s)` — one character is
   `linsert (1, c, NULL)`, and a UTF-8 slice must go in as a string
   `linsert (nbytes, 0, s)`, never byte-at-a-time (each single-byte
   insert would re-encode a continuation byte); `lchange (WFMOD)` — no
   such flag, use `WFEDIT`; command functions take `(int f, int n,
   int k)` uniformly (the sketch's `copy_rectangle` lacks `k`, and
   `kill_rectangle` calls it with two arguments); `b_linep->l_forw`
   should be `lforw (b_linep)`; `stdlib.h` is missing for
   `malloc`/`free`, `stdio.h` is not needed.
2. Rename to house style (`togglemouse`, `killregion`, no
   underscores): `rectanchor`, `rectcopy`, `rectkill`, `rectyank`,
   `rectclear`, `rectopen`.  `rectanchor` records the corner
   (`w_dot.p`, current column) and toggles the anchor off when called
   again; `rectcopy` and `rectkill` consume it.  Factor the body of
   `rectcopy` into a helper (find bounds, collect slices) so
   `rectkill` does not call the command itself.
3. Column mapping helpers, shared rather than private: promote the
   display-column walk of `mouse_line_offset` (mouse.c) into line.c
   as a function mapping a display column to a character offset on an
   arbitrary line, and generalize `getcolpos`'s loop into a function
   mapping a line plus offset to a display column.  mouse.c and
   rectangle.c then share one grid.  (`wloffset` maps characters to
   bytes and is the last step of the first helper.)
4. Rewrite the operators on the primitives: for each line in the
   walk, set `w_dot` to `(lp, byte offset)`, `ldelete` the bytes of
   the intersecting characters, `linsert` padding or yanked text as a
   string.  Padding moves dot to the line end and inserts spaces
   until the end column reaches the target (the sketch's
   `pad_line_to_col` never moved dot, so its `linsert` calls acted
   wherever dot happened to be, and compared a byte count with a
   column).  Restore or park dot per decision 4, and set the window
   flags (`WFEDIT` arrives via `lchange`; add `WFMOVE`/`WFHARD` as
   the dot juggling requires).
5. Wire the file in: add `rectangle.o` to `Make.Linux` (core editing,
   always built — no `#ifdef`), declare the six commands in def.h
   beside the other per-file sections, and register them in symbol.c
   with the `"rectangle-anchor"` … `"open-rectangle"` names.  Key
   bindings follow the `otherwindprefix` reader pattern (symbol.c):
   the lowercase `C-X r` chord is free (`C-X R` is `back-search`), so
   a `rectprefix` reader dispatches `a`nchor, `c`opy, `k`ill,
   `y`ank, `o`pen, c`l`ear.  Macro recording inside the prefix reader
   must work the way `otherwindprefix`'s does (`kbdmop`/`kbdmip`).
6. Tests, at the python pty level (rectangle logic is buffer logic,
   not display): `me_rect_e2e.py` with a fixture of ragged short
   lines, a tab-indented line, and UTF-8 text — anchor above and
   below dot, copy+kill+yank round trip restoring the killed bytes at
   a new corner, open shifting a block right, clear blanking in place
   without changing lengths, virtual-space padding on short lines,
   yank at the end of the buffer, one-step undo of a kill, and the
   read-only guard.  Then the standard gates (skill: build `-b test`,
   `-p`, full `test/python` loop).
7. Docs: the new commands in `doc/microemacs.md` (command reference
   and the `C-X r` prefix), a paragraph in `www/starting.md`, and
   this directory's README index entry.

## Future tasks

* `string-rectangle` (replace the rectangle with a string on every
  line) and a prefix argument as the width of `open-rectangle`.
* Rectangle highlight rendering while the anchor is set, as a
  rectangular variant of the mouse selection highlight (per-cell
  attributes in display.c; the anchor currently has no visual).
* Mouse selection of the rectangle: a shifted drag anchoring a
  column-locked region in mouse.c, reusing the anchor plumbing.
* Region corners: an alternative anchor source taking the corners
  from mark-to-dot, the Emacs rule, instead of a dedicated anchor.
