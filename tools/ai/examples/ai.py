#!/usr/bin/env python3

'''
## Install Dependencies
$ pip3 install requests

## Run AI
export OPENAI_API_KEY="your_actual_api_key_here"
export AI_API_URL="your_api_url_here"
export AI_MODEL="your_model_here"
export AI_TIMEOUT_SECONDS=60
export AI_COMMAND_TIMEOUT_SECONDS=60
python3 ai.py
'''

import os
import json
import shlex
import subprocess
import argparse
import readline  # side effect: enables line editing and history for input()
import requests

# Safely read integer from an environment variable
def get_env_int(key: str, default: int) -> int:
    try:
        return int(os.environ.get(key, default))
    except (TypeError, ValueError):
        return default

# API Configuration (Replace with your Key or read from environment variables)
API_KEY = os.environ.get("OPENAI_API_KEY", "your_api_key_here")
API_URL = os.environ.get("AI_API_URL", "http://10.11.70.11:8090/v1")
AI_MODEL = os.environ.get("AI_MODEL", "glm-5.3-flash")
AI_TIMEOUT_SECONDS = get_env_int("AI_TIMEOUT_SECONDS", 60)
AI_COMMAND_TIMEOUT_SECONDS = get_env_int("AI_COMMAND_TIMEOUT_SECONDS", 60)

# Best-effort guardrail for commands whose primary purpose is deletion or
# destructive disk access. The confirmation prompt remains the security gate;
# shell syntax cannot be classified safely with a simple regex.
BLOCKED_COMMANDS = {"rm", "rmdir", "shred", "unlink", "dd", "mkfs"}


# Define the host tool: Execute system commands
def run_terminal_command(command: str, full_access: bool = False) -> str:
    """Executes a terminal command on the host environment and returns the output."""
    try:
        argv = shlex.split(command, posix=True)
    except ValueError:
        return "Error: Invalid shell command syntax."
    executable = os.path.basename(argv[0]) if argv else ""
    if executable in BLOCKED_COMMANDS or executable.startswith("mkfs."):
        return "Error: Destructive commands are prohibited for security reasons."

    print(f"\n[AI requests to execute command]: {command}")
    if full_access:
        print("Running without confirmation (--full-access).")
    else:
        try:
            confirm = input("Allow execution? (y/n): ")
        except (EOFError, KeyboardInterrupt):
            print()
            return "Execution refused by the user."
        if confirm.lower() != 'y':
            return "Execution refused by the user."

    try:
        # Execute the command and capture output
        result = subprocess.run(command, shell=True, capture_output=True, text=True,
                                timeout=AI_COMMAND_TIMEOUT_SECONDS)
        output = result.stdout + result.stderr
        return output if output.strip() else "Command executed successfully with no output."
    except Exception as e:
        return f"Execution error: {str(e)}"

# Define the tool schema presented to the AI
tools_schema = [
    {
        "type": "function",
        "function": {
            "name": "run_terminal_command",
            "description": "Execute system commands on the user's local host terminal (e.g., list directories, check processes, run scripts).",
            "parameters": {
                "type": "object",
                "properties": {
                    "command": {
                        "type": "string",
                        "description": "The complete terminal command to execute, such as 'ls -la' or 'python --version'."
                    }
                },
                "required": ["command"]
            }
        }
    }
]

def ask_ai_and_run(user_prompt: str, messages=None, full_access=False):
    max_requests = 5
    headers = {
        "Content-Type": "application/json",
        "Authorization": f"Bearer {API_KEY}"
    }

    # Construct data sent to the AI
    if messages is None:
        messages = [
            {"role": "system", "content": "You are a local operations assistant. You can use tools to execute commands on the user's host machine to solve problems."}
        ]
    transcript_start = len(messages)
    messages.append({"role": "user", "content": user_prompt})

    payload = {
        "model": AI_MODEL,
        "messages": messages,
        "tools": tools_schema,
        "tool_choice": "auto"
    }

    try:
        for request_count in range(max_requests):
            try:
                response = requests.post(f"{API_URL}/chat/completions", headers=headers,
                                         json=payload, timeout=AI_TIMEOUT_SECONDS)
                response.raise_for_status()
                response = response.json()
                response_message = response['choices'][0]['message']
            except (requests.exceptions.RequestException, ValueError) as e:
                print(f"\n[AI Response]: Request failed: {e}")
                return messages
            except (KeyError, IndexError):
                print(f"\n[AI Response]: Unexpected response from server:\n{response}")
                return messages

            if not response_message.get("tool_calls"):
                print(f"\n[AI Response]:\n{response_message['content']}")
                messages.append({
                    "role": "assistant",
                    "content": response_message["content"]
                })
                return messages

            messages.append(response_message)
            for tool_call in response_message["tool_calls"]:
                name = tool_call["function"]["name"]
                if name != "run_terminal_command":
                    tool_output = "Error: Unsupported tool."
                else:
                    try:
                        arguments = json.loads(tool_call["function"]["arguments"])
                    except (TypeError, ValueError):
                        arguments = None
                    if not isinstance(arguments, dict):
                        arguments = {}
                    command = arguments.get("command")
                    if not isinstance(command, str) or not command.strip():
                        tool_output = "Error: Missing or invalid 'command' argument."
                    else:
                        tool_output = run_terminal_command(command, full_access)

                print(f"[Host Output]:\n{tool_output}")
                messages.append({
                    "tool_call_id": tool_call["id"],
                    "role": "tool",
                    "name": name,
                    "content": tool_output
                })

        print("\n[AI Response]: Stopped after the maximum number of tool rounds.")
        return messages
    except KeyboardInterrupt:
        del messages[transcript_start:]
        print("\n[Request interrupted]")
        return messages

def interactive_loop(full_access):
    messages = None

    print("Interactive AI assistant. Finish multiline input with an empty line.")
    print(f"Endpoint: {API_URL}")
    print(f"Model: {AI_MODEL}; API timeout: {AI_TIMEOUT_SECONDS}s; "
          f"command timeout: {AI_COMMAND_TIMEOUT_SECONDS}s")
    print("Type /exit or /quit to leave.")
    if full_access:
        print("Full access is enabled: commands will run without confirmation.")
    while True:
        try:
            first_line = input("\n[You]:\n")
        except (EOFError, KeyboardInterrupt):
            print()
            break
        if first_line.strip() in {"/exit", "/quit"}:
            break

        lines = [first_line]
        cancel = False
        while True:
            try:
                line = input("[...]:\n")
            except (EOFError, KeyboardInterrupt):
                cancel = True
                break
            if line.strip() == "":
                break
            lines.append(line)
        if cancel:
            print("\n[Input canceled]")
            continue

        user_prompt = "\n".join(lines).strip()
        if not user_prompt:
            continue
        try:
            messages = ask_ai_and_run(user_prompt, messages, full_access)
        except KeyboardInterrupt:
            print("\n[Request interrupted]")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--full-access", action="store_true",
                        help="execute AI-requested commands without confirmation")
    args = parser.parse_args()
    interactive_loop(args.full_access)
