---
title: "Lisp Extensions"
---

# Lisp Extensions

MicroEMACS includes a small built-in Lisp evaluator based on Tiny Lisp.
Start it with **M-X eval-lisp**; no key is bound to the command by default.
The evaluator accepts one expression at a time. Its environment persists
between expressions, so functions, macros and variables can be defined and
used in later expressions. It supports numbers, strings, symbols, lists,
lexical scopes, functions, macros, tail recursion, arithmetic, comparisons,
list operations, function references, and output primitives:

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
    (insert-string "text")
    (goto-bol)
    (goto-eol)
    (forward-line 2)
    (define-lisp-command "format-log-line" #'format-log-line)

The first form binds a key to a MicroEMACS command. Key names support
the **C-X**, **C-** and **M-** prefixes followed by one ASCII character.
The second form sets **tab-width** (or **tab-size**) or **fill-column**
to a positive integer. A quoted string is also accepted for the value.
The third form selects a syntax definition for the current buffer. It is
available when color is enabled.
The fourth form runs a MicroEMACS command by name, with an optional numeric
argument. It is intended for commands that do not require an echo-line reply;
use the direct buffer forms when a command would otherwise prompt for text.
The next three forms insert text at dot and move dot without replaying keys.
The final form registers a Lisp function as a named MicroEMACS command.
Use `#'name` to pass a function reference. For compatibility, a quoted
expression string is also accepted. The generated expression can be invoked
with **M-X**, or bound with `define-key`, and is limited to 243 bytes.

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
