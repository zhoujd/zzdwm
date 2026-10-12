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
list operations and output primitives:

    (+ 1 2)
    (defun plus1 (x) (+ x 1))
    (plus1 2)
    (defmacro inc (x) (list '+ x 1))
    (inc 2)

Three forms also provide access to MicroEMACS state:

    (define-key "C-x C-b" "buffer-menu")
    (set-variable "tab-width" 8)
    (set-syntax "sql")

The first form binds a key to a MicroEMACS command. Key names support
the **C-X**, **C-** and **M-** prefixes followed by one ASCII character.
The second form sets **tab-width** (or **tab-size**) or **fill-column**
to a positive integer. A quoted string is also accepted for the value.
The third form selects a syntax definition for the current buffer. It is
available when color is enabled.
