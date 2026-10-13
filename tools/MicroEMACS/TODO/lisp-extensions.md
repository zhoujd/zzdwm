
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
Implement this cleanly by mapping just four core primitive functions inside your C code to bridge into tiny-lisp:

```
// Example structural conceptual bridge to register inside tinylisp.c
{ "get-char",       me_getchar },    // Returns character under cursor
{ "insert-char",    me_insertchar }, // Inserts character at cursor
{ "get-line",       me_getline },    // Returns current line as a string
{ "editor-cmd",     me_editorcmd }   // Executes any internal MicroEMACS C string
```

---

## To add (message ...) support to embedded Lisp engine [DONE]

`(message "hello %s" "world")` now formats the message, displays it on the
echo line, and returns the formatted string. It supports `%s`, `%S`, `%d`,
and `%%`. The existing `echo` editor command remains available for profiles.

---

## Enabling the Native Floating-Point System in Lisp Extend [DONE]

Tiny Lisp already stores numbers as native `double` values. Decimal literals,
arithmetic, and comparisons work without an additional numeric subsystem; for
example, `(+ .1 .2)` evaluates to `0.3`.

---

## Lisp-Driven Auto-Indentation Pipeline

To decouple syntax rules from our core C binary, we implement language-agnostic indentation frameworks directly within the runtime Lisp interpreter loop.

### How It Coordinates With Core Primitives
1. **`editor-cmd`**: Seamlessly moves the cursor across line boundaries behind the scenes to scan surrounding blocks.
2. **`get-line`**: Extracts historical text lines straight from the active editor buffer to evaluate code structures.
3. **`insert-char`**: Rapidly stream-injects exact padding spaces into the current line point.
4. **`message`**: Logs diagnostic calculation matrices straight down to the user's echo line window.

This approach keeps our memory allocation completely lightweight and preserves our stable **399 KB static footprint** while providing advanced IDE intelligence.

### Auto-Indentation Engine Source Blueprint
```lisp
(defun count-char-in-string (str target-char)
  "Helper function to count occurrences of a specific character in a string."
  (let ((count 0) (len (length str)) (i 0))
    (while (< i len)
      (if (equal (substring str i (+ i 1)) target-char)
          (setq count (+ count 1)))
      (setq i (+ i 1)))
    count))

(defun calculate-indentation-depth ()
  "Walks backward through lines to compute open vs closed structural brackets."
  (let ((open-brackets 0) (closed-brackets 0) (lines-to-scan 20) (current-scan 0))
    (while (and (< current-scan lines-to-scan) (not (bobp)))
      (editor-cmd "previous-line")
      (let ((line-text (get-line)))
        (setq open-brackets (+ open-brackets (count-char-in-string line-text "{")))
        (setq closed-brackets (+ closed-brackets (count-char-in-string line-text "}"))))
      (setq current-scan (+ current-scan 1)))
    (let ((i 0))
      (while (< i current-scan) (editor-cmd "next-line") (setq i (+ i 1))))
    (let ((depth (- open-brackets closed-brackets)))
      (if (< depth 0) (setq depth 0))
      (* depth 4))))

(defun lisp-auto-indent ()
  "Main interactive command to insert a newline and calculate contextual indent space."
  (interactive)
  (let ((target-spaces (calculate-indentation-depth)))
    (editor-cmd "newline")
    (let ((inserted 0))
      (while (< inserted target-spaces)
        (insert-char " ")
        (setq inserted (+ inserted 1))))
    (message "Contextual Indentation Applied: %d spaces" target-spaces)))

(define-lisp-command "smart-indent" #'lisp-auto-indent)
(define-key "C-m" "smart-indent")
```
