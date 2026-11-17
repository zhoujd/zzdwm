# AI Agent Plan Tool [DONE]

Goal:

- Give the model a second built-in tool, `plan(checklist)`, that records
  the current task checklist for multi-step work.
- Pin the latest plan into every model request so long tasks survive
  `MAX_MESSAGES` drop-oldest pruning without spending a round re-reading
  a file.
- Keep it a state tool, not a capability tool: it runs nothing and touches
  no files, so it crosses no approval boundary; every update is printed to
  the human as `[Plan updated]:`.
- An empty checklist clears the plan; `/new` clears it too.
- Malformed or oversized checklist arguments stay visible to the user and
  model as a rejected `plan` call instead of being rewritten as a shell
  call.

```
 model ── plan(checklist) ──► record_plan() ──► static current_plan
   ▲                               │
   │                               └─► [Plan updated]: (stdout, no approval)
   │
   └── every build_request() appends a trailing system message:
       {"role":"system","content":"CURRENT TASK PLAN …\n<plan>"}
       (per-request state, never a conversation message: it cannot
        duplicate, go stale, or be dropped)
```

Implementation notes:

- `build_request()` advertises `plan` next to `run_terminal_command`;
  tool-call arguments stay a JSON string per the agent convention.
- `parse_tool_call()` recognizes the `plan` name and parses `checklist`;
  a missing or oversized checklist leaves the call invalid with the name
  intact. `process_tool_calls()` dispatches plan calls before the
  approval path and replies with a normal tool result.
- `add_assistant_response()` reconstructs stored tool-call arguments via
  `append_tool_call_arguments()`, which escapes the value for the
  argument object and the whole argument text again for the enclosing
  message string. This fixed a latent single-escape bug: a raw newline
  inside any tool argument produced invalid request JSON.
- The `task-split` skill now tracks its checklist with the plan tool
  instead of `.ai-plan.md`; the TUI routes `[Plan` lines to the thoughts
  pane with the magenta label style.

Manual test examples:

```sh
cd tools/ai/agent
./ai -e 'plan this: list the files, count the .c files, summarize'
# watch [Plan updated]: blocks appear with no approval prompt, and long
# tasks keep the pinned plan after "[context] Dropped the oldest" lines
```

Status: C implementation, tests, and documentation complete.
