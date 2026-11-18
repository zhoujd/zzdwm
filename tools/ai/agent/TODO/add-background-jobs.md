# Add Background Command Jobs

Status: proposed (2026-11-18)

Goal:

- Let the model start long-running commands (test suites, builds) as
  jobs that run while the conversation continues, then poll status and
  collect output later — instead of blocking a whole round on a
  9-minute suite or dropping to a manual terminal.
- Same boundaries as foreground commands: the a/e/s/n/f approval prompt
  before start, Landlock rules in the job child, 64 KiB tail-truncated
  output.
- A defined exit policy: jobs never outlive the agent silently.

Implementation sketch:

- Three tools in ai.c beside the terminal tool: `start_job command`
  (same approval prompt, returns `job:N started`), `job_status [id]`
  (running / exited N / stopped, elapsed seconds, tail of the log), and
  `stop_job id` (SIGTERM, grace second, SIGKILL — the timeout-expiry
  escalation reused; approval-prompted, since killing a job the human
  wanted running is also a decision).
- Job children are today's run_command children with two changes:
  stdout+stderr redirect to `<workspace>/.ai/jobs/<id>.log`, and no
  foreground wait. The landlock_apply call site is shared, so the
  sandbox comes along unchanged.
- Reaping via waitpid WNOHANG at each tool round and before each model
  request — no new thread; statuses are fresh whenever the model can
  ask.
- Jobs are exempt from `-t` (a full suite outruns the 600 s default);
  job_status reports elapsed time so the model and human can see a
  runaway, and stop_job bounds it.
- Agent exit with live jobs terminates their process groups and prints
  `[N job(s) stopped]`; an orphaned half-build nobody is watching is
  worse than a re-run.
- TUI: one status-window line, `jobs: N running (longest Xs)`.
- Roughly 300 lines in ai.c plus the tui.c status line. Tests:
  start/status round trip, log tail capture, landlock enforced on the
  job child ($HOME probe), exit reaps jobs, approval before start,
  stop_job escalation.
