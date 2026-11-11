# Mouse Indicators On The Window Border

Status: implemented.

The top border's right side carries two mouse marks beside the layout
symbol:

- `(M)` — dvtm's own mouse grabbing is on, the global `MOD+M` toggle.
  Every window gets it, as the toggle governs dvtm as a whole.
- `(m)` — this window's application enables mouse tracking (DECSET 1000)
  itself: clicks, drags and the wheel go to the application, not to
  dvtm's local bindings and scrollback view. Per window, the state of
  the vt inside it.

- Round brackets: the layout symbols own the square ones, and
  fullscreen's symbol is `[M]` itself (`config.h`), so a `[M]` indicator
  would read as the fullscreen layout twice over.
- `draw_border` composes the right side as one string — ` (M)` and
  ` (m)` as each applies, the layout symbol when `showinfo` is on —
  pads its end to a space so a lone mark keeps one on each side, and
  prints it right-aligned one column from the border's end. With the
  mouse fully off the rendering is byte for byte what it was.
- The border must follow both marks at run time. `togglemouse` redraws
  all borders with the global toggle; the main loop compares
  `vt_mousemode` around `vt_process` and redraws the client's border on
  a change, guarded as `draw()` is against the fullscreen layout hiding
  other windows' borders. `handle_editor` redraws the border when
  copymode's editor vt gives way to the app vt again.
- ncurses writes only the changed cells, so the marks stream as
  fragments when they appear beside an already-drawn layout symbol — a
  shifted mark can even lose its closing bracket to an unchanged cell.
  The pty test sends `MOD+CTRL('L')`, dvtm's full-repaint `redraw`,
  before asserting, so whole lines stream.

The `(m)` mark appears and disappears the moment the application asks
for the mouse and releases it, as `me`'s `toggle-mouse` shows at a
keystroke; the `(M)` mark follows `MOD+M` the same way.

Covered by `test/python/mouse_indicator.py`.
