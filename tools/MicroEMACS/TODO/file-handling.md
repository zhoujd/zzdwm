# File Handling

Status: closed — use the existing shell pipe integration for binary inspection.

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

Revisit native binary support only when direct binary patching from inside
MicroEMACS becomes a required workflow.

Reference code:
zzdwm/tools/utils/fileview/fileview.c
zzdwm/tools/utils/liveview/liveview.c
