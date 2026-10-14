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

`ai-set-endpoint` changes the HTTP endpoint and optionally sets a Bearer API
key. The default endpoint is the local Ollama service. An endpoint ending in
`/v1/chat/completions` uses the OpenAI-compatible chat request and response
formats:

```lisp
(ai-set-endpoint "http://127.0.0.1:11434/api/generate")
(ai-set-endpoint "http://your-gpustack-host:8090/v1/chat/completions"
                 (getenv "GPUSTACK_API_KEY"))
(ai-complete "prompt" "glm-5.3" 1024)
```

HTTPS endpoints are not supported by the compact socket client. Keep API keys
in an environment variable or a local startup file that is not committed to
source control.

The client uses a 30-second connect/read timeout. A GPUStack service can wait
up to 120 seconds for a provider slot; in that case MicroEMACS reports a
receive timeout before GPUStack returns its `503 ServiceUnavailable` response.

Ollama does not return a meaningful confidence score for a completed
response, so this extension deliberately does not expose one. Scripts can
inspect or transform the returned text instead.

`examples/ai-assist-ollama.l` and `examples/ai-assist-gpustack.l` define an
`ai-ask-line` command on **M-A**. They send the current line to the configured
model and append a Markdown transcript with `### You` and `### Assistant`
headings to the `*AI*` buffer.

## Future Work

* Add an `ai-complete-region` form when the Lisp bridge has a safe region API.
* Add configurable timeout variables if local deployments need different
  connection limits.
