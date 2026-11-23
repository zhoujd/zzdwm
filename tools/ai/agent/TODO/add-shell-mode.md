# Add Shell Mode (! Prefix)

Status: done (2026-11-23)

Goal:

- Let the human run a command on demand from the prompt, Claude Code
  style: submit `! make test` and it executes immediately — no approval
  prompt, no round-trip through the model.
- Share the result: command, bounded output tail, and exit status join
  the conversation as context, so the next request sees what happened
  and the human can follow up in plain words.
- Keep the human boundary intact everywhere else: this adds no new model
  power, only a faster path for the person already at the keyboard.

Implementation sketch:

- One branch in the interactive loop: a submitted prompt starting with
  `!` goes to run_shell_prompt instead of the endpoint; `!` alone prints
  a usage hint. One-shot mode is untouched — `-e` prompts stay verbatim.
- Execution reuses run_command unchanged: /bin/sh -c, live streamed
  output, live steering during the run, the usual `-t` timeout with
  TERM/KILL escalation, and the 64 KiB tail-truncated capture.
- A shell_direct flag exempts these commands from Landlock and the
  lexical scope filter — the human typed them, the same trust as
  --full-access — while model-issued commands stay confined under -C.
- The context message is XML-escaped `<bash-input>`/`<bash-output>`
  blocks prefixed HUMAN SHELL COMMAND, added as a plain user message via
  add_text_message, so session.jsonl records it and MAX_MESSAGES pruning
  applies like any other message. No request fires until the next real
  prompt.
- TUI/labels: `[shell command]:`, `[shell command completed]`, and the
  timed-out variant route to the commands pane with the magenta status
  color; the /help text and startup hint mention the mode.
