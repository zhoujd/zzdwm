import json
import os
import pty
import select
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import threading
import time
import unittest


ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXECUTABLE = os.path.join(ROOT, "ai")
TUI_EXECUTABLE = EXECUTABLE


class ProgressTestResult(unittest.TextTestResult):
    interval_percent = 10

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.completed = 0
        self.next_percent = self.interval_percent

    def addSuccess(self, test):
        pass

    def stopTest(self, test):
        super().stopTest(test)
        self.completed += 1
        while self.completed * 100 >= self.next_percent * self.total:
            print(
                f"[progress] {self.completed}/{self.total} tests "
                f"({self.next_percent}%)",
                file=sys.stderr,
                flush=True,
            )
            self.next_percent += self.interval_percent


class ProgressTestRunner(unittest.TextTestRunner):
    resultclass = ProgressTestResult

    def run(self, test):
        self.resultclass.total = test.countTestCases()
        return super().run(test)


def quiet_tmux_kill_session(server):
    subprocess.run(
        ["tmux", "kill-session", "-t", server],
        check=False,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


def quiet_tmux_kill_server():
    subprocess.run(
        ["tmux", "kill-server"],
        check=False,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


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
            if isinstance(response, SlowResponse):
                response.send(client)
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


def named_tool_response(name, arguments, call_id="call-tool"):
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
                            "function": {"name": name, "arguments": json.dumps(arguments)},
                        }
                    ],
                }
            }
        ]
    }


def landlock_supported():
    try:
        major, minor = os.uname().release.split(".")[:2]
        return (int(major), int(minor)) >= (5, 13)
    except ValueError:
        return False


def text_response(content, prompt_tokens=None, completion_tokens=None):
    response = {"choices": [{"message": {"role": "assistant", "content": content}}]}
    if prompt_tokens is not None:
        response["usage"] = {
            "prompt_tokens": prompt_tokens,
            "completion_tokens": completion_tokens or 0,
            "total_tokens": prompt_tokens + (completion_tokens or 0),
        }
    return response


def sse_response(deltas=None, tool_call_deltas=None, usage=None, done=True, extra_lines=()):
    """Raw chunked SSE reply assembled from content or tool-call deltas."""
    events = []
    for delta in deltas or []:
        events.append({"choices": [{"index": 0, "delta": {"content": delta}}]})
    for delta in tool_call_deltas or []:
        events.append({"choices": [{"index": 0, "delta": {"tool_calls": [delta]}}]})
    if usage is not None:
        events.append({"choices": [], "usage": usage})
    lines = list(extra_lines)
    for event in events:
        lines.append("data: " + json.dumps(event, separators=(",", ":")))
    if done:
        lines.append("data: [DONE]")
    body = "".join(line + "\n\n" for line in lines).encode()
    chunks = b""
    while body:
        piece, body = body[:40], body[40:]
        chunks += f"{len(piece):x}".encode() + b"\r\n" + piece + b"\r\n"
    return (
        b"HTTP/1.1 200 OK\r\n"
        b"Content-Type: text/event-stream\r\n"
        b"Transfer-Encoding: chunked\r\n"
        b"Connection: close\r\n\r\n" + chunks + b"0\r\n\r\n"
    )


class SlowResponse:
    """SSE reply whose content events trickle out one per interval."""

    def __init__(self, events, interval=0.15):
        self.events = events
        self.interval = interval

    def send(self, client):
        client.sendall(
            b"HTTP/1.1 200 OK\r\n"
            b"Content-Type: text/event-stream\r\n"
            b"Transfer-Encoding: chunked\r\n"
            b"Connection: close\r\n\r\n"
        )
        for event in self.events:
            time.sleep(self.interval)
            piece = (
                "data: "
                + json.dumps(
                    {"choices": [{"index": 0, "delta": {"content": event}}]},
                    separators=(",", ":"),
                )
                + "\n\n"
            ).encode()
            client.sendall(f"{len(piece):x}".encode() + b"\r\n" + piece + b"\r\n")
        client.sendall(b"0\r\n\r\n")


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
        flags=(),
        timeout=10,
        extra_env=None,
        url_host="127.0.0.1",
    ):
        endpoint = Endpoint(responses, delay)
        self.addCleanup(endpoint.thread.join, timeout=2)
        self.addCleanup(endpoint.close)
        if cwd is None:
            # Session transcripts land in <cwd>/.ai, so runs without an
            # explicit workspace get a scratch directory to keep the tree
            # clean and tests independent of each other's sessions.
            scratch = tempfile.TemporaryDirectory()
            self.addCleanup(scratch.cleanup)
            cwd = scratch.name
        settings = {
            "AI_API_URL": f"http://{url_host}:{endpoint.port}/v1",
            "AI_TIMEOUT_SECONDS": "2",
            "AI_COMMAND_TIMEOUT_SECONDS": command_timeout,
            "PATH": os.environ["PATH"],
        }
        if tool_rounds is not None:
            settings["AI_TOOL_ROUNDS"] = tool_rounds
        if extra_env is not None:
            settings.update(extra_env)
        command = [EXECUTABLE]
        if workspace is not None:
            command.extend(["-C", workspace])
        if full_access:
            command.append("--full-access")
        if model is not None:
            command.extend(["-m", model])
        if timeout_flag is not None:
            command.extend(["-t", timeout_flag])
        command.extend(flags)
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

    def make_workspace(self, entries):
        workspace = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, workspace, ignore_errors=True)
        for name, content in entries.items():
            path = os.path.join(workspace, name)
            if content is None:
                os.makedirs(path, exist_ok=True)
                continue
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, "w") as handle:
                handle.write(content)
        return workspace

    def tool_replies(self, endpoint, request_index=1):
        return [
            message
            for message in endpoint.requests[request_index]["messages"]
            if message["role"] == "tool"
        ]

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
        scratch = tempfile.TemporaryDirectory()
        self.addCleanup(scratch.cleanup)
        process = subprocess.Popen(
            command,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            env=settings,
            cwd=scratch.name,
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

    def test_shell_mode_runs_command_and_records_context(self):
        result, endpoint = self.run_agent(
            [text_response("noted")],
            "!echo shell-ok\n\n!exit 3\n\nwhat happened\n\n/exit\n\n",
        )
        self.assertIn("[shell command]: echo shell-ok", result.stdout)
        self.assertIn("shell-ok", result.stdout)
        self.assertIn("[shell command completed] exit status 0", result.stdout)
        self.assertIn("[shell command completed] exit status 3", result.stdout)
        self.assertEqual(len(endpoint.requests), 1)
        shell_messages = [
            message
            for message in endpoint.requests[0]["messages"]
            if message["role"] == "user" and "<bash-input>" in message["content"]
        ]
        self.assertEqual(len(shell_messages), 2)
        first = shell_messages[0]["content"]
        self.assertIn("HUMAN SHELL COMMAND", first)
        self.assertIn("<bash-input>echo shell-ok</bash-input>", first)
        self.assertIn("shell-ok", first)
        self.assertIn("[exit status: 0]", first)
        self.assertEqual(
            endpoint.requests[0]["messages"][-1]["content"],
            "what happened",
        )

    def test_shell_mode_escapes_output_for_context(self):
        result, endpoint = self.run_agent(
            [text_response("ok")],
            "!echo '<tag> & text'\n\nnext\n\n/exit\n\n",
        )
        self.assertIn("<tag> & text", result.stdout)
        shell_messages = [
            message
            for message in endpoint.requests[0]["messages"]
            if message["role"] == "user" and "<bash-input>" in message["content"]
        ]
        self.assertEqual(len(shell_messages), 1)
        content = shell_messages[0]["content"]
        self.assertIn(
            "<bash-input>echo &apos;&lt;tag&gt; &amp; text&apos;</bash-input>", content
        )
        self.assertIn("&lt;tag&gt; &amp; text", content)

    def test_shell_mode_usage_hint_without_command(self):
        result, endpoint = self.run_agent(
            [text_response("unused")],
            "!\n\n/exit\n\n",
        )
        self.assertIn("usage: ! <command>", result.stdout)
        self.assertEqual(len(endpoint.requests), 0)

    def test_start_job_status_and_stop_round_trip(self):
        result, endpoint = self.run_agent(
            [
                named_tool_response("start_job", {"command": "echo job-output; sleep 30"}),
                named_tool_response("job_status", {"id": 1}),
                named_tool_response("stop_job", {"id": 1}),
                text_response("done"),
            ],
            "watch the suite\n\n/exit\n\n",
            full_access=True,
            delay=0.3,
        )
        self.assertIn("[AI starts job]: echo job-output; sleep 30", result.stdout)
        self.assertIn("[job 1 started] log:", result.stdout)
        self.assertIn("[jobs] 1 running (longest", result.stdout)
        self.assertIn("[job 1] running", result.stdout)
        self.assertIn("job-output", result.stdout)
        self.assertIn("[job 1 stopped] status", result.stdout)
        self.assertIn("[jobs] none", result.stdout)
        status_reply = self.tool_replies(endpoint, 2)[-1]["content"]
        self.assertIn("job 1: running", status_reply)
        self.assertIn("job-output", status_reply)
        stop_reply = self.tool_replies(endpoint, 3)[-1]["content"]
        self.assertIn("[job 1 stopped] exit status 143", stop_reply)
        self.assertIn("Log tail:", stop_reply)

    def test_job_log_tail_drops_older_bytes(self):
        result, endpoint = self.run_agent(
            [
                named_tool_response("start_job", {"command": "seq 1 50000; sleep 30"}),
                named_tool_response("job_status", {"id": 1}),
                named_tool_response("stop_job", {"id": 1}),
                text_response("done"),
            ],
            "watch\n\n/exit\n\n",
            full_access=True,
            delay=0.5,
        )
        status_reply = self.tool_replies(endpoint, 2)[-1]["content"]
        self.assertIn("first", status_reply)
        self.assertIn("bytes dropped]", status_reply)
        self.assertIn("[output tail]", status_reply)
        self.assertIn("50000", status_reply)

    def test_status_notices_start_on_fresh_line_after_log_tail(self):
        # A log tail without a trailing newline (pytest dots are the real
        # case) leaves the cursor mid-line; the next notice must step below
        # it instead of gluing to the tail.
        responses = [
            named_tool_response("start_job", {"command": "printf abcdef"}),
            named_tool_response("job_status", {"id": 1}),
        ]
        responses.extend(tool_response("true") for _ in range(11))
        responses.append(text_response("done"))
        result, endpoint = self.run_agent(
            responses,
            "fill the window\n\n/exit\n\n",
            full_access=True,
            timeout=30,
        )
        # The raw tail updates the partial-line flag, so the very next
        # notice steps to a fresh line (pre-fix it glued as
        # "abcdef[AI request active]").
        self.assertNotIn("abcdef[AI request active]", result.stdout)
        self.assertIn("abcdef\n[AI request active]", result.stdout)

    def test_start_job_requires_approval(self):
        result, endpoint = self.run_agent(
            [
                named_tool_response("start_job", {"command": "sleep 30"}),
                text_response("done"),
            ],
            "go\n\nn\n/exit\n\n",
        )
        self.assertIn("[AI starts job]: sleep 30", result.stdout)
        self.assertNotIn("[job 1 started]", result.stdout)
        self.assertNotIn("[jobs] 1 running", result.stdout)
        self.assertIn("Job refused by the human.", self.tool_replies(endpoint, 1)[0]["content"])

    def test_stop_job_escalates_to_sigkill(self):
        result, endpoint = self.run_agent(
            [
                named_tool_response("start_job", {"command": "trap '' TERM; while :; do sleep 1; done"}),
                named_tool_response("stop_job", {"id": 1}),
                text_response("done"),
            ],
            "go\n\n/exit\n\n",
            full_access=True,
            delay=0.3,
        )
        stop_reply = self.tool_replies(endpoint, 2)[-1]["content"]
        self.assertIn("[job 1 stopped] exit status 137", stop_reply)

    def test_agent_exit_stops_running_jobs(self):
        result, endpoint = self.run_agent(
            [
                named_tool_response("start_job", {"command": "sleep 30"}),
                text_response("done"),
            ],
            "go\n\n/exit\n\n",
            full_access=True,
        )
        self.assertIn("[1 job stopped]", result.stdout)
        self.assertIn("[bye]", result.stdout)

    def test_job_status_without_jobs(self):
        result, endpoint = self.run_agent(
            [
                named_tool_response("job_status", {}),
                text_response("done"),
            ],
            "go\n\n/exit\n\n",
            full_access=True,
        )
        self.assertIn("[job status] no jobs", result.stdout)
        self.assertIn("No jobs have been started.", self.tool_replies(endpoint, 1)[0]["content"])

    def test_job_status_unknown_id(self):
        result, endpoint = self.run_agent(
            [
                named_tool_response("job_status", {"id": 9}),
                text_response("done"),
            ],
            "go\n\n/exit\n\n",
            full_access=True,
        )
        self.assertIn("no job with id 9", self.tool_replies(endpoint, 1)[0]["content"])

    @unittest.skipUnless(landlock_supported(), "kernel predates Landlock")
    def test_start_job_landlock_confines_home_write(self):
        probe = os.path.join(os.environ["HOME"], "ai-landlock-probe")
        if os.path.exists(probe):
            os.unlink(probe)
        workspace = self.make_workspace({"keep": "x"})
        result, endpoint = self.run_agent(
            [
                named_tool_response("start_job", {"command": "echo probe > $HOME/ai-landlock-probe; sleep 30"}),
                named_tool_response("job_status", {"id": 1}),
                named_tool_response("stop_job", {"id": 1}),
                text_response("done"),
            ],
            "go\n\na\n\na\n\n/exit\n\n",
            workspace=workspace,
            delay=0.5,
            extra_env={"HOME": os.environ["HOME"]},
        )
        status_reply = self.tool_replies(endpoint, 2)[-1]["content"]
        self.assertIn("Permission denied", status_reply)
        self.assertFalse(os.path.exists(probe), "job wrote outside the workspace despite Landlock")

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
        with tempfile.TemporaryDirectory() as workspace:
            resolved = os.path.realpath(workspace)
            result, endpoint = self.run_agent(
                [text_response("done")],
                "diagnose\n\n/exit\n\n",
                cwd=workspace,
            )
        self.assertIn(f"Workspace: {resolved} (unrestricted)", result.stdout)
        self.assertIn(
            f"Working directory: {resolved}", self.system_content(endpoint)
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

    def test_model_command_switches_model_for_next_request(self):
        result, endpoint = self.run_agent(
            [text_response("hi reply")],
            "/model other-model\n\nsay hi\n\n/exit\n\n",
            model="starting-model",
        )
        self.assertIn("[model is now other-model]", result.stdout)
        self.assertEqual(endpoint.requests[0]["model"], "other-model")

    def test_bare_model_command_reports_current_model(self):
        result, endpoint = self.run_agent(
            [text_response("unused")],
            "/model\n\n/exit\n\n",
            model="starting-model",
        )
        self.assertIn("[model: starting-model]", result.stdout)
        self.assertEqual(endpoint.requests, [])

    def test_model_command_rejects_over_long_name(self):
        result, endpoint = self.run_agent(
            [text_response("hi reply")],
            "/model " + "x" * 200 + "\n\nsay hi\n\n/exit\n\n",
            model="starting-model",
        )
        self.assertIn("model name must be shorter than", result.stdout)
        self.assertEqual(endpoint.requests[0]["model"], "starting-model")

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
        output += read_until(b"[You]: ", 10)
        os.write(master, b"aa")
        os.write(master, b"\n")            # C-J
        os.write(master, b"bb")
        os.write(master, b"\x10")          # C-P up, then C-J appends below "bb"
        os.write(master, b"\n")
        os.write(master, b"cc")
        os.write(master, b"\r")
        output += read_until(b"[AI Response]", 10)
        output += read_until(b"[You]: ", 10)
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
        quiet_tmux_kill_server()
        self.addCleanup(quiet_tmux_kill_server)
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
        self.addCleanup(quiet_tmux_kill_session, server)

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
        self.assertEqual(
            tool_names,
            {
                "run_terminal_command",
                "read_file",
                "list_dir",
                "apply_patch",
                "plan",
                "task",
                "start_job",
                "job_status",
                "stop_job",
            },
        )
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

    def test_read_file_returns_numbered_lines_without_approval(self):
        workspace = self.make_workspace({"notes.txt": "alpha\nbeta\ngamma\n"})
        result, endpoint = self.run_agent(
            [named_tool_response("read_file", {"path": "notes.txt"}), text_response("done")],
            "diagnose\n\n\n/exit\n\n",
            workspace=workspace,
        )
        self.assertIn("[AI reads file]: notes.txt", result.stdout)
        self.assertIn("1\talpha", result.stdout)
        self.assertIn("3\tgamma", result.stdout)
        self.assertNotIn("[a]llow", result.stdout)
        replies = self.tool_replies(endpoint)
        self.assertIn("1\talpha", replies[0]["content"])
        self.assertIn("[file read: notes.txt, 3 lines]", replies[0]["content"])

    def test_read_file_honors_offset_and_limit(self):
        workspace = self.make_workspace({"lines.txt": "one\ntwo\nthree\nfour\nfive\n"})
        result, endpoint = self.run_agent(
            [
                named_tool_response(
                    "read_file", {"path": "lines.txt", "offset": 2, "limit": 2}
                ),
                text_response("done"),
            ],
            "diagnose\n\n\n/exit\n\n",
            workspace=workspace,
        )
        replies = self.tool_replies(endpoint)
        self.assertIn("2\ttwo", replies[0]["content"])
        self.assertIn("3\tthree", replies[0]["content"])
        self.assertNotIn("1\tone", replies[0]["content"])
        self.assertNotIn("4\tfour", replies[0]["content"])
        self.assertIn("[file read: lines.txt, lines 2-3 of 5]", replies[0]["content"])

    def test_read_file_reports_missing_file(self):
        result, endpoint = self.run_agent(
            [named_tool_response("read_file", {"path": "absent.txt"}), text_response("done")],
            "diagnose\n\n\n/exit\n\n",
        )
        replies = self.tool_replies(endpoint)
        self.assertIn("Error: cannot read absent.txt", replies[0]["content"])

    def test_read_file_refuses_paths_outside_workspace(self):
        workspace = self.make_workspace({"inside.txt": "data\n"})
        result, endpoint = self.run_agent(
            [
                named_tool_response("read_file", {"path": "../../etc/passwd"}),
                text_response("done"),
            ],
            "diagnose\n\n\n/exit\n\n",
            workspace=workspace,
        )
        replies = self.tool_replies(endpoint)
        self.assertIn(
            "resolves outside the workspace scope", replies[0]["content"]
        )
        self.assertNotIn("root:", replies[0]["content"])

    def test_list_dir_reports_sorted_entries(self):
        workspace = self.make_workspace(
            {"zebra.txt": "0123456789\n", "apple": None, "mango.md": "x\n"}
        )
        result, endpoint = self.run_agent(
            [named_tool_response("list_dir", {"path": "."}), text_response("done")],
            "diagnose\n\n\n/exit\n\n",
            workspace=workspace,
        )
        self.assertIn("[AI lists dir]: .", result.stdout)
        replies = self.tool_replies(endpoint)
        # The session transcript directory .ai/ lives in the workspace too.
        self.assertIn("dir  -  apple/", replies[0]["content"])
        self.assertIn("file 11 zebra.txt", replies[0]["content"])
        self.assertIn("file 2 mango.md", replies[0]["content"])
        self.assertLess(
            replies[0]["content"].index("apple/"),
            replies[0]["content"].index("mango.md"),
        )
        self.assertLess(
            replies[0]["content"].index("mango.md"),
            replies[0]["content"].index("zebra.txt"),
        )
        self.assertIn("[dir listing: ., 4 entries]", replies[0]["content"])

    def test_apply_patch_edits_file_after_approval(self):
        workspace = self.make_workspace({"app.txt": "keep\nold line\nkeep too\n"})
        patch = (
            "app.txt\n"
            "<<<<<<< SEARCH\n"
            "old line\n"
            "=======\n"
            "new line\n"
            ">>>>>>> REPLACE\n"
        )
        result, endpoint = self.run_agent(
            [named_tool_response("apply_patch", {"patch": patch}), text_response("done")],
            "diagnose\n\na\n\n\n/exit\n\n",
            workspace=workspace,
        )
        self.assertIn("[AI requests patch]", result.stdout)
        self.assertIn("old line", result.stdout)
        self.assertIn(
            "[patch applied: 1 hunk, +1 -1 lines] to app.txt", result.stdout
        )
        with open(os.path.join(workspace, "app.txt")) as handle:
            self.assertEqual(handle.read(), "keep\nnew line\nkeep too\n")
        replies = self.tool_replies(endpoint)
        self.assertIn("[patch applied: 1 hunk, +1 -1 lines] to app.txt", replies[0]["content"])

    def test_apply_patch_creates_file_with_empty_search(self):
        workspace = self.make_workspace({})
        patch = (
            "new.txt\n"
            "<<<<<<< SEARCH\n"
            "=======\n"
            "created line\n"
            ">>>>>>> REPLACE\n"
        )
        result, endpoint = self.run_agent(
            [named_tool_response("apply_patch", {"patch": patch}), text_response("done")],
            "diagnose\n\na\n\n\n/exit\n\n",
            workspace=workspace,
        )
        self.assertIn("[patch applied: 1 hunk, +1 -0 lines] to new.txt", result.stdout)
        with open(os.path.join(workspace, "new.txt")) as handle:
            self.assertEqual(handle.read(), "created line\n")

    def test_apply_patch_refusal_leaves_file_untouched(self):
        workspace = self.make_workspace({"app.txt": "original\n"})
        patch = (
            "app.txt\n"
            "<<<<<<< SEARCH\n"
            "original\n"
            "=======\n"
            "rewritten\n"
            ">>>>>>> REPLACE\n"
        )
        result, endpoint = self.run_agent(
            [named_tool_response("apply_patch", {"patch": patch}), text_response("done")],
            "diagnose\n\nn\n/exit\n\n",
            workspace=workspace,
        )
        self.assertIn("Patch refused by the human.", self.tool_replies(endpoint)[0]["content"])
        with open(os.path.join(workspace, "app.txt")) as handle:
            self.assertEqual(handle.read(), "original\n")

    def test_apply_patch_reports_missing_search_text(self):
        workspace = self.make_workspace({"app.txt": "alpha\nbeta\n"})
        patch = (
            "app.txt\n"
            "<<<<<<< SEARCH\n"
            "missing line\n"
            "=======\n"
            "anything\n"
            ">>>>>>> REPLACE\n"
        )
        result, endpoint = self.run_agent(
            [named_tool_response("apply_patch", {"patch": patch}), text_response("done")],
            "diagnose\n\na\n\n\n/exit\n\n",
            workspace=workspace,
        )
        replies = self.tool_replies(endpoint)
        self.assertIn(
            "SEARCH text of hunk 1 not found in app.txt", replies[0]["content"]
        )
        with open(os.path.join(workspace, "app.txt")) as handle:
            self.assertEqual(handle.read(), "alpha\nbeta\n")

    def test_apply_patch_reports_ambiguous_search_text(self):
        workspace = self.make_workspace({"dup.txt": "same\nsame\n"})
        patch = (
            "dup.txt\n"
            "<<<<<<< SEARCH\n"
            "same\n"
            "=======\n"
            "other\n"
            ">>>>>>> REPLACE\n"
        )
        result, endpoint = self.run_agent(
            [named_tool_response("apply_patch", {"patch": patch}), text_response("done")],
            "diagnose\n\na\n\n\n/exit\n\n",
            workspace=workspace,
        )
        replies = self.tool_replies(endpoint)
        self.assertIn(
            "matches 2 locations in dup.txt", replies[0]["content"]
        )
        with open(os.path.join(workspace, "dup.txt")) as handle:
            self.assertEqual(handle.read(), "same\nsame\n")

    def test_apply_patch_blocks_outside_workspace_even_with_full_access(self):
        outer = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, outer, ignore_errors=True)
        workspace = os.path.join(outer, "ws")
        os.makedirs(workspace)
        with open(os.path.join(outer, "outside.txt"), "w") as handle:
            handle.write("untouched\n")
        patch = (
            "../outside.txt\n"
            "<<<<<<< SEARCH\n"
            "=======\n"
            "escaped\n"
            ">>>>>>> REPLACE\n"
        )
        result, endpoint = self.run_agent(
            [named_tool_response("apply_patch", {"patch": patch}), text_response("done")],
            "diagnose\n\n/exit\n\n",
            workspace=workspace,
            full_access=True,
        )
        self.assertIn("Patch blocked:", result.stdout)
        replies = self.tool_replies(endpoint)
        self.assertIn(
            "patch references a path outside the workspace scope", replies[0]["content"]
        )
        with open(os.path.join(outer, "outside.txt")) as handle:
            self.assertEqual(handle.read(), "untouched\n")

    def test_apply_patch_auto_applies_with_full_access(self):
        workspace = self.make_workspace({"app.txt": "before\n"})
        patch = (
            "app.txt\n"
            "<<<<<<< SEARCH\n"
            "before\n"
            "=======\n"
            "after\n"
            ">>>>>>> REPLACE\n"
        )
        result, endpoint = self.run_agent(
            [named_tool_response("apply_patch", {"patch": patch}), text_response("done")],
            "diagnose\n\n/exit\n\n",
            workspace=workspace,
            full_access=True,
        )
        self.assertEqual(result.stdout.count("[a]llow"), 0)
        self.assertIn("[patch applied: 1 hunk, +1 -1 lines] to app.txt", result.stdout)
        with open(os.path.join(workspace, "app.txt")) as handle:
            self.assertEqual(handle.read(), "after\n")

    def test_file_tools_do_not_follow_symlinks_outside_workspace(self):
        outer = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, outer, ignore_errors=True)
        workspace = os.path.join(outer, "ws")
        os.makedirs(workspace)
        secret = os.path.join(outer, "secret.txt")
        with open(secret, "w") as handle:
            handle.write("classified\n")
        os.symlink(secret, os.path.join(workspace, "leak.txt"))
        result, endpoint = self.run_agent(
            [named_tool_response("read_file", {"path": "leak.txt"}), text_response("done")],
            "diagnose\n\n\n/exit\n\n",
            workspace=workspace,
        )
        replies = self.tool_replies(endpoint)
        self.assertIn(
            "leak.txt resolves outside the workspace scope", replies[0]["content"]
        )
        self.assertNotIn("classified", replies[0]["content"])
        patch = (
            "leak.txt\n"
            "<<<<<<< SEARCH\n"
            "classified\n"
            "=======\n"
            "leaked\n"
            ">>>>>>> REPLACE\n"
        )
        result, endpoint = self.run_agent(
            [named_tool_response("apply_patch", {"patch": patch}), text_response("done")],
            "diagnose\n\n/exit\n\n",
            workspace=workspace,
            full_access=True,
        )
        self.assertIn("Patch blocked: leak.txt", result.stdout)
        with open(secret) as handle:
            self.assertEqual(handle.read(), "classified\n")

    def test_unsupported_tool_error_lists_available_tools(self):
        result, endpoint = self.run_agent(
            [named_tool_response("run_command", {"command": "ls"}), text_response("done")],
            "diagnose\n\n\n/exit\n\n",
        )
        replies = self.tool_replies(endpoint)
        self.assertIn(
            "Error: unsupported tool; use run_terminal_command, read_file, list_dir, "
            "apply_patch, plan, task, start_job, job_status, or stop_job.",
            replies[0]["content"],
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
            # Connection-refused retries back off 1s+2s+4s+8s before the
            # final failure, so this needs more than the 5s above.
            timeout=30,
            env={"AI_API_URL": "http://127.0.0.1:1/v1", "PATH": os.environ["PATH"]},
        )
        self.assertEqual(process.returncode, 1)

        with tempfile.TemporaryDirectory() as workspace:
            process = subprocess.run(
                [EXECUTABLE, "-f", os.path.join(workspace, "missing"), "-e", "diagnose"],
                input="",
                text=True,
                capture_output=True,
                timeout=30,
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
            command_timeout="30",
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

    def test_one_shot_mode_notice_in_system_messages(self):
        _, endpoint = self.run_agent(
            [text_response("done")],
            full_access=True,
            exec_prompt="diagnose",
        )
        system_texts = [
            message["content"]
            for message in endpoint.requests[0]["messages"]
            if message["role"] == "system"
        ]
        # The model must know there is no second turn, so it settles
        # questions itself instead of stranding the run.
        self.assertTrue(any("One-shot mode" in text for text in system_texts))

        _, endpoint = self.run_agent(
            [text_response("ok")],
            "go\n\n/exit\n\n",
        )
        system_texts = [
            message["content"]
            for message in endpoint.requests[0]["messages"]
            if message["role"] == "system"
        ]
        self.assertFalse(any("One-shot mode" in text for text in system_texts))

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
                b"Connection: close\r\n\r\n",
                text_response("second ok"),
            ],
            "first\n\nsecond\n\n/exit\n\n",
        )
        self.assertIn("AI endpoint returned HTTP 400", result.stdout)
        # The failed round kills only that round; the next turn still runs.
        self.assertIn("second ok", result.stdout)
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

    def test_hostname_endpoint_url_resolves(self):
        result, endpoint = self.run_agent(
            [text_response("hello over a hostname")],
            "go\n\n/exit\n\n",
            url_host="localhost",
        )
        self.assertIn("hello over a hostname", result.stdout)
        self.assertEqual(len(endpoint.requests), 1)

    def test_unresolvable_endpoint_host_fails_without_retry(self):
        result, _ = self.run_agent(
            [],
            "go\n\n/exit\n\n",
            extra_env={
                "AI_API_URL": "http://no-such-host-zz.invalid:8090/v1/chat/completions"
            },
        )
        self.assertEqual(
            result.stdout.count("error: resolve no-such-host-zz.invalid"), 1
        )
        self.assertNotIn("[retrying", result.stdout)
        self.assertIn("[bye]", result.stdout)

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
        self.assertIn("[retrying in 2s (attempt 2/5)]", result.stdout)
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
            [refused] * 5,
            "go\n\n/exit\n\n",
            timeout=40,
        )
        self.assertIn("AI endpoint returned HTTP 500", result.stdout)
        self.assertIn("[retrying in 1s (attempt 2/5)]", result.stdout)
        self.assertIn("[retrying in 2s (attempt 3/5)]", result.stdout)
        self.assertIn("[retrying in 4s (attempt 4/5)]", result.stdout)
        self.assertIn("[retrying in 8s (attempt 5/5)]", result.stdout)
        self.assertNotIn("attempt 6/5", result.stdout)
        self.assertEqual(len(endpoint.requests), 5)

    def test_empty_stream_is_retried(self):
        empty = (
            b"HTTP/1.1 200 OK\r\n"
            b"Content-Type: text/event-stream\r\n"
            b"Connection: close\r\n\r\n"
        )
        result, endpoint = self.run_agent(
            [empty, sse_response(["recovered"])],
            "go\n\n/exit\n\n",
        )
        # A recovered retry shows only the notice; the reason line waits
        # for a final failure.
        self.assertNotIn("AI stream ended without content", result.stdout)
        self.assertNotIn("Protocol error", result.stdout)
        self.assertIn("[retrying in 1s (attempt 2/5)]", result.stdout)
        self.assertIn("recovered", result.stdout)
        self.assertEqual(len(endpoint.requests), 2)
        self.assertEqual(endpoint.requests[0], endpoint.requests[1])

    def test_empty_stream_retries_until_exhausted(self):
        empty = (
            b"HTTP/1.1 200 OK\r\n"
            b"Content-Type: text/event-stream\r\n"
            b"Connection: close\r\n\r\n"
        )
        result, endpoint = self.run_agent(
            [empty] * 5,
            "go\n\n/exit\n\n",
            timeout=40,
        )
        # Silent while retrying; the reason surfaces once, on giving up.
        self.assertEqual(result.stdout.count("AI stream ended without content"), 1)
        self.assertNotIn("Protocol error", result.stdout)
        self.assertIn("[retrying in 8s (attempt 5/5)]", result.stdout)
        self.assertNotIn("attempt 6/5", result.stdout)
        self.assertEqual(len(endpoint.requests), 5)
        self.assertIn("[bye]", result.stdout)

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
        self.assertIn("[retrying in 1s (attempt 2/5)]", combined)
        self.assertIn("[retrying in 8s (attempt 5/5)]", combined)
        self.assertIn("connect", combined)

    def test_context_compaction_keeps_a_user_message(self):
        # MAX_MESSAGES forces a fold while the single user prompt is still
        # the only user message; every request must keep one. Two system
        # messages plus the prompt and eleven tool rounds fill the window,
        # so the tool reply of round eleven triggers the fold.
        responses = [tool_response("true") for _ in range(11)]
        responses.append(text_response("running summary of the diagnosis"))
        responses.append(text_response("done"))
        result, endpoint = self.run_agent(
            responses,
            full_access=True,
            exec_prompt="diagnose",
            timeout=60,
        )
        self.assertIn("done", result.stdout)
        self.assertIn("[context] Compacted 2 message(s)", result.stdout)
        self.assertIn(", compacted ×1", result.stdout)
        self.assertEqual(len(endpoint.requests), 13)
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

    def summarize_request(self, endpoint):
        for request in endpoint.requests:
            messages = request.get("messages", [])
            if (
                messages
                and messages[0]["role"] == "system"
                and "running summary" in messages[0]["content"]
            ):
                return request
        return None

    def pinned_summary(self, request):
        for message in request["messages"]:
            if (
                message.get("role") == "system"
                and "CONVERSATION SUMMARY" in (message.get("content") or "")
            ):
                return message
        return None

    def test_auto_compaction_folds_oldest_turn_into_summary(self):
        responses = [text_response(f"answer-{index:02d}") for index in range(1, 12)]
        responses.append(text_response("folded everything so far"))
        responses.append(text_response("answer-12"))
        responses.append(text_response("answer-13"))
        stdin = "".join(f"turn-{index:02d}\n\n" for index in range(1, 14)) + "/exit\n\n"
        result, endpoint = self.run_agent(responses, stdin, timeout=30)
        self.assertIn("[context] Compacted 1 message(s)", result.stdout)
        # The summarize request is buffered and single-shot.
        summarize = self.summarize_request(endpoint)
        self.assertIsNotNone(summarize)
        self.assertNotIn("stream", summarize)
        payload = summarize["messages"][1]["content"]
        self.assertIn("Existing summary:\n(none)", payload)
        self.assertIn("turn-01", payload)
        # The fold is pinned as a system message in the next chat request.
        followup = endpoint.requests[-1]
        pinned = self.pinned_summary(followup)
        self.assertIsNotNone(pinned)
        self.assertIn("folded everything so far", pinned["content"])
        contents = [m.get("content") or "" for m in followup["messages"]]
        self.assertNotIn("turn-01", contents)
        self.assertIn("turn-03", contents)
        self.assertIn("turn-12", contents)
        self.assertIn(", compacted ×1", result.stdout)

    def test_auto_compaction_is_rate_limited(self):
        responses = [text_response(f"answer-{index:02d}") for index in range(1, 12)]
        responses.append(text_response("folded everything so far"))
        responses.extend(text_response(f"answer-{index:02d}") for index in range(12, 14))
        stdin = "".join(f"turn-{index:02d}\n\n" for index in range(1, 14)) + "/exit\n\n"
        result, endpoint = self.run_agent(responses, stdin, timeout=30)
        self.assertEqual(result.stdout.count("[context] Compacted"), 1)
        # Overflows within the rate-limit window drop instead of
        # re-querying the endpoint for a fresh fold.
        summarize_count = sum(
            1
            for request in endpoint.requests
            if request.get("messages")
            and request["messages"][0]["role"] == "system"
            and "running summary" in request["messages"][0]["content"]
        )
        self.assertEqual(summarize_count, 1)
        self.assertIn("[context] Dropped the oldest non-system message.", result.stdout)
        self.assertIn(", compacted ×1", result.stdout)
        followup = endpoint.requests[-1]
        self.assertIsNotNone(self.pinned_summary(followup))
        contents = [m.get("content") or "" for m in followup["messages"]]
        self.assertNotIn("turn-01", contents)
        self.assertNotIn("turn-02", contents)
        self.assertIn("turn-03", contents)

    def test_compaction_failure_falls_back_to_plain_drop(self):
        responses = [text_response(f"answer-{index:02d}") for index in range(1, 12)]
        responses.append(text_response(""))  # broken summarize reply
        responses.extend(text_response(f"answer-{index:02d}") for index in range(12, 14))
        stdin = "".join(f"turn-{index:02d}\n\n" for index in range(1, 14)) + "/exit\n\n"
        result, endpoint = self.run_agent(responses, stdin, timeout=30)
        self.assertNotIn("[context] Compacted", result.stdout)
        self.assertIn("[context] Dropped the oldest non-system message.", result.stdout)
        self.assertIn("answer-13", result.stdout)
        followup = endpoint.requests[-1]
        self.assertIsNone(self.pinned_summary(followup))
        contents = [m.get("content") or "" for m in followup["messages"]]
        self.assertNotIn("turn-01", contents)
        self.assertIn("turn-03", contents)
        self.assertNotIn(", compacted", result.stdout)

    def test_compact_command_folds_older_half(self):
        result, endpoint = self.run_agent(
            [
                text_response("answer-01"),
                text_response("answer-02"),
                text_response("folded: goal and decisions"),
                text_response("answer-03"),
            ],
            "turn-01\n\nturn-02\n\n/compact\n\nturn-03\n\n/exit\n\n",
            timeout=30,
        )
        self.assertIn("[context] Compacted 2 message(s)", result.stdout)
        summarize = self.summarize_request(endpoint)
        self.assertIsNotNone(summarize)
        self.assertIn("Existing summary:\n(none)", summarize["messages"][1]["content"])
        followup = endpoint.requests[-1]
        pinned = self.pinned_summary(followup)
        self.assertIsNotNone(pinned)
        self.assertIn("folded: goal and decisions", pinned["content"])
        contents = [m.get("content") or "" for m in followup["messages"]]
        self.assertNotIn("turn-01", contents)
        self.assertIn("turn-02", contents)
        self.assertIn(", compacted ×1", result.stdout)

    def test_compact_command_with_little_conversation_refuses(self):
        result, endpoint = self.run_agent(
            [text_response("answer-01")],
            "turn-01\n\n/compact\n\n/exit\n\n",
        )
        self.assertIn("[context] Too little conversation to compact safely yet.", result.stdout)
        self.assertNotIn("[context] Compacted", result.stdout)
        self.assertEqual(len(endpoint.requests), 1)

    def test_session_records_summary_and_continue_resumes_it(self):
        with tempfile.TemporaryDirectory() as workspace:
            self.run_agent(
                [
                    text_response("answer-01"),
                    text_response("answer-02"),
                    text_response("folded state"),
                    text_response("answer-03"),
                ],
                "turn-01\n\nturn-02\n\n/compact\n\nturn-03\n\n/exit\n\n",
                cwd=workspace,
                timeout=30,
            )
            lines = self.read_session(workspace)
            self.assertEqual(
                [line.get("role") for line in lines],
                ["_summary", "user", "assistant", "user", "assistant"],
            )
            self.assertEqual(lines[0]["content"], "folded state")
            result, endpoint = self.run_agent(
                [text_response("fresh reply")],
                "turn-04\n\n/exit\n\n",
                cwd=workspace,
                flags=("--continue",),
            )
            self.assertIn("[session] resumed 4 messages", result.stdout)
            pinned = self.pinned_summary(endpoint.requests[0])
            self.assertIsNotNone(pinned)
            self.assertIn("folded state", pinned["content"])
            users = [
                message["content"]
                for message in endpoint.requests[0]["messages"]
                if message["role"] == "user"
            ]
            self.assertEqual(users, ["turn-02", "turn-03", "turn-04"])

    def test_task_tool_delegates_to_subagent(self):
        prompt = "Count the lines in notes.txt and report only the number."
        result, endpoint = self.run_agent(
            [
                named_tool_response("task", {"prompt": prompt}),
                text_response("notes.txt has 3 lines"),
                text_response("delegation complete"),
            ],
            full_access=True,
            exec_prompt="delegate the count",
            timeout=30,
        )
        self.assertIn("[AI delegates task]:", result.stdout)
        # The child's output streams live to the human...
        self.assertIn("notes.txt has 3 lines", result.stdout)
        # ...parent request, child request, parent follow-up.
        self.assertEqual(len(endpoint.requests), 3)
        child_users = [
            message
            for message in endpoint.requests[1]["messages"]
            if message["role"] == "user"
        ]
        self.assertIn(prompt, child_users[-1]["content"])
        # The child is a subagent: no task tool, transcripts off.
        child_tools = {
            tool["function"]["name"] for tool in endpoint.requests[1]["tools"]
        }
        self.assertNotIn("task", child_tools)
        self.assertIn("run_terminal_command", child_tools)
        tool_reply = self.tool_replies(endpoint, 2)[-1]
        self.assertIn("notes.txt has 3 lines", tool_reply["content"])
        self.assertIn("exit status 0", tool_reply["content"])

    def test_task_tool_disabled_inside_subagents(self):
        result, endpoint = self.run_agent(
            [
                named_tool_response("task", {"prompt": "nested work"}),
                text_response("handled it myself"),
            ],
            exec_prompt="delegate",
            extra_env={"AI_SUBAGENT": "1"},
            timeout=30,
        )
        self.assertIn("[AI delegates task]:", result.stdout)
        tool_names = {tool["function"]["name"] for tool in endpoint.requests[0]["tools"]}
        self.assertNotIn("task", tool_names)
        tool_reply = self.tool_replies(endpoint, 1)[-1]
        self.assertIn("delegation is disabled", tool_reply["content"])
        self.assertIn("handled it myself", result.stdout)

    def test_no_shell_disables_terminal_and_job_tools(self):
        result, endpoint = self.run_agent(
            [
                tool_response("true"),
                text_response("done"),
            ],
            exec_prompt="run something",
            flags=("--no-shell",),
            timeout=30,
        )
        tool_names = {tool["function"]["name"] for tool in endpoint.requests[0]["tools"]}
        self.assertNotIn("run_terminal_command", tool_names)
        self.assertNotIn("start_job", tool_names)
        self.assertNotIn("stop_job", tool_names)
        self.assertIn("read_file", tool_names)
        self.assertIn("apply_patch", tool_names)
        self.assertIn("plan", tool_names)
        tool_reply = self.tool_replies(endpoint, 1)[-1]
        self.assertIn("disabled by --no-shell", tool_reply["content"])
        self.assertIn("done", result.stdout)

    def test_task_output_tail_is_bounded(self):
        result, endpoint = self.run_agent(
            [
                named_tool_response("task", {"prompt": "print a numbered dump"}),
                tool_response("(echo HEADMARK; seq 2 30000; echo TAILMARK)"),
                text_response("dumped"),
                text_response("wrapped up"),
            ],
            full_access=True,
            exec_prompt="delegate the dump",
            timeout=60,
        )
        self.assertEqual(len(endpoint.requests), 4)
        tool_reply = self.tool_replies(endpoint, 3)[-1]
        self.assertIn("exit status 0", tool_reply["content"])
        self.assertIn("bytes dropped]", tool_reply["content"])
        self.assertIn("TAILMARK", tool_reply["content"])
        self.assertNotIn("HEADMARK", tool_reply["content"])

    def test_subagent_leaves_no_transcript(self):
        with tempfile.TemporaryDirectory() as workspace:
            self.run_agent(
                [
                    named_tool_response("task", {"prompt": "inspect the workspace"}),
                    text_response("all quiet"),
                    text_response("done"),
                ],
                full_access=True,
                exec_prompt="delegate",
                cwd=workspace,
                timeout=30,
            )
            lines = self.read_session(workspace)
        roles = [line.get("role") for line in lines]
        self.assertIn("user", roles)
        self.assertIn("assistant", roles)
        # The child never wrote its own conversation to the shared session.
        child_records = [
            line
            for line in lines
            if line.get("role") == "assistant" and line.get("content") == "all quiet"
        ]
        self.assertEqual(child_records, [])

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
            quiet_tmux_kill_server()
            self.addCleanup(quiet_tmux_kill_server)
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
                quiet_tmux_kill_session, server
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
            quiet_tmux_kill_server()
            self.addCleanup(quiet_tmux_kill_server)
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
                quiet_tmux_kill_session, server
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
        quiet_tmux_kill_server()
        self.addCleanup(quiet_tmux_kill_server)
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
            quiet_tmux_kill_session, server
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
        quiet_tmux_kill_server()
        self.addCleanup(quiet_tmux_kill_server)
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
        self.addCleanup(quiet_tmux_kill_session, server)
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
        quiet_tmux_kill_server()
        self.addCleanup(quiet_tmux_kill_server)
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
        self.addCleanup(quiet_tmux_kill_session, server)
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
        quiet_tmux_kill_server()
        self.addCleanup(quiet_tmux_kill_server)
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
        self.addCleanup(quiet_tmux_kill_session, server)
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
        quiet_tmux_kill_server()
        self.addCleanup(quiet_tmux_kill_server)
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
        self.addCleanup(quiet_tmux_kill_session, server)
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
        quiet_tmux_kill_server()
        self.addCleanup(quiet_tmux_kill_server)
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
        self.addCleanup(quiet_tmux_kill_session, server)
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
        quiet_tmux_kill_server()
        self.addCleanup(quiet_tmux_kill_server)
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
        self.addCleanup(quiet_tmux_kill_session, server)
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
        quiet_tmux_kill_server()
        self.addCleanup(quiet_tmux_kill_server)
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
        self.addCleanup(quiet_tmux_kill_session, server)
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

    def test_tui_patch_approval_routes_and_applies(self):
        if shutil.which("tmux") is None:
            self.skipTest("tmux is required for TUI patch forwarding")
        with tempfile.TemporaryDirectory() as workspace:
            target = os.path.join(workspace, "app.txt")
            with open(target, "w") as handle:
                handle.write("before\n")
            patch = (
                "app.txt\n"
                "<<<<<<< SEARCH\n"
                "before\n"
                "=======\n"
                "after\n"
                ">>>>>>> REPLACE\n"
            )
            endpoint = Endpoint(
                [
                    named_tool_response("apply_patch", {"patch": patch}),
                    text_response("done"),
                ]
            )
            self.addCleanup(endpoint.thread.join, timeout=2)
            self.addCleanup(endpoint.close)
            server = f"ai-tui-patch-{os.getpid()}"
            quiet_tmux_kill_server()
            self.addCleanup(quiet_tmux_kill_server)
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
                ],
                env=environment,
                check=True,
                timeout=5,
            )
            self.addCleanup(
                quiet_tmux_kill_session, server
            )
            time.sleep(0.5)
            subprocess.run(["tmux", "send-keys", "-t", server, "patch it"], check=True)
            subprocess.run(["tmux", "send-keys", "-t", server, "Enter"], check=True)
            deadline = time.monotonic() + 8
            decision = ""
            output = ""
            while time.monotonic() < deadline:
                output = subprocess.run(
                    ["tmux", "capture-pane", "-p", "-t", server],
                    check=True,
                    capture_output=True,
                    text=True,
                    timeout=5,
                ).stdout
                if "3. Decision (RET submit)" in output:
                    decision = output
                    break
                time.sleep(0.2)
            self.assertTrue(decision, f"patch decision prompt not shown in TUI:\n{output}")
            self.assertIn("Patch decision: a allow", decision)
            subprocess.run(["tmux", "send-keys", "-t", server, "a"], check=True)
            subprocess.run(["tmux", "send-keys", "-t", server, "Enter"], check=True)
            deadline = time.monotonic() + 8
            applied = ""
            while time.monotonic() < deadline:
                output = subprocess.run(
                    ["tmux", "capture-pane", "-p", "-t", server],
                    check=True,
                    capture_output=True,
                    text=True,
                    timeout=5,
                ).stdout
                if "patch applied" in output:
                    applied = output
                    break
                time.sleep(0.2)
            self.assertTrue(applied, f"patch result not shown in TUI:\n{output}")
            with open(target) as handle:
                self.assertEqual(handle.read(), "after\n")
            self.assertEqual(len(endpoint.requests), 2)

    def test_tui_multiline_banner_matches_input_keys(self):
        if shutil.which("tmux") is None:
            self.skipTest("tmux is required for TUI input rendering")

        server = f"ai-tui-banner-{os.getpid()}"
        quiet_tmux_kill_server()
        self.addCleanup(quiet_tmux_kill_server)
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
        self.addCleanup(quiet_tmux_kill_session, server)
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

    def read_session(self, workspace, name="session.jsonl"):
        path = os.path.join(workspace, ".ai", name)
        with open(path, encoding="utf-8") as handle:
            return [json.loads(line) for line in handle if line.strip()]

    def test_session_file_records_transcript(self):
        with tempfile.TemporaryDirectory() as workspace:
            result, endpoint = self.run_agent(
                [text_response("hi there")],
                "hello\n\n/exit\n\n",
                cwd=workspace,
            )
            self.assertIn("hi there", result.stdout)
            lines = self.read_session(workspace)
        self.assertEqual([line["role"] for line in lines], ["user", "assistant"])
        self.assertEqual(lines[0]["content"], "hello")
        self.assertEqual(lines[1]["content"], "hi there")

    def test_session_file_excludes_system_messages(self):
        with tempfile.TemporaryDirectory() as workspace:
            self.run_agent(
                [text_response("done")],
                "hello\n\n/exit\n\n",
                cwd=workspace,
            )
            lines = self.read_session(workspace)
        self.assertTrue(lines)
        self.assertTrue(all(line["role"] != "system" for line in lines))

    def test_session_log_round_trips_file_tool_calls(self):
        workspace = self.make_workspace({"notes.txt": "hello\n"})
        self.run_agent(
            [
                named_tool_response("read_file", {"path": "notes.txt"}),
                text_response("done"),
            ],
            "read the notes\n\n\n/exit\n\n",
            cwd=workspace,
        )
        lines = self.read_session(workspace)
        tool_messages = [line for line in lines if line.get("tool_calls")]
        self.assertEqual(len(tool_messages), 1)
        self.assertEqual(
            tool_messages[0]["tool_calls"][0]["function"]["name"], "read_file"
        )
        self.assertEqual(
            json.loads(tool_messages[0]["tool_calls"][0]["function"]["arguments"]),
            {"path": "notes.txt", "offset": 1, "limit": 0},
        )
        result, endpoint = self.run_agent(
            [text_response("ok")],
            "again\n\n/exit\n\n",
            cwd=workspace,
            flags=("--continue",),
        )
        self.assertIn("[session] resumed", result.stdout)
        replayed = [
            call
            for message in endpoint.requests[0]["messages"]
            if message.get("tool_calls")
            for call in message["tool_calls"]
        ]
        self.assertEqual(replayed[0]["function"]["name"], "read_file")
        self.assertEqual(
            json.loads(replayed[0]["function"]["arguments"])["path"], "notes.txt"
        )

    def test_continue_resumes_previous_conversation(self):
        with tempfile.TemporaryDirectory() as workspace:
            self.run_agent(
                [text_response("earlier reply")],
                "hello\n\n/exit\n\n",
                cwd=workspace,
            )
            result, endpoint = self.run_agent(
                [text_response("fresh reply")],
                "again\n\n/exit\n\n",
                cwd=workspace,
                flags=("--continue",),
            )
        self.assertIn("[session] resumed 2 messages from .ai/session.jsonl", result.stdout)
        self.assertIn("fresh reply", result.stdout)
        messages = endpoint.requests[0]["messages"]
        self.assertEqual(
            [message["content"] for message in messages if message["role"] == "user"],
            ["hello", "again"],
        )
        self.assertIn("earlier reply", "".join(m.get("content") or "" for m in messages))

    def test_continue_without_session_starts_fresh(self):
        with tempfile.TemporaryDirectory() as workspace:
            result, endpoint = self.run_agent(
                [text_response("ok")],
                "hello\n\n/exit\n\n",
                cwd=workspace,
                flags=("--continue",),
            )
        self.assertIn("[session] no previous session in .ai/session.jsonl", result.stdout)
        self.assertIn("starting fresh", result.stdout)
        self.assertEqual(len(endpoint.requests), 1)

    def test_resume_replays_tools_plan_and_skips_torn_line(self):
        with tempfile.TemporaryDirectory() as workspace:
            path = os.path.join(workspace, "custom.jsonl")
            with open(path, "w", encoding="utf-8") as handle:
                handle.write(
                    '{"role":"user","content":"list files"}\n'
                    '{"role":"assistant","content":null,"tool_calls":[{"id":"c1",'
                    '"type":"function","function":{"name":"run_terminal_command",'
                    '"arguments":"{\\"command\\":\\"ls\\"}"}}]}\n'
                    '{"role":"tool","tool_call_id":"c1","content":"file1\\nfile2"}\n'
                    '{"role":"_plan","content":"1. list files\\n2. done"}\n'
                    '{"role":"user","content":"torn line no newline'
                )
            result, endpoint = self.run_agent(
                [text_response("done")],
                "next\n\n/exit\n\n",
                cwd=workspace,
                flags=("--resume", "custom.jsonl"),
            )
            self.assertIn("[session] resumed 3 messages from custom.jsonl", result.stdout)
            messages = endpoint.requests[0]["messages"]
            users = [message for message in messages if message["role"] == "user"]
            self.assertEqual(users[-1]["content"], "next")
            tool = next(m for m in messages if m["role"] == "tool")
            self.assertEqual(tool["content"], "file1\nfile2")
            self.assertTrue(any(m.get("tool_calls") for m in messages))
            system = self.system_content(endpoint)
            self.assertIn("CURRENT TASK PLAN", system)
            self.assertIn("1. list files", system)
            with open(path, encoding="utf-8") as handle:
                saved = handle.read()
        self.assertTrue(saved.endswith('{"role":"assistant","content":"done"}\n'))
        self.assertIn('\n{"role":"user","content":"next"}\n', saved)
        self.assertNotIn("torn line no newline{", saved)

    def test_resume_missing_file_is_usage_error(self):
        with tempfile.TemporaryDirectory() as workspace:
            process = subprocess.run(
                [EXECUTABLE, "--resume", "absent.jsonl", "-exec", "hi"],
                input="",
                text=True,
                capture_output=True,
                timeout=5,
                env={"AI_API_URL": "http://127.0.0.1:9/v1", "PATH": os.environ["PATH"]},
                cwd=workspace,
            )
        self.assertEqual(process.returncode, 2)
        self.assertIn("cannot read session absent.jsonl", process.stdout + process.stderr)

    def test_continue_and_resume_conflict_is_usage_error(self):
        process = subprocess.run(
            [EXECUTABLE, "--continue", "--resume", "x.jsonl"],
            input="",
            text=True,
            capture_output=True,
            timeout=5,
            env={"AI_API_URL": "http://127.0.0.1:9/v1", "PATH": os.environ["PATH"]},
        )
        self.assertEqual(process.returncode, 2)
        self.assertIn("excludes", process.stderr)

    def test_new_restarts_the_session_transcript(self):
        endpoint = Endpoint([text_response("first reply"), text_response("second reply")])
        self.addCleanup(endpoint.thread.join, timeout=2)
        self.addCleanup(endpoint.close)
        workspace = tempfile.TemporaryDirectory()
        self.addCleanup(workspace.cleanup)
        master, slave = pty.openpty()
        process = subprocess.Popen(
            [EXECUTABLE],
            stdin=slave,
            stdout=slave,
            stderr=subprocess.STDOUT,
            cwd=workspace.name,
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

        read_until(b"[You]: ", 10)
        os.write(master, b"hello\r")
        read_until(b"first reply", 10)
        read_until(b"[You]: ", 10)
        os.write(master, b"/new\r")
        read_until(b"[context reset]", 5)
        read_until(b"[You]: ", 10)
        os.write(master, b"after\r")
        read_until(b"second reply", 10)
        read_until(b"[You]: ", 10)
        os.write(master, b"/exit\r")
        read_until(b"[bye]", 5)
        lines = self.read_session(workspace.name)
        self.assertEqual([line["role"] for line in lines], ["user", "assistant"])
        self.assertEqual(lines[0]["content"], "after")
        self.assertEqual(lines[1]["content"], "second reply")

    def test_tui_continue_flag_is_forwarded(self):
        if shutil.which("tmux") is None:
            self.skipTest("tmux is required for TUI resume forwarding")
        workspace = tempfile.TemporaryDirectory()
        self.addCleanup(workspace.cleanup)
        os.makedirs(os.path.join(workspace.name, ".ai"))
        with open(
            os.path.join(workspace.name, ".ai", "session.jsonl"), "w", encoding="utf-8"
        ) as handle:
            handle.write('{"role":"user","content":"saved question"}\n')
        endpoint = Endpoint([text_response("done")])
        self.addCleanup(endpoint.thread.join, timeout=2)
        self.addCleanup(endpoint.close)
        server = f"ai-tui-continue-{os.getpid()}"
        quiet_tmux_kill_server()
        self.addCleanup(quiet_tmux_kill_server)
        environment = os.environ.copy()
        environment.update(
            {
                "AI_API_URL": f"http://127.0.0.1:{endpoint.port}/v1",
                "AI_TIMEOUT_SECONDS": "5",
                "AI_COMMAND_TIMEOUT_SECONDS": "2",
                "PATH": os.environ["PATH"],
            }
        )
        subprocess.run(
            [
                "tmux", "-f", "/dev/null", "new-session", "-d", "-x", "100", "-y", "24",
                "-c", workspace.name, "-n", server, TUI_EXECUTABLE, "tui", "--continue",
            ],
            env=environment,
            check=True,
            timeout=5,
        )
        self.addCleanup(
            quiet_tmux_kill_session, server
        )
        deadline = time.monotonic() + 8
        output = ""
        while time.monotonic() < deadline:
            output = subprocess.run(
                ["tmux", "capture-pane", "-p", "-t", server],
                check=True,
                capture_output=True,
                text=True,
                timeout=5,
            ).stdout
            if "resumed 1 messages" in output:
                break
            time.sleep(0.2)
        self.assertIn("resumed 1 messages from .ai/session.jsonl", output)

    def test_streamed_text_prints_once_with_header(self):
        result, endpoint = self.run_agent(
            [sse_response(deltas=["Hello", " streamed", " world"])],
            "go\n\n/exit\n\n",
        )
        self.assertEqual(result.stdout.count("[AI Response]:"), 1)
        self.assertIn("Hello streamed world", result.stdout)
        self.assertIs(endpoint.requests[0].get("stream"), True)

    def test_streamed_tool_call_arguments_assemble_across_deltas(self):
        result, endpoint = self.run_agent(
            [
                sse_response(
                    tool_call_deltas=[
                        {
                            "index": 0,
                            "id": "call-1",
                            "type": "function",
                            "function": {
                                "name": "run_terminal_command",
                                "arguments": '{"comm',
                            },
                        },
                        {"index": 0, "function": {"arguments": 'and": "echo hi"}'}},
                    ]
                ),
                text_response("done"),
            ],
            "diagnose\n\na\n\n\n/exit\n\n",
        )
        self.assertIn("[AI requests command]: echo hi", result.stdout)
        self.assertIn("hi", result.stdout)
        replies = self.tool_replies(endpoint)
        self.assertIn("hi", replies[0]["content"])

    def test_stream_falls_back_to_buffered_json(self):
        result, endpoint = self.run_agent(
            [text_response("buffered")],
            "go\n\n/exit\n\n",
        )
        self.assertEqual(result.stdout.count("[AI Response]:"), 1)
        self.assertIn("buffered", result.stdout)
        self.assertIs(endpoint.requests[0].get("stream"), True)

    def test_stream_ignores_comments_and_metadata_fields(self):
        result, _ = self.run_agent(
            [
                sse_response(
                    deltas=["alpha", "beta"],
                    extra_lines=[": keep-alive", "event: delta", "id: 42", "retry: 100"],
                )
            ],
            "go\n\n/exit\n\n",
        )
        self.assertIn("alphabeta", result.stdout)
        self.assertNotIn("keep-alive", result.stdout)
        self.assertNotIn("retry", result.stdout)
        self.assertNotIn("event", result.stdout)

    def test_stream_reports_midstream_error(self):
        result, _ = self.run_agent(
            [
                sse_response(
                    deltas=["partial"],
                    extra_lines=[
                        "data: " + json.dumps({"error": {"message": "boom"}})
                    ],
                )
            ],
            "go\n\n/exit\n\n",
        )
        self.assertIn("error: AI endpoint: boom", result.stdout)
        # An explicit error event is definitive: no retry notice, no
        # generic protocol-error companion line.
        self.assertNotIn("retrying", result.stdout)
        self.assertNotIn("Protocol error", result.stdout)
        self.assertNotIn("Hello", result.stdout)

    def test_stream_completes_without_done_marker(self):
        result, _ = self.run_agent(
            [sse_response(deltas=["tail-less"], done=False)],
            "go\n\n/exit\n\n",
        )
        self.assertIn("tail-less", result.stdout)
        self.assertEqual(result.stdout.count("[AI Response]:"), 1)

    def test_streamed_usage_feeds_exit_summary(self):
        result, _ = self.run_agent(
            [
                sse_response(
                    deltas=["ok"],
                    usage={
                        "prompt_tokens": 12400,
                        "completion_tokens": 3100,
                        "total_tokens": 15500,
                    },
                )
            ],
            "go\n\n/exit\n\n",
        )
        self.assertIn(
            "[usage] tokens: 12.4k in / 3.1k out, 1 rounds, context 12.4k",
            result.stdout,
        )

    def test_ai_stream_zero_sends_no_stream_flag(self):
        result, endpoint = self.run_agent(
            [text_response("plain")],
            "go\n\n/exit\n\n",
            extra_env={"AI_STREAM": "0"},
        )
        self.assertNotIn("stream", endpoint.requests[0])
        self.assertEqual(result.stdout.count("[AI Response]:"), 1)
        self.assertIn("plain", result.stdout)

    def test_tui_renders_streamed_text(self):
        if shutil.which("tmux") is None:
            self.skipTest("tmux is required for TUI streaming output")
        endpoint = Endpoint([sse_response(deltas=["counting", " to ", "ten"])])
        self.addCleanup(endpoint.thread.join, timeout=2)
        self.addCleanup(endpoint.close)
        server = f"ai-tui-stream-{os.getpid()}"
        quiet_tmux_kill_server()
        self.addCleanup(quiet_tmux_kill_server)
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
        self.addCleanup(
            quiet_tmux_kill_session, server
        )
        # Keys injected before the TUI takes over the tty can be dropped.
        time.sleep(0.3)
        subprocess.run(
            ["tmux", "send-keys", "-t", server, "count", "Enter"], check=True, timeout=5
        )
        deadline = time.monotonic() + 8
        output = ""
        while time.monotonic() < deadline:
            output = subprocess.run(
                ["tmux", "capture-pane", "-p", "-t", server],
                check=True,
                capture_output=True,
                text=True,
                timeout=5,
            ).stdout
            if "counting to ten" in output:
                break
            time.sleep(0.2)
        self.assertIn("counting to ten", output)

    def capture_tui(self, server):
        return subprocess.run(
            ["tmux", "capture-pane", "-p", "-t", server],
            check=True,
            capture_output=True,
            text=True,
            timeout=5,
        ).stdout

    def test_tui_focus_scrolls_pane_to_bottom(self):
        if shutil.which("tmux") is None:
            self.skipTest("tmux is required for TUI pane focus")
        lines = "\n".join(f"scroll-line-{i}" for i in range(1, 41))
        endpoint = Endpoint(
            [sse_response(deltas=[lines]), sse_response(deltas=["pushed-after-scroll"])]
        )
        self.addCleanup(endpoint.thread.join, timeout=2)
        self.addCleanup(endpoint.close)
        server = f"ai-tui-focus-{os.getpid()}"
        quiet_tmux_kill_server()
        self.addCleanup(quiet_tmux_kill_server)
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
        self.addCleanup(
            quiet_tmux_kill_session, server
        )
        # Keys injected before the TUI takes over the tty can be dropped.
        time.sleep(0.3)
        subprocess.run(
            ["tmux", "send-keys", "-t", server, "go", "Enter"], check=True, timeout=5
        )
        deadline = time.monotonic() + 8
        output = ""
        while time.monotonic() < deadline:
            output = self.capture_tui(server)
            if "scroll-line-40" in output:
                break
            time.sleep(0.2)
        self.assertIn("scroll-line-40", output)

        def wait_for(predicate, timeout=4):
            end = time.monotonic() + timeout
            captured = self.capture_tui(server)
            while time.monotonic() < end:
                if predicate(captured):
                    return captured
                time.sleep(0.2)
                captured = self.capture_tui(server)
            return captured

        def press(key):
            subprocess.run(
                ["tmux", "send-keys", "-t", server, key], check=True, timeout=5
            )

        # PageUp leaves the bottom of pane 1 behind.
        press("PageUp")
        output = wait_for(lambda text: "scroll-line-40" not in text)
        self.assertNotIn("scroll-line-40", output)

        # New content pushed to the pane snaps the view back to the bottom
        # without any pane switch.
        subprocess.run(
            ["tmux", "send-keys", "-t", server, "more", "Enter"], check=True, timeout=5
        )
        output = wait_for(lambda text: "pushed-after-scroll" in text)
        self.assertIn("pushed-after-scroll", output)

        # Tab moves the [*] focus to pane 2, and back to pane 1, whose view
        # must have snapped to the bottom again.
        press("PageUp")
        output = wait_for(lambda text: "pushed-after-scroll" not in text)
        self.assertNotIn("pushed-after-scroll", output)
        press("Tab")
        output = wait_for(lambda text: "2. AI Command Status [*]" in text)
        self.assertIn("2. AI Command Status [*]", output)
        press("Tab")
        output = wait_for(lambda text: "pushed-after-scroll" in text)
        self.assertIn("pushed-after-scroll", output)

    def test_tui_streaming_keeps_rendering_after_pane_switch(self):
        if shutil.which("tmux") is None:
            self.skipTest("tmux is required for TUI streaming output")
        endpoint = Endpoint([SlowResponse([f"trickle-line-{i}\n" for i in range(1, 51)])])
        self.addCleanup(endpoint.thread.join, timeout=2)
        self.addCleanup(endpoint.close)
        server = f"ai-tui-trickle-{os.getpid()}"
        quiet_tmux_kill_server()
        self.addCleanup(quiet_tmux_kill_server)
        environment = os.environ.copy()
        environment.update(
            {
                "AI_API_URL": f"http://127.0.0.1:{endpoint.port}/v1",
                "AI_TIMEOUT_SECONDS": "10",
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
        self.addCleanup(
            quiet_tmux_kill_session, server
        )
        # Keys injected before the TUI takes over the tty can be dropped.
        time.sleep(0.3)
        subprocess.run(
            ["tmux", "send-keys", "-t", server, "go", "Enter"], check=True, timeout=5
        )
        # Wait until the stream is underway, then switch panes twice; the
        # input window is recreated by every layout redraw.
        deadline = time.monotonic() + 8
        output = ""
        while time.monotonic() < deadline:
            output = self.capture_tui(server)
            if "trickle-line-5" in output:
                break
            time.sleep(0.2)
        self.assertIn("trickle-line-5", output)
        subprocess.run(["tmux", "send-keys", "-t", server, "Tab"], check=True, timeout=5)
        subprocess.run(["tmux", "send-keys", "-t", server, "Tab"], check=True, timeout=5)
        # With no further keys the pane must keep following the stream; a
        # blocking wgetch after the redraw would freeze it partway.
        deadline = time.monotonic() + 15
        while time.monotonic() < deadline:
            output = self.capture_tui(server)
            if "trickle-line-50" in output:
                break
            time.sleep(0.2)
        self.assertIn("trickle-line-50", output)

    def test_tui_paints_streamed_partial_line(self):
        if shutil.which("tmux") is None:
            self.skipTest("tmux is required for TUI streaming output")
        # Deltas trickle in without newlines; only the last one ends the line.
        endpoint = Endpoint(
            [
                SlowResponse(
                    ["mid-line-alpha ", "mid-line-beta ", "mid-line-gamma", " done\n"],
                    interval=0.4,
                )
            ]
        )
        self.addCleanup(endpoint.thread.join, timeout=2)
        self.addCleanup(endpoint.close)
        server = f"ai-tui-partial-{os.getpid()}"
        quiet_tmux_kill_server()
        self.addCleanup(quiet_tmux_kill_server)
        environment = os.environ.copy()
        environment.update(
            {
                "AI_API_URL": f"http://127.0.0.1:{endpoint.port}/v1",
                "AI_TIMEOUT_SECONDS": "10",
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
        self.addCleanup(
            quiet_tmux_kill_session, server
        )
        # Keys injected before the TUI takes over the tty can be dropped.
        time.sleep(0.3)
        subprocess.run(
            ["tmux", "send-keys", "-t", server, "go", "Enter"], check=True, timeout=5
        )
        # The growing line must appear before its newline arrives; holding it
        # back until a quiet second passes would leave the pane empty here.
        deadline = time.monotonic() + 10
        output = ""
        while time.monotonic() < deadline:
            output = self.capture_tui(server)
            if "mid-line-alpha mid-line-beta" in output and "done" not in output:
                break
            time.sleep(0.1)
        self.assertIn("mid-line-alpha mid-line-beta", output)
        self.assertNotIn("done", output)
        # The finished line replaces its paint instead of duplicating it.
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            output = self.capture_tui(server)
            if "mid-line-gamma done" in output:
                break
            time.sleep(0.2)
        self.assertIn("mid-line-alpha mid-line-beta mid-line-gamma done", output)
        self.assertEqual(output.count("mid-line-alpha"), 1)

    def landlock_ready(self, result):
        if "landlock rules + lexical path filter" not in result.stdout:
            self.skipTest("kernel has no Landlock support")
        return result

    def scoped_home(self):
        # A writable directory outside any workspace AND outside /tmp (the
        # landlock ruleset deliberately keeps /tmp writable as scratch), so
        # writes there must be denied; $HOME is explicit because run_agent
        # settings do not carry it by default.
        home = tempfile.mkdtemp(prefix=".ai-landlock-", dir=os.path.expanduser("~"))
        self.addCleanup(shutil.rmtree, home, True)
        return home, {"HOME": home}

    def test_landlock_blocks_writes_outside_workspace(self):
        workspace = self.make_workspace({"keep.txt": "data\n"})
        home, env = self.scoped_home()
        # The path is spelled through $HOME so the lexical filter passes it;
        # only the OS-level rules can stop the write.
        result, _ = self.run_agent(
            [tool_response('echo x > $HOME/probe; printf RC_%s\\n "$?"'),
             text_response("done")],
            "go\n\na\n/exit\n\n",
            workspace=workspace,
            extra_env=env,
        )
        self.landlock_ready(result)
        self.assertIn("Permission denied", result.stdout)
        self.assertFalse(os.path.exists(os.path.join(home, "probe")))

    def test_landlock_keeps_workspace_writable(self):
        workspace = self.make_workspace({"src.txt": "one\n"})
        home, env = self.scoped_home()
        result, _ = self.run_agent(
            [tool_response("printf works > src.txt; cat src.txt"), text_response("done")],
            "go\n\na\n/exit\n\n",
            workspace=workspace,
            extra_env=env,
        )
        self.landlock_ready(result)
        self.assertIn("works", result.stdout)
        with open(os.path.join(workspace, "src.txt")) as handle:
            self.assertEqual(handle.read(), "works")

    def test_landlock_allows_tmp_scratch(self):
        workspace = self.make_workspace({})
        result, _ = self.run_agent(
            [tool_response('T=$(mktemp) && printf made > "$T" && rm -f "$T" && echo TMP-OK'),
             text_response("done")],
            "go\n\na\n/exit\n\n",
            workspace=workspace,
        )
        self.landlock_ready(result)
        self.assertIn("TMP-OK", result.stdout)

    def test_ai_landlock_zero_disables_enforcement(self):
        workspace = self.make_workspace({})
        home, env = self.scoped_home()
        env["AI_LANDLOCK"] = "0"
        result, _ = self.run_agent(
            [tool_response('printf x > $HOME/probe && echo OPT-OUT-WROTE'),
             text_response("done")],
            "go\n\na\n/exit\n\n",
            workspace=workspace,
            extra_env=env,
        )
        self.assertIn("Sandbox: lexical path filter (landlock disabled)", result.stdout)
        self.assertIn("OPT-OUT-WROTE", result.stdout)
        self.assertNotIn("Permission denied", result.stdout)
        self.assertTrue(os.path.exists(os.path.join(home, "probe")))

    def test_unscoped_runs_have_no_sandbox_banner(self):
        result, _ = self.run_agent(
            [tool_response("echo plain"), text_response("done")],
            "go\n\na\n/exit\n\n",
        )
        self.assertIn("unrestricted", result.stdout)
        self.assertNotIn("Sandbox:", result.stdout)


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
    unittest.main(testRunner=ProgressTestRunner)
