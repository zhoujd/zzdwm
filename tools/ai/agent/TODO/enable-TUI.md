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

Requirment:

- Perfer to ncurses library
- UTF-8 Support
- Scroll support

Refer to: tools/learn/ai/agent_tui.c
