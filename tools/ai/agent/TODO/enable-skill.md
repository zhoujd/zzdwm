# Enable Skills [DONE]

Reuse the SKILL.md convention already present in this repository
(`.agents/skills/<name>/SKILL.md`: YAML frontmatter with `name:` and
`description:`, then a markdown body) so the agent can follow the same
project skills a human or Claude Code session would. Progressive
disclosure keeps context small: the model sees only names and descriptions
up front and reads a full body on demand.

```
 .agents/skills/*/SKILL.md                    ai
 ┌──────────────────────────┐  load_skills()
 │ ---                      │ ─────────────►  Skill {name, description, directory}
 │ name: ...                │                 frontmatter only, sorted by name
 │ description: ...         │
 │ ---                      │  conversation_reset()
 │ body …                   │ ─────────────►  one extra system message:
 └──────────────────────────┘                 listing + read-on-demand rule
        ▲                                     (system messages survive context
        │ cat .agents/skills/<dir>/SKILL.md   dropping and /new)
        │ read-only tool call, human approval at every boundary
        ▼
      model
```

Requirements:

- Discovery in `ai.c` at startup: scan `.agents/skills/*/SKILL.md`,
  parse only the frontmatter `name:` and `description:` lines (trim
  spaces, strip optional quotes); a directory without frontmatter still
  loads with the directory as its name. Sort by name so listings are
  deterministic.
- Injection: `conversation_reset()` adds one skills system message right
  after `SYSTEM_PROMPT`; when a skill name differs from its directory the
  listing shows the directory so the model can build the path. The message
  instructs the model to read the matching `SKILL.md` with a read-only
  command, keeping every skill-guided action behind the existing
  allow/edit/steer/refuse boundary.
- Slash commands: `/skills` lists `name: description`; `/skill <name>`
  loads one full body into the context as a system message. Names are
  validated against the discovered set (no path traversal); bodies that
  cannot fit are rejected with an error line.
- Banner: a `Skills: name, name, …` line after the Model line. `ai-tui`
  needs no changes: banner lines and slash commands already flow through
  the conversation window and prompt box.
- Limits stay static and dependency-free: 16 skills, 64-byte names,
  512-byte descriptions, 4 KiB bodies (truncated with a marker when
  larger).
- Tests: run the agent inside a temporary workspace containing a dummy
  skill; assert the listing reaches the model as a system message and
  that `/skill <name>` injects the body.

Refer to: `.agents/skills/ai-agent-test/SKILL.md` for the format in use
and `TODO/enable-TUI.md` for the two-window interface these lines flow
through.
