# Tag Location Ring

## Implemented: Tag Origin Ring

MicroEMACS now keeps a ring of the sixteen most recent locations from which
tag, cscope, or grep navigation started.

* `M-.` prompts for a tag and jumps to the first match.
* `C-u M-.` jumps to the next match for that tag.
* `M-- M-.` jumps to the previous match for that tag.
* `M-*` runs `pop-tag-mark` and returns to the most recent origin.
* Repeated `M-*` commands retrace earlier origins; oldest entries beyond the
  ring limit are discarded.
* Origins are stored by buffer, line number, and character offset, so ordinary
  edits do not leave stale line pointers in the ring.
* Offsets are clamped when restoring a position if the originating line became
  shorter.
* Entries are removed when their buffer is killed.
* Existing `M-.` argument behavior is unchanged: an argument still selects the
  next or previous match in the tag list.

## Future Work

* Add a separate destination history for returning to previously visited tag
  targets without consuming the origin ring.
* Consider a GNU Emacs-style `find-tag-regexp` command.
* Consider other-window and other-frame tag navigation variants if window and
  frame support make them useful.
