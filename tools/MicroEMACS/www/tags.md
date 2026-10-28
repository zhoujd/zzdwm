# Tags

MicroEMACS can use the source-code tagging programs `cscope` and `ctags`
to make it easier to find functions and variables in C programs.  The `find-` functions
prompt for a name to find, but they also attempt to extract a name from the
current buffer and location, to be used if you do not enter a name and simply hit
`Enter`.

**M-,** (**find-cscope**)

This command prompts for an identifier, then uses `cscope` to find the first
occurence of the identifier.  Usually, the first occurrence is the one
that defines the identifier.  The command then visits the corresponding
file and places the dot at the line containing the identifier.  On PCs,
this function is also bound to `F11`.

**M-G** (**find-grep**)

This command prompts for a string, then uses `cscope`'s "grep" function to find the first
occurence of the string.  The command then visits the corresponding
file and places the dot at the line containing the string.

**[unbound]** (**next-cscope**)

This command uses `cscope` finds the next occurrence of an identifier that
was found in a previous `find-cscope` or `find-grep` command.  On PCs, this
function is bound to `F12`.

**M-.** (**find-tag**)

This command prompts for an identifier, then reads the `TAGS` file (generated
by `ctags`) to find an occurrence of the identifier.  If no argument is present,
the command finds the first occurrence; if an argument is present, the command
finds the next occurence.  Use `C-u M-.` to find the next match and `M-- M-.`
to find the previous match.  The command then visits the corresponding file
and places the dot at the line containing the identifier.

**M-C-.** (**find-tag-regexp**)

This command prompts for a regular expression, then searches tag names in the
`TAGS` file. Its argument behavior matches **find-tag**. Regular-expression
and identifier tag searches keep separate patterns and match positions.

**C-X 4 .** (**find-tag-other-window**)

This command behaves like **find-tag**, but selects the next window; if there
is only one window, it splits the window first. **C-X 4 M-C-.** runs
**find-tag-regexp-other-window** and behaves like **find-tag-regexp**.

**M-*** (**pop-tag-mark**)

This command returns to the buffer and location from which the most recent
`find-tag`, `find-cscope`, or `find-grep` navigation started. Repeating the
command retraces earlier origins; MicroEMACS keeps the sixteen most recent
origins. If a line has become shorter since the jump, the saved offset is
clamped to the end of that line.
