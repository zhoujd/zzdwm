import json
import os
import pty
import select
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


def text_response(content, prompt_tokens=None, completion_tokens=None):
    response = {"choices": [{"message": {"role": "assistant", "content": content}}]}
    if prompt_tokens is not None:
        response["usage"] = {
            "prompt_tokens": prompt_tokens,
            "completion_tokens": completion_tokens or 0,
            "total_tokens": prompt_tokens + (completion_tokens or 0),
        }
    return response


def plan_response(checklist, call_id="call-plan"):
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
                                "name": "plan",
                                "arguments": json.dumps({"checklist": checklist}),
                            },
                        }
                    ],
                }
            }
        ]
    }


class AgentTests(unittest.TestCase):
    def run_agent(
        self, responses, stdin="", full_access=False, command_timeout="2",
        tool_rounds=None, delay=0, cwd=None, exec_prompt=None, files=(),
        workspace=None,
        model=None,
        timeout_flag=None,
        timeout=10,
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
        if timeout_flag is not None:
            command.extend(["-t", timeout_flag])
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
            timeout=timeout,
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

    def test_force_choice_enables_full_access_at_runtime(self):
        result, endpoint = self.run_agent(
            [
                tool_response("printf 'first\\n'"),
                tool_response("printf 'second\\n'"),
                text_response("done"),
            ],
            "diagnose\n\nf\n/exit\n\n",
        )
        self.assertIn("[force mode is now ON; commands run without approval]", result.stdout)
        self.assertIn("first", result.stdout)
        self.assertIn("second", result.stdout)
        self.assertEqual(result.stdout.count("[a]llow"), 1)
        self.assertEqual(len(endpoint.requests), 3)

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

    def test_startup_banner_reports_scoped_workspace(self):
        with tempfile.TemporaryDirectory() as workspace:
            resolved = os.path.realpath(workspace)
            result, endpoint = self.run_agent(
                [text_response("done")],
                "diagnose\n\n/exit\n\n",
                workspace=workspace,
            )
        self.assertIn(f"Workspace: {resolved} (scoped)", result.stdout)
        self.assertIn(
            f"Working directory: {resolved}", self.system_content(endpoint)
        )
        self.assertIn("scoped", self.system_content(endpoint))

    def test_startup_banner_reports_unrestricted_workspace(self):
        result, endpoint = self.run_agent(
            [text_response("done")],
            "diagnose\n\n/exit\n\n",
        )
        self.assertIn(f"Workspace: {os.getcwd()} (unrestricted)", result.stdout)
        self.assertIn(
            f"Working directory: {os.getcwd()}", self.system_content(endpoint)
        )

    def test_one_shot_omits_banner_but_states_working_directory(self):
        with tempfile.TemporaryDirectory() as workspace:
            resolved = os.path.realpath(workspace)
            result, endpoint = self.run_agent(
                [text_response("done")], exec_prompt="diagnose", workspace=workspace
            )
        self.assertNotIn("Workspace:", result.stdout)
        self.assertIn(
            f"Working directory: {resolved}", self.system_content(endpoint)
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
            api_timeout="5",
            delay=1.5,
            live_delay=1.0,
            wait=4.5,
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

    def test_live_steering_tty_echoes_the_typed_line_once(self):
        endpoint = Endpoint([text_response("done")], delay=2)
        self.addCleanup(endpoint.close)
        master, slave = pty.openpty()
        process = subprocess.Popen(
            [EXECUTABLE],
            stdin=slave,
            stdout=slave,
            stderr=subprocess.STDOUT,
            env={
                "AI_API_URL": f"http://127.0.0.1:{endpoint.port}/v1",
                "AI_TIMEOUT_SECONDS": "8",
                "AI_COMMAND_TIMEOUT_SECONDS": "2",
                "NO_COLOR": "1",
                "PATH": os.environ["PATH"],
            },
        )
        os.close(slave)

        def cleanup():
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=5)
            os.close(master)

        self.addCleanup(cleanup)

        def read_until(marker, timeout):
            data = b""
            deadline = time.time() + timeout
            while time.time() < deadline and marker not in data:
                ready, _, _ = select.select([master], [], [], 0.05)
                if not ready:
                    continue
                try:
                    chunk = os.read(master, 4096)
                except OSError:
                    break
                if not chunk:
                    break
                data += chunk
            return data

        output = b""
        output += read_until(b"[You]: ", 10)
        os.write(master, b"diagnose\r")
        output += read_until(b"[live steering active]", 10)
        os.write(master, b"single echo please\n")
        output += read_until(b"single echo please", 5)
        os.write(master, b"\n")
        output += read_until(b"[live steering queued]", 10)
        os.write(master, b"/exit\r")
        output += read_until(b"[bye]", 5)
        self.assertIn(b"[AI Response]", output)
        self.assertIn(b"done", output)
        # Exactly two renderings: the agent's own character echo and the
        # "[live steering queued]" notice. A tty-driver echo or a
        # whole-buffer redraw would add a third copy.
        self.assertEqual(output.count(b"single echo please"), 2)

    def test_tty_multiline_cj_newline_enter_submit(self):
        endpoint = Endpoint([text_response("done")])
        self.addCleanup(endpoint.thread.join, timeout=2)
        self.addCleanup(endpoint.close)
        master, slave = pty.openpty()
        process = subprocess.Popen(
            [EXECUTABLE],
            stdin=slave,
            stdout=slave,
            stderr=subprocess.STDOUT,
            env={
                "AI_API_URL": f"http://127.0.0.1:{endpoint.port}/v1",
                "AI_TIMEOUT_SECONDS": "8",
                "AI_COMMAND_TIMEOUT_SECONDS": "2",
                "NO_COLOR": "1",
                "PATH": os.environ["PATH"],
            },
        )
        os.close(slave)

        def cleanup():
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=5)
            os.close(master)

        self.addCleanup(cleanup)

        def read_until(marker, timeout):
            data = b""
            deadline = time.time() + timeout
            while time.time() < deadline and marker not in data:
                ready, _, _ = select.select([master], [], [], 0.05)
                if not ready:
                    continue
                try:
                    chunk = os.read(master, 4096)
                except OSError:
                    break
                if not chunk:
                    break
                data += chunk
            return data

        output = read_until(b"[You]: ", 10)
        self.assertIn(b"Multiline: C-J newline, Enter submit", output)
        self.assertNotIn(b"Multiline input ends with an empty line", output)
        os.write(master, b"first line")
        os.write(master, b"\n")
        output += read_until(b"[...]: ", 5)
        os.write(master, b"second line")
        os.write(master, b"\r")
        output += read_until(b"[AI Response]", 10)
        os.write(master, b"/exit\r")
        output += read_until(b"[bye]", 5)
        self.assertIn(b"second line", output)
        messages = endpoint.requests[0]["messages"]
        user = next(m for m in messages if m["role"] == "user")
        self.assertIn("first line\nsecond line", user["content"])

    def test_tty_backspace_removes_full_runes(self):
        endpoint = Endpoint([text_response("done")])
        self.addCleanup(endpoint.thread.join, timeout=2)
        self.addCleanup(endpoint.close)
        master, slave = pty.openpty()
        process = subprocess.Popen(
            [EXECUTABLE],
            stdin=slave,
            stdout=slave,
            stderr=subprocess.STDOUT,
            env={
                "AI_API_URL": f"http://127.0.0.1:{endpoint.port}/v1",
                "AI_TIMEOUT_SECONDS": "8",
                "AI_COMMAND_TIMEOUT_SECONDS": "2",
                "NO_COLOR": "1",
                "PATH": os.environ["PATH"],
            },
        )
        os.close(slave)

        def cleanup():
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=5)
            os.close(master)

        self.addCleanup(cleanup)

        def read_until(marker, timeout):
            data = b""
            deadline = time.time() + timeout
            while time.time() < deadline and marker not in data:
                ready, _, _ = select.select([master], [], [], 0.05)
                if not ready:
                    continue
                try:
                    chunk = os.read(master, 4096)
                except OSError:
                    break
                if not chunk:
                    break
                data += chunk
            return data

        output = read_until(b"[You]: ", 10)
        os.write(master, "你好".encode())
        output += read_until("你好".encode(), 5)
        os.write(master, b"\x7f")
        os.write(master, b"\r")
        output += read_until(b"[AI Response]", 10)
        os.write(master, b"/exit\r")
        read_until(b"[bye]", 5)
        messages = endpoint.requests[0]["messages"]
        user = next(m for m in messages if m["role"] == "user")
        self.assertEqual(user["content"], "你")

    def test_editor_command_prints_edited_text(self):
        master, slave = pty.openpty()
        process = subprocess.Popen(
            [EXECUTABLE, "editor"],
            stdin=slave,
            stdout=slave,
            stderr=subprocess.STDOUT,
            env={"PATH": os.environ["PATH"]},
        )
        os.close(slave)

        def cleanup():
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=5)
            try:
                os.close(master)
            except OSError:
                pass

        self.addCleanup(cleanup)

        def read_until(marker, timeout):
            data = b""
            deadline = time.time() + timeout
            while time.time() < deadline and marker not in data:
                ready, _, _ = select.select([master], [], [], 0.05)
                if not ready:
                    continue
                try:
                    chunk = os.read(master, 4096)
                except OSError:
                    break
                if not chunk:
                    break
                data += chunk
            return data

        output = read_until(b"[Edit]: ", 10)
        os.write(master, "你好".encode())
        output += read_until("你好".encode(), 5)
        os.write(master, b"\x7f")
        os.write(master, b"\r")
        deadline = time.time() + 5
        while time.time() < deadline and process.poll() is None:
            output += read_until(b"\n", 0.2)
        self.assertEqual(process.poll(), 0)
        self.assertEqual(output.count("好".encode()), 1)
        self.assertTrue(output.rstrip().endswith("你".encode()))

    def test_editor_command_eof_cancels_with_status_130(self):
        master, slave = pty.openpty()
        process = subprocess.Popen(
            [EXECUTABLE, "editor"],
            stdin=slave,
            stdout=slave,
            stderr=subprocess.STDOUT,
            env={"PATH": os.environ["PATH"]},
        )
        os.close(slave)
        self.addCleanup(process.wait, timeout=5)

        def read_until(marker, timeout):
            data = b""
            deadline = time.time() + timeout
            while time.time() < deadline and marker not in data:
                ready, _, _ = select.select([master], [], [], 0.05)
                if not ready:
                    continue
                try:
                    chunk = os.read(master, 4096)
                except OSError:
                    break
                if not chunk:
                    break
                data += chunk
            return data

        read_until(b"[Edit]: ", 10)
        os.close(master)
        self.assertEqual(process.wait(timeout=5), 130)

    def test_editor_help_lists_keys(self):
        result = subprocess.run(
            [EXECUTABLE, "editor", "--help"],
            capture_output=True,
            text=True,
            timeout=10,
        )
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stderr, "")
        for keys in ("C-J", "C-A / C-E", "C-B / C-F", "C-P / C-N", "C-K", "C-W", "C-U"):
            self.assertIn(keys, result.stdout)
        self.assertIn("status 130", result.stdout)

        rejected = subprocess.run(
            [EXECUTABLE, "editor", "bogus"],
            capture_output=True,
            text=True,
            timeout=10,
        )
        self.assertEqual(rejected.returncode, 2)
        self.assertIn("Usage: ai editor", rejected.stderr)

    def test_tty_prompt_labels_are_colorized(self):
        endpoint = Endpoint([text_response("done")])
        self.addCleanup(endpoint.thread.join, timeout=2)
        self.addCleanup(endpoint.close)
        master, slave = pty.openpty()
        process = subprocess.Popen(
            [EXECUTABLE],
            stdin=slave,
            stdout=slave,
            stderr=subprocess.STDOUT,
            env={
                "AI_API_URL": f"http://127.0.0.1:{endpoint.port}/v1",
                "AI_TIMEOUT_SECONDS": "8",
                "AI_COMMAND_TIMEOUT_SECONDS": "2",
                "TERM": "xterm",
                "PATH": os.environ["PATH"],
            },
        )
        os.close(slave)

        def cleanup():
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=5)
            os.close(master)

        self.addCleanup(cleanup)

        def read_until(marker, timeout):
            data = b""
            deadline = time.time() + timeout
            while time.time() < deadline and marker not in data:
                ready, _, _ = select.select([master], [], [], 0.05)
                if not ready:
                    continue
                try:
                    chunk = os.read(master, 4096)
                except OSError:
                    break
                if not chunk:
                    break
                data += chunk
            return data

        output = read_until(b"[You]: ", 10)
        self.assertIn(b"\x1b[1;32m", output)
        os.write(master, b"hi\r")
        output += read_until(b"[AI Response]", 10)
        self.assertIn(b"\x1b[1;36m", output)
        os.write(master, b"/exit\r")
        read_until(b"[bye]", 5)

    def test_tty_decision_prompts_take_answers_on_the_prompt_line(self):
        endpoint = Endpoint([tool_response("true"), text_response("done")])
        self.addCleanup(endpoint.thread.join, timeout=2)
        self.addCleanup(endpoint.close)
        master, slave = pty.openpty()
        process = subprocess.Popen(
            [EXECUTABLE],
            stdin=slave,
            stdout=slave,
            stderr=subprocess.STDOUT,
            env={
                "AI_API_URL": f"http://127.0.0.1:{endpoint.port}/v1",
                "AI_TIMEOUT_SECONDS": "8",
                "AI_COMMAND_TIMEOUT_SECONDS": "2",
                "NO_COLOR": "1",
                "PATH": os.environ["PATH"],
            },
        )
        os.close(slave)

        def cleanup():
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=5)
            os.close(master)

        self.addCleanup(cleanup)

        def read_until(marker, timeout):
            data = b""
            deadline = time.time() + timeout
            while time.time() < deadline and marker not in data:
                ready, _, _ = select.select([master], [], [], 0.05)
                if not ready:
                    continue
                try:
                    chunk = os.read(master, 4096)
                except OSError:
                    break
                if not chunk:
                    break
                data += chunk
            return data

        output = read_until(b"[You]: ", 10)
        os.write(master, b"run command\r")
        output += read_until(b"[q]uit: ", 10)
        os.write(master, b"n\r")
        output += read_until(b"[AI Response]", 10)
        os.write(master, b"/exit\r")
        output += read_until(b"[bye]", 5)
        self.assertIn(b"[You]: run command", output)
        self.assertIn(b"[q]uit: n", output)
        self.assertNotIn(b"[q]uit: \r\n", output)

    def test_tty_editing_keys_move_and_kill_within_the_line(self):
        endpoint = Endpoint(
            [text_response("one"), text_response("two"), text_response("three")]
        )
        self.addCleanup(endpoint.thread.join, timeout=2)
        self.addCleanup(endpoint.close)
        master, slave = pty.openpty()
        process = subprocess.Popen(
            [EXECUTABLE],
            stdin=slave,
            stdout=slave,
            stderr=subprocess.STDOUT,
            env={
                "AI_API_URL": f"http://127.0.0.1:{endpoint.port}/v1",
                "AI_TIMEOUT_SECONDS": "8",
                "AI_COMMAND_TIMEOUT_SECONDS": "2",
                "NO_COLOR": "1",
                "PATH": os.environ["PATH"],
            },
        )
        os.close(slave)

        def cleanup():
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=5)
            os.close(master)

        self.addCleanup(cleanup)

        def read_until(marker, timeout):
            data = b""
            deadline = time.time() + timeout
            while time.time() < deadline and marker not in data:
                ready, _, _ = select.select([master], [], [], 0.05)
                if not ready:
                    continue
                try:
                    chunk = os.read(master, 4096)
                except OSError:
                    break
                if not chunk:
                    break
                data += chunk
            return data

        output = read_until(b"[You]: ", 10)
        os.write(master, b"world")           # "world"
        os.write(master, b"\x01")            # C-A to the start
        os.write(master, b"hello ")          # insert mid-line -> "hello world"
        os.write(master, b"\x05")            # C-E to the end
        os.write(master, b"!")
        os.write(master, b"\r")
        output += read_until(b"[You]: ", 10)
        os.write(master, b"abc")
        os.write(master, b"\x02")            # C-B onto 'c'
        os.write(master, b"\x04")            # C-D deletes 'c' -> "ab"
        os.write(master, b"\r")
        output += read_until(b"[You]: ", 10)
        os.write(master, b"nope")
        os.write(master, b"\x01")            # C-A
        os.write(master, b"\x0b")            # C-K kills to the end -> ""
        os.write(master, b"ok")
        os.write(master, b"\r")
        output += read_until(b"[You]: ", 10)
        os.write(master, b"/exit\r")
        output += read_until(b"[bye]", 5)
        contents = [
            next(
                m["content"] for m in reversed(endpoint.requests[index]["messages"])
                if m["role"] == "user"
            )
            for index in range(3)
        ]
        self.assertEqual(contents[0], "hello world!")
        self.assertEqual(contents[1], "ab")
        self.assertEqual(contents[2], "ok")

    def test_tty_control_u_clears_all_multiline_input(self):
        endpoint = Endpoint([text_response("done")])
        self.addCleanup(endpoint.thread.join, timeout=2)
        self.addCleanup(endpoint.close)
        master, slave = pty.openpty()
        process = subprocess.Popen(
            [EXECUTABLE],
            stdin=slave,
            stdout=slave,
            stderr=subprocess.STDOUT,
            env={
                "AI_API_URL": f"http://127.0.0.1:{endpoint.port}/v1",
                "AI_TIMEOUT_SECONDS": "8",
                "AI_COMMAND_TIMEOUT_SECONDS": "2",
                "NO_COLOR": "1",
                "PATH": os.environ["PATH"],
            },
        )
        os.close(slave)

        def cleanup():
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=5)
            os.close(master)

        self.addCleanup(cleanup)

        def read_until(marker, timeout):
            data = b""
            deadline = time.time() + timeout
            while time.time() < deadline and marker not in data:
                ready, _, _ = select.select([master], [], [], 0.05)
                if not ready:
                    continue
                try:
                    chunk = os.read(master, 4096)
                except OSError:
                    break
                if not chunk:
                    break
                data += chunk
            return data

        output = read_until(b"[You]: ", 10)
        os.write(master, b"first")
        os.write(master, b"\n")            # C-J starts a second line
        os.write(master, b"second")
        os.write(master, b"\x15")          # C-U clears every line like the TUI
        os.write(master, b"fresh")
        os.write(master, b"\r")
        output += read_until(b"[You]: ", 10)
        os.write(master, b"/exit\r")
        output += read_until(b"[bye]", 5)
        self.assertIn(b"\x1b[A", output)   # continuation lines erased via cursor-up
        content = next(
            m["content"] for m in reversed(endpoint.requests[0]["messages"])
            if m["role"] == "user"
        )
        self.assertEqual(content, "fresh")

    def test_tty_control_p_n_move_between_input_lines(self):
        endpoint = Endpoint([text_response("one"), text_response("two")])
        self.addCleanup(endpoint.thread.join, timeout=2)
        self.addCleanup(endpoint.close)
        master, slave = pty.openpty()
        process = subprocess.Popen(
            [EXECUTABLE],
            stdin=slave,
            stdout=slave,
            stderr=subprocess.STDOUT,
            env={
                "AI_API_URL": f"http://127.0.0.1:{endpoint.port}/v1",
                "AI_TIMEOUT_SECONDS": "8",
                "AI_COMMAND_TIMEOUT_SECONDS": "2",
                "NO_COLOR": "1",
                "PATH": os.environ["PATH"],
            },
        )
        os.close(slave)

        def cleanup():
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=5)
            os.close(master)

        self.addCleanup(cleanup)

        def read_until(marker, timeout):
            data = b""
            deadline = time.time() + timeout
            while time.time() < deadline and marker not in data:
                ready, _, _ = select.select([master], [], [], 0.05)
                if not ready:
                    continue
                try:
                    chunk = os.read(master, 4096)
                except OSError:
                    break
                if not chunk:
                    break
                data += chunk
            return data

        output = read_until(b"[You]: ", 10)
        os.write(master, b"one two")
        os.write(master, b"\n")            # C-J starts a second line
        os.write(master, b"three four")
        os.write(master, b"\x10")          # C-P up to line one (column clamps)
        os.write(master, b"\x7f")          # backspace trims "one two" -> "one tw"
        os.write(master, b"\x0e")          # C-N down, same column
        os.write(master, b"\x05")          # C-E to the end of the second line
        os.write(master, b"!")
        os.write(master, b"\r")
        # C-P reprints the "[You]: " prompt, so wait for the response marker.
        output += read_until(b"[AI Response]", 10)
        os.write(master, b"aa")
        os.write(master, b"\n")            # C-J
        os.write(master, b"bb")
        os.write(master, b"\x10")          # C-P up, then C-J appends below "bb"
        os.write(master, b"\n")
        os.write(master, b"cc")
        os.write(master, b"\r")
        output += read_until(b"[AI Response]", 10)
        os.write(master, b"/exit\r")
        output += read_until(b"[bye]", 5)
        self.assertIn(b"\x1b[A", output)   # C-P stepped up a row
        self.assertIn(b"\x1b[B", output)   # C-N / C-J stepped down a row
        contents = [
            next(
                m["content"] for m in reversed(endpoint.requests[index]["messages"])
                if m["role"] == "user"
            )
            for index in range(2)
        ]
        self.assertEqual(contents[0], "one tw\nthree four!")
        self.assertEqual(contents[1], "aa\nbb\ncc")

    def test_tty_prompt_redraw_keeps_the_screen_clean(self):
        if shutil.which("tmux") is None:
            self.skipTest("tmux is required for tty rendering")

        endpoint = Endpoint([text_response("ok")])
        self.addCleanup(endpoint.thread.join, timeout=2)
        self.addCleanup(endpoint.close)
        server = f"ai-tty-clean-{os.getpid()}"
        subprocess.run(["tmux", "kill-server"], check=False)
        self.addCleanup(subprocess.run, ["tmux", "kill-server"], check=False)
        environment = os.environ.copy()
        environment.update(
            {
                "AI_API_URL": f"http://127.0.0.1:{endpoint.port}/v1",
                "AI_TIMEOUT_SECONDS": "5",
                "AI_COMMAND_TIMEOUT_SECONDS": "2",
                "NO_COLOR": "1",
            }
        )
        subprocess.run(
            [
                "tmux", "-f", "/dev/null", "new-session", "-d", "-x", "100", "-y", "24",
                "-n", server, EXECUTABLE,
            ],
            env=environment,
            check=True,
            timeout=5,
        )
        self.addCleanup(subprocess.run, ["tmux", "kill-session", "-t", server], check=False)

        def pane():
            time.sleep(0.3)
            return subprocess.run(
                ["tmux", "capture-pane", "-p", "-t", server],
                check=True,
                capture_output=True,
                text=True,
                timeout=5,
            ).stdout

        time.sleep(1.0)
        subprocess.run(["tmux", "send-keys", "-t", server, "one two"], check=True)
        subprocess.run(["tmux", "send-keys", "-t", server, "C-j"], check=True)
        subprocess.run(["tmux", "send-keys", "-t", server, "three four"], check=True)
        screen = pane()
        self.assertEqual(screen.count("[You]:"), 1)
        self.assertIn("[...]: three four", screen)
        subprocess.run(["tmux", "send-keys", "-t", server, "C-p"], check=True)
        screen = pane()
        self.assertEqual(screen.count("[You]:"), 1)  # the reprint stays on its row
        self.assertIn("[...]: three four", screen)    # the second row is untouched
        subprocess.run(["tmux", "send-keys", "-t", server, "C-u"], check=True)
        screen = pane()
        self.assertEqual(screen.count("[You]:"), 1)
        self.assertNotIn("[...]:", screen)            # continuation row erased too
        subprocess.run(["tmux", "send-keys", "-t", server, "ok"], check=True)
        subprocess.run(["tmux", "send-keys", "-t", server, "Enter"], check=True)
        time.sleep(2)
        self.assertIn("ok", pane())

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
        self.assertIn("malformed or missing tool arguments", result.stdout)
        self.assertIn(
            "malformed", endpoint.requests[1]["messages"][-1]["content"]
        )

    def test_plan_tool_records_and_pins_checklist(self):
        checklist = "goal: fix the build\n1. [ ] reproduce\n2. [ ] patch"
        result, endpoint = self.run_agent(
            [plan_response(checklist), text_response("done")],
            "diagnose\n\n\n/exit\n\n",
        )
        self.assertIn("[Plan updated]:", result.stdout)
        self.assertIn("2. [ ] patch", result.stdout)
        self.assertNotIn("[AI requests command]", result.stdout)  # no approval boundary
        tool_names = {tool["function"]["name"] for tool in endpoint.requests[0]["tools"]}
        self.assertEqual(tool_names, {"run_terminal_command", "plan"})
        pinned = [
            message for message in endpoint.requests[1]["messages"]
            if message["role"] == "system" and message["content"].startswith("CURRENT TASK PLAN")
        ]
        self.assertEqual(len(pinned), 1)
        self.assertIn("1. [ ] reproduce", pinned[0]["content"])
        calls = [
            call
            for message in endpoint.requests[1]["messages"]
            if message.get("tool_calls")
            for call in message["tool_calls"]
        ]
        self.assertEqual(calls[0]["function"]["name"], "plan")
        self.assertEqual(json.loads(calls[0]["function"]["arguments"])["checklist"], checklist)
        replies = [
            message for message in endpoint.requests[1]["messages"]
            if message["role"] == "tool"
        ]
        self.assertIn("Plan recorded", replies[0]["content"])

    def test_plan_tool_clears_with_empty_checklist(self):
        result, endpoint = self.run_agent(
            [plan_response("1. [ ] only step"), plan_response(""), text_response("done")],
            "diagnose\n\n\n/exit\n\n",
        )
        self.assertIn("[Plan updated]:", result.stdout)
        self.assertIn("[Plan cleared]", result.stdout)
        self.assertNotIn(
            "CURRENT TASK PLAN",
            json.dumps(endpoint.requests[2]["messages"]),
        )

    def test_plan_without_checklist_is_rejected_visibly(self):
        response = {
            "choices": [
                {
                    "message": {
                        "role": "assistant",
                        "content": None,
                        "tool_calls": [
                            {
                                "type": "function",
                                "id": "call-plan",
                                "function": {"name": "plan", "arguments": "{}"},
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
        self.assertIn("malformed or missing tool arguments", result.stdout)
        self.assertNotIn("CURRENT TASK PLAN", json.dumps(endpoint.requests[1]["messages"]))
        replies = [
            message for message in endpoint.requests[1]["messages"]
            if message["role"] == "tool"
        ]
        self.assertEqual(replies[0]["name"], "plan")
        self.assertIn("malformed", replies[0]["content"])

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

    def run_terminal_one_shot(self, no_color=False):
        endpoint = Endpoint([text_response("done")])
        self.addCleanup(endpoint.thread.join, timeout=2)
        self.addCleanup(endpoint.close)
        master, slave = pty.openpty()
        environment = {
            "AI_API_URL": f"http://127.0.0.1:{endpoint.port}/v1",
            "AI_TIMEOUT_SECONDS": "2",
            "TERM": "xterm",
            "PATH": os.environ["PATH"],
        }
        if no_color:
            environment["NO_COLOR"] = ""
        process = subprocess.Popen(
            [EXECUTABLE, "-e", "diagnose"],
            stdin=subprocess.DEVNULL,
            stdout=slave,
            stderr=subprocess.STDOUT,
            env=environment,
        )
        os.close(slave)
        self.addCleanup(os.close, master)
        process.wait(timeout=5)
        output = b""
        while True:
            try:
                chunk = os.read(master, 4096)
            except OSError:
                break
            if not chunk:
                break
            output += chunk
        return output

    def test_terminal_structural_styling(self):
        output = self.run_terminal_one_shot()
        self.assertIn(b"\033[1;36m\r\n[AI Response]", output)
        self.assertIn(b"\033[0m", output)

    def test_no_color_disables_terminal_styling(self):
        output = self.run_terminal_one_shot(no_color=True)
        self.assertNotIn(b"\033[", output)

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
        # glibc says "Connection timed out", musl "Operation timed out".
        self.assertIn("timed out", output)
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

    def test_timeout_flag_overrides_env(self):
        result, endpoint = self.run_agent(
            [tool_response("sleep 3"), text_response("done")],
            "diagnose\n\na\n\n\n/exit\n\n",
            command_timeout="600",
            timeout_flag="1",
        )
        self.assertIn("command timeout: 1s", result.stdout)
        self.assertIn("command timed out after 1 seconds", result.stdout)
        self.assertIn(
            "[command timed out after 1 seconds] exit status",
            endpoint.requests[1]["messages"][-1]["content"],
        )

    def test_timeout_flag_zero_disables_timeout(self):
        result, _ = self.run_agent(
            [tool_response("sleep 2; echo slept-well"), text_response("done")],
            "diagnose\n\na\n\n\n/exit\n\n",
            command_timeout="1",
            timeout_flag="0",
        )
        self.assertIn("slept-well", result.stdout)
        self.assertNotIn("command timed out", result.stdout)
        self.assertIn("command timeout: none", result.stdout)

    def test_timeout_flag_rejects_invalid_values(self):
        invalid = (["-t", "abc"], ["-t", "-1"], ["-t", "1801"], ["-t", ""], ["-t", "1", "-t", "2"])
        for args in invalid:
            with self.subTest(args=args):
                process = subprocess.run(
                    [EXECUTABLE, *args, "-e", "x"],
                    input="",
                    text=True,
                    capture_output=True,
                    env={"PATH": os.environ["PATH"]},
                )
                self.assertEqual(process.returncode, 2)

    def test_command_output_is_tail_truncated(self):
        result, endpoint = self.run_agent(
            [
                tool_response("(echo HEADMARK; seq 2 22000; echo TAILMARK)"),
                text_response("done"),
            ],
            full_access=True,
            exec_prompt="dump",
        )
        self.assertIn("HEADMARK", result.stdout)
        self.assertIn("TAILMARK", result.stdout)
        content = endpoint.requests[1]["messages"][-1]["content"]
        self.assertIn("bytes dropped]", content)
        self.assertIn("[output tail]", content)
        self.assertIn("TAILMARK", content)
        self.assertNotIn("HEADMARK", content)

    def test_usage_stats_printed_at_interactive_exit(self):
        result, _ = self.run_agent(
            [text_response("ok", prompt_tokens=12400, completion_tokens=3100)],
            "go\n\n/exit\n\n",
        )
        self.assertIn(
            "[usage] tokens: 12.4k in / 3.1k out, 1 rounds, context 12.4k",
            result.stdout,
        )

    def test_usage_stats_survive_new_conversation(self):
        result, _ = self.run_agent(
            [text_response("a", 1000, 10), text_response("b", 1100, 20)],
            "go\n\n/new\n\nagain\n\n/exit\n\n",
        )
        self.assertIn(
            "[usage] tokens: 2.1k in / 30 out, 2 rounds, context 1.1k",
            result.stdout,
        )

    def test_usage_stats_printed_after_each_one_shot_round(self):
        result, _ = self.run_agent(
            [tool_response("printf 'x\\n'"), text_response("done", 500, 25)],
            full_access=True,
            exec_prompt="do",
        )
        self.assertIn(
            "[usage] tokens: 0 in / 0 out, 1 rounds, context 0", result.stdout
        )
        self.assertIn(
            "[usage] tokens: 500 in / 25 out, 2 rounds, context 500",
            result.stdout,
        )

    def test_tty_usage_line_starts_after_interrupt(self):
        endpoint = Endpoint([text_response("ok", 100, 10)])
        self.addCleanup(endpoint.thread.join, timeout=2)
        self.addCleanup(endpoint.close)
        master, slave = pty.openpty()
        process = subprocess.Popen(
            [EXECUTABLE],
            stdin=slave,
            stdout=slave,
            stderr=subprocess.STDOUT,
            env={
                "AI_API_URL": f"http://127.0.0.1:{endpoint.port}/v1",
                "AI_TIMEOUT_SECONDS": "8",
                "AI_COMMAND_TIMEOUT_SECONDS": "2",
                "NO_COLOR": "1",
                "PATH": os.environ["PATH"],
            },
        )
        os.close(slave)

        def cleanup():
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=5)
            os.close(master)

        self.addCleanup(cleanup)

        def read_until(marker, timeout):
            data = b""
            deadline = time.time() + timeout
            while time.time() < deadline and marker not in data:
                ready, _, _ = select.select([master], [], [], 0.05)
                if not ready:
                    continue
                try:
                    chunk = os.read(master, 4096)
                except OSError:
                    break
                if not chunk:
                    break
                data += chunk
            return data

        output = read_until(b"[You]: ", 10)
        os.write(master, b"hi\r")
        output += read_until(b"[You]: ", 10)
        process.send_signal(signal.SIGINT)
        output += read_until(b"[bye]", 5)
        self.assertIn(b"[usage] tokens: 100 in / 10 out, 1 rounds, context 100", output)
        self.assertNotIn(b"[You]: [usage]", output)

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

    def test_endpoint_http_error_reports_detail(self):
        body = json.dumps(
            {"error": {"code": "1214", "message": "messages 参数非法"}}
        ).encode()
        result, endpoint = self.run_agent(
            [
                b"HTTP/1.1 400 Bad Request\r\n"
                b"Content-Type: application/json\r\n"
                b"Content-Length: %d\r\n" % len(body)
                + b"Connection: close\r\n\r\n"
                + body,
                text_response("recovered"),
            ],
            "first\n\nsecond\n\n/exit\n\n",
        )
        self.assertIn("AI endpoint returned HTTP 400: messages 参数非法", result.stdout)
        self.assertIn("recovered", result.stdout)
        self.assertIn("[bye]", result.stdout)
        self.assertEqual(len(endpoint.requests), 2)

    def test_http_429_is_retried_with_retry_after(self):
        result, endpoint = self.run_agent(
            [
                b"HTTP/1.1 429 Too Many Requests\r\n"
                b"Retry-After: 2\r\n"
                b"Content-Length: 0\r\n"
                b"Connection: close\r\n\r\n",
                text_response("recovered"),
            ],
            "go\n\n/exit\n\n",
        )
        self.assertIn("AI endpoint returned HTTP 429", result.stdout)
        self.assertIn("[retrying in 2s (attempt 2/3)]", result.stdout)
        self.assertIn("recovered", result.stdout)
        self.assertEqual(len(endpoint.requests), 2)
        self.assertEqual(endpoint.requests[0], endpoint.requests[1])

    def test_http_500_is_retried_until_exhausted(self):
        refused = (
            b"HTTP/1.1 500 Internal Server Error\r\n"
            b"Content-Length: 0\r\n"
            b"Connection: close\r\n\r\n"
        )
        result, endpoint = self.run_agent(
            [refused, refused, refused],
            "go\n\n/exit\n\n",
        )
        self.assertIn("AI endpoint returned HTTP 500", result.stdout)
        self.assertIn("[retrying in 1s (attempt 2/3)]", result.stdout)
        self.assertIn("[retrying in 2s (attempt 3/3)]", result.stdout)
        self.assertNotIn("attempt 4/3", result.stdout)
        self.assertEqual(len(endpoint.requests), 3)

    def test_http_400_is_not_retried(self):
        result, endpoint = self.run_agent(
            [
                b"HTTP/1.1 400 Bad Request\r\n"
                b"Content-Length: 0\r\n"
                b"Connection: close\r\n\r\n",
                text_response("recovered"),
            ],
            "first\n\nsecond\n\n/exit\n\n",
        )
        self.assertIn("AI endpoint returned HTTP 400", result.stdout)
        self.assertNotIn("retrying", result.stdout)
        self.assertEqual(len(endpoint.requests), 2)

    def test_connection_refused_is_retried(self):
        probe = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        probe.bind(("127.0.0.1", 0))
        port = probe.getsockname()[1]
        probe.close()
        process = subprocess.run(
            [EXECUTABLE, "-e", "diagnose"],
            input="",
            text=True,
            capture_output=True,
            timeout=30,
            env={
                "AI_API_URL": f"http://127.0.0.1:{port}/v1",
                "AI_TIMEOUT_SECONDS": "2",
                "AI_COMMAND_TIMEOUT_SECONDS": "2",
                "PATH": os.environ["PATH"],
            },
        )
        self.assertEqual(process.returncode, 1)
        combined = process.stdout + process.stderr
        self.assertIn("[retrying in 1s (attempt 2/3)]", combined)
        self.assertIn("[retrying in 2s (attempt 3/3)]", combined)
        self.assertIn("connect", combined)

    def test_context_drop_keeps_a_user_message(self):
        # MAX_MESSAGES forces a drop while the single user prompt is still
        # the only user message; every request must keep one.
        responses = [tool_response("true") for _ in range(12)]
        responses.append(text_response("done"))
        result, endpoint = self.run_agent(
            responses,
            full_access=True,
            exec_prompt="diagnose",
            timeout=60,
        )
        self.assertIn("done", result.stdout)
        self.assertIn("[context] Dropped", result.stdout)
        self.assertGreaterEqual(len(endpoint.requests), 13)
        for request in endpoint.requests:
            roles = [message["role"] for message in request["messages"]]
            self.assertIn("user", roles)
            for index, message in enumerate(request["messages"]):
                if message.get("role") != "tool":
                    continue
                previous = request["messages"][index - 1]
                self.assertIn("tool_calls", previous)
                ids = {call["id"] for call in previous["tool_calls"]}
                self.assertIn(message["tool_call_id"], ids)

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

    def test_tui_timeout_flag_is_forwarded(self):
        if shutil.which("tmux") is None:
            self.skipTest("tmux is required for TUI timeout forwarding")
        endpoint = Endpoint([text_response("done")])
        self.addCleanup(endpoint.thread.join, timeout=2)
        self.addCleanup(endpoint.close)
        server = f"ai-tui-timeout-{os.getpid()}"
        subprocess.run(["tmux", "kill-server"], check=False)
        self.addCleanup(subprocess.run, ["tmux", "kill-server"], check=False)
        environment = os.environ.copy()
        environment.update(
            {
                "AI_API_URL": f"http://127.0.0.1:{endpoint.port}/v1",
                "AI_TIMEOUT_SECONDS": "2",
                "AI_COMMAND_TIMEOUT_SECONDS": "600",
                "PATH": os.environ["PATH"],
            }
        )
        subprocess.run(
            [
                "tmux", "-f", "/dev/null", "new-session", "-d", "-x", "100", "-y", "24",
                "-n", server, TUI_EXECUTABLE, "tui", "-t", "17",
            ],
            env=environment,
            check=True,
            timeout=5,
        )
        self.addCleanup(
            subprocess.run, ["tmux", "kill-session", "-t", server], check=False
        )
        found = False
        output = ""
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            output = subprocess.run(
                ["tmux", "capture-pane", "-p", "-t", server],
                check=True,
                capture_output=True,
                text=True,
                timeout=5,
            ).stdout
            if "command timeout: 17s" in output:
                found = True
                break
            time.sleep(0.2)
        self.assertTrue(found, f"timeout banner not shown in TUI:\n{output}")

    def test_tui_plan_updates_replace_previous_block(self):
        if shutil.which("tmux") is None:
            self.skipTest("tmux is required for TUI plan rendering")

        first = "goal: two steps\n1. [ ] echo alpha\n2. [ ] echo beta"
        second = "goal: two steps\n1. [x] echo alpha\n2. [ ] echo beta"
        endpoint = Endpoint([plan_response(first), plan_response(second), text_response("done")])
        self.addCleanup(endpoint.thread.join, timeout=2)
        self.addCleanup(endpoint.close)
        server = f"ai-tui-plan-{os.getpid()}"
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
        subprocess.run(["tmux", "send-keys", "-t", server, "run it"], check=True)
        subprocess.run(["tmux", "send-keys", "-t", server, "Enter"], check=True)
        time.sleep(4)
        output = subprocess.run(
            ["tmux", "capture-pane", "-p", "-t", server],
            check=True,
            capture_output=True,
            text=True,
            timeout=5,
        ).stdout
        thoughts = output.split("2. AI Command Status")[0]
        self.assertEqual(thoughts.count("[Plan updated]:"), 1)
        self.assertIn("1. [x] echo alpha", thoughts)   # the latest checklist is shown
        self.assertIn("done", thoughts)

    def test_tui_plan_cleared_removes_block(self):
        if shutil.which("tmux") is None:
            self.skipTest("tmux is required for TUI plan rendering")

        checklist = "goal: one step\n1. [ ] echo alpha"
        endpoint = Endpoint([plan_response(checklist), plan_response(""), text_response("done")])
        self.addCleanup(endpoint.thread.join, timeout=2)
        self.addCleanup(endpoint.close)
        server = f"ai-tui-plan-clear-{os.getpid()}"
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
        subprocess.run(["tmux", "send-keys", "-t", server, "run it"], check=True)
        subprocess.run(["tmux", "send-keys", "-t", server, "Enter"], check=True)
        time.sleep(4)
        output = subprocess.run(
            ["tmux", "capture-pane", "-p", "-t", server],
            check=True,
            capture_output=True,
            text=True,
            timeout=5,
        ).stdout
        thoughts = output.split("2. AI Command Status")[0]
        self.assertIn("[Plan cleared]", thoughts)
        self.assertNotIn("[Plan updated]:", thoughts)
        self.assertNotIn("goal: one step", thoughts)

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

    def test_tui_control_w_kills_the_previous_word(self):
        if shutil.which("tmux") is None:
            self.skipTest("tmux is required for TUI input editing")

        endpoint = Endpoint([text_response("done")])
        self.addCleanup(endpoint.thread.join, timeout=2)
        self.addCleanup(endpoint.close)
        server = f"ai-tui-kill-word-{os.getpid()}"
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
        subprocess.run(["tmux", "send-keys", "-t", server, "one two three"], check=True)
        subprocess.run(["tmux", "send-keys", "-t", server, "C-w"], check=True)
        subprocess.run(["tmux", "send-keys", "-t", server, "C-w"], check=True)
        subprocess.run(["tmux", "send-keys", "-t", server, "Enter"], check=True)
        deadline = time.monotonic() + 8
        while time.monotonic() < deadline:
            if endpoint.requests:
                break
            time.sleep(0.1)
        self.assertTrue(endpoint.requests, "TUI did not reach the endpoint")
        content = endpoint.requests[0]["messages"][-1]["content"]
        self.assertEqual(content, "one ")

    def test_tui_control_p_n_move_the_visible_cursor(self):
        if shutil.which("tmux") is None:
            self.skipTest("tmux is required for TUI input editing")

        def cursor_position(server):
            result = subprocess.run(
                ["tmux", "display-message", "-p", "-t", server, "#{cursor_y} #{cursor_x}"],
                check=True,
                capture_output=True,
                text=True,
                timeout=5,
            ).stdout
            row, column = result.split()
            return int(row), int(column)

        endpoint = Endpoint([text_response("done")])
        self.addCleanup(endpoint.thread.join, timeout=2)
        self.addCleanup(endpoint.close)
        server = f"ai-tui-cursor-{os.getpid()}"
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
        subprocess.run(["tmux", "send-keys", "-t", server, "one two"], check=True)
        subprocess.run(["tmux", "send-keys", "-t", server, "C-j"], check=True)
        subprocess.run(["tmux", "send-keys", "-t", server, "three four"], check=True)
        time.sleep(0.3)
        # Bottom window: row 19 is the border, the input lines follow.
        self.assertEqual(cursor_position(server), (21, 11))
        subprocess.run(["tmux", "send-keys", "-t", server, "C-p"], check=True)
        time.sleep(0.3)
        self.assertEqual(cursor_position(server), (20, 8))
        subprocess.run(["tmux", "send-keys", "-t", server, "C-p"], check=True)
        time.sleep(0.3)
        self.assertEqual(cursor_position(server), (20, 8))
        subprocess.run(["tmux", "send-keys", "-t", server, "C-n"], check=True)
        time.sleep(0.3)
        self.assertEqual(cursor_position(server), (21, 8))
        subprocess.run(["tmux", "send-keys", "-t", server, "Enter"], check=True)
        deadline = time.monotonic() + 8
        while time.monotonic() < deadline:
            if endpoint.requests:
                break
            time.sleep(0.1)
        self.assertTrue(endpoint.requests, "TUI did not reach the endpoint")
        content = endpoint.requests[0]["messages"][-1]["content"]
        self.assertEqual(content, "one two\nthree four")

    def test_tui_control_p_lands_on_rune_boundaries(self):
        if shutil.which("tmux") is None:
            self.skipTest("tmux is required for TUI input editing")

        def cursor_position(server):
            result = subprocess.run(
                ["tmux", "display-message", "-p", "-t", server, "#{cursor_y} #{cursor_x}"],
                check=True,
                capture_output=True,
                text=True,
                timeout=5,
            ).stdout
            row, column = result.split()
            return int(row), int(column)

        endpoint = Endpoint([text_response("done")])
        self.addCleanup(endpoint.thread.join, timeout=2)
        self.addCleanup(endpoint.close)
        server = f"ai-tui-rune-{os.getpid()}"
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
        subprocess.run(["tmux", "send-keys", "-t", server, "你好"], check=True)
        subprocess.run(["tmux", "send-keys", "-t", server, "C-j"], check=True)
        subprocess.run(["tmux", "send-keys", "-t", server, "abcdef"], check=True)
        time.sleep(0.3)
        self.assertEqual(cursor_position(server), (21, 7))
        subprocess.run(["tmux", "send-keys", "-t", server, "C-p"], check=True)
        time.sleep(0.3)
        # Byte column 6 of "你好" is display column 4: the cursor sits at
        # the end of the second rune, not past it or inside it.
        self.assertEqual(cursor_position(server), (20, 5))
        subprocess.run(["tmux", "send-keys", "-t", server, "z"], check=True)
        subprocess.run(["tmux", "send-keys", "-t", server, "Enter"], check=True)
        deadline = time.monotonic() + 8
        while time.monotonic() < deadline:
            if endpoint.requests:
                break
            time.sleep(0.1)
        self.assertTrue(endpoint.requests, "TUI did not reach the endpoint")
        content = endpoint.requests[0]["messages"][-1]["content"]
        self.assertEqual(content, "你好z\nabcdef")

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

    def test_tui_multiline_banner_matches_input_keys(self):
        if shutil.which("tmux") is None:
            self.skipTest("tmux is required for TUI input rendering")

        server = f"ai-tui-banner-{os.getpid()}"
        subprocess.run(["tmux", "kill-server"], check=False)
        self.addCleanup(subprocess.run, ["tmux", "kill-server"], check=False)
        environment = os.environ.copy()
        environment["AI_API_URL"] = "http://127.0.0.1:1/v1"
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
        output = subprocess.run(
            ["tmux", "capture-pane", "-p", "-t", server],
            check=True,
            capture_output=True,
            text=True,
            timeout=5,
        ).stdout
        self.assertIn("Multiline: C-J newline, Enter submit", output)
        self.assertNotIn("Multiline input ends with an empty line", output)

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
                "AI_TOOL_ROUNDS": "1025",
            },
            timeout=5,
        )
        self.assertEqual(process.returncode, 2)
        self.assertIn("AI_TOOL_ROUNDS is invalid", process.stdout + process.stderr)

    def test_tool_round_limit_accepts_long_tasks(self):
        result, _ = self.run_agent(
            [text_response("done")],
            "diagnose\n\n/exit\n\n",
            tool_rounds="512",
        )
        self.assertIn("tool rounds: 512", result.stdout)

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
