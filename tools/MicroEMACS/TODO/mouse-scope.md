# Mouse Support Scope

## Decision: 2026-10-30

MicroEMACS remains terminal-native and terminal-independent by default. The
core event model is normalized and protocol-neutral, while mouse reporting
remains disabled in standard builds.

This deliberately excludes mouse reporting modes, wheel-event decoding,
X10/SGR compatibility layers, drag selection, selection rendering, and
dvtm/screen passthrough logic. Those features add terminal-specific state and
behavior that is difficult to test consistently across terminal emulators and
multiplexers.

Wheel handling is intentionally deferred. It is not reliably delivered through
dvtm and screen in all configurations, so enabling a mouse mode would not
guarantee a consistent editor experience.

Click-to-move-point is implemented for unmodified button-one presses: map the
press to its page row and column, select the window containing that row, and
move point to the corresponding visible line and column. Drag selection,
double-click word semantics, and clipboard integration remain out of scope.

Native Windows mouse support should remain a separate backend feature. Its
message loop and window/event API boundary are naturally backend-specific and
should not complicate the terminal-independent event model.

## Status: 2026-10-31

1. Done: `toggle-mouse` enables or disables terminal mouse reporting during
   runtime; positive, negative, and zero prefix arguments explicitly enable,
   disable, and toggle it.
2. Done: OSC 52 clipboard copy and bracketed paste are independent of mouse
   support and are implemented in `osc52.c`. The `osc52` Lisp variable controls
   clipboard copy, and minimal builds include both features.
3. Done: the mouse-selected region is drawn in the selection
   highlight color while the selection is active (bright white on deep blue
   by default, settable with the `set-selection-color` Lisp form; reverse
   video in non-COLOR builds), and any non-mouse key ends the highlight
   (transient, like Emacs; COLOR builds only, since the per-cell attribute
   array does not exist otherwise). A double click selects the word at the
   clicked cell (or the run of blanks), and a third click selects the whole
   line. The SGR protocol reports raw presses and releases, so consecutive
   plain clicks on one cell are counted in `mouse.c`; the count breaks when
   a click lands elsewhere or another mouse event intervenes (there is no
   clock in the core to time the interval). Backends that pair clicks
   themselves can report a complete `MOUSE_CLICK` gesture with the count.
   Keyboard region selection remains unchanged, so non-mouse and minimal
   builds do not need selection rendering.
4. Done: a button-0 press anchors the mark at the pressed cell and a drag
   extends the mark-to-point region until the release; a press and release
   on the same cell is a plain click that leaves the mark ring as it was,
   and a drag stays in the window it started in. The press anchors right
   away, so drags work on terminals that report no motion while the button
   is held. The region is not rendered yet (item 3).
5. Done: the wheel scrolls the window under the mouse, by the
   `mouse-wheel-step` Lisp variable's lines per tick (3 by default); a wheel
   report outside every window scrolls the current window. A shifted wheel
   scrolls the window horizontally by the `scroll-left`/`scroll-right`
   columns (four), clamped to the left edge and to the widest visible line,
   with the view pinned (`w_hscroll`) until dot next moves or is edited.
   The screen model now counts display columns on every build (wide
   characters occupy two cells, as the Windows backend always drew them),
   so the horizontal skip, the clamp, and the cursor stay aligned on lines
   with wide characters; the ncurses and termcap output paths skip the
   filler cell because the terminal advances two columns itself.
   Delivery through dvtm and GNU screen still needs the multiplexer
   forwarding work.

## Future task: native Windows mouse backend

Translate Windows console mouse records from `ReadConsoleInput` into
`EMOUSE` values in the MinGW backend, initially exposing unmodified
button-one presses for click-to-move-point. Keep event-mode changes, coordinate
conversion, and input draining inside `tools/MicroEMACS/tty/mingw`; do not
introduce SGR parsing or Linux terminal state into that backend.

The horizontal-scroll commands remain valuable without that layer:

* `C-x <` runs `scroll-left` and `C-x >` runs `scroll-right`; they use the
  prefix argument or four columns.
* Right scrolling is clamped while visible text remains, and left scrolling is
  clamped at column zero.
* `w_hscroll` pins the view until dot moves or the buffer is edited.

The normalized foundation keeps a bounded thirty-two-entry queue in `mouse.c`.
Backends translate reports into `EMOUSE` values, `mouse-event` dispatches
the oldest event, `define-key "Mouse"` lets Lisp replace that dispatcher, and
the handler can consume the event with `read-mouse-event`. A compile-time
`MOUSE=yes` ncurses backend opts into normal SGR press/release reporting and
decodes button events directly, avoiding ncurses' click interval. It does not
request motion, wheel, drag selection, or clipboard behavior. The legacy
termcap backend decodes X10 reports for compatibility.

Build the standard editor with `make`, then run `./me -m`. Linux builds enable
`MOUSE` by default; `./build.sh -m` still builds the minimal editor and does
not include mouse support.
