# File Handling

Status: the native mode of [`hexl-mode.md`](hexl-mode.md) covers the
Hexl mode editing original proposal, and its layout commands —
`hexl-group-size`, `hexl-endian`, `hexl-packed` — give the dump the
inspection views the shell filters below were kept for; the filters
remain the path for files over the dump's 8 MB limit.

---

## Binary Inspection Through Shell Filters

A native Hexl mode is unnecessary for read-only binary inspection. MicroEMACS
already provides **C-X @** (`pipe-command`), which runs a shell command and
displays its output in the `*pipe*` buffer. On systems with Vim's `xxd`
utility, use one of:

```text
C-X @ xxd -g 1 /path/to/file RET
C-X @ xxd -g 2 /path/to/file RET
C-X @ xxd -e -g 4 /path/to/file RET
```

This covers byte, grouped-word, and little-endian-style viewing without adding
binary-safe buffer editing, endian state, reverse hex parsing, or extra code
to the editor.

The implemented mode now gives these views on the editable dump itself:
`hexl-group-size` sets the group, `hexl-endian` the byte order within it,
and `hexl-packed` switches between the packed groups and the GNU layout
the dump opens in, each re-laying the dump in place at run time. The
filters keep their place for a file the dump refuses over 8 MB.

## Reference code

zzdwm/tools/utils/fileview/fileview.c
zzdwm/tools/utils/liveview/liveview.c
zzdwm/tools/utils/hexl/hexl.c (GNU's converter, source of the layout name)
