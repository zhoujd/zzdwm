# Add Hostname Endpoints

Status: done (2026-11-18)

Goal:

- `AI_API_URL` accepts hostnames, not just dotted quads: the lab
  bridge stays `127.0.0.1`, but a LAN hostname (GLM proxy, another
  box's llama.cpp) should work without a gateway.
- `getaddrinfo` only — no TLS, no IPv6 plumbing; the zero-dependency
  static-musl design and the documented "HTTP, local gateway for
  encryption" boundary stay as they are.
- Fold in the one-line durability fix from the same review:
  `fdatasync` after each session append, so a machine crash cannot
  lose the transcript tail (a process crash already cannot —
  `fflush` per line reaches the page cache).

Implementation sketch:

- `connect_endpoint` keeps `sockaddr_in` and gains a fallback:
  `inet_pton` first (dotted-quad fast path unchanged), then
  `getaddrinfo (api_host, NULL, ...)` with `ai_family = AF_INET`,
  taking the first address. musl's built-in resolver works in static
  binaries, so no new dependency.
- Parse-time validation relaxes from `valid_ipv4` to a hostname
  charset check (`[A-Za-z0-9.-]`); the `localhost` → `127.0.0.1`
  special case is dropped (the resolver handles it, and `localhost`
  URLs then exercise the real resolution path in tests).
- Resolve failures get their own error line (`error: resolve %s:
  <gai_strerror>`) instead of surfacing as a bogus `Invalid argument`;
  `EAI_AGAIN` counts as transient for the retry machinery, `EAI_NONAME`
  and `EAI_FAIL` do not.
- Resolution runs per request inside the existing worker thread — no
  stale address pinning, DNS cost is one lookup per round, and
  steering/interrupt stay live. Honest limit: an interrupt during a
  blocked resolver call waits out the system resolver timeout
  (seconds), since getaddrinfo cannot be cancelled.
- `session_write_line` calls `fdatasync (fileno (session_file))` after
  the successful `fflush`, on the existing failure path (disable
  saving, one notice) if it fails.
- Roughly 60 lines. Tests: `AI_API_URL=http://localhost:PORT/...`
  against the fake endpoint (now true resolution), dotted quad still
  connects, unresolvable host prints the resolve error once without
  retrying forever, session save still works with fdatasync in the
  path.

Status notes:

- `connect_endpoint` keeps the `inet_pton` fast path and falls back to
  `getaddrinfo` (`AF_INET`, `SOCK_STREAM`, first address). A resolve
  failure prints `error: resolve <host>: <gai_strerror>` and returns
  its own code so the caller does not double-print a connect error;
  `EAI_AGAIN` sets a per-attempt transient flag (reset in
  `call_endpoint_once` alongside the stream flags), `EAI_NONAME` and
  `EAI_FAIL` fail immediately.
- `valid_ipv4` became `valid_host` (charset `[A-Za-z0-9.-]`); the
  `localhost` → `127.0.0.1` rewrite is gone, so `localhost` URLs now
  exercise real resolution. Malformed dotted quads pass the charset
  check and fail at resolution with the resolve error, which is
  clearer than the old parse-time rejection.
- `session_write_line` chains `fdatasync (fileno (...))` after the
  `fflush`, on the existing disable-saving failure path if it fails.
  Durability is now machine-crash-safe, not just process-crash-safe.
- Tests: `url_host` parameter on `run_agent` (default `127.0.0.1`
  unchanged); hostname exchange via `localhost`, unresolvable `.invalid`
  host fails once with no retry line and the session survives. Dotted
  quads and session persistence are covered by the existing suite.
