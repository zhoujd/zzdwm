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
| `me_hscroll.py` | Shift+wheel horizontal scroll, view pinning, no leaks |
| `me_shiftkey_e2e.py` | Shift+PageUp/PageDown bytes (st convention), with and without `-m` |
| `me_x10_e2e.py` | legacy X10 reports on `st-256color` and `linux` terminfo |
| `me_mouse_e2e.py` | click-to-dot, double-click word select, vertical wheel |
| `me_toggle_e2e.py` | `toggle-mouse` runtime reporting on/off |
| `me_dvtm_e2e.py` | me inside dvtm: reporting relay (`-m`, `toggle-mouse`), X10 wheel/click forwarding, scrollback skip (needs the dvtm binary in `../dvtm`, skips when absent) |

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
