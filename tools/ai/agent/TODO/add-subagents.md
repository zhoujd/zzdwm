# Add Subagent Delegation

Status: done (2026-11-23)

Goal:

- A `task` tool: hand a self-contained subtask ("list every caller of
  validate_workspace_command with file and line") to a fresh nested
  `ai` with its own empty context, and get its final answer back as the
  tool result — the parent's 24-message window keeps only the answer.
- The subagent is a re-exec of the same static binary: no new moving
  parts beyond the child plumbing add-background-jobs.md already
  builds, so propose that first.
- Bounded blast radius: depth 1, shared approval philosophy, child
  costs visible in the tool result.

Implementation sketch:

- Phase 1 is sequential — one task in flight; the parent forks, pipes
  the child's stdout, and waits.
- Child: `ai -C <workspace> -exec <prompt>` with cloned `AI_*` env. The
  child's whole stdout, tail-truncated like any command output, becomes
  the tool result; its `[usage]` line lands in that tail, so the parent
  sees what the delegation cost.
- Approval routing is the core design question. Terminal mode: a child
  with piped stdin already reads approvals from /dev/tty, so prompts
  reach the human (with some screen interleaving). TUI mode: no safe
  routing exists, so the child runs with a new `--no-shell` flag (file
  tools and plan only) unless the task call itself was approved with
  `f` (child gets --full-access). `--no-shell` is useful on its own as
  a read-only analyst mode.
- Depth limit via AI_SUBAGENT=1 in the child environment: the `task`
  tool is not registered, so subagents cannot nest.
- Child transcripts are disabled in phase 1 — the tool result in the
  parent's session.jsonl is the record; two writers appending to the
  same `.ai/session.jsonl` is a corruption risk for no benefit yet.
- Landlock applies inside the child to its own approved commands,
  unchanged.
- Roughly 250 lines in ai.c plus the `--no-shell` flag. Tests:
  delegation round trip against the fake endpoint (one Endpoint serves
  parent and child alike), `--no-shell` refuses the terminal tool,
  depth limit, result tail bounding.
