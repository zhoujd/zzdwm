# MicroEMACS Examples

Run these examples from the `tools/MicroEMACS` directory. Build the host
editor first:

```sh
./build.sh -b
```

## Lisp editor bridge

`logline.l` defines a Tiny Lisp function, registers it as the
`format-log-line` command with `#'format-log-line`, binds it to `M-L`,
formats the three sample lines, saves the buffer, and exits. When used
interactively, `M-L` formats one line and `M-3 M-L` formats three lines; the
function stops at the end of the buffer.

Copy the input file before running the example so the source fixture is not
modified:

```sh
cp examples/logline-input.txt /tmp/logline.txt
./me -l examples/logline.l /tmp/logline.txt
cat /tmp/logline.txt
```

Expected result:

```text
	* first:
	* second:
	* third:
```

## Profile and keyboard macro

`logline.pro` records a keyboard macro, names it `format-log-line`, binds it
to `M-L`, applies it to the remaining sample lines, saves the buffer, and
exits.

```sh
cp examples/logline-input.txt /tmp/logline.txt
./me -p examples/logline.pro /tmp/logline.txt
cat /tmp/logline.txt
```

The expected result is the same as the Lisp example.

## Defensive Lisp variant

`logline-prefer.l` is the same editing example with explicit fallbacks for
`save-buffer` and `kill-emacs`. It is useful when a startup configuration may
not have registered those Lisp bridge commands.
