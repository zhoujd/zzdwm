import json
import os
import socket
import subprocess
import tempfile
import threading
import time
import unittest


ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXECUTABLE = os.path.join(ROOT, "ai")


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
        self, responses, stdin, full_access=False, command_timeout="2",
        tool_rounds=None, delay=0, cwd=None
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
        if full_access:
            command.append("--full-access")
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
