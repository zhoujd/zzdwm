# Legacy X10 Mouse Testing

me enables mouse reporting by writing modes 1000 (press/release), 1002
(drag), and 1006 (SGR encoding) when mouse support is on (`./me -m` or
`toggle-mouse`).  Terminals that implement 1000 but not 1006 — older rxvt
builds, an xterm with SGR disallowed, the Linux console behind GPM —
answer in the legacy X10 encoding instead, so the ncurses backend decodes
both (`mouse_decode_x10` in `tty/ncurses/ttykbd.c`).  This note records
how to exercise that decoder without hunting down an old terminal.

## Wire format

An X10 report is six bytes with no terminator:

    ESC [ M  <button>  <column>  <row>

* The prefix `ESC [ M` (`\033[M`) is the same three bytes terminfo
  advertises as `kmous`, which is why ttykbd.c claims the sequence with
  `define_key` before ncurses can swallow the prefix plus the three data
  bytes and return an undecodable `KEY_MOUSE`.
* Each data byte is raw (not ASCII digits): the value plus 32.
* `<button>` holds the button in the low bits (0 = left, 1 = middle,
  2 = right), 4 = Shift, 8 = Alt, 16 = Ctrl, 32 = motion, 64 = wheel.
* `<column>` then `<row>` are 1-based, counted from the top left of the
  terminal.
* There are no release reports.  A press is the whole gesture, so the
  decoder queues it as one complete `MOUSE_CLICK` and counts consecutive
  presses on one cell itself.
* Coordinates past 223, or any data byte above 127, are unreliable in
  UTF-8 terminals — that is the reason the SGR encoding exists.  The
  examples below stay in the low bytes.

## Byte cheat sheet

| Action                | Button bits | `<button>` byte |
|-----------------------|-------------|-----------------|
| Left press            | 0           | `\040`          |
| Middle press          | 1           | `\041`          |
| Right press           | 2           | `\042`          |
| Wheel up              | 64          | `\140`          |
| Wheel down            | 65          | `\141`          |
| Shift+wheel up        | 68          | `\144`          |
| Shift+wheel down      | 69          | `\145`          |

Compute coordinate bytes with `printf '\0%o' $((32 + col))` for 1-based
`col` and `row`.

## Method 1: inject reports through tmux

`tmux send-keys` writes into the pane's pty master — exactly the path a
real terminal's reports take, bypassing tmux's own input parsing, so the
bytes arrive verbatim.  This method was used to verify the decoder
end to end.

Make a fixture with a word worth selecting and lines below the top for
the wheel to move, then start me in a session:

    for i in $(seq 30); do echo "wheel scroll line $i with words to select"; done > /tmp/x10fix.txt
    cd tools/MicroEMACS
    tmux new-session -d -s x10 'TERM=xterm-256color ./me -m /tmp/x10fix.txt'
    tmux attach -t x10

From another shell (or after detaching with `C-b d`):

    # wheel down at column 5, row 2 (1-based)
    tmux send-keys -t x10 -l "$(printf '\033[M\141\045\042')"
    # left click at column 10, row 2
    tmux send-keys -t x10 -l "$(printf '\033[M\040\052\042')"
    # double-click: the identical report again (the decoder counts presses)
    tmux send-keys -t x10 -l "$(printf '\033[M\040\052\042')"
    # triple-click: a third time
    tmux send-keys -t x10 -l "$(printf '\033[M\040\052\042')"

To verify what me draws, capture its output and look for the selection
color cells:

    tmux pipe-pane -t x10 'cat > /tmp/x10.log'
    # ...click...
    grep -c '48;5;24' /tmp/x10.log

    tmux kill-session -t x10

### Pitfall: /dev/pts is the output side

Writing a report to me's pty slave — `printf '\033[M...' > /dev/pts/4` —
sends the bytes to the terminal, not to the editor: tmux executes them
(`CSI M` is reverse-index, the rest overwrite screen text) and me
receives nothing.  Input must arrive through the pty master, which is
what send-keys does.

## Method 2: a real terminal that answers in X10

Stock xterm can be pinned to X10 for the session:

    xterm -xrm '*disallowedMouseOps: SGR' -xrm '*shiftEscape: always' \
          -e ./me -m /tmp/x10fix.txt

me still requests mode 1006; xterm refuses it and answers modes 1000 and
1002 in X10 bytes, so a real mouse exercises the same decoder.

`shiftEscape` matters for the shifted wheel: by default xterm reserves
Shift to override the mouse protocol and use its own selection and
scrollback, so Shift+wheel sends nothing to the application in any
encoding (the same terminal policy that makes st translate Shift+wheel
into the Shift+PageUp/PageDown keys).  `always` lets the report through
with the Shift bit set, which arrives as the `\144`/`\145` bytes of the
cheat sheet.

## Method 3: the Linux console and GPM (historical)

The classic X10-only environment is the Linux console with GPM repeating
mouse events to console applications.  It was not exercised for this
port; the decoder treats it as any other mode-1000 terminal.

## What to expect

* A plain left click moves point to the clicked cell; the click streak
  resets when a press lands on another cell.
* The second press on one cell selects the word under it (or the run of
  blanks); a third selects the whole line.  The highlight uses the
  selection color (`set-selection-color`; bright white on deep blue,
  `48;5;24`, by default) and lasts until the next non-mouse key.
* A middle click yanks the kill buffer at the cell; a right click
  extends the region.
* The wheel scrolls the window under the mouse by `mouse-wheel-step`
  lines per tick (3 by default), clamped at the start and end of the
  buffer; wheel up at the top of the file is a no-op.
* Shift+wheel scrolls horizontally by the `scroll-left`/`scroll-right`
  amount (four columns), clamped at column zero and the widest visible
  line.
* Drag selection is SGR-only: with no release report there is no
  press-anchored drag, which is why a press is reported as a complete
  click instead.

The core calling pattern the decoder relies on is pinned by the X10
block in `test/display.c` (wheel and click bytes run through
`mousedecodesgr`); the paths above cover what that block cannot — the
terminfo prefix claim and the raw-byte read.  A pty harness like
`test/python/common.py` can automate Method 1 later.
