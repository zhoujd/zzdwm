# AI Agent One-Shot Exit Codes [DONE]

Goal:

- Make `ai -e` and `ai -exec` suitable for reliable shell and CI branching.

Implemented status codes:

- `0`: completed successfully.
- `1`: general runtime, API, or file I/O error.
- `2`: CLI usage or invalid flag error.
- `124`: `AI_TIMEOUT_SECONDS` or `AI_COMMAND_TIMEOUT_SECONDS` exceeded,
  matching GNU `timeout`.
- `130`: cancelled by `SIGINT`.

Implementation notes:

- API timeout classification covers connect, send, and response phases.
- A timed-out command terminates a one-shot run with `124`; interactive mode
  still reports the timeout to the model and continues unless the user cancels.
- One-shot API failures terminate rather than repeatedly retrying after the
  conversation context grows.
- Exit codes are documented in `ai --help` and the README.

Manual test examples:

```sh
cd tools/ai/agent
./ai --invalid; echo $?         # 2
./ai -f missing -e test; echo $? # 1
AI_TIMEOUT_SECONDS=1 ./ai -e test; echo $? # against a delayed endpoint: 124
AI_COMMAND_TIMEOUT_SECONDS=1 ./ai --full-access -e test; echo $? # timed command: 124
```

Status: C implementation and tests complete.
