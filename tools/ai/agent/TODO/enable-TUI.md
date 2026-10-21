# AI Agent TUI

- "Agent Thoughts & System Status" (or simply "Agent Logs"):
   Fits the standard wording used in modern AI frameworks (like LangChain/LangGraph) when displaying Agent traces.
- "AI Response (Streaming Output)":
   Indicates that text is rendered token-by-token in real-time.
- "Keyboard Input" or "User Prompt":
   Standard terminal layout naming.

+-------------------------------------------------------------+
| [1] Agent Thoughts & System Status (Log / Tool Calls)       |
| > Invoking system tool: gcc -o test test.c                  |
| > Compilation error: test.c:5:10: error...                  |
+-------------------------------------------------------------+
| [2] AI Response (Streaming Output)                          |
| I noticed your code is missing a semicolon. I have fixed    |
| this bug for you.                                           |
|                                                             |
+-------------------------------------------------------------+
| [3] Keyboard Input (User Prompt)                            |
| Prompt > Help me fix the C code error listed above          |
+-------------------------------------------------------------+

Requirements:

- Use `ncursesw` so locale-aware UTF-8 rendering works.
- Keep the dependency-free static `ai` CLI as the default build.
- Build a separate `ai-tui` front-end that communicates with `ai --ui`. [DONE]
- Render three independently scrollable panes:
  - Agent Thoughts & System Status
  - AI Response
  - Keyboard Input / User Prompt
- Support automatic tail-following and PageUp/PageDown history in panes 1--2.
- Route keyboard input to normal prompts, tool decisions, and live steering.
- Keep Ctrl+C cancellation and the existing allow/edit/steer/refuse/quit modes.
- Preserve CLI behavior and the non-interactive Python integration tests.

Refer to: tools/learn/ai/agent_tui.c
