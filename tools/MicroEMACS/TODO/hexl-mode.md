# Hexl Mode

Status: implemented.

Byte editing in a hex dump, in the manner of GNU's hexl mode. The buffer
holds the dump itself — an eight digit offset, sixteen bytes in hex in two
groups of eight, and the printable characters beside them, one line per
sixteen bytes of the file — so the display engine needs no changes: the
dump is text, and undo, regions, and search work on it as text.

- `C-X C-H` (`hexl-find-file`) reads a file into a dump buffer, named for
  the file, with the mode line showing `(hexl)`. A file already dumped is
  revisited. The mainline `-H` option opens the file arguments this way on
  startup, through `hexlread`, which shares `hexlfill` with the command.
- Editing goes through `selfinsert`, which routes printable keys to
  `hexlself` on a hexl buffer: two hex digits in the hex area write the
  byte, a character in the ASCII area writes its own value, both columns
  follow each other, and TAB steps to the next byte. The blank slots that
  pad the last line take nothing, so a file cannot grow or shrink.
- `filesave` delegates to `hexlsave` on a hexl buffer, which parses the
  hex back out and writes raw bytes; `file-write` is refused. Every line
  is validated before the file is touched, so damage from commands that
  are not hexl aware stops the save with the file as it was.
- `hexl-goto-address` (M-X) moves dot to a byte by its offset, decimal or
  `0x` hex.
- `file-read` into a hexl buffer clears the hexl flag and mode, as the
  text it reads makes the buffer a text buffer again.

The file I/O is raw stdio in hexl.c, as the ffopen layer is line oriented
and a binary file is not lines.

Covered by `test/python/me_hexl_e2e.py`.

Not carried over from GNU: no `exit-hexl-mode` conversion back to a text
buffer (the buffer is the dump; kill it or read the file with `C-X C-F`
to edit it as text), and no byte insertion or deletion — a file dumps and
saves at the same length.
