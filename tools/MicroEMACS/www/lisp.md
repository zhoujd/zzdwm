# Lisp Extensions

MicroEMACS includes a small built-in Lisp evaluator based on Tiny Lisp.
Start it with **M-X eval-lisp**; no key is bound to the command by default.
The evaluator accepts one expression at a time. Its environment persists
between expressions, so functions, macros and variables can be defined and
used in later expressions. It supports numbers, strings, symbols, lists,
lexical scopes, optional function parameters, functions, macros, tail
recursion, arithmetic, comparisons, list operations, function references,
`let`, `while`, `dotimes`, `unless`, and output primitives:

    (+ 1 2)
    (defun plus1 (x) (+ x 1))
    (plus1 2)
    (defmacro inc (x) (list '+ x 1))
    (inc 2)

These forms also provide access to MicroEMACS state and the current buffer:

    (define-key "C-x C-b" "buffer-menu")
    (set-variable "tab-width" 8)
    (message "Done: %s" "ok")
    (set-lisp-debug 2)
    (debug-message 1 "Done: %s" "ok")
    (set-syntax "sql")
    (set-modeline-color 15 22 0 2)
    (execute-command "file-save")
    (get-char)
    (get-line)
    (get-line -1)
    (get-region)
    (count-char "{}}" "{")
    (insert-string "text")
    (insert-char "X")
    (save-buffer)
    (kill-emacs)
    (goto-bol)
    (goto-eol)
    (forward-line 2)
    (eobp)
    (define-lisp-command "format-log-line" #'format-log-line)

Builds with AI enabled add these forms:

    (switch-to-buffer "*AI*")
    (ai-complete "current line")

`define-key` binds a key to a MicroEMACS command. Key names support the
**C-X**, **C-** and **M-** prefixes followed by one ASCII character.
`set-variable` sets **tab-width** (or **tab-size**) or **fill-column** to a
positive integer; a quoted string is also accepted for the value.
`message` formats text with `%s`, `%S`, `%d`, and `%%`, displays it on the
echo line, and returns the formatted string.
`set-lisp-debug` sets the Lisp debug level; level 0 disables debug output.
`debug-message` takes a positive message level, a format string, and optional
arguments. It uses the same conversions as `message`, displays
`[Lisp debug: ...]` when its level is less than or equal to the current debug
level, and returns `t`. A final `t` result is quiet unless the debug level is
nonzero.
`set-syntax` selects a syntax definition for the current buffer and is
available when color is enabled. `execute-command`, also available as
`editor-cmd`, runs a MicroEMACS command by name with an optional numeric
argument; use direct buffer forms when a command would otherwise prompt for
text.
`set-modeline-color` customizes the mode line when color is enabled. The
first two arguments are the foreground and background 256-color indexes; the
last two are the fallback colors for 8/16-color terminals and the Windows
console. All color indexes must be from 0 through 255.

`ai-complete` is available in AI-enabled builds, including the default Linux
and Windows builds. It sends a prompt to the configured AI service with a
built-in instruction for concise Markdown assistant commentary, returns the
generated text as a string, and returns `nil` after displaying an error. The
default service is local Ollama. The optional second argument
selects the model and the optional positive third argument sets the prediction
limit:

```lisp
(ai-complete "prompt")
(ai-complete "prompt" "qwen2.5-coder:1.5b" 64)
(ai-complete-region)
(ai-complete-region "qwen2.5-coder:1.5b" 64)
```

On failure, `ai-complete` returns `nil` after displaying the underlying error.
The `ai-last-error` form returns that error text as a string for scripts that
need to format it themselves.
An empty or whitespace-only successful response is treated as a failure.
`ai-complete-region` sends the text between dot and the mark, accepts the same
optional model and prediction-limit arguments, and leaves dot, the mark, and
the kill ring unchanged. The AI examples use `get-region` to retain that text
so both the selected prompt and the assistant response appear in the Markdown
transcript.

`ai-set-endpoint` configures an HTTP endpoint and optionally sets a Bearer API
key. An endpoint ending in `/v1/chat/completions` uses the OpenAI-compatible
chat request and response formats. HTTPS is not supported by the compact
socket client. Keep API keys in a local startup file that is not committed:

```lisp
(ai-set-endpoint "http://127.0.0.1:11434/api/generate")
(ai-set-endpoint "http://your-gpustack-host:8090/v1/chat/completions"
                 (getenv "GPUSTACK_API_KEY"))
(ai-set-reasoning-effort "low")
(ai-set-timeout 180)
```

`ai-set-reasoning-effort` accepts `minimal`, `low`, `medium`, or `high` for
OpenAI-compatible services that support the field. An empty string disables it,
and configuring a new endpoint clears it. Use `low` with reasoning models that
can otherwise consume the entire
token limit before producing final message content.
`ai-set-timeout` sets the connect/read timeout from 1 through 86400 seconds;
the default is 30 seconds.
The AI examples bind `ai-ask-line` to **M-A** and `ai-ask-region` to **C-X A**.

Anthropic Claude is not supported directly because the official Messages API
requires HTTPS and uses different request and response formats. Claude Code and
`claude.ai` URLs are not chat-completion endpoints. Use an OpenAI-compatible
HTTP proxy, such as LiteLLM, when Claude access is needed:

```lisp
(ai-set-endpoint "http://127.0.0.1:4000/v1/chat/completions"
                 (getenv "LITELLM_API_KEY"))
```

`get-char` returns the UTF-8 character under dot as a string, or `nil` at the
end of a line. `get-line` returns the current line as a string without the
newline; with a numeric argument, it returns a line relative to dot and
returns `nil` outside the buffer. `get-region` returns the text between dot
and the mark, including newline characters, as a string; it returns `nil`
when no region is marked or the region is too large for the Lisp result
buffer. In AI-enabled builds, `getenv` returns an
environment variable as a string, or `nil` when it is not set. `count-char` returns the number of
occurrences of a one-character string in another string. `insert-string`
inserts a string at dot, while `insert-char` inserts one character given as a
Unicode code point or a one-character string. `goto-bol`, `goto-eol`, and
`forward-line` move dot without replaying keys. `save-buffer` and `kill-emacs`
perform those editor actions directly. `eobp` returns `t` when dot is at the
end of the buffer and `nil` otherwise. In AI-enabled builds,
`switch-to-buffer` finds or creates a named buffer and makes it current.

`define-lisp-command` registers a Lisp function as a named MicroEMACS
command. Use `#'name` to pass a function reference. For compatibility, a
quoted expression string is also accepted. The generated expression can be
invoked with **M-X**, or bound with `define-key`, and is limited to 243 bytes.
An interactive Lisp command can declare `(interactive "p")`; the command's
numeric prefix is placed in its first optional parameter. For example, `M-L`
runs `format-log-line` once and `M-3 M-L` runs it three times.

## Startup File

MicroEMACS loads a Lisp startup file after the files named on the command
line and before commands from the startup profile are executed. The default
name is `.mel` on Linux and Windows. MicroEMACS looks for it in the current
directory, then in the user's home directory.
If no default file is found, startup continues normally.
Use the **-q** option to skip the default Lisp startup file and the default
profile; explicit **-l** and **-p** files are still loaded.

Use the **-l** option to load a specific Lisp file instead:

```sh
me -l startup.lisp file.c
```

At runtime, **C-x F** evaluates files ending in `.l`, `.lisp`, or `.mel`
as Lisp source; other files are read as profiles. The explicit
**M-X read-lisp** command always evaluates a Lisp file.

The startup file may contain comments and multiple top-level expressions:

```lisp
; MicroEMACS Lisp startup file
(set-variable "tab-width" 8)
(set-variable "fill-column" 72)
(define-key "C-x C-b" "buffer-menu")
(defun double (x) (* x 2))
```

The `examples/logline.l` file is the Tiny Lisp equivalent of the
`examples/logline.pro` profile example:

```sh
me -l examples/logline.l filename
```

The `examples/smart-indent.l` file is a startup-ready auto-indentation
example. It counts unmatched braces with `count-char`, scans surrounding lines
with relative `get-line` forms, and binds **Enter** to `smart-indent`:

```sh
me -l examples/smart-indent.l examples/smart-indent-input.c
```

The `examples/ai-assist-ollama.l` and `examples/ai-assist-gpustack.l` files
define `ai-ask-line` on **M-A**. The command sends the current line to the
configured AI service and appends a Markdown transcript with `### You` and
`### Assistant` headings to the `*AI*` buffer. They require an AI-enabled
build; the normal Linux and Windows builds include AI.
