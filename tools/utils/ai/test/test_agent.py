import json
import os
import socket
import subprocess
import threading
import unittest


ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXECUTABLE = os.path.join(ROOT, "ai")


class Endpoint:
    def __init__(self, responses):
        self.responses = list(responses)
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
        self, responses, stdin, full_access=False, command_timeout="2"
    ):
        endpoint = Endpoint(responses)
        self.addCleanup(endpoint.thread.join, timeout=2)
        self.addCleanup(endpoint.close)
        settings = {
            "AI_API_URL": f"http://127.0.0.1:{endpoint.port}/v1",
            "AI_TIMEOUT_SECONDS": "2",
            "AI_COMMAND_TIMEOUT_SECONDS": command_timeout,
            "PATH": os.environ["PATH"],
        }
        command = [EXECUTABLE]
        if full_access:
            command.append("--full-access")
        return subprocess.run(
            command,
            input=stdin,
            text=True,
            capture_output=True,
            env=settings,
            timeout=10,
            check=True,
        ), endpoint

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

    def test_utf8_output_is_preserved(self):
        result, endpoint = self.run_agent(
            [tool_response("printf '中文 UTF-8\\n'"), text_response("done")],
            "diagnose\n\na\n\n\n/exit\n\n",
        )
        self.assertIn("中文 UTF-8", result.stdout)
        self.assertIn(
            "中文 UTF-8", endpoint.requests[1]["messages"][-1]["content"]
        )

    def test_full_access_runs_and_allows_post_execution_steering(self):
        result, endpoint = self.run_agent(
            [tool_response("printf 'auto\\n'"), text_response("done")],
            "diagnose\n\ns\nstop here\n\n\n/exit\n\n",
            full_access=True,
        )
        self.assertIn("auto", result.stdout)
        self.assertIn(
            "HUMAN STEERING DURING TOOL EXECUTION",
            endpoint.requests[1]["messages"][-1]["content"],
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
