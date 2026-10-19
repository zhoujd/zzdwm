
# Lisp Extensions

---

## Extra Quality-of-Life Tips (logline-prefer.l) [DONE]

1. Interactive Count Support: By adding (&optional arg) and (interactive "p"), you can now type M-3 M-L in your editor to format exactly 3 lines on the fly during manual editing sessions, matching classic Emacs behaviors.
2. Buffer Traversal Safety: The (unless (eobp) ...) condition checks if the cursor has hit the end of the buffer. This blocks the script from wrapping lines or injecting infinite spacing fragments if your text file loops shorter than 3 lines.

---

## Evaluate a Lisp script on runtime using key combination (C-x F) [DONE]

`C-x F` now keeps its existing profile behavior, but files ending in
`.l`, `.lisp`, or `.mel` are evaluated as Lisp scripts. The explicit
`M-x read-lisp` command is also available.

---

## The Architecture: Exposing Editor Primitives to Lisp [DONE]
The editor bridge is registered in `lisp.c` with `tiny_lisp_define_host`.
The original four-primitive concept evolved into direct buffer operations plus
the command bridge:

```
{ "get-char",        lisp_get_char },        // Character under dot
{ "insert-char",     lisp_insert_char },     // One character at dot
{ "get-line",        lisp_get_line },        // Current line as a string
{ "execute-command", lisp_execute_command }, // Run an editor command
{ "editor-cmd",      lisp_execute_command }  // Alias for execute-command
```

---

## Captured Shell Commands [DONE]

`shell-command-to-string` runs one command line through the system shell and
returns its standard output as a Lisp string. It returns `nil` when spawning
or capturing fails, including when output exceeds the Lisp result limit. A
command's nonzero exit status does not hide valid output, and scripts can use
`2>&1` when error text is needed. The primitive does not add automatic AI
command execution; Lisp scripts and bindings must call it explicitly.

---

## Enabling (message ...) support to embedded Lisp engine [DONE]

`(message "hello %s" "world")` now formats the message, displays it on the
echo line, and returns the formatted string. It supports `%s`, `%S`, `%d`,
and `%%`. The existing `echo` editor command remains available for profiles.

---

## Enabling the Native Floating-Point System in Lisp Extend [DONE]

Tiny Lisp already stores numbers as native `double` values. Decimal literals,
arithmetic, and comparisons work without an additional numeric subsystem; for
example, `(+ .1 .2)` evaluates to `0.3`.

---

## Lisp-Driven Auto-Indentation Pipeline [DONE]

Language-specific indentation rules now live in Lisp while the C core provides
only compact generic primitives.

### Implementation Notes

- `editor-cmd` is only an alias for `execute-command`; both names call the same
  host function. No separate `editor-command` primitive is needed.
- `get-line` accepts an optional relative line offset, so indentation scripts
  can scan surrounding lines without moving dot.
- `count-char` is a Tiny Lisp numeric primitive and keeps brace counting out
  of language-specific C code.
- `forward-line` returns Lisp `nil` at a buffer boundary instead of raising a
  host-command error.
- `insert-string` inserts a newline and accepts a repeat count, so indentation
  uses direct buffer primitives instead of replaying editor commands.

The runnable library is `lisp/smart-indent.l`, with the test
fixture `examples/smart-indent-input.c`. It defines `smart-indent`, registers
it as a MicroEMACS command, and binds **Enter** (`C-m`).

```lisp
(defun smart-indent-depth ()
  "Count unmatched opening braces on this and preceding lines."
  (let ((open-brackets 0)
        (closed-brackets 0)
        (lines-to-scan 20)
        (scanned-lines 0))
    (while (and (< scanned-lines lines-to-scan)
                (get-line (- scanned-lines)))
      (let ((line-text (get-line (- scanned-lines))))
        (setq open-brackets
              (+ open-brackets (count-char line-text "{")))
        (setq closed-brackets
              (+ closed-brackets (count-char line-text "}"))))
      (setq scanned-lines (+ scanned-lines 1)))
    (let ((depth (- open-brackets closed-brackets)))
      (if (< depth 0) (setq depth 0))
      depth)))

(define-lisp-command "smart-indent" #'smart-indent)
(define-key "C-m" "smart-indent")
```

---

## C-Driven Package Introspection [DONE]

Implemented in the current working tree.

### Native `provide` and `require`
`provide` is a public Lisp macro that expands into a native host bridge named
`%host-provide` plus an update to the Lisp-visible `features` list. The `%`
prefix is only a convention marking the host bridge as internal; it is not
special syntax. The native bridge records the feature in the C module cache,
so a later `require` skips loading the module again. `dired.l` now provides
`dired`.

### Native `M-x list-features`
`list-features` displays the C module cache in the standard pop-up buffer.
This avoids walking Lisp cells and makes the loaded feature names directly
inspectable.

Validated with `lisp-test`, the source test build, and standard/minimal Linux
builds.

---

## Aligned Lisp Feature Columns [DONE]

`M-x list-features` keeps its `Feature` heading but now formats feature names
with the same 16-column grid used by the autocomplete choice pop-up in
`addchoice`. Rows wrap to the display width rather than placing every feature
on its own line.

Validated with `lisp-test`.
