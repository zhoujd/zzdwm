# Enhance Mouse Support

Status: in progress — P0 implemented on the ncurses backend (click
to move point, mode-line click selects window; `mouse-test` covers
the column mapping). Wheel, drag, and clipboard phases pending.
Last updated: 2026-10-30

## Current State

`me -m` enables "experimental mouse support" (`help.c` usage line). What it
actually does today:

| Piece | Location | Behavior |
| :--- | :--- | :--- |
| Flag | `main.c` | `-m` sets `mouse = TRUE` |
| Enable | `tty/termcap/tty.c` | Only when `TERM` starts with `xterm`; sends `\033[?9h` (X10 mode) |
| Disable | `tty/termcap/tty.c` | Sends `\033[?9l` on exit |
| Decode | `tty/termcap/ttykbd.c` `parse_or_mouse()` | Reads 3 raw bytes: `button, column, row` (each `byte - 32`) |
| Handle | `display.c` `mouseevent()` | Prints `[Mouse button %d, row %d, column %d]` — debug stub, no action |

Why this is weak:

- **X10 protocol** (`\033[?9h`): button-press only. No release, no motion,
  no wheel, no modifiers. Coordinates cap at 223 (`byte - 32`).
- **Hard-coded `xterm*` TERM gate**: excludes `st`, `rxvt`, `screen`,
  `dvtm`, `tmux-*`, `alacritty`, `foot`, `kitty`, `wezterm` — most of the
  terminals this repo carries terminfo entries for (`misc/term/`).
- **No editor actions**: a click does nothing except echo.

## Goal

Make the mouse a first-class input device on par with modern TUI editors:
click to place point, drag to select, wheel to scroll, paste on middle
click — over the modern SGR protocol on Unix and the console API on
Windows (MinGW), with one shared event dispatcher. Without breaking dumb
terminals.

## Protocol: SGR (mode 1006) with fallback

Primary: DECSET 1006 (SGR). Sequences `\033[<b;x;y M` (press) and
`\033[<b;x;y m` (release). Button byte encodes button (0/1/2), wheel
(64/65), motion flag (32), and modifiers (shift 4, alt 8, ctrl 16).
Coordinates are decimal, effectively unbounded, and UTF-8 safe.

Fallback chain — enable the best the terminal is known to support:

```txt
1006 SGR        modern xterm, st, rxvt, alacritty, kitty, foot, wezterm
1015 urxvt      decimal, urxvt-style
1005 UTF-8      avoid: ambiguous with wide input
9    X10        legacy press-only (current behavior)
```

Enable/disable pairs, sent together:

```txt
on : \033[?1006h \033[?1000h \033[?1002h   (SGR + press/release + drag)
off: \033[?1006l \033[?1000l \033[?1002l
```

Keep 1002 (button-event tracking) rather than 1003 (any-motion) so idle
mouse movement does not flood the input queue.

Fallback rule: if a terminal reports X10 only, keep today's 3-byte path.
Decoding must be driven by which mode was enabled, not guessed from bytes.

## Event Model

Extend the existing globals (`mouse_button`, `mouse_row`, `mouse_column`,
`display.c`) with kind and modifiers, decoded once in the tty layer:

```c
int mouse_kind;   /* 0=press 1=release 2=motion 3=wheel */
int mouse_mods;   /* bit 0 shift, bit 1 alt, bit 2 ctrl */
```

`mouseevent()` in `display.c` becomes the dispatcher; the tty layer only
decodes. `KMOUSE` stays a normal keymap entry, so binding and Lisp
integration keep working unchanged.

## Features by Phase

### P0 — Decode and click-to-point

- SGR decode in `parse_or_mouse()` behind the mode used.
- Left click: move point to the screen position in the window under the
  cursor (map row → window, then screen column → buffer offset, honoring
  horizontal scroll and UTF-8 wide chars).
- Click on a modeline: select that window (`nextwind`-style).
- Click outside the text area (echo line, status): ignore.

### P1 — Wheel and drag select

- Wheel up/down: scroll the window under the cursor 3 lines (default),
  configurable via a Lisp variable, e.g. `mouse-wheel-step`.
- Shift+wheel: horizontal scroll by 4 columns.
- Drag with left button: set mark at press position, extend region on
  motion, redisplay with the region highlight.
- Motion events during drag must coalesce — update point, let the normal
  redisplay pick it up; do not force a synchronous redraw per event.

### P2 — Click counting and paste

- Double click: select word at point. Triple click: select line.
  (Track timing between consecutive presses at the same position.)
- Middle click: yank killbuffer at the clicked position (primary
  selection semantics).
- Right click: extend selection to click point.

### P3 — Clipboard and paste interop

- OSC 52 (`\033]52;c;BASE64\007`) copy of the selected region, so
  selection lands in the system clipboard even over ssh. Gate behind a
  variable (some terminals/pagers refuse OSC 52).
- Bracketed paste (mode 2004) as a separate toggle: pasted text inserts
  as one unit instead of replaying keystrokes.

### P4 — Terminal gate and multiplexer passthrough

- Replace the `strncmp(TERM, "xterm", 5)` gate with an explicit allow
  list or a capability probe; `-m` is already opt-in, so when in doubt,
  trust the user.
- Document passthrough: inside `tmux` needs `set -g mouse off` or
  `allow-passthrough`; inside this repo's own `dvtm`, the multiplexer
  grabs the mouse first — document `MOD+` escape or run dvtm with mouse
  disabled. Same for `screen`.

### P5 — Windows MinGW console

First-class, not an afterthought: the MinGW build already receives mouse
events and throws them away. In `tty/mingw/ttyio.c` `ttgetc()` the event
loop does `PeekConsoleInput`, handles `WINDOW_BUFFER_SIZE_EVENT` and
`KEY_EVENT`, and consumes-and-discards everything else — including
`MOUSE_EVENT`. The integration point is exactly that discard branch:
map the record onto the shared `mouse_*` fields and return `KMOUSE`, and
`mouseevent()` plus every feature in P0–P3 works unchanged, because
decoding is the only platform-specific part.

No VT sequences on this path — the console API replaces DECSET:

| Concern | Unix (termcap) | Windows (mingw) |
| :--- | :--- | :--- |
| Enable | `\033[?1006h` ... | `SetConsoleMode`: set `ENABLE_MOUSE_INPUT`, clear `ENABLE_QUICK_EDIT_MODE` (with `ENABLE_EXTENDED_FLAGS`) |
| Disable | `\033[?1006l` ... | restore saved console mode on exit |
| Event source | escape sequences | `record.Event.MouseEvent` |
| Coordinates | decimal in sequence | `dwMousePosition`, but in **screen-buffer** coords — subtract the window origin from `CONSOLE_SCREEN_BUFFER_INFO`, or clicks land offset in scrolled buffers |
| Buttons | byte encoding | `FROM_LEFT_1ST_BUTTON_PRESSED` (left), `FROM_LEFT_2ND_BUTTON_PRESSED` (middle), `RIGHTMOST_BUTTON_PRESSED` (right) |
| Press/release | `M` vs `m` byte | synthesize by diffing `dwButtonState` against the previous record |
| Wheel | button 64/65 | `MOUSE_WHEELED` with `dwButtonState >> 16` (±`WHEEL_DELTA`); horizontal via `MOUSE_HWHEELED` |
| Double click | timing heuristic | `dwEventFlags & DOUBLE_CLICK` — the console detects it |
| Modifiers | byte bits | `dwControlKeyState` (`SHIFT_PRESSED`, `*_CTRL_PRESSED`, `*_ALT_PRESSED`) |

Details that will bite if skipped:

- **QuickEdit**: conhost's own mouse selection intercepts events unless
  cleared; restore the original mode on every exit path, same rule as
  the Unix disable sequence.
- **Buffer vs window coordinates**: `dwMousePosition` is relative to the
  screen buffer top, not the visible window; a scrolled-back console
  makes raw coordinates wrong until corrected.
- **`_getch` mixed with `ReadConsoleInput`**: the key path uses both the
  record (to peek) and `_getch` (to read); mouse must be captured from
  the record side of that split or events double-deliver.
- **Windows Terminal**: when `me.exe` runs under WT the console API
  still works through condrv; if a future build enables VT input
  (`ENABLE_VIRTUAL_TERMINAL_INPUT`) the termcap/SGR path applies
  instead — out of scope, noted for completeness.

Build integration: `build.sh -w` already defines the flag plumbing
pattern (`COLOR`/`LISP`/`AI`); mouse needs no new compile flag, only the
`-m` runtime flag gating the `SetConsoleMode` calls.

## Lisp Integration

- `mouse` as a runtime settable variable plus a `toggle-mouse` command
  (enables/disables reporting without restarting).
- `mouse-wheel-step`, `mouse-osc52` variables for P1/P3 knobs.
- Expose `%(mouse-p)` style predicate only if a use case appears.

## Testing

- New `test/mouse.c` + `mouse-test` following the existing per-feature
  pattern (`display.c`/`display-test`): feed recorded byte sequences
  (SGR press/release/wheel/motion, X10 legacy, modifier combos, garbled
  partials) to the decoder, assert decoded fields. The Windows
  `INPUT_RECORD`-to-`mouse_*` mapping belongs in the same test as a
  pure function (record in, fields out) so it runs on Linux CI too.
- Suite: `./build.sh -b test clean all` plus
  `./test/mouse-test` alongside syntax/display/lisp tests.
- Manual matrix: xterm, st, rxvt (terminfo from `misc/term/`), inside
  dvtm, inside screen, inside tmux — click, drag, wheel, double-click,
  suspend/resume (`^Z`) must leave no stuck mouse mode.
- Windows: `./build.sh -w` then `./docker/run.sh wine` smoke test from
  the repository root (same version check as the standard suite);
  `./docker/run.sh wine gui` for interactive click/wheel/drag checks —
  the Wine console delivers real `INPUT_RECORD`s, so the mingw path is
  exercisable without Windows hardware. Confirm the console mode is
  restored after quit (no stuck QuickEdit-off state).
- Exit paths: normal quit, `panic()`, crash — the disable sequence (or
  console-mode restore) must always run (terminal reset hook), or the
  user's terminal is left with mouse reporting on after me dies.

## Risks

- Mode mismatch: if we enable 1006 but the terminal silently ignores it,
  partial sequences leak into the keymap. Mitigate with the allow list
  and the fallback chain; decoder treats `\033[<` that fails to parse as
  plain input.
- Flood: drag/motion can emit hundreds of events per second; coalescing
  is mandatory, not optional.
- Historical bug reference: symlinks aside, this codebase previously had
  circular-input issues around extended sequences — keep decoding in the
  tty layer only, never in `mouseevent()`.
