# Add Command Timeout and Output Cap

Status: proposal (Tier 1)

Goal:

- Add `-t <seconds>` (default 30, `0` = unlimited) to bound each `terminal`
  tool call, plus a hard cap on captured output (e.g. 64 KiB, tail-truncated)
  so a `cat` on a huge file cannot blow the context window.
- A hung command (`tail -f`, a blocked `make`) currently stalls the entire
  agent loop and can wedge a session; a timeout turns that into a clean
  error message the model can react to.

Implementation sketch:

- fork/exec as today; parent does a bounded `waitpid(WNOHANG)` poll loop or
  `alarm()` + SIGALRM handler, then `kill(pid, SIGKILL)` on expiry.
- Report expiry to the model as a normal tool result:
  `[timeout after 30s, killed]` plus whatever stdout/stderr was captured.
- Applies uniformly to interactive and `-e` one-shot mode.
