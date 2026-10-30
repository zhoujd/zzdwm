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

The horizontal-scroll commands remain valuable without that layer:

* `C-x <` runs `scroll-left` and `C-x >` runs `scroll-right`; they use the
  prefix argument or four columns.
* Right scrolling is clamped while visible text remains, and left scrolling is
  clamped at column zero.
* `w_hscroll` pins the view until dot moves or the buffer is edited.

The normalized foundation keeps a bounded thirty-two-entry queue in `mouse.c`.
Backends translate reports into `MOUSE_EVENT` values, `mouse-event` dispatches
the oldest event, `define-key "Mouse"` lets Lisp replace that dispatcher, and
the handler can consume the event with `read-mouse-event`. A compile-time
`MOUSE=yes` ncurses backend opts into normal SGR press/release reporting only.
It does not request motion, drag selection, or clipboard behavior. The legacy
termcap backend decodes X10 reports for compatibility, including multipage
coordinate conversion where available.
