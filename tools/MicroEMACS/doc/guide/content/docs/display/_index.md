---
weight: 24
bookFlatSection: true
title: "Display Features"
---

# Display Features

MicroEMACS can display optional colors, syntax highlighting, and line
numbers. These features are compiled into the current Linux and Windows
builds and are designed to add little overhead to the editor's small
executable size.

## Colors

Color support is compiled in with the `COLOR` macro. The Linux build uses
ncursesw or termcap colors, and the MinGW build uses the native Windows
console API. Mode lines are rendered with a distinct color when color
output is active.

Start MicroEMACS with `-C` to disable color for a session:

```
me -C filename
```

The `set-color` extended command toggles color while the editor is
running. It is not bound to a key. Provide a numeric argument of zero to
turn color off, or a nonzero numeric argument to turn it on.

The current implementation preserves double-width CJK characters on the
Windows console, including lines that contain syntax highlighting.

## Syntax Highlighting

Syntax highlighting is selected automatically from the file extension or,
for executable scripts, from the interpreter named in the shebang line.
The built-in definitions cover:

* C: `.c`, `.h`
* C++: `.cpp`, `.cc`, `.cxx`, `.C`, `.hpp`, `.hh`, `.hxx`, `.ipp`, `.tcc`
* Bash: `.sh`, `.bash`, `.bashrc`, `.bash_profile`, `.profile`, and `bash` or `sh` shebangs
* Python: `.py`, `.pyw`, `.pyi`, and `python` shebangs
* Markdown: `.md`, `.markdown`, `.mdown`, `.mkd`
* Lisp: `.lisp`, `.lsp`, `.l`, `.cl`, and `sbcl`, `clisp`, or `lisp` shebangs
* Emacs Lisp: `.el`, `.emacs`, and `emacs` shebangs

The renderers support block comments, line comments, strings, keywords,
preprocessor directives, shell variables, Python triple-quoted strings,
Markdown fenced code, and Lisp block comments. Multi-line constructs are
highlighted consistently while scrolling.

Use the `c-mode`, `cpp-mode`, `bash-mode`, `python-mode`, `markdown-mode`,
`lisp-mode`, or `emacs-lisp-mode` extended command to select syntax for the
current buffer explicitly. Use `text-mode` to clear that selection and return
to automatic detection. These commands are not bound to keys.

Only the lines currently visible in each window are rendered with syntax
colors, and syntax state is cached to avoid redundant rescanning.

## Line Numbers

Start MicroEMACS with `-n` to display Vim-style line numbers:

```
me -n filename
```

The `set-number` extended command toggles line numbers while the editor is
running. Provide a numeric argument of zero to turn line numbers off, or a
nonzero numeric argument to turn them on. The gutter uses a compact width
based on the current line-number width and refreshes when that width
changes.
