# AI Connection

Status: implemented as a default Lisp extension.

The AI connection is enabled by default. It is compiled only when both Lisp
and AI are enabled:

```sh
./build.sh -a
./build.sh -w
```

Use `AI=no` for a build without AI; this is required when `LISP=no` is used.

The client defaults to a local Ollama service at `127.0.0.1:11434`. It uses
the Ollama `/api/generate` endpoint with streaming disabled. Linux uses POSIX
sockets and the Windows build uses Winsock 2. The implementation uses bounded
buffers, JSON escaping, send/receive loops, a connect/read timeout, HTTP
status checking, chunked-body decoding, and JSON response parsing.

## Lisp API

```lisp
(ai-complete "prompt")
(ai-complete "prompt" "model-name")
(ai-complete "prompt" "model-name" 64)
(ai-complete-region)
(ai-complete-region "model-name")
(ai-complete-region "model-name" 64)
```

The first form uses `qwen2.5-coder:1.5b` and a 128-token prediction limit.
The second form selects a model. The third form also sets the positive
prediction limit. The return value is the generated text as a string. On
failure, the echo line shows the last AI error and the form returns `nil`.
Empty or whitespace-only successful responses are treated as failures.
Requests include a built-in system instruction that asks the model for concise
Markdown assistant commentary, including explanations and useful code rather
than code-only output.
Scripts can also retrieve the last failure with `(ai-last-error)`.
`ai-complete-region` sends the text between dot and the mark instead of a
string prompt. It accepts the same optional model and prediction-limit
arguments as `ai-complete`, and does not modify dot, the mark, or the kill
ring. The examples use `get-region` before requesting a completion so the
selected region appears under `### You` in the `*AI*` transcript.

`ai-set-endpoint` changes the HTTP endpoint and optionally sets a Bearer API
key. The default endpoint is the local Ollama service. An endpoint ending in
`/v1/chat/completions` uses the OpenAI-compatible chat request and response
formats:

```lisp
(ai-set-endpoint "http://127.0.0.1:11434/api/generate")
(ai-set-endpoint "http://your-gpustack-host:8090/v1/chat/completions"
                 (getenv "GPUSTACK_API_KEY"))
(ai-set-reasoning-effort "low")
(ai-set-timeout 180)
(ai-complete "prompt" "glm-5.3" 1024)
```

`ai-set-reasoning-effort` accepts `minimal`, `low`, `medium`, or `high` for
OpenAI-compatible services that support the field. An empty string disables
it, and configuring a new endpoint clears it. The GPUStack example uses `low`
so reasoning models leave room in
`max_tokens` for final message content instead of returning an empty
`message.content`.

The GPUStack example reads `GPUSTACK_API_KEY` first and falls back to
`OPENAI_API_KEY`, matching a LiteLLM provider configured for Codex.

HTTPS endpoints are not supported by the compact socket client. Keep API keys
in an environment variable or a local startup file that is not committed to
source control.

Anthropic Claude is not supported directly. The official Claude Messages API
uses HTTPS, different authentication headers, and a different response format.
Claude Code and `claude.ai` URLs are not chat-completion API endpoints. Use an
OpenAI-compatible HTTP proxy such as LiteLLM when Claude access is needed:

```lisp
(ai-set-endpoint "http://127.0.0.1:4000/v1/chat/completions"
                 (getenv "LITELLM_API_KEY"))
(ai-complete "prompt" "claude-model-name" 1024)
```

The client uses a 30-second connect/read timeout by default. `ai-set-timeout`
accepts a value from 1 through 86400 seconds. A GPUStack service can wait up
to 120 seconds for a provider slot, so its example uses 180 seconds.

Ollama does not return a meaningful confidence score for a completed
response, so this extension deliberately does not expose one. Scripts can
inspect or transform the returned text instead.

`examples/ai-assist-ollama.l` and `examples/ai-assist-gpustack.l` define an
`ai-ask-line` command on **M-A**. They send the current line to the configured
model and append a Markdown transcript with `### You` and `### Assistant`
headings to the `*AI*` buffer. They also define `ai-ask-region` on **C-X A**;
mark a region and it sends the region to the configured model, preserving both
the selected text and the assistant response in the same transcript.
