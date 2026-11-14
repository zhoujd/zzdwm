# AI Agent TUI [DONE]

The current TUI uses three stacked regions. The first two are scrollable
content panes with equal heights by default; the third is the keyboard-driven
input box.

```text
+-------------------------------------------------------------+
| 1. AI & User Thoughts                                       |
| [You]: help me fix the C compiler error                     |
| [AI Response]: The missing semicolon is on line 5.          |
+-------------------------------------------------------------+
| 2. AI Command Status                                        |
| [AI requests command]: gcc -o test test.c                   |
| [command completed] exit status 1. Output: ...              |
+-------------------------------------------------------------+
| 3. Prompt (C-J newline, RET submit)                         |
| > Fix the error and explain the change                      |
+-------------------------------------------------------------+
```

Implemented behavior:

- Uses `ncursesw` for locale-aware UTF-8 rendering.
- Runs as `ai tui` in the combined executable and communicates with the
  agent backend through `ai --ui`.
- Renders two content panes plus one input box:
  - **1. AI & User Thoughts**: conversation transcript and user prompts.
  - **2. AI Command Status**: requested commands, live output, and results.
  - **3. Prompt / Decision**: multiline prompts and single-line decisions.
- Splits the content area equally by default. `F2` maximizes the thoughts
  pane and `F3` maximizes the command pane; pressing either key again restores
  the equal split.
- Switches the input title automatically between `Prompt` and `Decision`
  modes according to the backend prompt.
- Routes the same input box to normal prompts, tool decisions, and live
  steering.
- Supports automatic tail-following plus `PageUp`/`PageDown` scrolling in the
  selected content pane.
- Uses `Tab` to switch the selected content pane and `F1` to quit.
- Supports terminal resizing with a minimum size of 60 columns by 18 rows.
- Preserves Ctrl+C cancellation and the existing allow/edit/steer/refuse/quit
  workflow.
- Accepts `-C dir` and `--full-access` and forwards both options to the
  backend.
- Preserves the non-interactive CLI and Python integration tests.

Origin refer to: tools/learn/ai/agent_tui.c
