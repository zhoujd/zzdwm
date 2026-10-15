# Path Boundary Design

## Status

Implemented. This note records the path representation used by the editor and
the rules for MinGW file-system access.

## Internal Path Format

MicroEMACS uses POSIX-style paths internally and in the user interface:

- Windows drive paths are displayed and stored as `/c/path/file`.
- Unix paths remain unchanged, such as `/home/user/file`.
- Relative paths remain relative until they need to be resolved.

Buffer file names, modelines, Lisp `buffer-file-name`, completion results, and
Dired headers use this internal representation. A path may originate as
`C:/path/file` or `C:\path\file`, but `fftilde()` normalizes it to
`/c/path/file` before it reaches a buffer.

## MinGW Boundary Rule

The native form, such as `C:/path/file`, exists only immediately before a
Windows file-system call. `path_to_posix()` converts input at normalization
boundaries, while `path_to_native()` converts at:

- `open()` calls for reading, writing, and profile files.
- `fopen()` calls made directly by Lisp.
- `stat()` and `opendir()` calls.
- backup `rename()` and `unlink()` calls.
- `SetCurrentDirectoryA()`.
- `_fullpath()` in `path_canonical_directory()`.

After `_fullpath()` resolves a directory, its result is converted back to the
internal POSIX form. Shell command construction is also an OS boundary and may
use a native temporary path while the directory shown to the user remains in
the internal form.

## Rationale

Keeping one internal representation avoids mixed `C:/...`, `C:\...`, and
`/c/...` paths in buffers, modelines, Lisp scripts, and Dired. It also makes
path behavior predictable across Linux and Windows while confining Windows
conversion to the MinGW system layer.

