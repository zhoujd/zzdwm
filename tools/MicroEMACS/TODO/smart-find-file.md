# Symlink-Aware File Opening

## Status

Implemented by [`lisp/smart-find-file.l`](../lisp/smart-find-file.l).
Loading the library registers the `smart-find-file` command and rebinds
**C-X C-F**. The native opener remains available as `me-find-file`.

## Design

- `read-file-name` prompts with filename completion and accepts an optional
  default. In `--script` mode it returns the default without prompting.
- `resolve-path` normalizes the requested path and returns the canonical
  target on Linux and Windows. Missing paths return nil.
- `smart-find-file` uses the resolved path when available and falls back to
  the original input for new files whose parent directories cannot yet be
  resolved. This preserves normal file creation while opening existing
  symlinks at their final targets.
- The Lisp `find-file` binding is overloaded after the native opener is saved
  in `native-find-file`. Existing Lisp libraries, including Dired, inherit
  symlink resolution without duplicating resolution logic.
- Dired directory entries use `resolve-path` before refreshing so directory
  symlinks display their real target contents.
- The library remains opt-in and does not alter file-opening behavior until
  loaded.

## Future Work

- Consider an optional policy that reports broken symlinks while still
  allowing new-file creation.
- Expose a link-chain inspection form if detailed diagnostics are needed.
