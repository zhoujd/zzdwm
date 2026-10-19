import os
import json
import subprocess
import requests

# 1. Your API Configuration (Replace with your Key or read from environment variables)
API_KEY = os.environ.get("OPENAI_API_KEY", "your_api_key_here")
API_URL = "https://openai.com"

# 2. Define the host tool: Execute system commands
def run_terminal_command(command: str) -> str:
    """Executes a terminal command on the host environment and returns the output."""
    # ⚠️ Security Guardrail: Prohibit dangerous deletion commands
    if "rm " in command or "format" in command:
        return "Error: Deletion or formatting commands are prohibited for security reasons."

    print(f"\n[🤖 AI requests to execute command]: {command}")
    # Enable human-in-the-loop confirmation
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

def ask_ai_and_run(user_prompt: str):
    headers = {
        "Content-Type": "application/json",
        "Authorization": f"Bearer {API_KEY}"
    }

    # Construct data sent to the AI
    messages = [
        {"role": "system", "content": "You are a local operations assistant. You can use tools to execute commands on the user's host machine to solve problems."},
        {"role": "user", "content": user_prompt}
    ]

    payload = {
        "model": "gpt-4o",  # Or any other model supporting Tool Calls
        "messages": messages,
        "tools": tools_schema,
        "tool_choice": "auto"
    }

    # Initiate the first request
    response = requests.post(API_URL, headers=headers, json=payload).json()
    response_message = response['choices'][0]['message']

    # Check if the AI wants to call a tool
    if response_message.get("tool_calls"):
        messages.append(response_message)  # Save the AI's decision to history

        for tool_call in response_message["tool_calls"]:
            if tool_call["function"]["name"] == "run_terminal_command":
                # Parse the command the AI wants to run
                arguments = json.loads(tool_call["function"]["arguments"])
                cmd = arguments.get("command")

                # 🛠️ Critical Step: Actually run the command on your host environment!
                tool_output = run_terminal_command(cmd)
                print(f"[💻 Host Output]:\n{tool_output}")

                # Wrap the host output and send it back to the AI
                messages.append({
                    "tool_call_id": tool_call["id"],
                    "role": "tool",
                    "name": "run_terminal_command",
                    "content": tool_output
                })

        # Initiate the second request: Let the AI synthesize the host output and respond to the user
        final_response = requests.post(API_URL, headers=headers, json=payload).json()
        print(f"\n[🤖 AI Final Response]:\n{final_response['choices'][0]['message']['content']}")
    else:
        # The AI did not need any tools and answered directly
        print(f"\n[🤖 AI Response]:\n{response_message['content']}")

# Run test
if __name__ == "__main__":
    # Ask a question that requires inspecting your computer's state
    ask_ai_and_run("Help me check what files are in my current directory and tell me the Python version.")
