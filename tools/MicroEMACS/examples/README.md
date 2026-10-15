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

## Lisp-driven auto-indentation

`smart-indent.l` is a startup-ready Lisp configuration. It defines the
`smart-indent` command, counts unmatched braces on preceding lines with the
built-in `count-char` primitive, and binds **Enter** (`C-m`) to contextual
indentation.

Use it directly for interactive testing:

```sh
./me -l examples/smart-indent.l examples/smart-indent-input.c
```

Move dot to the end of the `if (x) {` line and press **Enter**. The new line is
indented by eight spaces. Copy the file to `.mel` to use it as your normal
Lisp startup configuration.

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
