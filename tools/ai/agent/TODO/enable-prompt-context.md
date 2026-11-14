# AI Agent One-Shot Context [DONE]

Goal:

- Keep one-shot instructions last and predictable.
- Assemble the initial user message as:
  1. repeated `-f file` blocks,
  2. non-terminal piped stdin,
  3. the `-e` / `-exec` prompt.
- Wrap files in `<file path="...">...</file>` and piped input in
  `<stdin>...</stdin>`.
- XML-escape paths and content so source text cannot forge tag boundaries.
- Reject NUL-bearing input and cap combined file/stdin bytes at 64 KiB.
- Require a terminal for command approval when stdin is piped; never consume
  context bytes as approval input.

Implementation notes:

- The context cap counts decoded input bytes before XML/JSON escaping.
- Oversized input is rejected with a clear error rather than silently truncated.
- `/dev/tty` is opened lazily for interactive follow-ups when one-shot stdin is
  not a terminal.
- Without `--full-access` and an available terminal, command approval fails
  safely instead of executing automatically.

Status: C implementation, tests, and documentation complete.

Manual test examples:

```sh
cd tools/ai/agent

printf 'linker output\n' | ./ai -C . -e 'diagnose this failure'
./ai -C . -f main.c -f Makefile -e 'why will this not link?'
printf 'build log\n' | ./ai -f Makefile -e 'explain the likely failure'

printf 'before\0after\n' >/tmp/ai-binary
./ai -f /tmp/ai-binary -e 'explain'
python3 -c 'print("x"*65537,end="")' | ./ai -e 'explain'

./ai -f Makefile
```

The binary and oversized-context commands should exit `2` with their respective
guard messages. `-f` without `-e` or `-exec` should also exit `2`. Real requests
prompt for command approval from `/dev/tty` unless `--full-access` is supplied.
