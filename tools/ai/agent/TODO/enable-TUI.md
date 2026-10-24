# AI Agent TUI [DONE]

The final layout uses two windows, like the Claude Code CLI: one scrolling
conversation transcript and one keyboard input box.

+-------------------------------------------------------------+
| Conversation                                                |
| [You]: help me fix the C code error listed above            |
| > Invoking system tool: gcc -o test test.c                  |
| > Compilation error: test.c:5:10: error...                  |
| [AI Response]: I noticed your code is missing a semicolon.  |
| I have fixed this bug for you.                              |
+-------------------------------------------------------------+
| Prompt (RET submit)                                         |
| > Help me fix the C code error listed above                 |
+-------------------------------------------------------------+

Requirements:

- Use `ncursesw` so locale-aware UTF-8 rendering works.
- Keep the dependency-free static `ai` CLI as the default build.
- Build a separate `ai-tui` front-end that communicates with `ai --ui`.
- Render two windows:
  - Conversation (thoughts, status, and AI responses in one transcript)
  - Keyboard Input / User Prompt
- Support automatic tail-following and PageUp/PageDown history in the
  conversation window.
- Route keyboard input to normal prompts, tool decisions, and live steering.
- Keep Ctrl+C cancellation and the existing allow/edit/steer/refuse/quit modes.
- Preserve CLI behavior and the non-interactive Python integration tests.

Origin refer to: tools/learn/ai/agent_tui.c
