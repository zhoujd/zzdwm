# Enhance Mouse Support

Status: in progress — P0 through P3 implemented on the ncurses
backend: click to move point, mode-line click selects window,
wheel scrolls the window under the mouse by the Lisp-settable
`mouse-wheel-step` lines, and left-drag selects the
mark-to-point region (the press anchors the mark right away and the
release position completes the region, so drags work even on
terminals that report no motion while the button is held).  The
region is drawn in the selection highlight color while the selection is
active (CSELECT per-cell attribute; COLOR builds only — color pair 6,
defaults 15 24 7 4 = bright white on deep blue, settable with the
set-selection-color Lisp form; other builds use reverse video), and any
non-mouse key ends the highlight (endmouseselect, transient like
Emacs).  Double click selects the word at the click (or the run
of blanks), triple click selects the line (ncurses counts the
clicks; mouse_clicks), middle click yanks the kill buffer at the
clicked cell, and right click extends the region to the clicked
cell.  Copy-region and kill-region also send the region to the
system clipboard with OSC 52 (osc52send in display.c, gated by
the `osc52` Lisp variable, on by default; regions over a million
characters are skipped), and bracketed paste (mode 2004) is
always enabled on the ncurses backend: pasted text inserts as
one unit and one undo step (bracketedpaste in random.c), with
function keys and other non-text keys dropped.  Shift+wheel scrolls
horizontally in steps of four columns: the window is pinned
(w_hscroll) so update()'s automatic recentring around dot does not
undo the scroll, the pin drops when dot next moves or is edited
(forwchar/backchar now set WFMOVE for same-line motion too), and
the physical cursor is parked at the window edge while dot is
scrolled off view.  The mouse transport is now owned by the editor
on the ncurses backend: ttopen enables modes 1000/1002/1006 itself
and an SGR decoder (mouse_decode_sgr) replaces ncurses' decoding,
which cannot express modified wheel events.  Terminals that bypass
mouse reporting while Shift is held (st, whose forcemousemod routes
Shift+wheel to Shift+PageUp/PageDown keys) are covered too: those
byte sequences are claimed ahead of terminfo with define_key and
decoded back into wheel events anchored at the cursor.  Legacy X10
reports ("\033[M" plus three raw bytes) are decoded as well, so
terminals that honor mode 1000 but not 1006 — the Linux console,
older rxvt — keep click and wheel support, and terminfo kmous
entries can no longer swallow mouse bytes into an undecodable
KEY_MOUSE.  The P4 terminal gate is done by this decode coverage
(no allow list needed) and the multiplexer passthrough notes are in
the manual's -m entry.  The mouse interactions are also strippable
at build time now: MOUSE=no (Make.Linux/Make.Mingw, default yes,
same pattern as COLOR/LISP/AI) compiles out the reporting, the
decoders, and the click/drag/select/paste interactions — what
remains is the wheel-and-scroll spine, so the shifted page keys
still scroll horizontally through the wheel-only dispatcher
(wheelevent) bound to KMOUSE, and `toggle-mouse` turns reporting
off and back on without a restart.  Pending:
P5.
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
- Shift+wheel: horizontal scroll by 4 columns — done.  The scroll sets
  `w_hscroll` on the window so the automatic recentring around dot
  (display.c `update`) leaves the view alone until dot next moves, is
  edited, or a reframe is forced; scrolling right is clamped against
  the widest visible line, and the cursor is parked at the window edge
  while dot sits outside the view.
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

### P3 — Clipboard and paste interop — done

- OSC 52 (`\033]52;c;BASE64\007`) copy of the selected region, so
  selection lands in the system clipboard even over ssh. Gate behind a
  variable (some terminals/pagers refuse OSC 52).  Done: copy-region
  and kill-region call `osc52send` (display.c) after `getregion`;
  the variable shipped as `osc52` (default 1) since the copy also
  fires on plain `M-W`/`^W` with no mouse involved.  The sequence is
  written raw by `ttosc52` in each tty backend (a no-op on the
  Windows consoles).
- Bracketed paste (mode 2004) as a separate toggle: pasted text inserts
  as one unit instead of replaying keystrokes.  Done on the ncurses
  backend: `ttopen` sends `\033[?2004h` unconditionally (no mouse flag
  needed) and `ttclose` sends `\033[?2004l`; the markers are taught to
  curses with `define_key` and decoded in `getkbd` as `KPASTE` with
  `pastestart` distinguishing open from close.  The termcap backend
  does not enable the mode (it has no decoder for the markers, so they
  would leak into the keymap).

### P4 — Terminal gate and multiplexer passthrough

Partially pulled forward by the Shift+wheel work: the ncurses backend
no longer uses ncurses' mouse decoding at all.  `ttopen` enables
modes 1000/1002/1006 with a raw write (like bracketed paste), the
fixed `\033[<` SGR prefix is taught to curses with `define_key`, and
`mouse_decode_sgr` (display.c, a pure function with the click-count
clock injectable for tests) turns the parameters into the shared
`mouse_*` fields — ncurses' own decoder mangles X10 modifier bits on
wheel events and garbles SGR sequences (tested on ncurses 6.4).

The gate is now done by decoding instead of by `TERM` inspection —
no allow list or capability probe.  Every terminal is safe to enable
reporting on: SGR terminals answer in SGR, terminals that honor only
mode 1000 answer in legacy X10 bytes, which `mouse_decode_x10`
(ttykbd.c) decodes with the same `mouse_decode_sgr` semantics (a
press is completed as a click right away, since X10 has no release
event), and terminals without any mouse support ignore the modes and
send nothing.  The `\033[M` prefix is claimed with `define_key`
ahead of terminfo `kmous`, which would otherwise eat the prefix plus
three data bytes and return an undecodable `KEY_MOUSE` (probed on
ncurses 6.4).  The old `strncmp(TERM, "xterm", 5)` gate lives only
in the legacy termcap backend, which `build.sh` does not build.
Passthrough is documented in the `-m` entry of the manual and
www/starting.md: tmux `set -g mouse off`, GNU screen `mousetrack
off`.  dvtm relays by itself now: it eats the editor's DECSET 1000
rather than passing it through, but when any client tracks the
mouse it enables reporting on the outer terminal (recomputed as
client output is processed, so me's runtime `toggle-mouse` opens
and closes the relay), its ncurses decodes the reports (terminfo
`kmous=\E[M` under `st-256color`), and `vt_mouse` re-encodes them
as X10 for the client, with the local button bindings skipped in
forwarding-only mode.  dvtm's wheel forwarding (BUTTON4/5 → X10
buttons 64/65), the scrollback-binding skip, and the one-based X10
coordinates shipped in the zzdwm dvtm tree (2026-10).  All covered
by test/python/me_dvtm_e2e.py: relay on `-m` and on `toggle-mouse`,
closed on quit and on toggle off, no byte leak, wheel scrolls me,
clicks land exactly, and the local scrollback no longer steals the
wheel.

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
  (enables/disables reporting without restarting).  Done: the
  `toggle-mouse` command flips reporting at runtime (no prefix flips,
  zero turns it off, any other prefix turns it on; ttsetmouse in each
  tty backend writes the modes), reachable from Lisp as
  `(execute-command "toggle-mouse")`.  The `mouse` variable itself
  stayed a plain global rather than a `set-variable` knob: the table
  has no hook for the terminal-mode side effect, so a plain set would
  silently desync the flag from the terminal.  Also shipped: the
  horizontal scroll as the `scroll-left`/`scroll-right` commands
  (default four columns, prefix argument overrides),
  settable `mouse-wheel-step`/`osc52`, and the selection highlight
  color as `set-selection-color` (COLOR+MOUSE builds, same four
  arguments as `set-modeline-color`).  A `MOUSE=no` build drops
  `mouse`, `-m`, `toggle-mouse`, and `set-selection-color` entirely.
- `mouse-wheel-step` for the P1 knob and `osc52` for P3 (the planned
  `mouse-osc52` shipped as `osc52`: the copy fires on plain region
  commands too, not just mouse selections).
- Expose `%(mouse-p)` style predicate only if a use case appears.

## Testing

- New `test/mouse.c` + `mouse-test` following the existing per-feature
  pattern (`display.c`/`display-test`): feed recorded byte sequences
  (SGR press/release/wheel/motion, X10 legacy, modifier combos, garbled
  partials) to the decoder, assert decoded fields. The Windows
  `INPUT_RECORD`-to-`mouse_*` mapping belongs in the same test as a
  pure function (record in, fields out) so it runs on Linux CI too.
- Suite: `./build.sh -b test clean all` plus
  `./test/mouse-test` alongside syntax/display/lisp tests.  The pty
  end-to-end scripts that drive the real binary over the wire live in
  `test/python/` (see its README); they are run manually after a build.
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
