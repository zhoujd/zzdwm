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
- The layout changes at run time, per buffer, and also sets the session
  defaults for dumps made later: `hexl-group-size` (M-X, or C-U n) takes
  `1`, `2`, `4`, `8` or `16` bytes per group, `hexl-endian` (M-X, or C-U 0
  for big, C-U 1 for little) the byte order within a group. Each re-lays
  the dump out in place through `hexlchange`, which parses every line
  under the old layout into a malloc'd byte array and rebuilds the lines
  under the new one, so unsaved edits survive. The relaying kills the undo
  history: undo records hold line contents, and they speak the old layout.
  A short trailing group is never reversed, so the blank suffix that pads
  the last line stays a suffix in either order. The hex area is typed over
  by display slot — the dot steps to the slot on the right, which in
  little-endian is the next lower byte — while the ASCII area keeps the
  file order in any layout.
- The group display follows `xxd`, Vim's hex dumper: as its `-g` option
  packs each group into one hex number (`xxd -g 8` shows
  `48656c6c6f2c2057 6f726c64210a0001`), a group here reads as the value
  it holds — a word, a dword, a qword, an octaword. The padding around
  the groups, rather than xxd's single spaces, is `fileview.c`'s
  (`tools/utils/fileview`), which gives each byte three columns so the
  hex area stays 48 wide and the line width (76) and the ASCII column
  (60) never move, whichever size is set. The GNU layout the dump opens
  in — spaced pairs, two groups of eight — is left once a group size is
  set, and no command returns to it; killing the buffer and dumping the
  file anew does.
- `file-read` into a hexl buffer clears the hexl flag and mode, as the
  text it reads makes the buffer a text buffer again.

The file I/O is raw stdio in hexl.c, as the ffopen layer is line oriented
and a binary file is not lines.

Covered by `test/python/me_hexl_e2e.py`.

Not carried over from GNU: no `exit-hexl-mode` conversion back to a text
buffer (the buffer is the dump; kill it or read the file with `C-X C-F`
to edit it as text), and no byte insertion or deletion — a file dumps and
saves at the same length.

# Big files

`hexlfill` refuses a file over `HEXLLIMIT` (8 MB) before reading a byte:
a dump of one would hold over half a million lines, more than an editor
of this design is meant to move through. A file that will not seek (a
pipe, a character device) is refused the same way, as the dump reads the
whole file up front.

The way out, when a big file must be edited, is to dump a window of it:
give the buffer a base offset, dump `HEXLLIMIT` bytes from there, save
by opening `r+b`, seeking to the base and writing without truncating,
and page the window forward and back over the file with a pair of
commands, with `hexl-goto-address` sliding the window when the address
falls outside it. The offset column would carry the base, so addresses
stay file-wide. Not built; the guard above holds the fort until it is.
