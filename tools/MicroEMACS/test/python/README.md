# Python end-to-end tests

These scripts drive the real `./me` binary in a pseudo-terminal: raw
bytes (mouse reports, key sequences) go in through the master side,
everything the editor writes comes back out, and the assertions check
both the wire bytes and the files the editor leaves behind. They cover
what the C tests cannot: the tty layer end to end, from terminal input
escape sequences to terminal output modes.

| Script | Covers |
| :--- | :--- |
| `me_paste_osc52.py` | bracketed paste as one unit, OSC 52 copy-region |
| `me_replace_region.py` | query replacement within a Unicode region |
| `me_hscroll.py` | Shift+wheel horizontal scroll, view pinning, no leaks |
| `me_shiftkey_e2e.py` | Shift+PageUp/PageDown bytes (st convention), with and without `-m` |
| `me_x10_e2e.py` | legacy X10 reports on `st-256color` and `linux` terminfo |
| `me_mouse_e2e.py` | click-to-dot, double-click word select, vertical wheel |
| `me_toggle_e2e.py` | `toggle-mouse` runtime reporting on/off |
| `me_file_e2e.py` | save-all through C-X S: the argument form writing every changed buffer, the prompted tour answering per buffer, C-G stopping it, and the nothing-to-save report |
| `me_hexl_e2e.py` | hexl through C-X C-H and the -H startup option: the dump layout, bytes written by hex pairs, ASCII characters, and addresses, the refused blank slots, and the save refusing a damaged line |
| `me_rect_e2e.py` | rectangle kill/yank/clear/open/string through the C-X r prefix, the anchor highlight, mark-to-dot corners with the prefix's preview of them, and a shifted or control mouse drag selecting the rectangle |
| `me_search_e2e.py` | i-search on C-S/C-R/M-S, RET falling into the non-incremental search |
| `me_dvtm_e2e.py` | mouse forwarding through dvtm, with and without its own mouse handling |

`common.py` is the shared pty harness, not a test. The tests are
self-contained otherwise: each builds its own scratch files and can be
run alone. They are not part of `build.sh -b test`, because they need a
built Linux binary and a pty-capable environment.

Run them after building:

```sh
./build.sh -p
for s in test/python/me_*.py; do python3 "$s" || break; done
```

The terminfo entries used (`st-256color`, `linux`) must be installed;
`TERM=xterm-256color` is the default for the rest.
