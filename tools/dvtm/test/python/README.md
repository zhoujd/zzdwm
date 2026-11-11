# Python end-to-end tests

These scripts drive the real `../../dvtm` binary in a pseudo-terminal:
raw bytes (keys, X10 mouse reports) go in through the master side,
everything dvtm writes comes back out, and the assertions check the
wire bytes — often fragments, as ncurses writes only the changed cells.

| Script | Covers |
| :--- | :--- |
| `mouse_indicator.py` | the border's `(M)` and `(m)` mouse indicators — the global `MOD+M` grabbing and the window's own mouse tracking — beside the layout symbol and alone |

The scripts are self-contained: each builds its own scratch files and
skips when a needed binary is absent. `mouse_indicator.py` also needs a
built MicroEMACS `me` in `../../../MicroEMACS`, whose `toggle-mouse`
flips the tracking under test; it asserts on whole border lines by
sending `MOD+CTRL('L')` first — dvtm's `redraw` repaints the terminal
completely, where a plain update would stream only changed cells.

Run them after building:

```sh
make -C ../..
for s in *.py; do python3 "$s" || break; done
```

The terminfo entry `st-256color` must be installed.
