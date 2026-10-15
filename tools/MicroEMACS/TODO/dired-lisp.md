# Lisp Dired

## Status

Implemented by [`examples/dired.l`](../examples/dired.l). The native `dired`
command remains available as **M-X dired**; loading the Lisp example rebinds
**C-X D** to `lisp-dired`.

## Design

- `dired-list` reads directories with `opendir`, `readdir`, and `stat`; it does
  not invoke a shell. This works in Linux, Windows console, and Git Bash
  environments without requiring `ls` in `PATH`.
- Entries use a GNU Emacs-style directory header and fixed columns for mode,
  link count, size, timestamp, and name. Directories are grouped first and
  sorted lexically within each group; directories receive a trailing `/`.
- **C-X D** prompts for a directory with filename completion. An empty reply
  uses the current directory. In `--script` mode, the default is returned
  without prompting.
- `create-mode` installs a buffer-local `Dired` mode. `define-mode-key` binds
  **Enter**, **g**, and **r** only in that buffer.
- **Enter** opens a file with `find-file` or reloads Dired for a directory.
  **g** and **r** refresh the listing.
- The Dired buffer is read-only and records its directory in the buffer file
  name for refresh and path construction.

## Lisp Bridge

The implementation uses these host forms:

```lisp
(switch-to-buffer "*dired*")
(dired-list "directory")
(create-mode "Dired")
(define-mode-key "C-m" "dired-find-file")
(set-buffer-readonly t)
(buffer-file-name)
(string-suffix-p "directory/" "/")
(string-trim-suffix "directory/" "/")
(join-path "directory" "file")
(read-directory-name "Dired" ".")
(dired-entry-name)
```

## Future Work

- Add parent-directory and up-directory convenience keys.
- Add mark, copy, rename, and delete operations with explicit confirmation.
- Consider hiding the `./` entry or adding a configurable hidden-file filter.
