# Add Workspace Banner [DONE]

Status: complete and validated. Banner prints `Workspace: <dir> (scoped|unrestricted)`; the working directory also reaches the model as a system message (`Working directory: ...`, with the scope rule when `-C` is given). One-shot output stays banner-free; plain `ai` remains unrestricted.

Goal:

- On startup print `workspace: <dir> (scoped|unrestricted)` so the human
  can see at a glance whether commands are sandboxed (`-C` given) or not.
- Related UX nit: the model's system prompt never mentions the workspace
  directory (getcwd() is unused in ai.c), so the model does not know where
  it is; including the cwd in the system prompt is worth considering in
  the same change.

Implementation sketch:

- ~10 lines in the startup banner / `--version` output.
- Must not flip the sandbox on implicitly: plain `ai` in a directory stays
  unrestricted; only `-C` scopes.
