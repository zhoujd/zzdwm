# Add Landlock Enforcement for -C

Status: proposal (Tier 2)

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
