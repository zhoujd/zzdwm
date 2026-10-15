# MicroEMACS Examples

Run these examples from the `tools/MicroEMACS` directory. Build the host
editor first:

```sh
./build.sh -b
```

## Lisp editor bridge

`logline.l` defines a Tiny Lisp function, registers it as the
`format-log-line` command with `#'format-log-line`, binds it to `M-L`,
formats the three sample lines, saves the buffer, and exits. When used
interactively, `M-L` formats one line and `M-3 M-L` formats three lines; the
function stops at the end of the buffer.

Copy the input file before running the example so the source fixture is not
modified:

```sh
cp examples/logline-input.txt /tmp/logline.txt
./me -l examples/logline.l /tmp/logline.txt
cat /tmp/logline.txt
```

Expected result:

```text
	* first:
	* second:
	* third:
```

## Profile and keyboard macro

`logline.pro` records a keyboard macro, names it `format-log-line`, binds it
to `M-L`, applies it to the remaining sample lines, saves the buffer, and
exits.

```sh
cp examples/logline-input.txt /tmp/logline.txt
./me -p examples/logline.pro /tmp/logline.txt
cat /tmp/logline.txt
```

The expected result is the same as the Lisp example.

## Defensive Lisp variant

`logline-prefer.l` is the same editing example with explicit fallbacks for
`save-buffer` and `kill-emacs`. It is useful when a startup configuration may
not have registered those Lisp bridge commands.

## Headless Lisp script

`script.l` is an executable-style example with a `#!` shebang. Linux uses
`env -S` so `--script` is passed as an option to the local `./me` binary:

```sh
./me --script examples/script.l
```

After making it executable, it can also run directly:

```sh
chmod +x examples/script.l
./examples/script.l
```

Expected result:

```text
Hello from MicroEMACS
2 + 3 = 5
```

## Lisp Modules

`modules/main.l` demonstrates `require` with an explicit module path:

```sh
./me --script examples/modules/main.l
```

Expected result:

```text
Hello from a Lisp module
```

`modules/search-main.l` demonstrates logical module lookup without an explicit
path. It searches the current directory, `~/.mel.d/`, `~/.config/me/lisp/`, and
the system Lisp directories:

```sh
mkdir -p ~/.mel.d
cp examples/modules/demo.l ~/.mel.d/demo.l
./me --script examples/modules/search-main.l
```

The expected result is the same. Remove the copied module if it is no longer
needed:

```sh
rm ~/.mel.d/demo.l
```

## Headless code formatting

`format-code.l` is a non-interactive formatting example. It removes the
existing indentation and rewrites each line using the brace depth calculated
by `lisp/smart-indent.l`:

```sh
cp examples/smart-indent-input.c /tmp/format-code.c
./me -l lisp/smart-indent.l --script examples/format-code.l /tmp/format-code.c
cat /tmp/format-code.c
```

## Lisp-driven auto-indentation

`lisp/smart-indent.l` is a reusable Lisp library. It defines the
`smart-indent` command, counts unmatched braces on preceding lines with the
built-in `count-char` primitive, and binds **Enter** (`C-m`) to contextual
indentation.

Use it directly for interactive testing:

```sh
./me -l lisp/smart-indent.l examples/smart-indent-input.c
```

Move dot to the end of the `if (x) {` line and press **Enter**. The new line is
indented by eight spaces. Copy the file to `.mel` to use it as your normal
Lisp startup configuration.

## Lisp Dired

`lisp/dired.l` defines a shell-free directory editor. It uses MicroEMACS
directory and buffer primitives, creates a read-only `Dired` mode, and rebinds
**C-X D** to `lisp-dired`:

```sh
./me -l lisp/dired.l
```

Press **C-X D**, choose a directory (an empty reply uses the current
directory), and MicroEMACS displays a GNU Emacs-style listing with mode, link
count, size, timestamp, and name columns. **Enter** opens a file or navigates
into a directory. File entries are resolved before being opened with the
built-in `me-find-file`; **g** or **r** refreshes the listing. The native
`dired` command remains available with **M-X dired**.

## AI assistance

`ai-assist-ollama.l` uses the default local Ollama endpoint.
`ai-assist-gpustack.l` uses an OpenAI-compatible GPUStack endpoint and reads
`GPUSTACK_API_KEY` from the environment, falling back to `OPENAI_API_KEY`.
Both define `ai-ask-line` on **M-A**, send the current line to the configured
model, and append a Markdown transcript to the `*AI*` buffer. AI is enabled in
the default builds; use `AI=no` to build without it.
Both also define `ai-ask-region` on **C-X A** for the text between dot and the
mark. The region transcript includes the selected prompt and the assistant
response.
The GPUStack example sets reasoning effort to `low`, which prevents reasoning
models from using the entire token budget before generating final content.
It also raises the AI timeout to 180 seconds because GPUStack can wait for a
provider slot.
Anthropic Claude is not supported directly; use an OpenAI-compatible HTTP proxy
such as LiteLLM and point `ai-set-endpoint` at the proxy's
`/v1/chat/completions` URL.
