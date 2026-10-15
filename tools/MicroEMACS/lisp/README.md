# MicroEMACS Lisp Libraries

This directory holds reusable Tiny Lisp libraries distributed with
MicroEMACS.

Use one of the `.l`, `.lisp`, or `.mel` extensions. A library can be loaded
explicitly with `(load "lisp/library.l")`.

- [`dired.l`](dired.l) — shell-free directory editor with symlink-aware file
  opening through `me-find-file`.
- [`smart-indent.l`](smart-indent.l) — contextual brace indentation.
- [`smart-find-file.l`](smart-find-file.l) — optional symlink-aware prompt and
  file-opening library that defines the Lisp-level `find-file` wrapper.
