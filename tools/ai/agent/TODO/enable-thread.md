# Enable Threading [DONE]

The current design deliberately keeps the agent on
one thread: the conversation, command execution, signal handling, and human
steering boundaries are serialized and therefore easier to audit. Blocking
network and command waits already use bounded polling and remain interruptible
with SIGINT/SIGTERM.

If this becomes necessary, split responsibilities into distinct roles using
standard POSIX threads (pthread):

 ┌─────────────────────────────────────────────────────────┐
 │                       MAIN THREAD                       │
 │  • Renders CLI / UI Engine                              │
 │  • Collects live user inputs & system events            │
 └───────────┬─────────────────────────────────▲───────────┘
             │                                 │
   Spawns thread task                 Thread pushes result
             ▼                                 │
 ┌─────────────────────────────────────────────┴───────────┐
 │                    AI STEERING THREAD                   │
 │  • Prepares context & packs JSON data                   │
 │  • Performs blocking network HTTP request to LLM        │
 │  • Parses the target Linux bash tool to execute         │
 └───────────────────────────┬─────────────────────────────┘
                             │
                  Runs child shell process
                             ▼
 ┌─────────────────────────────────────────────────────────┐
 │                   SYSTEM TOOL EXECUTION                 │
 │  • Executes child action via popen() / fork()           │
 └─────────────────────────────────────────────────────────┘

Requirements:
- Own the conversation state in one thread; never share mutable cJSON trees.
- Keep cJSON objects in one thread's local scope.
- Pass immutable strings or serialized JSON between threads, not shared trees.
- Route signals and cancellation through one coordination point.
- Preserve command approval and post-command steering exactly once per tool.
- Join every thread before shutdown; do not detach background work.
