# AI Agent Workspace Scope [DONE]

Goal:

- Add `-C dir` / `--cwd dir`, like the working-directory options of `git -C`
  and `tar -C`.
- Resolve the directory to a canonical absolute path and make it the working
  directory before skills, prompt context, commands, or output-relative paths
  are used.
- Keep every scoped command subject to the existing human approval boundary.
- Reject commands that explicitly reference absolute paths outside the scope
  before execution, even with `--full-access`.
- Treat shell syntax as inherently impossible to classify perfectly. Ambiguous
  commands—including variables, command substitution, `cd`, sudo, pipes, and
  relative escapes such as `../../etc`—remain subject to approval; they are not
  considered a filesystem jail.

Implementation notes:

- `-C` changes the process working directory before `-f` paths and skills load.
- Obvious outside-root references are detected by lexical path normalization
  plus `realpath()` for existing path prefixes.
- The scope is informational and protective only. It does not use containers,
  chroot, namespaces, seccomp, or shell parsing, and cannot prevent indirect
  programs or network-accessible side effects.
- Full access skips prompts but never skips the explicit outside-root check.

Manual test examples:

```sh
./ai -C . -f ai.c -e 'review this file'
./ai -C /tmp -e 'list files' --full-access
./ai -C . -e 'touch /tmp/outside' --full-access
```

Status: C implementation, tests, and documentation complete.
