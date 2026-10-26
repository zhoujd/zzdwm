# ai

`ai` is a compact, static Linux agent for OpenAI-compatible chat
completion endpoints. It keeps a multi-turn conversation, exposes one shell
tool to the model, and keeps a human at every tool boundary.

## Build

```sh
tools/ai/agent/build.sh
```

Build the agent and optional ncurses TUI with:

```sh
make -C tools/ai/agent ai
```

Run `tools/ai/agent/ai tui`. It launches the adjacent dependency-free `ai`
executable in `--ui` mode and renders three windows: shared AI/user thoughts,
AI command status/output, and a keyboard input box.

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

Configuration is read from the environment:

* `AI_API_URL`: HTTP endpoint or base URL. Default:
  `http://127.0.0.1:8090/v1/chat/completions`.
* `AI_MODEL`: model name. Default: `glm-5.3-flash`.
* `AI_API_KEY` or `OPENAI_API_KEY`: optional bearer token.
* `AI_TIMEOUT_SECONDS`: API timeout, 1--1800. Default: 300.
* `AI_COMMAND_TIMEOUT_SECONDS`: shell timeout, 1--1800. Default: 120.
* `AI_TOOL_ROUNDS`: maximum consecutive tool/model rounds per user task,
  1--64. Default: 64.

Only HTTP IPv4 endpoints (including `localhost`) are supported. HTTPS would
require a TLS implementation and violate the zero-runtime-dependency design;
use a local gateway or a trusted loopback proxy when encryption is required.

At each requested command, choose `a` to allow, `e` to edit it, `s` to supply a
steering instruction without running it, or `n` to refuse it. With
`--full-access`, commands run immediately, but a steer/continue prompt still
appears after every command before the next model request. Commands execute
through `/bin/sh -c` with the current process privileges. All output is shown
live and a bounded tail is returned to the model.

While an AI request or command is running, input is accepted as live steering.
Use one line per steering line and a blank line to queue the completed
direction. Steering typed during a request is sent before the next request;
steering typed during a command is sent with the command result before the next
request. Live steering cannot alter a request already submitted, but it can
guide the next turn.

Commands in the interactive editor are terminated by an empty line. `/new`
starts a fresh conversation, while `/exit` and `/quit` leave the program.

At startup the agent reads the frontmatter of `.agents/skills/*/SKILL.md`
and adds one system message listing each skill's name and description. When
a task matches a skill, the model reads the full `SKILL.md` with a read-only
command, so skill-guided actions still pass the human approval boundary.
`/skills` lists the skills and `/skill <name>` loads one body into the
context.

`ai --version` reports the program version, build date, and short Git commit ID.
