# Add Landlock Enforcement for -C

Status: done (2026-11-18)

Goal:

- Turn the `-C` workspace scope from a lexical path filter into a real
  OS-level boundary using Landlock: restrict filesystem access to the
  workspace (and read-only access to /usr, /etc alternatives as needed).
- Kernel 5.13+ for filesystem rules; network rules need 6.7+. Must
  degrade gracefully: if `landlock_create_ruleset` fails, keep today's
  lexical filter and print a warning.

Implementation sketch:

- ~50-80 lines, no library dependency (direct syscalls), preserving the
  static-musl zero-dependency design.
- Apply after fork, before exec, so the restriction covers all children.
- Complements, not replaces, add-file-tools.md: argument validation stops
  the agent's own tool calls; Landlock stops what actually runs.

Status notes:

- New `landlock.c` / `landlock.h` module: direct `syscall()` calls with
  locally defined UAPI constants, so the publish container needs no
  linux-headers package. ABI is probed at runtime; rights beyond the
  running kernel (REFER ABI 2, IOCTL_DEV ABI 3) are masked out.
- Policy: workspace read-write; `/tmp` scratch without execute or device
  nodes (`mktemp` and compiler temporaries keep working); read-execute on
  `/usr` `/bin` `/sbin` `/lib` `/lib64` `/etc` `/proc` `/dev`; write on
  `/dev/null`. `$HOME` and everything else is inaccessible. Network rules
  are deliberately not used (they would break curl/pip, and need 6.7+).
- Applied in the `run_command` child after fork, before `execl`, only when
  `-C` scoped, not `--full-access`, and `AI_LANDLOCK != 0`. Any setup
  failure prints a one-time warning and leaves the lexical filter as the
  boundary. `PR_SET_NO_NEW_PRIVS` is set as `restrict_self` requires.
- The startup banner reports `Sandbox: landlock rules + lexical path
  filter` / `lexical path filter (landlock unavailable|disabled)` when
  scoped, and nothing when unrestricted.
- Five tests: $HOME write denied through variable indirection, workspace
  writes still work, /tmp scratch works, `AI_LANDLOCK=0` opts out (write
  succeeds, banner says disabled), unscoped runs show no sandbox banner.
