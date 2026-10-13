
# TIP

## Extra Quality-of-Life Tips (logline-prefer.l) [DONE]

1. Interactive Count Support: By adding (&optional arg) and (interactive "p"), you can now type M-3 M-L in your editor to format exactly 3 lines on the fly during manual editing sessions, matching classic Emacs behaviors.
2. Buffer Traversal Safety: The (unless (eobp) ...) condition checks if the cursor has hit the end of the buffer. This blocks the script from wrapping lines or injecting infinite spacing fragments if your text file loops shorter than 3 lines.

## Evaluate a Lisp script on runtime using key combination (C-x F) [DONE]

`C-x F` now keeps its existing profile behavior, but files ending in
`.l`, `.lisp`, or `.mel` are evaluated as Lisp scripts. The explicit
`M-x read-lisp` command is also available.

## The Architecture: Exposing Editor Primitives to Lisp [DONE]
Implement this cleanly by mapping just four core primitive functions inside your C code to bridge into tiny-lisp:

```
// Example structural conceptual bridge to register inside tinylisp.c
{ "get-char",       me_getchar },    // Returns character under cursor
{ "insert-char",    me_insertchar }, // Inserts character at cursor
{ "get-line",       me_getline },    // Returns current line as a string
{ "editor-cmd",     me_editorcmd }   // Executes any internal MicroEMACS C string
```

## To add (message ...) support to embedded Lisp engine [DONE]

`(message "hello %s" "world")` now formats the message, displays it on the
echo line, and returns the formatted string. It supports `%s`, `%S`, `%d`,
and `%%`. The existing `echo` editor command remains available for profiles.

## Enabling the Native Floating-Point System in Lisp Extend
