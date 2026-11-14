import json
import os
import shutil
import signal
import socket
import subprocess
import tempfile
import threading
import time
import unittest


ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXECUTABLE = os.path.join(ROOT, "ai")
TUI_EXECUTABLE = EXECUTABLE


class Endpoint:
    def __init__(self, responses, delay=0):
        self.responses = list(responses)
        self.delay = delay
        self.requests = []
        self.raw_requests = []
        self.lock = threading.Lock()
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.socket.bind(("127.0.0.1", 0))
        self.socket.listen(8)
        self.port = self.socket.getsockname()[1]
        self.thread = threading.Thread(target=self.serve, daemon=True)
        self.thread.start()

    def close(self):
        self.socket.close()
        self.thread.join(timeout=2)

    def serve(self):
        while True:
            try:
                client, _ = self.socket.accept()
            except OSError:
                return
            threading.Thread(target=self.handle, args=(client,), daemon=True).start()

    def handle(self, client):
        with client:
            request = b""
            while b"\r\n\r\n" not in request:
                chunk = client.recv(4096)
                if not chunk:
                    return
                request += chunk
            headers, body = request.split(b"\r\n\r\n", 1)
            length = int(
                next(
                    line.split(b":", 1)[1]
                    for line in headers.split(b"\r\n")
                    if line.lower().startswith(b"content-length:")
                )
            )
            while len(body) < length:
                chunk = client.recv(length - len(body))
                if not chunk:
                    return
                body += chunk
            try:
                with self.lock:
                    self.raw_requests.append(body)
                    request = json.loads(self.raw_requests[-1])
                    self.requests.append(request)
                    response = self.responses.pop(0)
            except (ValueError, IndexError):
                return
            time.sleep(self.delay)
            if isinstance(response, bytes):
                client.sendall(response)
                return
            payload = json.dumps(response, separators=(",", ":")).encode()
            client.sendall(
                b"HTTP/1.1 200 OK\r\n"
                b"Content-Type: application/json\r\n"
                b"Transfer-Encoding: chunked\r\n"
                b"Connection: close\r\n\r\n"
            )
            client.sendall(
                f"{len(payload):x}".encode()
                + b"\r\n"
                + payload
                + b"\r\n0\r\n\r\n"
            )
            client.shutdown(socket.SHUT_WR)


def tool_response(command, call_id="call-1", name="run_terminal_command"):
    return {
        "choices": [
            {
                "message": {
                    "role": "assistant",
                    "content": None,
                    "tool_calls": [
                        {
                            "type": "function",
                            "id": call_id,
                            "function": {
                                "name": name,
                                "arguments": json.dumps({"command": command}),
                            },
                        }
                    ],
                }
            }
        ]
    }


def text_response(content):
    return {"choices": [{"message": {"role": "assistant", "content": content}}]}


class AgentTests(unittest.TestCase):
    def run_agent(
        self, responses, stdin="", full_access=False, command_timeout="2",
        tool_rounds=None, delay=0, cwd=None, exec_prompt=None, files=(),
        workspace=None,
        model=None,
    ):
        endpoint = Endpoint(responses, delay)
        self.addCleanup(endpoint.thread.join, timeout=2)
        self.addCleanup(endpoint.close)
        settings = {
            "AI_API_URL": f"http://127.0.0.1:{endpoint.port}/v1",
            "AI_TIMEOUT_SECONDS": "2",
            "AI_COMMAND_TIMEOUT_SECONDS": command_timeout,
            "PATH": os.environ["PATH"],
        }
        if tool_rounds is not None:
            settings["AI_TOOL_ROUNDS"] = tool_rounds
        command = [EXECUTABLE]
        if workspace is not None:
            command.extend(["-C", workspace])
        if full_access:
            command.append("--full-access")
        if model is not None:
            command.extend(["-m", model])
        for source in files:
            command.extend(["-f", source])
        if exec_prompt is not None:
            command.extend(["-exec", exec_prompt])
        return subprocess.run(
            command,
            input=stdin,
            text=True,
            capture_output=True,
            env=settings,
            cwd=cwd,
            timeout=10,
            check=True,
        ), endpoint

    def run_agent_later(self, responses, live, tail, **kwargs):
        endpoint = Endpoint(responses, kwargs.pop("delay", 0))
        self.addCleanup(endpoint.close)
        settings = {
            "AI_API_URL": f"http://127.0.0.1:{endpoint.port}/v1",
            "AI_TIMEOUT_SECONDS": kwargs.pop("api_timeout", "2"),
            "AI_COMMAND_TIMEOUT_SECONDS": kwargs.pop(
                "command_timeout", "2"
            ),
            "PATH": os.environ["PATH"],
        }
        command = [EXECUTABLE]
        if kwargs.pop("full_access", False):
            command.append("--full-access")
        process = subprocess.Popen(
            command,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            env=settings,
        )
        process.stdin.write("diagnose\n\n")
        process.stdin.flush()
        time.sleep(kwargs.pop("live_delay", 0.5))
        process.stdin.write(live)
        process.stdin.flush()
        time.sleep(kwargs.pop("wait", 3.3))
        if tail:
            process.stdin.write(tail)
            process.stdin.flush()
        output, _ = process.communicate(timeout=10)
        return output, endpoint

    def test_allow_executes_and_continues(self):
        result, endpoint = self.run_agent(
            [tool_response("printf 'hello\\n'"), text_response("final answer")],
            "diagnose\n\na\n\n\n/exit\n\n",
        )
        self.assertIn("hello", result.stdout)
        self.assertIn("final answer", result.stdout)
        self.assertEqual(len(endpoint.requests), 2)

    def test_one_shot_sends_prompt_and_exits(self):
        result, endpoint = self.run_agent(
            [tool_response("printf 'one-shot\\n'"), text_response("done")],
            full_access=True,
            exec_prompt="/exit should be sent verbatim",
        )
        self.assertIn("one-shot", result.stdout)
        self.assertIn("done", result.stdout)
        self.assertNotIn("[bye]", result.stdout)
        self.assertEqual(len(endpoint.requests), 2)
        self.assertEqual(
            endpoint.requests[0]["messages"][-1]["content"],
            "/exit should be sent verbatim",
        )

    def test_one_shot_assembles_files_stdin_and_prompt(self):
        with tempfile.TemporaryDirectory() as workspace:
            source = os.path.join(workspace, "source.txt")
            with open(source, "w", encoding="utf-8") as handle:
                handle.write("file <value> & \"path\"")
            result, endpoint = self.run_agent(
                [text_response("done")],
                "piped <input> & text\n",
                cwd=workspace,
                exec_prompt="final & <prompt>",
                files=("source.txt",),
            )
        content = endpoint.requests[0]["messages"][-1]["content"]
        self.assertIn(
            '<file path="source.txt">\nfile &lt;value&gt; &amp; &quot;path&quot;\n</file>',
            content,
        )
        self.assertIn("<stdin>\npiped &lt;input&gt; &amp; text\n\n</stdin>", content)
        self.assertTrue(content.endswith("final & <prompt>"))

    def test_one_shot_rejects_binary_context(self):
        with tempfile.TemporaryDirectory() as workspace:
            source = os.path.join(workspace, "binary")
            with open(source, "wb") as handle:
                handle.write(b"text\x00file")
            process = subprocess.run(
                [EXECUTABLE, "-f", source, "-exec", "diagnose"],
                input=b"",
                capture_output=True,
                timeout=5,
                env={
                    "AI_API_URL": "http://127.0.0.1:1/v1",
                    "PATH": os.environ["PATH"],
                },
            )
        self.assertEqual(process.returncode, 1)
        self.assertIn(b"contains a NUL byte", process.stdout + process.stderr)

    def test_piped_one_shot_never_reads_approval_from_stdin(self):
        endpoint = Endpoint([tool_response("printf 'must-not-run\\n'"), text_response("done")])
        self.addCleanup(endpoint.thread.join, timeout=2)
        self.addCleanup(endpoint.close)
        process = subprocess.Popen(
            [EXECUTABLE, "-exec", "diagnose"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            env={
                "AI_API_URL": f"http://127.0.0.1:{endpoint.port}/v1",
                "AI_TIMEOUT_SECONDS": "2",
                "AI_COMMAND_TIMEOUT_SECONDS": "2",
                "PATH": os.environ["PATH"],
            },
        )
        process.stdin.write(b"approval collision\n")
        process.stdin.flush()
        output, _ = process.communicate(timeout=10)
        self.assertEqual(process.returncode, 1)
        self.assertIn(b"command approval needs /dev/tty", output)
        self.assertNotIn(b"Command output:", output)
        self.assertEqual(len(endpoint.requests), 1)

    def test_one_shot_rejects_oversized_context(self):
        process = subprocess.run(
            [EXECUTABLE, "-exec", "diagnose"],
            input=b"x" * (64 * 1024 + 1),
            capture_output=True,
            timeout=5,
            env={
                "AI_API_URL": "http://127.0.0.1:1/v1",
                "PATH": os.environ["PATH"],
            },
        )
        self.assertEqual(process.returncode, 1)
        self.assertIn(
            b"one-shot context exceeds 65536 bytes", process.stdout + process.stderr
        )

    def test_workspace_scope_changes_working_directory(self):
        with tempfile.TemporaryDirectory() as workspace:
            marker = os.path.join(workspace, "marker")
            result, endpoint = self.run_agent(
                [tool_response("pwd"), text_response("done")],
                full_access=True,
                exec_prompt="where am I",
                workspace=workspace,
            )
            self.assertIn(workspace, result.stdout)
            self.assertIn(workspace, endpoint.requests[1]["messages"][-1]["content"])
            self.assertFalse(os.path.exists(marker))

    def test_workspace_scope_blocks_outside_absolute_path(self):
        with tempfile.TemporaryDirectory() as workspace:
            result, endpoint = self.run_agent(
                [
                    tool_response(f"touch {workspace}/outside"),
                    text_response("done"),
                ],
                full_access=True,
                exec_prompt="write outside",
                workspace=".",
            )
            self.assertIn("references a path outside the workspace scope", result.stdout)
            self.assertFalse(os.path.exists(os.path.join(workspace, "outside")))
            self.assertIn(
                "outside the workspace scope",
                endpoint.requests[1]["messages"][-1]["content"],
            )

    def test_model_override_takes_precedence(self):
        result, endpoint = self.run_agent(
            [text_response("done")],
            exec_prompt="diagnose",
            model="cli-model",
        )
        self.assertEqual(endpoint.requests[0]["model"], "cli-model")

    def test_interactive_attachment_preloads_first_prompt(self):
        with tempfile.TemporaryDirectory() as workspace:
            source = os.path.join(workspace, "source.txt")
            with open(source, "w", encoding="utf-8") as handle:
                handle.write("attached <value>")
            result, endpoint = self.run_agent(
                [text_response("done")],
                "diagnose\n\n/exit\n\n",
                cwd=workspace,
                files=("source.txt",),
            )
        content = endpoint.requests[0]["messages"][-1]["content"]
        self.assertIn(
            '<file path="source.txt">\nattached &lt;value&gt;\n</file>', content
        )
        self.assertTrue(content.endswith("diagnose"))

    def test_one_shot_full_access_executes_without_prompt(self):
        result, endpoint = self.run_agent(
            [tool_response("printf 'automatic\\n'"), text_response("done")],
            full_access=True,
            exec_prompt="diagnose",
        )
        self.assertIn("automatic", result.stdout)
        self.assertIn("done", result.stdout)
        self.assertEqual(len(endpoint.requests), 2)

    def test_attachment_without_prompt_exits(self):
        process = subprocess.run(
            [EXECUTABLE, "-f", "source.txt"],
            input="",
            text=True,
            capture_output=True,
            timeout=5,
            env={
                "AI_API_URL": "http://127.0.0.1:1/v1",
                "PATH": os.environ["PATH"],
            },
        )
        self.assertEqual(process.returncode, 0)

    def test_one_shot_requires_prompt(self):
        process = subprocess.run(
            [EXECUTABLE, "-exec"],
            text=True,
            capture_output=True,
            timeout=5,
            env={
                "AI_API_URL": "http://127.0.0.1:1/v1",
                "PATH": os.environ["PATH"],
            },
        )
        self.assertEqual(process.returncode, 2)
        self.assertIn("-exec requires a non-empty prompt", process.stderr)

    def test_edit_command_before_execution(self):
        result, endpoint = self.run_agent(
            [tool_response("printf 'original\\n'"), text_response("done")],
            "diagnose\n\ne\nprintf 'edited\\n'\n\n\n/exit\n\n",
        )
        self.assertIn("edited", result.stdout)
        self.assertNotIn("Command output:\noriginal", result.stdout)
        self.assertIn("edited", endpoint.requests[1]["messages"][-1]["content"])

    def test_refuse_and_steer_without_execution(self):
        result, endpoint = self.run_agent(
            [tool_response("printf 'not-run\\n'"), text_response("changed")],
            "diagnose\n\ns\nuse journalctl instead\n\n\n/exit\n\n",
        )
        self.assertNotIn("Command output:\nnot-run", result.stdout)
        self.assertIn("changed", result.stdout)
        messages = endpoint.requests[1]["messages"]
        self.assertIn("Execution skipped", messages[-2]["content"])
        self.assertIn("HUMAN STEERING DURING TOOL EXECUTION", messages[-1]["content"])

    def test_multiline_steer_preserves_newlines(self):
        result, endpoint = self.run_agent(
            [tool_response("printf 'not-run\n'"), text_response("changed")],
            "diagnose\n\ns\nfirst line\nsecond line\n\n\n/exit\n\n",
        )
        self.assertNotIn("Command output:\nnot-run", result.stdout)
        messages = endpoint.requests[1]["messages"]
        self.assertIn(
            "HUMAN STEERING DURING TOOL EXECUTION: first line\nsecond line",
            messages[-1]["content"],
        )

    def test_utf8_output_is_preserved(self):
        result, endpoint = self.run_agent(
            [tool_response("printf '中文 UTF-8\\n'"), text_response("done")],
            "diagnose\n\na\n\n\n/exit\n\n",
        )
        self.assertIn("中文 UTF-8", result.stdout)
        self.assertIn(
            "中文 UTF-8", endpoint.requests[1]["messages"][-1]["content"]
        )

    def test_full_access_runs_and_accepts_live_steering(self):
        result, endpoint = self.run_agent_later(
            [tool_response("printf 'auto\\n'"), text_response("done")],
            "stop here\n\n",
            "/exit\n\n",
            full_access=True,
            live_delay=1.0,
        )
        self.assertIn("auto", result)
        self.assertIn(
            "HUMAN STEERING DURING TOOL EXECUTION: stop here",
            endpoint.requests[1]["messages"][-1]["content"],
        )

    def test_runtime_force_toggle_allows_then_requires_approval(self):
        result, endpoint = self.run_agent(
            [
                tool_response("printf 'forced\\n'"),
                text_response("done"),
                tool_response("printf 'not-run\\n'"),
                text_response("done"),
            ],
            "/force\n\ndiagnose\n\n\n\n/force\n\ndiagnose\n\nn\n\n/exit\n\n",
        )
        self.assertIn("forced", result.stdout)
        self.assertIn("not-run", result.stdout)
        refused_messages = [
            message["content"] for message in endpoint.requests[2]["messages"]
            if message["role"] == "tool"
        ]
        refused_messages += [
            message["content"] for message in endpoint.requests[3]["messages"]
            if message["role"] == "tool"
        ]
        self.assertTrue(
            any("Execution refused by the human" in content for content in refused_messages)
        )
        self.assertEqual(len(endpoint.requests), 4)

    def test_live_steering_during_command(self):
        result, endpoint = self.run_agent_later(
            [tool_response("sleep 1"), text_response("done")],
            "stop after this output\n",
            "\n\n/exit\n\n",
            full_access=True,
            delay=0.5,
            wait=1.8,
        )
        self.assertIn("live steering queued", result)
        messages = endpoint.requests[1]["messages"]
        self.assertIn(
            "HUMAN STEERING DURING TOOL EXECUTION: stop after this output",
            messages[-1]["content"],
        )

    def test_live_steering_during_command_preserves_multiple_lines(self):
        result, endpoint = self.run_agent_later(
            [tool_response("sleep 1"), text_response("done")],
            "first line\nsecond line\n\n",
            "/exit\n\n",
            full_access=True,
            delay=0.5,
            wait=1.8,
        )
        self.assertIn("live steering queued", result)
        messages = endpoint.requests[1]["messages"]
        self.assertIn(
            "HUMAN STEERING DURING TOOL EXECUTION: first line\nsecond line",
            messages[-1]["content"],
        )

    def test_live_steering_editing_uses_backspace(self):
        output, endpoint = self.run_agent_later(
            [tool_response("sleep 1"), text_response("done")],
            "stop after output" + "\x7f" + "t",
            "\n\n/exit\n\n",
            full_access=True,
            delay=0.5,
            wait=1.8,
        )
        self.assertNotIn("^?", output)
        self.assertNotIn("outpu\x7f", output)
        messages = endpoint.requests[1]["messages"]
        self.assertIn(
            "HUMAN STEERING DURING TOOL EXECUTION: stop after output",
            messages[-1]["content"],
        )

    def test_live_steering_during_request(self):
        result, endpoint = self.run_agent_later(
            [text_response("first"), text_response("done")],
            "prioritize read-only diagnosis\n",
            "continue\n\n/exit\n\n",
            api_timeout="5",
            delay=2,
            wait=3.5,
        )
        self.assertIn("live steering queued", result)
        self.assertEqual(len(endpoint.requests), 2)
        messages = endpoint.requests[1]["messages"]
        steering = next(
            message for message in messages
            if "HUMAN STEERING DURING AI REQUEST" in message.get("content", "")
        )
        self.assertIn(
            "HUMAN STEERING DURING AI REQUEST: prioritize read-only diagnosis",
            steering["content"],
        )

    def test_live_steering_during_request_preserves_multiple_lines(self):
        result, endpoint = self.run_agent_later(
            [text_response("first"), text_response("done")],
            "prioritize safety\nthen summarize\n\n",
            "continue\n\n/exit\n\n",
            api_timeout="5",
            delay=2,
            wait=3.5,
        )
        self.assertIn("live steering queued", result)
        messages = endpoint.requests[1]["messages"]
        steering = next(
            message for message in messages
            if "HUMAN STEERING DURING AI REQUEST" in message.get("content", "")
        )
        self.assertIn(
            "HUMAN STEERING DURING AI REQUEST: prioritize safety\nthen summarize",
            steering["content"],
        )

    def test_malformed_tool_arguments_are_reported(self):
        response = {
            "choices": [
                {
                    "message": {
                        "role": "assistant",
                        "content": None,
                        "tool_calls": [
                            {
                                "id": "bad",
                                "type": "function",
                                "function": {
                                    "name": "run_terminal_command",
                                    "arguments": "{bad",
                                },
                            }
                        ],
                    }
                }
            ]
        }
        result, endpoint = self.run_agent(
            [response, text_response("done")],
            "diagnose\n\n\n/exit\n\n",
        )
        self.assertIn("malformed or missing command arguments", result.stdout)
        self.assertIn(
            "malformed", endpoint.requests[1]["messages"][-1]["content"]
        )

    def test_one_shot_exit_codes(self):
        process = subprocess.run(
            [EXECUTABLE, "--invalid"],
            input="",
            text=True,
            capture_output=True,
            timeout=5,
            env={"AI_API_URL": "http://127.0.0.1:1/v1", "PATH": os.environ["PATH"]},
        )
        self.assertEqual(process.returncode, 2)

        process = subprocess.run(
            [EXECUTABLE, "-e", "diagnose"],
            input="",
            text=True,
            capture_output=True,
            timeout=5,
            env={"AI_API_URL": "http://127.0.0.1:1/v1", "PATH": os.environ["PATH"]},
        )
        self.assertEqual(process.returncode, 1)

        with tempfile.TemporaryDirectory() as workspace:
            process = subprocess.run(
                [EXECUTABLE, "-f", os.path.join(workspace, "missing"), "-e", "diagnose"],
                input="",
                text=True,
                capture_output=True,
                timeout=5,
                env={"AI_API_URL": "http://127.0.0.1:1/v1", "PATH": os.environ["PATH"]},
            )
        self.assertEqual(process.returncode, 1)

    def test_one_shot_sigint_exit_code(self):
        listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        listener.bind(("127.0.0.1", 0))
        listener.listen(1)
        client = None

        def accept_client():
            nonlocal client
            client, _ = listener.accept()

        thread = threading.Thread(target=accept_client)
        thread.start()
        self.addCleanup(lambda: client.close() if client else None)
        self.addCleanup(listener.close)
        self.addCleanup(thread.join, timeout=2)
        with tempfile.TemporaryDirectory() as workspace:
            self.write_dummy_skill(workspace)
            process = subprocess.Popen(
                [EXECUTABLE, "-e", "diagnose"],
                cwd=workspace,
                stdin=subprocess.DEVNULL,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                env={
                    "AI_API_URL": f"http://127.0.0.1:{listener.getsockname()[1]}/v1",
                    "PATH": os.environ["PATH"],
                },
            )
            self.addCleanup(process.kill)
            self.assertIn("Skills: dummy", process.stdout.readline())
            time.sleep(0.1)
            process.send_signal(signal.SIGINT)
            output, _ = process.communicate(timeout=5)
        self.assertEqual(process.returncode, 130)
        self.assertIn("request interrupted", output)

    def test_one_shot_api_timeout_exit_code(self):
        listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        listener.bind(("127.0.0.1", 0))
        listener.listen(1)
        port = listener.getsockname()[1]
        client = None

        def accept_client():
            nonlocal client
            client, _ = listener.accept()

        thread = threading.Thread(target=accept_client, daemon=True)
        thread.start()
        self.addCleanup(lambda: client.close() if client else None)
        self.addCleanup(listener.close)
        self.addCleanup(thread.join, timeout=2)
        process = subprocess.Popen(
            [EXECUTABLE, "-e", "diagnose"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            env={
                "AI_API_URL": f"http://127.0.0.1:{port}/v1",
                "AI_TIMEOUT_SECONDS": "1",
                "PATH": os.environ["PATH"],
            },
        )
        output, _ = process.communicate(timeout=5)
        thread.join(timeout=2)
        self.assertIn("Connection timed out", output)
        self.assertEqual(process.returncode, 124)

    def test_one_shot_command_timeout_exit_code(self):
        endpoint = Endpoint([tool_response("sleep 3"), text_response("done")])
        self.addCleanup(endpoint.thread.join, timeout=2)
        self.addCleanup(endpoint.close)
        process = subprocess.Popen(
            [EXECUTABLE, "--full-access", "-exec", "diagnose"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            env={
                "AI_API_URL": f"http://127.0.0.1:{endpoint.port}/v1",
                "AI_TIMEOUT_SECONDS": "2",
                "AI_COMMAND_TIMEOUT_SECONDS": "1",
                "PATH": os.environ["PATH"],
            },
        )
        output, _ = process.communicate(timeout=5)
        self.assertIn("command timed out after 1 seconds", output)
        self.assertEqual(process.returncode, 124)

    def test_command_timeout(self):
        result, _ = self.run_agent(
            [tool_response("sleep 3"), text_response("done")],
            "diagnose\n\na\n\n\n/exit\n\n",
            command_timeout="1",
        )
        self.assertIn("command timed out after 1 seconds", result.stdout)

    def test_endpoint_http_error_keeps_agent_alive(self):
        result, endpoint = self.run_agent(
            [
                b"HTTP/1.1 400 Bad Request\r\n"
                b"Content-Length: 0\r\n"
                b"Connection: close\r\n\r\n"
            ],
            "first\n\nsecond\n\n/exit\n\n",
        )
        self.assertIn("AI endpoint returned HTTP 400", result.stdout)
        self.assertIn("[bye]", result.stdout)
        self.assertGreaterEqual(len(endpoint.requests), 1)

    def test_tui_model_and_attachment_are_forwarded(self):
        if shutil.which("tmux") is None:
            self.skipTest("tmux is required for TUI option forwarding")
        with tempfile.TemporaryDirectory() as workspace:
            source = os.path.join(workspace, "source.txt")
            with open(source, "w", encoding="utf-8") as handle:
                handle.write("attached <value>")
            endpoint = Endpoint([text_response("done")])
            self.addCleanup(endpoint.thread.join, timeout=2)
            self.addCleanup(endpoint.close)
            server = f"ai-tui-options-{os.getpid()}"
            subprocess.run(["tmux", "kill-server"], check=False)
            self.addCleanup(subprocess.run, ["tmux", "kill-server"], check=False)
            environment = os.environ.copy()
            environment["AI_API_URL"] = f"http://127.0.0.1:{endpoint.port}/v1"
            environment["AI_TIMEOUT_SECONDS"] = "2"
            environment["AI_COMMAND_TIMEOUT_SECONDS"] = "2"
            environment["AI_MODEL"] = "environment-model"
            subprocess.run(
                [
                    "tmux", "-f", "/dev/null", "new-session", "-d", "-x", "120", "-y", "30",
                    "-n", server, TUI_EXECUTABLE, "tui", "-C", workspace,
                    "-f", "source.txt", "-m", "tui-model",
                ],
                env=environment,
                check=True,
                timeout=5,
            )
            self.addCleanup(
                subprocess.run, ["tmux", "kill-session", "-t", server], check=False
            )
            time.sleep(0.5)
            subprocess.run(["tmux", "send-keys", "-t", server, "explain"], check=True)
            subprocess.run(["tmux", "send-keys", "-t", server, "Enter"], check=True)
            deadline = time.monotonic() + 8
            while time.monotonic() < deadline:
                if endpoint.requests:
                    break
                time.sleep(0.1)
            self.assertTrue(endpoint.requests, "TUI did not reach the endpoint")
            request = endpoint.requests[0]
            self.assertEqual(request["model"], "tui-model")
            content = request["messages"][-1]["content"]
            self.assertIn('<file path="source.txt">\nattached &lt;value&gt;\n</file>', content)
            self.assertTrue(content.endswith("explain"))
            subprocess.run(["tmux", "send-keys", "-t", server, "F1"], check=True)

    def test_tui_workspace_scope_is_forwarded(self):
        if shutil.which("tmux") is None:
            self.skipTest("tmux is required for TUI workspace forwarding")
        with tempfile.TemporaryDirectory() as workspace:
            endpoint = Endpoint([tool_response("pwd"), text_response("done")])
            self.addCleanup(endpoint.thread.join, timeout=2)
            self.addCleanup(endpoint.close)
            server = f"ai-tui-workspace-{os.getpid()}"
            subprocess.run(["tmux", "kill-server"], check=False)
            self.addCleanup(subprocess.run, ["tmux", "kill-server"], check=False)
            environment = os.environ.copy()
            environment.update(
                {
                    "AI_API_URL": f"http://127.0.0.1:{endpoint.port}/v1",
                    "AI_TIMEOUT_SECONDS": "2",
                    "AI_COMMAND_TIMEOUT_SECONDS": "2",
                    "PATH": os.environ["PATH"],
                }
            )
            subprocess.run(
                [
                    "tmux", "-f", "/dev/null", "new-session", "-d", "-x", "100", "-y", "24",
                    "-n", server, TUI_EXECUTABLE, "tui", "-C", workspace,
                    "--full-access",
                ],
                env=environment,
                check=True,
                timeout=5,
            )
            self.addCleanup(
                subprocess.run, ["tmux", "kill-session", "-t", server], check=False
            )
            time.sleep(0.5)
            subprocess.run(["tmux", "send-keys", "-t", server, "where"], check=True)
            subprocess.run(["tmux", "send-keys", "-t", server, "Enter"], check=True)
            time.sleep(0.5)
            deadline = time.monotonic() + 5
            found = False
            while time.monotonic() < deadline:
                output = subprocess.run(
                    ["tmux", "capture-pane", "-p", "-t", server],
                    check=True,
                    capture_output=True,
                    text=True,
                    timeout=5,
                ).stdout
                if workspace in output:
                    found = True
                    break
                time.sleep(0.2)
            self.assertTrue(found, f"workspace path not shown in TUI:\n{output}")

    def test_tui_enter_submits_prompt_and_clears_input(self):
        if shutil.which("tmux") is None:
            self.skipTest("tmux is required for TUI input rendering")

        endpoint = Endpoint([text_response("done")])
        self.addCleanup(endpoint.thread.join, timeout=2)
        self.addCleanup(endpoint.close)
        server = f"ai-tui-test-{os.getpid()}"
        subprocess.run(["tmux", "kill-server"], check=False)
        self.addCleanup(subprocess.run, ["tmux", "kill-server"], check=False)
        environment = os.environ.copy()
        environment.update(
            {
                "AI_API_URL": f"http://127.0.0.1:{endpoint.port}/v1",
                "AI_TIMEOUT_SECONDS": "3",
                "AI_COMMAND_TIMEOUT_SECONDS": "2",
            }
        )
        subprocess.run(
            [
                "tmux", "-f", "/dev/null", "new-session", "-d", "-x", "100", "-y", "24",
                "-n", server, TUI_EXECUTABLE, "tui",
            ],
            env=environment,
            check=True,
            timeout=5,
        )
        self.addCleanup(subprocess.run, ["tmux", "kill-session", "-t", server], check=False)
        time.sleep(0.5)
        subprocess.run(["tmux", "send-keys", "-t", server, "time?"], check=True)
        subprocess.run(["tmux", "send-keys", "-t", server, "Enter"], check=True)
        time.sleep(2)
        output = subprocess.run(
            ["tmux", "capture-pane", "-p", "-t", server],
            check=True,
            capture_output=True,
            text=True,
            timeout=5,
        ).stdout
        self.assertIn("time?", output)
        self.assertIn("done", output)
        self.assertNotIn("time?", output[output.rfind("3. Prompt"):])

    def test_tui_command_execution_switches_input_to_prompt(self):
        if shutil.which("tmux") is None:
            self.skipTest("tmux is required for TUI input rendering")

        endpoint = Endpoint([tool_response("sleep 1"), text_response("done")])
        self.addCleanup(endpoint.thread.join, timeout=2)
        self.addCleanup(endpoint.close)
        server = f"ai-tui-command-{os.getpid()}"
        subprocess.run(["tmux", "kill-server"], check=False)
        self.addCleanup(subprocess.run, ["tmux", "kill-server"], check=False)
        environment = os.environ.copy()
        environment.update(
            {
                "AI_API_URL": f"http://127.0.0.1:{endpoint.port}/v1",
                "AI_TIMEOUT_SECONDS": "2",
                "AI_COMMAND_TIMEOUT_SECONDS": "2",
                "PATH": os.environ["PATH"],
            }
        )
        subprocess.run(
            [
                "tmux", "-f", "/dev/null", "new-session", "-d", "-x", "100", "-y", "24",
                "-n", server, TUI_EXECUTABLE, "tui",
            ],
            env=environment,
            check=True,
            timeout=5,
        )
        self.addCleanup(subprocess.run, ["tmux", "kill-session", "-t", server], check=False)
        time.sleep(0.5)
        subprocess.run(["tmux", "send-keys", "-t", server, "run command"], check=True)
        subprocess.run(["tmux", "send-keys", "-t", server, "Enter"], check=True)
        time.sleep(2)
        output = subprocess.run(
            ["tmux", "capture-pane", "-p", "-t", server],
            check=True,
            capture_output=True,
            text=True,
            timeout=5,
        ).stdout
        self.assertIn("3. Decision (RET submit)", output)
        subprocess.run(["tmux", "send-keys", "-t", server, "a"], check=True)
        subprocess.run(["tmux", "send-keys", "-t", server, "Enter"], check=True)
        time.sleep(0.2)
        output = subprocess.run(
            ["tmux", "capture-pane", "-p", "-t", server],
            check=True,
            capture_output=True,
            text=True,
            timeout=5,
        ).stdout
        self.assertIn("3. Prompt (C-J newline, RET submit)", output)

    def test_tool_round_limit_is_configurable(self):
        result, _ = self.run_agent(
            [text_response("done")],
            "diagnose\n\n/exit\n\n",
            tool_rounds="3",
        )
        self.assertIn("tool rounds: 3", result.stdout)

    def test_invalid_tool_round_limit_is_rejected(self):
        process = subprocess.run(
            [EXECUTABLE],
            input="diagnose\n\n/exit\n\n",
            text=True,
            capture_output=True,
            env={
                "AI_API_URL": "http://127.0.0.1:1/v1",
                "AI_TOOL_ROUNDS": "65",
            },
            timeout=5,
        )
        self.assertEqual(process.returncode, 2)
        self.assertIn("AI_TOOL_ROUNDS is invalid", process.stdout + process.stderr)

    def write_dummy_skill(self, workspace):
        skill_dir = os.path.join(workspace, ".agents", "skills", "dummy")
        os.makedirs(skill_dir)
        with open(os.path.join(skill_dir, "SKILL.md"), "w") as handle:
            handle.write(
                "---\n"
                "name: dummy\n"
                "description: Pretend deployment helper\n"
                "---\n"
                "Follow the dummy plan.\n"
            )

    def system_content(self, endpoint):
        return "".join(
            message.get("content", "")
            for message in endpoint.requests[0]["messages"]
            if message["role"] == "system"
        )

    def test_skills_listing_reaches_model(self):
        with tempfile.TemporaryDirectory() as workspace:
            self.write_dummy_skill(workspace)
            result, endpoint = self.run_agent(
                [text_response("done")],
                "hello\n\n/exit\n\n",
                cwd=workspace,
            )
        self.assertIn("Skills: dummy", result.stdout)
        system = self.system_content(endpoint)
        self.assertIn("dummy: Pretend deployment helper", system)
        self.assertIn("SKILL.md", system)

    def test_skill_command_injects_body(self):
        with tempfile.TemporaryDirectory() as workspace:
            self.write_dummy_skill(workspace)
            result, endpoint = self.run_agent(
                [text_response("done")],
                "/skill dummy\n\nhello\n\n/exit\n\n",
                cwd=workspace,
            )
        self.assertIn("[skill] dummy added to the context", result.stdout)
        system = self.system_content(endpoint)
        self.assertIn("Follow the dummy plan.", system)


class BinaryTests(unittest.TestCase):
    def test_version_includes_build_metadata(self):
        version = subprocess.run(
            [EXECUTABLE, "--version"], text=True, capture_output=True, check=True
        ).stdout
        self.assertRegex(version, r"^ai 1\.0 \d{4}-\d{2}-\d{2} git-\S+\n$")

    def test_static_nx_and_relro(self):
        executable = subprocess.run(
            ["file", EXECUTABLE], text=True, capture_output=True, check=True
        ).stdout
        program_headers = subprocess.run(
            ["readelf", "-lW", EXECUTABLE],
            text=True,
            capture_output=True,
            check=True,
        ).stdout
        self.assertIn("statically linked", executable)
        self.assertRegex(program_headers, r"GNU_STACK\s+.*RW")
        self.assertIn("GNU_RELRO", program_headers)


if __name__ == "__main__":
    unittest.main()
