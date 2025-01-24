---
weight: 15
bookFlatSection: true
title: "Window Management"
---

# Window Management


MicroEMACS lets you have multiple windows on the screen.
Each window has its own mode line,
its own value of dot and mark,
and its own associated buffer.
If you have a buffer displayed in more
that one window and you edit a line, it is updated in all windows.

A window is as only wide as can fit on your terminal.
If a text line is too long to be displayed in a window, the last
column is displayed as a `$` character.  However,
MicroEMACS will let you
view long lines by automatically scrolling the window left or right
if you move the cursor past the left or right edges of the window.
When MicroEMACS performs a horizontal scroll, it will attempt to
reframe the window so that the cursor is centered horizontally.

C-L

:   **refresh**

    This command clears the screen, and completely redisplays all
    of the text is all of the windows. It is useful if a line error has garbaged
    your screen.

    If you give **C-L** an argument, it will attempt to split the
    screen vertically into that number of side-by-side pages.  This has the effect
    of giving you a screen that has more rows but fewer columns than normal.
    To restore the screen to normal, give an argument of 1.
    Currently this feature only works with the ncurses display code;
    see [**Building with GCC**](#building-with-gcc).

C-X 2

:   **split-window**

    This is the basic window making command. The current window
    is split in two. Each window is displaying the same buffer, and has
    the same value of dot and mark. The window must be at least three lines
    high, otherwise there isn't enough room for two text lines and the
    new mode line. Any argument is ignored.

    After the you create a window, you usually switch it to a private
    buffer using **C-X B**
    or (perhaps more often) **C-X C-V**.
    Note that because
    both windows are displaying the same buffer, reading a file in one
    window with **C-X C-R** probably does not do what you want.

C-X 1

:   **only-window**

    This is the basic window destroying command. All windows but
    the current window are removed from the screen. The current window grows
    to fill the vacated screen space. Any argument is ignored.
    On PCs, this command is also bound to `F10`.

C-X N, C-X O

:   **forw-window**

    Move to the next window down the screen.
    If the current window
    is the bottom window, move to the top window. Ignores any argument.

C-X P

:   **back-window**

    Move to the previous window up the screen. If the current window
    is the top window, move to the bottom window. Ignores any argument.

C-X Z

:   **enlarge-window**

    The current window is enlarged, if possible. Any argument is
    used as a "number of lines by which to grow". The default argument is 1.
    Screen space is stolen from the window immediately below the current
    window. If the current window is the bottom window on the screen then
    space is stolen from the window immediately above the current window.
    You cannot steal all of the lines away from a window.

C-X C-N

:   **down-window**

    Scroll the current window down. Any argument is used as a
    "number of lines by which to scroll" count. The default argument is 1.
    On PCs, this command is also bound to `C-PgDn`.

C-X C-P

:   **up-window**

    Scroll the current window up. Any argument is used as a
    "number of lines by which to scroll" count. The default argument is 1.
    On PCs, this command is also bound to `C-PgUp`.

C-X C-Z

:   **shrink-window**

    The current window is shrunk, if possible. Any argument is
    used as a "number of lines by which to shrink". The default argument is 1.
    Screen space is given to the window immediately below the current window.
    If the current window is the bottom window on the screen, the space is
    given to the window immediately above the current window. You cannot shrink
    a window out of existence.

M-!

:   **reposition-window**

    This command is used to control the line of a window upon
    which dot is displayed. If its argument is positive, then that number is
    taken to be the origin 1 line number of the current window upon which
    the line containing dot should be placed. If its argument is 0, then dot
    is moved to the center of the window. If the number is less then zero
    then it is taken to be the negation of the origin 1 line number of the
    current window starting at the bottom upon which the line containing dot
    should be placed. If no argument is supplied, a default argument of 1 is
    used; This lets **M-!** function as a "move dot to the top of the window"
    command, which is very useful.

C-X +

:   **balance-windows**

    This command adjusts the windows so that they all have approximately
    the same height.  This is useful after several **split-window**
    commands have created some windows that are too small.

