# ai

`ai` is a compact, static Linux agent for OpenAI-compatible chat
completion endpoints. It keeps a multi-turn conversation, exposes one shell
tool to the model, and keeps a human at every tool boundary.

## Build

```sh
tools/utils/ai/build.sh
```

Run the integration and binary checks with:

```sh
make -C tools/utils/ai test
```

The release build uses local `musl-gcc` when available and otherwise runs the
same source build in the `zhoujd/alpine` Docker image. Each model request runs
in a joined worker thread; cancellation is coordinated through one bounded
polling loop. It writes the stripped,
musl-static executable to `tools/utils/ai/ai`. Override the image with
`MUSL_IMG`. `make -C tools/utils/ai` performs a native static build without
Docker.

Install the binary into `/usr/local/bin` by default, or another directory with
`DEST`:

```sh
make -C tools/utils/ai install
make -C tools/utils/ai install DEST="$HOME/.local/bin"
```

## Run

```sh
tools/utils/ai/ai
```

Configuration is read from the environment:

* `AI_API_URL`: HTTP endpoint or base URL. Default:
  `http://127.0.0.1:8090/v1/chat/completions`.
* `AI_MODEL`: model name. Default: `glm-5.3-flash`.
* `AI_API_KEY` or `OPENAI_API_KEY`: optional bearer token.
* `AI_TIMEOUT_SECONDS`: API timeout, 1--600. Default: 60.
* `AI_COMMAND_TIMEOUT_SECONDS`: shell timeout, 1--600. Default: 60.

Only HTTP IPv4 endpoints (including `localhost`) are supported. HTTPS would
require a TLS implementation and violate the zero-runtime-dependency design;
use a local gateway or a trusted loopback proxy when encryption is required.

At each requested command, choose `a` to allow, `e` to edit it, `s` to supply a
steering instruction without running it, or `n` to refuse it. With
`--full-access`, commands run immediately, but a steer/continue prompt still
appears after every command before the next model request. Commands execute
through `/bin/sh -c` with the current process privileges. All output is shown
live and a bounded tail is returned to the model.

Commands in the interactive editor are terminated by an empty line. `/new`
starts a fresh conversation, while `/exit` and `/quit` leave the program.

`ai --version` reports the program version, build date, and short Git commit ID.
