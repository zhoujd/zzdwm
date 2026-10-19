#!/usr/bin/env python3

import os
import json
import subprocess
import argparse
import readline
import requests

# 1. Your API Configuration (Replace with your Key or read from environment variables)
API_KEY = os.environ.get("OPENAI_API_KEY", "your_api_key_here")
API_URL = os.environ.get("AI_API_URL", "http://10.11.70.11:8090/v1")
MODEL = os.environ.get("AI_MODEL", "glm-5.3-flash")

# 2. Define the host tool: Execute system commands
def run_terminal_command(command: str, full_access: bool = False) -> str:
    """Executes a terminal command on the host environment and returns the output."""
    # Security Guardrail: Prohibit dangerous deletion commands
    if "rm " in command or "format" in command:
        return "Error: Deletion or formatting commands are prohibited for security reasons."

    print(f"\n[AI requests to execute command]: {command}")
    if full_access:
        print("Running without confirmation (--full-access).")
    else:
        confirm = input("Allow execution? (y/n): ")
        if confirm.lower() != 'y':
            return "Execution refused by the user."

    try:
        # Execute the command and capture output
        result = subprocess.run(command, shell=True, capture_output=True, text=True, timeout=15)
        output = result.stdout + result.stderr
        return output if output.strip() else "Command executed successfully with no output."
    except Exception as e:
        return f"Execution error: {str(e)}"

# 3. Define the tool schema presented to the AI
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
    messages.append({"role": "user", "content": user_prompt})

    payload = {
        "model": MODEL,
        "messages": messages,
        "tools": tools_schema,
        "tool_choice": "auto"
    }

    for request_count in range(max_requests):
        response = requests.post(f"{API_URL}/chat/completions", headers=headers,
                                 json=payload, timeout=30).json()
        response_message = response['choices'][0]['message']

        if not response_message.get("tool_calls"):
            print(f"\n[AI Response]:\n{response_message['content']}")
            messages.append({
                "role": "assistant",
                "content": response_message["content"]
            })
            return messages

        messages.append(response_message)
        for tool_call in response_message["tool_calls"]:
            if tool_call["function"]["name"] != "run_terminal_command":
                tool_output = "Error: Unsupported tool."
            else:
                arguments = json.loads(tool_call["function"]["arguments"])
                tool_output = run_terminal_command(arguments.get("command"),
                                                   full_access)

            print(f"[Host Output]:\n{tool_output}")
            messages.append({
                "tool_call_id": tool_call["id"],
                "role": "tool",
                "name": "run_terminal_command",
                "content": tool_output
            })

        payload["messages"] = messages

    print("\n[AI Response]: Stopped after the maximum number of tool rounds.")
    return messages


def interactive_loop(full_access):
    messages = None

    print("Interactive AI assistant (readline enabled). Finish multiline input with an empty line.")
    print("Type /exit or /quit to leave.")
    if full_access:
        print("Full access is enabled: commands will run without confirmation.")
    while True:
        try:
            first_line = input("\n[You]: ")
        except (EOFError, KeyboardInterrupt):
            print()
            break
        if first_line.strip() in {"/exit", "/quit"}:
            break

        lines = [first_line]
        while True:
            try:
                line = input("[...]: ")
            except (EOFError, KeyboardInterrupt):
                print()
                break
            if line.strip() == "":
                break
            lines.append(line)

        user_prompt = "\n".join(lines).strip()
        if not user_prompt:
            continue
        messages = ask_ai_and_run(user_prompt, messages, full_access)

# Run test
if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--full-access", action="store_true",
                        help="execute AI-requested commands without confirmation")
    args = parser.parse_args()
    interactive_loop(args.full_access)
