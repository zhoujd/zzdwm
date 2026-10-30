# Mouse Support Scope

## Decision: 2026-10-30

MicroEMACS remains terminal-native and terminal-independent by default. Mouse
reporting stays disabled unless a terminal or multiplexer provides it directly;
the editor does not enable or decode mouse protocols itself.

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

If mouse interaction is needed later, add a small optional layer that supports
one normalized event representation, preferably SGR-only. Terminal protocol
decoding should remain in the backend, and `display.c` should consume only
normalized events.
