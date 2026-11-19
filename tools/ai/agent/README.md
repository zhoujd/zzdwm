# ai

`ai` is a compact, static Linux agent for OpenAI-compatible chat
completion endpoints. It keeps a multi-turn conversation, exposes a shell
tool, file tools, and a plan tool to the model, and keeps a human at every
command boundary.

## Build

```sh
tools/ai/agent/build.sh
```

Build the agent and optional ncurses TUI with:

```sh
make -C tools/ai/agent ai
```

Run `tools/ai/agent/ai tui`. It launches the backend in `--ui` mode and
renders three windows: shared AI/user thoughts, AI command status/output, and
a keyboard input box. Pass `-C dir` to the TUI to start in a scoped workspace,
`-f <file>` to preload context into the first prompt, and `-m <name>` to
override `AI_MODEL`. Streamed response text paints into the thoughts pane as
the deltas arrive — including the still-incomplete last line — instead of
waiting for each line to finish.

Run the same multiline editor standalone with:

```sh
tools/ai/agent/ai editor
```

Enter prints the edited text to standard output and exits with status 0;
`C-C`, `C-D` on an empty input, or EOF cancels with status 130, so it can
back shell prompts and capture scripts. An `ai-editor` symlink works too.

Press `PageUp`/`PageDown` to scroll the conversation history.

Run the integration and binary checks with:

```sh
make -C tools/ai/agent test
```

The release build uses local toolchains: `musl-gcc` builds both executables
when it can compile `ncurses.h`; because it compiles with `-nostdinc`, it
usually cannot see the host ncurses headers, so it builds `ai`. Without any
local ncursesw the release builds `ai` only and prints a note. Each model
request runs in a joined worker thread while the main thread collects live
steering; cancellation is coordinated through one bounded polling loop.

Publish stripped, statically linked `ai` executables built in
the `zhoujd/alpine` Docker image with:

```sh
tools/ai/agent/build.sh publish
```

Override the image with `MUSL_IMG`. `build.sh` also accepts `build`, `release`,
and `clean`.

Install `ai` into `/usr/local/bin` by default, or another directory with
`DEST`:

```sh
make -C tools/ai/agent install
make -C tools/ai/agent install DEST="$HOME/.local/bin"
```

## Run

```sh
tools/ai/agent/ai
```

For a single request, use one-shot mode:

```sh
tools/ai/agent/ai -exec "summarize this repository"
```

The prompt is sent verbatim, startup banners are omitted, and command approval
remains enabled. Add `--full-access` for unattended one-shot execution.

Attach up to eight text files and/or pipe context while keeping the instruction
last:

```sh
tools/ai/agent/ai -f main.c -f Makefile -e "why won't this link"
printf 'linker output\n' | tools/ai/agent/ai -e "diagnose this failure"
```

One-shot context is assembled as escaped `<file path="...">...</file>` blocks,
then escaped `<stdin>...</stdin>` content, then the prompt. In interactive
mode, attached files are prepended to the first submitted prompt. Combined file
and piped input is limited to 64 KiB and is rejected if it contains a NUL byte.
When stdin is piped, interactive command approval is read from `/dev/tty`;
without an available terminal, approval fails safely unless `--full-access` is
explicitly supplied.

One-shot exit codes are `0` for success, `1` for runtime/API/file errors, `2`
for usage errors, `124` for API or command timeout, and `130` for Ctrl+C
cancellation.

Structural labels such as user input, AI activity, commands, and errors are
colorized in supported terminals. Set `NO_COLOR` to disable ANSI/ncurses
semantic colors.

Use `-C dir` or `--cwd dir` to resolve and enter a workspace before files,
skills, or commands are handled. Commands that explicitly reference absolute
paths outside that workspace are rejected, including in full-access mode. This
is scope validation, not a sandbox: ambiguous shell constructs and indirect
program actions still require normal approval unless `--full-access` is
explicitly supplied. The startup banner prints `Workspace: <dir>` tagged
`scoped` or `unrestricted` so the mode is visible at a glance; plain `ai` in a
directory stays unrestricted, and only `-C` scopes. The working directory is
also announced to the model in a system message, so relative paths resolve
against a known location.

On kernels with Landlock (5.13+) every approved command under `-C` is
additionally confined by filesystem rules applied after fork and before exec,
so variable expansion, command substitution, and symlinks can no longer reach
outside the workspace even though the command string looked clean. The ruleset
grants the workspace read-write, `/tmp` scratch (no execution or device nodes,
so `mktemp` and compiler temporaries keep working), read-execute on `/usr`,
`/bin`, `/sbin`, `/lib`, `/lib64`, `/etc`, `/proc`, and `/dev` (plus write on
`/dev/null`); everything else — including `$HOME` — is inaccessible. The
banner prints `Sandbox: landlock rules + lexical path filter` to confirm
enforcement; on unsupported kernels it degrades to the lexical filter with a
one-time warning, and `AI_LANDLOCK=0` opts out entirely. Approval of each
command is still required as usual: Landlock bounds what runs, the human
still decides whether it runs.

Use `-t <seconds>` or `--timeout <seconds>` to bound every terminal command
for one run; `0` disables the limit. It overrides
`AI_COMMAND_TIMEOUT_SECONDS` (default 600). On expiry the command's process
group receives SIGTERM, then SIGKILL after a grace second, the session
continues, and the model receives `[command timed out ...]` plus whatever
output was captured, so it can react instead of stalling the loop.

Conversations persist as they run: non-system messages (including bounded
tool results and plan updates) are appended to `<workspace>/.ai/session.jsonl`
one JSON object per line, flushed and synced per message. `--continue` replays the
latest saved session before the first request, and `--resume <file>` resumes
a specific transcript; both work in one-shot mode and are forwarded by the
TUI. Replayed messages re-enter the conversation verbatim through the usual
`MAX_MESSAGES` pruning window, the active plan is restored, a torn final
line from a crash is skipped rather than fatal, and `--continue` with no
saved session starts fresh. `/new` restarts the transcript, so a later
`--continue` replays what was on screen rather than replaced context.

Configuration is read from the environment:

* `AI_API_URL`: HTTP endpoint or base URL. Default:
  `http://127.0.0.1:8090/v1/chat/completions`.
* `AI_MODEL`: model name. Default: `glm-5.3-flash`; `-m` / `--model`
  overrides it for one run.
* `AI_API_KEY` or `OPENAI_API_KEY`: optional bearer token.
* `AI_TIMEOUT_SECONDS`: API timeout, 1--1800. Default: 300.
* `AI_COMMAND_TIMEOUT_SECONDS`: shell timeout, 1--1800. Default: 600;
  `-t` / `--timeout` overrides it for one run (`0` = unlimited).
* `AI_TOOL_ROUNDS`: maximum consecutive tool/model rounds per user task,
  1--1024. Default: 128.
* `AI_STREAM`: stream responses token-by-token over SSE instead of buffering
  them. Default: on; set `AI_STREAM=0` to buffer.
* `AI_LANDLOCK`: enforce OS-level workspace rules (Landlock) on approved
  commands under `-C`. Default: on; set `AI_LANDLOCK=0` to rely on the
  lexical path filter alone.

Endpoints are plain HTTP over IPv4; `AI_API_URL` hosts may be dotted quads or
hostnames (resolved per request with `getaddrinfo`, first IPv4 address wins).
HTTPS would require a TLS implementation and violate the zero-runtime-dependency
design; use a local gateway or a trusted loopback proxy when encryption is
required.

Transient endpoint failures are retried automatically, in interactive and
one-shot mode alike: HTTP 429 and 5xx responses, and connections that are
refused or reset mid-request, are retried up to 3 attempts with exponential
backoff (1s, then 2s), honoring an integer `Retry-After` header when the
endpoint supplies one (capped at 60s). Other 4xx responses are never retried —
a retry cannot fix a protocol or request error — and a failed attempt mutates
nothing, so the conversation stays intact. Each retry prints
`[retrying in Ns (attempt N/3)]` so the stall stays visible. A stream whose
tokens already reached the screen is never replayed: once any output is
visible, the attempt is final.

Responses stream token-by-token when the endpoint answers with
`text/event-stream`: the request asks for `stream`, content deltas print
under `[AI Response]:` as they arrive, and the first text appears in about one
round-trip instead of after the whole reply buffers. Deltas are reassembled
into the same message the buffered path parses, so tool calls, session logs,
and conversation state are byte-identical; keep-alive comments, `event:` and
`id:` fields are ignored, and a plain JSON reply (streaming declined) falls
back to buffering automatically. The API timeout acts as a per-quiet-period
limit while streaming — every received chunk resets it.

Token usage is reported from the endpoint's `usage` object. The interactive
agent prints one line at exit, and one-shot mode prints it after each round:
`[usage] tokens: 12.4k in / 3.1k out, 7 rounds, context 3.2k` — cumulative
billed prompt and completion tokens summed per round, the model round count,
and the latest request's context size, so prompt growth and the
`MAX_MESSAGES` drop-oldest pruning become visible. Totals accumulate across
`/new` and are omitted when no round succeeded.

At each requested command, choose `a` to allow, `e` to edit it, `s` to supply a
steering instruction without running it, `n` to refuse it, or `f` to allow it
and enable full access for the rest of the session. With
`--full-access`, commands run immediately, but a steer/continue prompt still
appears after every command before the next model request. Commands execute
through `/bin/sh -c` with the current process privileges. All output is shown
live; the agent keeps the last 64 KiB (earlier bytes are dropped and noted in
the tool result) and returns a bounded tail to the model, so a huge `cat`
cannot blow the context window.
`/force` toggles full access while the agent is running; the startup mode
remains the safer approval-based default.

The file tools work directly on paths instead of shell strings.
`read_file path [offset] [limit]` returns tab-numbered lines from one
workspace file (the same lines stream to your terminal, so nothing is
concealed), `list_dir path` returns sorted entries with type and size, and
both run without approval. `apply_patch` edits files with whole-line
SEARCH/REPLACE hunks: the file path on its own line, then
`<<<<<<< SEARCH`, the exact existing lines, `=======`, the replacement
lines, and `>>>>>>> REPLACE`. Each SEARCH block must match exactly one
location (add surrounding lines to disambiguate), an empty SEARCH block
creates the file, and an empty REPLACE block deletes the matched lines;
several hunks and several files fit in one call. The patch is shown before
anything runs, then the choice is `a` allow, `s` steer without applying,
`n` refuse, or `f` allow and enable full access for the rest of the session
(`--full-access` skips the prompt). Every file-tool path — reads included —
is resolved against the workspace root with symlinks followed and must stay
inside it, regardless of `--full-access`; a patch is scope-checked before
the approval prompt, so a violating patch is never half-applied. When a
later hunk fails, earlier files in the same call are already patched and
the error says so, alongside guidance to re-read the file and copy the
lines exactly.

The `plan` tool records the current task checklist. Each update prints
`[Plan updated]:` with the new plan, and the latest plan is pinned into
every later request as a system message, so long tasks survive context
pruning without re-reading a file. Recording or clearing a plan runs
nothing and touches no files, so it needs no approval; an empty checklist
clears the plan, as does `/new`. In the TUI a new plan block replaces the
previous one in the thoughts pane, so the pane shows only the latest
checklist; the tty still echoes every update.

While an AI request or command is running, input is accepted as live steering.
Use one line per steering line and a blank line to queue the completed
direction. Steering typed during a request is sent before the next request;
steering typed during a command is sent with the command result before the next
request. Live steering cannot alter a request already submitted, but it can
guide the next turn.

In the TUI and on a terminal, the interactive editor takes multiline input
with `C-J` for a newline and Enter to submit, and edits with the Emacs-style
keys `C-A`/`C-E` (line ends), `C-B`/`C-F` (char moves), `C-P`/`C-N`
(line moves, as Up/Down), backspace/`C-D` (deletes), `C-K`/`C-W` (kill to
line end or word start), and `C-U`, which clears every line of the input. With piped input (and in the TUI, which drives
`ai` through a pipe), input ends with an empty line instead. `/new`
starts a fresh conversation, while `/exit` and `/quit` leave the program.

At startup the agent reads the frontmatter of `.agents/skills/*/SKILL.md`
and adds one system message listing each skill's name and description. When
a task matches a skill, the model reads the full `SKILL.md` with a read-only
command, so skill-guided actions still pass the human approval boundary.
`/skills` lists the skills and `/skill <name>` loads one body into the
context.

`ai --version` reports the program version, build date, and short Git commit ID.
