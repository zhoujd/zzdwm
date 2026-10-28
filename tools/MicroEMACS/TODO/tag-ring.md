# Tag Location Ring

## Implemented: Tag Location Rings

MicroEMACS keeps separate sixteen-entry rings for navigation origins and
targets. The target history records locations reached by tag, cscope, or
grep navigation.

* `M-.` prompts for a tag and jumps to the first match.
* `M-C-.` runs `find-tag-regexp` and matches tag names with a regular
  expression.
* `C-X 4 .` runs `find-tag-other-window`, and `C-X 4 M-C-.` runs
  `find-tag-regexp-other-window`; with one window, the window is split only
  after a matching tag is found.
* `C-u M-.` jumps to the next match for that tag.
* `M-- M-.` jumps to the previous match for that tag.
* `M-*` runs `pop-tag-mark` and returns to the most recent origin.
* Repeated `M-*` commands retrace earlier origins; oldest entries beyond the
  ring limit are discarded.
* `next-tag-target` and `back-tag-target` are unbound commands that
  traverse the sixteen most recent tag, cscope, and grep targets without
  consuming tag origins.
* Internally, destination entries use compact `tagtarget` identifiers, and the
  command names use the same `tag-target` wording.
* Origins are stored by buffer, line number, and character offset, so ordinary
  edits do not leave stale line pointers in the ring.
* Offsets are clamped when restoring a position if the originating line became
  shorter.
* Entries are removed when their buffer is killed.
* Existing `M-.` argument behavior is unchanged: an argument still selects the
  next or previous match in the tag list.

## Target History Cursor

The two rings use different navigation models. Origins form a stack:
`tagorigincount` is both the number of entries and the next push position, and
popping an origin removes it. No separate cursor is needed because origin
navigation only moves backward.

Targets form a browsable history. `tagtargetcount` is the number of retained
destinations, while `tagtargetindex` is the user's current cursor into that
history. A new destination is appended and `tagtargetindex` is set to
`tagtargetcount`. `back-tag-target` decrements the cursor, and
`next-tag-target` increments it, without consuming destinations:

```text
targets: [A, B, C]
count:   3
index:   3             /* on C, the newest destination */

back-tag-target
index:   2             /* on B */

next-tag-target
index:   3             /* on C again */
```

## Future Work

* Consider other-frame tag navigation if frame support is added.
