#!/usr/bin/env python3
"""Runs every command in skill/hooks.json with sample Claude Code events against a fake
device on localhost, and checks exactly what reaches it. No hardware needed.
Standard library + sh + curl.
"""
import http.server, json, os, subprocess, threading, time, unittest, urllib.parse

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
HOOKS = json.load(open(os.path.join(ROOT, "skill", "hooks.json")))["hooks"]
CWD = "/Users/someone/Development/my project & co"


class Fake(http.server.BaseHTTPRequestHandler):
    seen = []

    def do_POST(self):
        u = urllib.parse.urlparse(self.path)
        n = int(self.headers.get("Content-Length") or 0)
        Fake.seen.append({"path": u.path, "query": dict(urllib.parse.parse_qsl(u.query)),
                          "body": self.rfile.read(n).decode() if n else ""})
        self.send_response(200)
        self.end_headers()
        self.wfile.write(b'{"ok":true}')

    def log_message(self, *a):
        pass


class Hooks(unittest.TestCase):
    _keep_project = False  # tests run inside Claude Code may inherit CLAUDE_PROJECT_DIR

    @classmethod
    def setUpClass(cls):
        cls.server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Fake)
        threading.Thread(target=cls.server.serve_forever, daemon=True).start()
        cls.host = "127.0.0.1:%d" % cls.server.server_address[1]

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()

    def run_hook(self, event, payload, host=None):
        Fake.seen.clear()
        cmd = HOOKS[event][0]["hooks"][0]["command"]
        t0 = time.time()
        env = {k: v for k, v in os.environ.items() if k != "CLAUDE_PROJECT_DIR" or self._keep_project}
        r = subprocess.run(["sh", "-c", cmd], input=json.dumps(payload), text=True, capture_output=True,
                           env={**env, "NEBULADECK_HOST": host or self.host}, timeout=10)
        took = time.time() - t0
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(r.stdout, "", "hooks must not print")
        for _ in range(50):  # the curl runs in the background
            if Fake.seen:
                break
            time.sleep(0.05)
        return Fake.seen[0] if Fake.seen else None, took

    def test_all_six_events_are_installed(self):
        self.assertEqual(set(HOOKS), {"SessionStart", "UserPromptSubmit", "PreToolUse", "Notification", "Stop", "SessionEnd"})

    def test_each_event_sends_its_name_and_project(self):
        for ev in HOOKS:
            req, _ = self.run_hook(ev, {"session_id": "s", "cwd": CWD, "hook_event_name": ev})
            self.assertIsNotNone(req, ev)
            self.assertEqual(req["path"], "/api/hook")
            self.assertEqual(req["query"]["event"], ev)
            self.assertEqual(req["query"]["project"], "my project & co")  # folder name, URL-encoded intact

    def test_project_is_the_session_root_not_the_current_folder(self):
        # Regression: a session that cd'd into a subfolder showed up as a second agent.
        old = os.environ.pop("CLAUDE_PROJECT_DIR", None)
        try:
            os.environ["CLAUDE_PROJECT_DIR"] = "/Users/someone/Development/nebuladeck"
            type(self)._keep_project = True
            req, _ = self.run_hook("Stop", {"cwd": "/Users/someone/Development/nebuladeck/firmware"})
            type(self)._keep_project = False
            self.assertEqual(req["query"]["project"], "nebuladeck")
            del os.environ["CLAUDE_PROJECT_DIR"]
            req, _ = self.run_hook("Stop", {"cwd": "/Users/someone/Development/other"})
            self.assertEqual(req["query"]["project"], "other")  # no project dir: the cwd's folder
        finally:
            os.environ.pop("CLAUDE_PROJECT_DIR", None)
            if old is not None:
                os.environ["CLAUDE_PROJECT_DIR"] = old

    def test_tool_name_and_nested_traps(self):
        # tool_input may nest keys called cwd/tool_name: the top-level ones (first) must win.
        req, _ = self.run_hook("PreToolUse", {"cwd": CWD, "hook_event_name": "PreToolUse", "tool_name": "Edit",
                                              "tool_input": {"cwd": "/nope", "tool_name": "Bash"}})
        self.assertEqual(req["query"]["tool"], "Edit")
        self.assertEqual(req["query"]["project"], "my project & co")

    def test_never_sends_content(self):
        # A Write carries the whole file; only names may leave the machine.
        secret = "TOP-SECRET-FILE-CONTENTS"
        req, _ = self.run_hook("PreToolUse", {"cwd": CWD, "tool_name": "Write",
                                              "tool_input": {"file_path": "/x", "content": secret}})
        self.assertEqual(req["body"], "")
        self.assertNotIn(secret, json.dumps(req))
        self.assertEqual(set(req["query"]), {"event", "project", "tool"})

    def test_notification_idle_vs_needs_you(self):
        req, _ = self.run_hook("Notification", {"cwd": CWD, "message": "Claude is waiting for your input"})
        self.assertEqual(req["query"]["kind"], "idle")
        req, _ = self.run_hook("Notification", {"cwd": CWD, "message": "Claude needs your permission to use Bash"})
        self.assertEqual(req["query"]["kind"], "attn")
        self.assertNotIn("message", req["query"])  # the text itself isn't sent

    def test_hooks_return_immediately_without_a_device(self):
        # Regression: a missing screen must never slow Claude down.
        _, took = self.run_hook("Stop", {"cwd": CWD}, host="192.0.2.1")  # TEST-NET, unroutable
        self.assertLess(took, 0.5)

    def test_ipv4_only(self):
        # Regression: without -4, .local lookups waited 5 s for an IPv6 answer.
        for ev in HOOKS:
            self.assertIn("curl -4 ", HOOKS[ev][0]["hooks"][0]["command"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
