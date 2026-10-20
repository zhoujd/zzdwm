---
name: ai-agent-test
description: Build, test, and validate the local zero-dependency static AI DevOps agent under tools/utils/ai. Use for changes to ai.c, the Makefile or build script, agent behavior, static musl artifacts, endpoint integration, or the bundled Python tests.
---

# AI Agent Testing

## Overview

Validate the local `tools/utils/ai/ai` binary while preserving the project's
zero-runtime-dependency and human-steering design.

## Commit Convention

Use the `AI : ` prefix for AI agent commit subjects, for example:
`AI : add local static AI agent`.

## Workflow

1. Keep implementation and generated output inside `tools/utils/ai` unless the
   user explicitly asks for integration elsewhere.
2. Build the release artifact:

   ```sh
   tools/utils/ai/build.sh
   ```

   The build prefers local `musl-gcc`, then `MUSL_IMG` or `zhoujd/alpine`.

3. Run the complete agent suite:

   ```sh
   make -C tools/utils/ai test
   ```

4. Confirm the binary is static and hardened:

   ```sh
   file tools/utils/ai/ai
   readelf -lW tools/utils/ai/ai | grep -E 'GNU_STACK|GNU_RELRO'
   ```

   Expect `statically linked`, a non-executable stack (`RW`, not `RWE`), and
   `GNU_RELRO`.

5. Check whitespace before reporting results:

   ```sh
   git diff --check
   ```

## Environment and Endpoint Checks

- Use `AI_API_URL`, `AI_API_KEY`, and `AI_MODEL` from the environment.
- Never write API keys, endpoint credentials, or command output from a live
  session into repository files.
- The default URL is HTTP-only and intended for a local gateway or trusted
  loopback proxy; do not add a TLS dependency.
- If supplied a live endpoint, run only a minimal non-destructive smoke test
  and avoid logging authorization headers or the API key.

## Guardrails

- The binary and source must remain dependency-free C11 with embedded JSON.
- Function-call arguments must be a JSON string, not a JSON object.
- Preserve human intervention modes: allow, edit, steer, refuse, and quit.
- Preserve `--full-access` post-command steering before the next model request.
- Keep malformed tool-call arguments visible to both the user and model.
- Do not commit the generated `ai` binary.
