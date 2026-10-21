# How to Structurally Split Your C Agent

To make your architecture clean and safe, organize your threads into distinct 
roles using standard POSIX threads (pthread):

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
