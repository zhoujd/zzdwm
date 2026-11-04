# Hexl Editing

**C-X C-H** (**hexl-find-file**)

Prompt for a file name, then read the file as a hex dump: one line per
sixteen bytes, an eight digit offset, the bytes in hex in two groups of
eight, and the printable characters beside them, with `.` for a byte that
is not printable. The buffer takes the name of the file, the mode line
shows `(hexl)`, and a file already dumped is revisited, as **C-X C-F**
revisits its buffer. The ASCII area is display only; saving reads the hex.
Invoking MicroEMACS as `me -H file` opens the file this way on startup.

    00000000: 48 65 6c 6c 6f 2c 20 57  6f 72 6c 64 21 0a 00 01  Hello, World!...

The blank slots that pad the last line hold no bytes, so a file always
dumps and saves at the same length.

Bytes are edited by typing over them, in either column. In the hex area
two hex digits write the byte, and the first stays on display until the
second arrives; in the ASCII area a character writes its own value. Both
columns follow each other, and the dot steps to the next byte once a byte
is written; **TAB** steps to the next byte without writing. **C-X C-S**
saves the dump by parsing the hex back out of the lines and writing the
file as raw bytes, and it is that save which the **quit** and
**file-save-all** commands carry out on a hexl buffer; **C-X C-W** is
refused, as it would write the dump instead of the bytes. Commands that
are not hexl aware, as backspace and the kills, can damage a line; the
save checks every line before the file is touched and refuses a damaged
one by its number, and **undo** puts the line back.

The **hexl-goto-address** extended command (M-X) prompts for a byte's
offset in the file, as decimal or as hex with a `0x` prefix, and moves
dot onto it; a prefix argument is taken as the address.
