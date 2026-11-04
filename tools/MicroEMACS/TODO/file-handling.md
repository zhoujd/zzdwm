# File Handling

Status: the shell filters below remain the read-only inspection path;
native Hexl editing has since been implemented, reopening this proposal
for the editing half. See [`hexl-mode.md`](hexl-mode.md).

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

## Closed Proposal

The original proposal described an integrated C Hexl mode with display-only
endianness switching and reverse reconstruction through `sscanf`. It was
closed because:

- external `xxd` already supplies the requested read-only views;
- text-oriented MicroEMACS buffers are not a safe editing representation for
  arbitrary NUL-containing binary data;
- reverse writing requires a strict canonical format and robust error
  handling for arbitrary layouts and partial rows;
- native support would increase the static executable despite the proposal's
  footprint goal.

The reopening came when direct binary patching from inside MicroEMACS
became the requested workflow; the implemented mode answers the closure
concerns by keeping the buffer as plain ASCII dump text (no NULs), one
fixed-width canonical line format, and a save that validates every line
before touching the file.

Reference code:
zzdwm/tools/utils/fileview/fileview.c
zzdwm/tools/utils/liveview/liveview.c
