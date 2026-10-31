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

Build the demo with `./build.sh -b me`, then run `./me -m`. Linux builds
enable `MOUSE` by default; `./build.sh -m` still builds the minimal editor and
does not include mouse support.
