---
weight: 19
bookFlatSection: true
title: "Error Navigation"
---

# Error Navigation


MicroEMACS is able to parse a buffer containing compiler error messages,
standard `filename:line` locations, or cscope-style matches, and load the
source files mentioned in those messages.  Typically, you would save compiler
error messages to a file when running `make`:

    make >&errs

Then load the resulting `errs` file into MicroEMACS and use either of the
following equivalent commands to process the messages contained within:

M-C-E

:   **goto-error**

C-X `

:   **next-error**

    These commands look for the next location in the current buffer.
    If one is found, then a new window is opened if there is currently only
    one window; otherwise the next window below the current one is used.
    MicroEMACS reads into that window the file indicated in
    the location, positions the cursor at the line and column given
    in the location, and displays the remainder of the message
    in the echo line.  To find the next error location, switch
    back to the error buffer (using the `back-window` command (**C-X P**)
    or the `forw-window` command (**C-X N**)),
    and issue either command again.
