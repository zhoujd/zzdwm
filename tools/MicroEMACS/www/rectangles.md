# Rectangles

A *rectangle* is a column range of lines, cut on the *display-cell
grid*: the grid a line renders on, where a tab counts as the cells up
to the next tab stop and a control character as two. Set the anchor on
one corner with **C-X r C-@** (**C-Space** sends the same key), put
dot on the opposite corner, and run a command; the corners work in any
order, and each command drops the anchor once it has used it.

A character is part of the rectangle when its cells intersect the
range, so one straddling an edge is taken whole; the commands that
edit the buffer first expand a straddling tab into the spaces it
renders as. Lines shorter than the rectangle read as blanks where text
is copied. The rectangle clipboard is separate from the kill buffer.

The prefix is **C-X r**, as in GNU Emacs. The keys under it take
the region verbs one prefix deeper —
**C-w** kill, **M-W** copy ([copy-region](deleting.md)), **C-y**
yank — with **C-c** clear and **C-o** open from GNU's rectangle
chords, and the anchor on **C-@**, the
[set-mark](moving-around.md) key, as GNU's rectangles run between
point and mark.

**C-X r C-@** (**rectangle-anchor**)

Drop the rectangle anchor at dot, remembering its line and column, or
drop the anchor when one is already set. The anchor cannot be set past
the last line. Editing that removes the anchored line drops the
anchor; the next command reports "Rectangle anchor lost".

**C-X r C-c** (**clear-rectangle**)

Blank the rectangle in place: its characters become spaces, one per
cell they occupied, so nothing to the right moves. Dot stays at its
column.

**C-X r C-o** (**open-rectangle**)

Insert a column of blanks the width of the rectangle, shifting the
text to the right of it. Lines too short to reach the rectangle are
padded out to its left edge first. Dot is left on the upper left
corner of the opened column.

**C-X r C-w** (**kill-rectangle**)

Copy the rectangle as **copy-rectangle** does, then remove its
characters from each line. Dot is left on the upper left corner of
what was the rectangle. One **C-X U** undoes the whole command.

**C-X r M-W** (**copy-rectangle**)

Copy the rectangle between the anchor and dot into the rectangle
clipboard, leaving the buffer alone. A line of the rectangle that ends
before the right edge contributes blanks.

**C-X r C-y** (**yank-rectangle**)

Insert the rectangle clipboard at dot's column, one slice on dot's
line and the lines below. A line that ends before the column is padded
out with spaces, and the yank stops at the end of the buffer,
reporting how many lines fit.
