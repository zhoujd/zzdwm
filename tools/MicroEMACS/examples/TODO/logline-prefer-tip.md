
# TODO

## Extra Quality-of-Life Tips (logline-prefer.l)

1. Interactive Count Support: By adding (&optional arg) and (interactive "p"), you can now type M-3 M-L in your editor to format exactly 3 lines on the fly during manual editing sessions, matching classic Emacs behaviors.
2. Buffer Traversal Safety: The (unless (eobp) ...) condition checks if the cursor has hit the end of the buffer. This blocks the script from wrapping lines or injecting infinite spacing fragments if your text file loops shorter than 3 lines.

## To evaluate a Lisp script using your runtime key combination (C-x F)

{KCTLX | 'F',         readprofile,       "read-profile"},
