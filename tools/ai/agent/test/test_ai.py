import os
import subprocess
import unittest

import test_agent
from test_agent import ProgressTestRunner, named_tool_response, text_response


class AITests(unittest.TestCase):
    # Borrow the AgentTests harness as plain functions, and keep the class
    # itself out of this module's namespace: unittest.main() runs every
    # TestCase subclass it finds here, imported or not, and subclassing
    # AgentTests would re-run all of its test methods a second time too.
    run_agent = test_agent.AgentTests.run_agent
    make_workspace = test_agent.AgentTests.make_workspace
    tool_replies = test_agent.AgentTests.tool_replies

    def test_apply_patch_fixes_broken_c_program(self):
        # Replays a real GLM session end to end: a tab-indented C file with
        # seven compile errors, one git-style diff attempt the scope check
        # blocks, then the five-hunk SEARCH/REPLACE patch (two hunks span
        # multiple lines) that fixed the file in a single call. The patched
        # file must keep its tabs, compile, and run.
        workspace = self.make_workspace(
            {
                "test.c": (
                    "/* test.c: a small running-total program, deliberately broken with\n"
                    "   several compile errors. Fix them all so `cc -o test test.c`\n"
                    "   succeeds and ./test prints the total. */\n"
                    "\n"
                    "#include <stdio.h>\n"
                    "\n"
                    "#define STEPS 5\n"
                    "\n"
                    "static int\n"
                    "step(int value, int by)\n"
                    "{\n"
                    "\treturn value + by;\n"
                    "}\n"
                    "\n"
                    "int\n"
                    "main(void)\n"
                    "{\n"
                    "\tint total\n"
                    "\tint i;\n"
                    "\n"
                    "\ttotal = 0;\n"
                    "\tfor (i = 0; i < STEPS; i++) {\n"
                    "\t\ttotal = step(total i + 1);\n"
                    "\t\tcount++;\n"
                    "\t}\n"
                    '\tprintf("total: %d\\n" total);\n'
                    "\tif (total > LIMIT\n"
                    '\t\tprintf("over the limit\\n");\n'
                    "\treturn 0\n"
                    "}\n"
                )
            }
        )
        git_style = (
            "--- a/test.c\n"
            "+++ b/test.c\n"
            "@@ -18,7 +18,8 @@ main(void)\n"
            " \tint total\n"
            " \tint i;\n"
            " \n"
            " \ttotal = 0;\n"
            " \tfor (i = 0; i < STEPS; i++) {\n"
            " \t\ttotal = step(total i + 1);\n"
            "-\t\tcount++;\n"
            "+\t\ti = i;\n"
            " \t}\n"
            ' \tprintf("total: %d\\n" total);\n'
            " \tif (total > LIMIT\n"
            ' \t\tprintf("over the limit\\n");\n'
            " \treturn 0\n"
            " }\n"
        )
        search_replace = (
            "test.c\n"
            "<<<<<<< SEARCH\n"
            "#define STEPS 5\n"
            "=======\n"
            "#define STEPS 5\n"
            "#define LIMIT 10\n"
            ">>>>>>> REPLACE\n"
            "<<<<<<< SEARCH\n"
            "\tint total\n"
            "=======\n"
            "\tint total;\n"
            ">>>>>>> REPLACE\n"
            "<<<<<<< SEARCH\n"
            "\t\ttotal = step(total i + 1);\n"
            "\t\tcount++;\n"
            "=======\n"
            "\t\ttotal = step(total, i + 1);\n"
            ">>>>>>> REPLACE\n"
            "<<<<<<< SEARCH\n"
            '\tprintf("total: %d\\n" total);\n'
            "\tif (total > LIMIT\n"
            "=======\n"
            '\tprintf("total: %d\\n", total);\n'
            "\tif (total > LIMIT)\n"
            ">>>>>>> REPLACE\n"
            "<<<<<<< SEARCH\n"
            "\treturn 0\n"
            "=======\n"
            "\treturn 0;\n"
            ">>>>>>> REPLACE\n"
        )
        result, endpoint = self.run_agent(
            [
                named_tool_response("apply_patch", {"patch": git_style}),
                named_tool_response("apply_patch", {"patch": search_replace}),
                text_response("done"),
            ],
            "diagnose\n\n/exit\n\n",
            full_access=True,
            workspace=workspace,
        )
        self.assertIn(
            "Patch blocked: --- a/test.c resolves outside the workspace scope", result.stdout
        )
        replies = self.tool_replies(endpoint, 2)
        self.assertIn(
            "Error: the patch references a path outside the workspace scope.",
            replies[0]["content"],
        )
        self.assertIn(
            "[patch applied: 5 hunks, +7 -7 lines] to test.c", replies[1]["content"]
        )
        with open(os.path.join(workspace, "test.c")) as handle:
            self.assertEqual(
                handle.read(),
                (
                    "/* test.c: a small running-total program, deliberately broken with\n"
                    "   several compile errors. Fix them all so `cc -o test test.c`\n"
                    "   succeeds and ./test prints the total. */\n"
                    "\n"
                    "#include <stdio.h>\n"
                    "\n"
                    "#define STEPS 5\n"
                    "#define LIMIT 10\n"
                    "\n"
                    "static int\n"
                    "step(int value, int by)\n"
                    "{\n"
                    "\treturn value + by;\n"
                    "}\n"
                    "\n"
                    "int\n"
                    "main(void)\n"
                    "{\n"
                    "\tint total;\n"
                    "\tint i;\n"
                    "\n"
                    "\ttotal = 0;\n"
                    "\tfor (i = 0; i < STEPS; i++) {\n"
                    "\t\ttotal = step(total, i + 1);\n"
                    "\t}\n"
                    '\tprintf("total: %d\\n", total);\n'
                    "\tif (total > LIMIT)\n"
                    '\t\tprintf("over the limit\\n");\n'
                    "\treturn 0;\n"
                    "}\n"
                ),
            )
        build = subprocess.run(
            ["cc", "-o", "test", "test.c"], cwd=workspace, capture_output=True, text=True
        )
        self.assertEqual(build.returncode, 0, build.stderr)
        run = subprocess.run(
            [os.path.join(workspace, "test")], capture_output=True, text=True
        )
        self.assertEqual(run.stdout, "total: 15\nover the limit\n")


if __name__ == "__main__":
    unittest.main(testRunner=ProgressTestRunner)
