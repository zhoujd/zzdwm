# Lisp Extensions

MicroEMACS includes a small built-in Lisp evaluator. Start it with
**M-X eval-lisp**; no key is bound to the command by default.
The evaluator accepts one expression at a time and supports two forms:

    (define-key "C-x C-b" "buffer-menu")
    (set-variable "tab-width" "8")

The first form binds a key to a MicroEMACS command. Key names support
the **C-X**, **C-** and **M-** prefixes followed by one ASCII character.
The second form sets **tab-width** (or **tab-size**) or **fill-column**
to a positive integer.
