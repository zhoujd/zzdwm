# AI Agent CLI Overrides [DONE]

Goal:

- Add `-m <name>` / `--model <name>` as a per-run override for `AI_MODEL`.
- Allow `-f <file>` to preload context into interactive mode, including TUI mode.
- Keep one-shot prompt assembly unchanged: file blocks, piped stdin, then the
  prompt.
- In interactive mode, attach files before the first submitted prompt, then use
  the normal conversation for later prompts.
- Forward TUI `-f` and `-m` options to the backend without removing existing
  `-C` or `--full-access` forwarding.

Implementation notes:

- CLI model override takes precedence over `AI_MODEL`.
- Interactive attachments use the same escaped `<file path="...">...</file>`
  format and guards as one-shot attachments.
- Attachments are consumed after the first successful prompt is added to the
  conversation.
- The TUI launches the backend with a dynamically assembled argument vector.

Manual test examples:

```sh
cd tools/ai/agent
./ai -m glm-5.3-flash -e 'quick review'
./ai -m glm-5.3-air -f ai.c -e 'review this file'
./ai tui -m glm-5.3-air -f ai.c
./ai tui -C . -m glm-5.3-air -f ai.c --full-access
```

Status: C implementation, tests, and documentation complete.
