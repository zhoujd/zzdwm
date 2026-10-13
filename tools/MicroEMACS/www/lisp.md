# Lisp Extensions

MicroEMACS includes a small built-in Lisp evaluator based on Tiny Lisp.
Start it with **M-X eval-lisp**; no key is bound to the command by default.
The evaluator accepts one expression at a time. Its environment persists
between expressions, so functions, macros and variables can be defined and
used in later expressions. It supports numbers, strings, symbols, lists,
lexical scopes, optional function parameters, functions, macros, tail
recursion, arithmetic, comparisons, list operations, function references,
`let`, `while`, `dotimes`, `unless`, and output primitives:

    (+ 1 2)
    (defun plus1 (x) (+ x 1))
    (plus1 2)
    (defmacro inc (x) (list '+ x 1))
    (inc 2)

These forms also provide access to MicroEMACS state and the current buffer:

    (define-key "C-x C-b" "buffer-menu")
    (set-variable "tab-width" 8)
    (set-syntax "sql")
    (execute-command "file-save")
    (get-char)
    (get-line)
    (insert-string "text")
    (insert-char "X")
    (save-buffer)
    (kill-emacs)
    (goto-bol)
    (goto-eol)
    (forward-line 2)
    (eobp)
    (define-lisp-command "format-log-line" #'format-log-line)

`define-key` binds a key to a MicroEMACS command. Key names support the
**C-X**, **C-** and **M-** prefixes followed by one ASCII character.
`set-variable` sets **tab-width** (or **tab-size**) or **fill-column** to a
positive integer; a quoted string is also accepted for the value.
`set-syntax` selects a syntax definition for the current buffer and is
available when color is enabled. `execute-command`, also available as
`editor-cmd`, runs a MicroEMACS command by name with an optional numeric
argument; use direct buffer forms when a command would otherwise prompt for
text.

`get-char` returns the UTF-8 character under dot as a string, or `nil` at the
end of a line. `get-line` returns the current line as a string without the
newline; the current line must fit in the host result buffer. `insert-string`
inserts a string at dot, while `insert-char` inserts one character given as a
Unicode code point or a one-character string. `goto-bol`, `goto-eol`, and
`forward-line` move dot without replaying keys. `save-buffer` and `kill-emacs`
perform those editor actions directly. `eobp` returns `t` when dot is at the
end of the buffer and `nil` otherwise.

`define-lisp-command` registers a Lisp function as a named MicroEMACS
command. Use `#'name` to pass a function reference. For compatibility, a
quoted expression string is also accepted. The generated expression can be
invoked with **M-X**, or bound with `define-key`, and is limited to 243 bytes.
An interactive Lisp command can declare `(interactive "p")`; the command's
numeric prefix is placed in its first optional parameter. For example, `M-L`
runs `format-log-line` once and `M-3 M-L` runs it three times.

## Startup File

MicroEMACS loads a Lisp startup file after the files named on the command
line and before commands from the startup profile are executed. The default
name is `.mel` on UNIX-like systems and `me.l` on Windows. MicroEMACS looks
for it in the current directory, then in the user's home directory.
If no default file is found, startup continues normally.

Use the **-l** option to load a specific Lisp file instead:

```sh
me -l startup.lisp file.c
```

The startup file may contain comments and multiple top-level expressions:

```lisp
; MicroEMACS Lisp startup file
(set-variable "tab-width" 8)
(set-variable "fill-column" 72)
(define-key "C-x C-b" "buffer-menu")
(defun double (x) (* x 2))
```

The `examples/logline.l` file is the Tiny Lisp equivalent of the
`examples/logline.pro` profile example:

```sh
me -l examples/logline.l filename
```
