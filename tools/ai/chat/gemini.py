#!/usr/bin/env python3

'''
## Install Dependencies
$ pip install google-genai python-dotenv

## Run Chatbot
export GEMINI_API_KEY="your_actual_api_key_here"
python3 gemini.py
'''

import os
import sys
from dotenv import load_dotenv
from google import genai
from google.genai.errors import APIError

# Load environment configuration variables (.env file or system envs)
load_dotenv()

def run_gemini_chat():
    # 1. Fetch API credentials securely from environment arrays
    api_key = os.getenv("GEMINI_API_KEY") or os.getenv("OPENAI_API_KEY")
    
    if not api_key:
        print("[ERROR]: Missing API credential token.", file=sys.stderr)
        print("Please export GEMINI_API_KEY into your environment context.", file=sys.stderr)
        sys.exit(1)

    try:
        # 2. Instantiate the standardized engine client
        client = genai.Client(api_key=api_key)
        
        # 3. Open a session container to automatically track conversation context
        # Using the standard multi-purpose developer flash model
        chat = client.chats.create(model="gemini-3.8-flash")
        
        print("=== Gemini Interactive Chat Session Established ===")
        print("Type your message and press Enter. Enter '/exit' or '/quit' to close.")
        print("-" * 51)

        # 4. Main interface execution loop
        while True:
            try:
                # Capture standard user input cleanly
                user_input = input("[You]   : ").strip()
                
                # Check for loop termination controls
                if not user_input:
                    continue
                if user_input.lower() in ["/exit", "/quit"]:
                    print("\n[System]: Chat workspace session terminated safely.")
                    break
                
                # Render processing indicator to simulate system tracking
                print("[steer] : Streaming AI response chunks...")
                print("[Gemini]: ", end="", flush=True)
                
                # 5. Fetch stream text chunks directly from the engine pipeline
                response_stream = chat.send_message_stream(user_input)
                for chunk in response_stream:
                    if chunk.text:
                        print(chunk.text, end="", flush=True)
                print("\n" + "-" * 51)

            except KeyboardInterrupt:
                print("\n\n[System]: Process interrupted by user hotkey sequence. Exiting.")
                break
                
    except APIError as api_err:
        print(f"\n[API ERROR]: Failed communication frame - {api_err}", file=sys.stderr)
    except Exception as err:
        print(f"\n[CRITICAL]: Unexpected handler fault - {err}", file=sys.stderr)

if __name__ == "__main__":
    run_gemini_chat()
