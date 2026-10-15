# Script Mode

---

## Implemented Headless Automation Pipeline: Script Mode (`--script`)
MicroEMACS provides a native standalone script execution interface for Lisp automation.

* **Non-Interactive Execution:** Passing `--script <file>` skips terminal
  initialization and frame rendering, opens remaining filename arguments in
  one logical headless edit context with the last file current, evaluates the
  designated Lisp script, and exits with the script status.
* **Startup Isolation:** Default profile and `.mel` startup files are skipped;
  an explicit `-l` library is loaded first. Place `-l` before `--script`,
  because `--script` consumes the filename that immediately follows it.
* **Batch Output:** Lisp `message` output goes to standard output; bracketed
  diagnostics and Lisp errors go to standard error.
* **Shebang Compliance (`#!`):** The Tiny Lisp tokenizer skips `#!` lines while
  preserving `#'function` references. Linux scripts can use
  `#!/usr/bin/env -S ./me --script` for a repository-local binary, or
  `#!/usr/bin/env -S me --script` after installing the new editor.

The runnable formatter is `../examples/format-code.l`. It formats the current
buffer, so pass the editable file after the script:

```bash
$ cp examples/smart-indent-input.c /tmp/microemacs-script-input.c
$ ./me -l lisp/smart-indent.l --script examples/format-code.l \
    /tmp/microemacs-script-input.c
Formatted current buffer
```

`../examples/script.l` is a smaller directly executable smoke test:

```bash
$ chmod +x examples/script.l
$ ./examples/script.l
Hello from MicroEMACS
2 + 3 = 5
```

---

## Implemented Self-Documenting CLI: GNU Emacs-Style Help (`--help`)
The command-line help interface is grouped by operation as the argument list grows.

* **Domain Grouping:** Options are organized into Initialization, Automation,
  Display, File Handling, and Tools sections.
* **Feature Awareness:** Lisp and color options appear only in builds that
  include those features.
* **Stream Behavior:** Successful `--help` output goes to standard output;
  parser errors continue to use standard error.

---

## Implemented Modular Architecture Core: System `(load)` & `(require)` Support
* `(load "file.l")` evaluates a file in the persistent global Lisp environment.
* `(require "module")` and `(require 'module)` load a logical module once.
* Lookup searches `./`, `~/.mel.d/`, `~/.config/me/lisp/`,
  `/usr/local/share/me/lisp/`, and `/usr/share/me/lisp/`, trying `.l`,
  `.lisp`, and `.mel` suffixes.
* `require` supports GNU Emacs-style optional `FILENAME` and `NOERROR`
  arguments.
* Nested loads are safe across Tiny Lisp garbage collection, and
  `examples/modules/main.l` provides a runnable example.
