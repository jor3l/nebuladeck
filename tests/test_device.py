#!/usr/bin/env python3
"""End-to-end tests against a real NebulaDeck on the network (`make e2e`).

Uses its own agents ("E2E-A", "E2E-B", ...) in project "e2e", so real sessions on the screen
are left alone, and says bye to them at the end. Questions are never tapped: the tests check
they stay open and expire on time. Device: NEBULADECK_HOST (default nebuladeck.local).

Requests go through `curl -4`, exactly like agents make them (and macOS's Local Network
privacy often blocks Python's own sockets but not curl).
"""
import json, os, subprocess, sys, time, unittest, urllib.parse

HOST = os.environ.get("NEBULADECK_HOST", "nebuladeck.local")
AGENTS = ["E2E-A", "E2E-B", "E2E-C"]


def base():
    return "http://%s/api" % HOST


def curl(url, method="GET", data=None, headers=()):
    """Returns (status, body text, response headers lowercased)."""
    cmd = ["curl", "-4", "-s", "-m", "5", "-X", method, "-D", "-", url]
    for h in headers:
        cmd += ["-H", h]
    if data is not None:
        cmd += ["--data-binary", data]
    r = subprocess.run(cmd, capture_output=True)  # bytes: text mode would turn \r\n into \n
    if r.returncode:
        raise OSError("curl exit %d" % r.returncode)
    head, _, body = r.stdout.decode().partition("\r\n\r\n")
    lines = head.split("\r\n")
    hdrs = {l.split(":", 1)[0].lower(): l.split(":", 1)[1].strip() for l in lines[1:] if ":" in l}
    return int(lines[0].split()[1]), body, hdrs


def call(path, body=None, method=None):
    data = json.dumps(body) if body is not None else None
    code, raw, _ = curl(base() + path, method or ("POST" if data is not None else "GET"), data,
                        ["Content-Type: application/json"] if data is not None else [])
    return code, (json.loads(raw) if raw.startswith("{") else raw)


def post(t, body=None):
    return call("/" + t, body if body is not None else {})


def agents():
    return {a["agent"]: a for a in call("/info")[1]["agents"] if a["project"] == "e2e"}


def hook(event, **params):
    q = urllib.parse.urlencode({"event": event, **params})
    return call("/hook?" + q, method="POST")


class Device(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        try:
            code, info = call("/info")
        except OSError as e:
            raise unittest.SkipTest("no device at %s (%s)" % (HOST, e))
        print("device: NebulaDeck %s at %s" % (info["fw"], info["ip"]), file=sys.stderr)
        cls.tearDownClass()

    @classmethod
    def tearDownClass(cls):
        for name in AGENTS:
            post("bye", {"agent": name, "project": "e2e"})

    def tearDown(self):
        self.tearDownClass()

    def test_info(self):
        code, info = call("/info")
        self.assertEqual(code, 200)
        for k in ("fw", "ip", "ssid", "agents"):
            self.assertIn(k, info)

    def test_state_creates_an_agent(self):
        post("state", {"agent": "E2E-A", "project": "e2e", "status": "working", "title": "Testing state"})
        a = agents()["E2E-A"]
        self.assertEqual(a["status"], "working")
        self.assertEqual(a["line"], "Testing state")

    def test_two_agents_are_kept_apart(self):
        post("state", {"agent": "E2E-A", "project": "e2e", "status": "working", "title": "First"})
        post("state", {"agent": "E2E-B", "project": "e2e", "status": "error", "title": "Second"})
        ag = agents()
        self.assertEqual(ag["E2E-A"]["line"], "First")
        self.assertEqual(ag["E2E-B"]["line"], "Second")
        self.assertEqual(ag["E2E-B"]["status"], "error")

    def test_msg_without_project_goes_to_the_agent_by_name(self):
        post("state", {"agent": "E2E-A", "project": "e2e", "status": "working", "title": "Task"})
        post("msg", {"agent": "E2E-A", "text": "A milestone"})
        a = agents()["E2E-A"]
        self.assertEqual(a["activity"], 1)
        self.assertEqual(a["last"], "A milestone")

    def test_ask_stays_open_then_expires(self):
        # Regression: asks were cancelled within milliseconds (session check wrapped around).
        post("state", {"agent": "E2E-A", "project": "e2e", "status": "working", "title": "Asking"})
        code, r = post("ask", {"agent": "E2E-A", "project": "e2e", "id": "e2e-q1",
                               "prompt": "E2E test question", "options": ["Yes", "No"], "timeout": 3})
        self.assertEqual((code, r["id"]), (200, "e2e-q1"))
        time.sleep(1)
        self.assertEqual(call("/answer?id=e2e-q1")[1], "pending")
        self.assertEqual(call("/ask?id=e2e-q1")[1]["state"], "pending")
        self.assertTrue(agents()["E2E-A"]["asking"])
        time.sleep(3)
        self.assertEqual(call("/answer?id=e2e-q1")[1], "cancelled")
        a = agents()["E2E-A"]
        self.assertFalse(a["asking"])
        self.assertEqual(a["status"], "working")  # back to what it showed before the question

    def test_ask_outside_a_task_returns_to_presence(self):
        post("presence", {"agent": "E2E-A", "project": "e2e", "text": "Ready"})
        post("ask", {"agent": "E2E-A", "project": "e2e", "id": "e2e-q2", "prompt": "Q", "options": ["A"], "timeout": 1})
        time.sleep(2)
        self.assertEqual(agents()["E2E-A"]["status"], "idle")

    def test_ask_cancel_by_id(self):
        post("ask", {"agent": "E2E-B", "project": "e2e", "id": "e2e-q3", "prompt": "Q", "options": ["A", "B"], "timeout": 30})
        self.assertEqual(call("/answer?id=e2e-q3")[1], "pending")
        post("ask_cancel", {"id": "e2e-q3"})
        self.assertEqual(call("/answer?id=e2e-q3")[1], "cancelled")

    def test_asks_of_two_agents_are_independent(self):
        post("ask", {"agent": "E2E-A", "project": "e2e", "id": "e2e-qa", "prompt": "A?", "options": ["1"], "timeout": 30})
        post("ask", {"agent": "E2E-B", "project": "e2e", "id": "e2e-qb", "prompt": "B?", "options": ["1"], "timeout": 30})
        self.assertEqual(call("/answer?id=e2e-qa")[1], "pending")
        self.assertEqual(call("/answer?id=e2e-qb")[1], "pending")
        post("ask_cancel", {"id": "e2e-qa"})
        self.assertEqual(call("/answer?id=e2e-qb")[1], "pending")

    def test_unknown_ask_id_reads_cancelled(self):
        self.assertEqual(call("/answer?id=does-not-exist")[1], "cancelled")

    def test_hooks_find_the_agent_by_project(self):
        self.assertEqual(hook("UserPromptSubmit", agent="E2E-C", project="e2e")[0], 200)
        self.assertEqual(agents()["E2E-C"]["line"], "Thinking...")
        hook("PreToolUse", agent="E2E-C", project="e2e", tool="Edit")
        self.assertEqual(agents()["E2E-C"]["line"], "Editing...")
        hook("Notification", agent="E2E-C", project="e2e", kind="idle")
        c = agents()["E2E-C"]
        self.assertFalse(c["attn"])
        self.assertEqual(c["line"], "Waiting for you")
        hook("Notification", agent="E2E-C", project="e2e")
        self.assertTrue(agents()["E2E-C"]["attn"])

    def test_tool_steps_feed_a_running_task(self):
        post("state", {"agent": "E2E-C", "project": "e2e", "status": "working", "title": "Task"})
        for _ in range(3):
            hook("PreToolUse", agent="E2E-C", project="e2e", tool="Edit")
        c = agents()["E2E-C"]
        self.assertEqual((c["activity"], c["last"]), (1, "Editing"))  # one merged step line

    def test_bye_removes_only_that_agent(self):
        post("state", {"agent": "E2E-A", "project": "e2e", "status": "working", "title": "A"})
        post("state", {"agent": "E2E-B", "project": "e2e", "status": "working", "title": "B"})
        hook("SessionEnd", agent="E2E-A", project="e2e")
        ag = agents()
        self.assertNotIn("E2E-A", ag)
        self.assertIn("E2E-B", ag)

    def test_done(self):
        post("state", {"agent": "E2E-B", "project": "e2e", "status": "working", "title": "B"})
        post("done", {"agent": "E2E-B", "project": "e2e", "summary": "All good"})
        b = agents()["E2E-B"]
        self.assertTrue(b["completed"])
        self.assertEqual(b["line"], "All good")

    def test_errors(self):
        self.assertEqual(post("nope")[0], 404)
        self.assertEqual(call("/state", method="POST")[0], 200)  # empty body is fine
        self.assertEqual(post("ask", {"agent": "E2E-A", "project": "e2e", "prompt": "Q", "options": ["1", "2", "3", "4", "5"]})[0], 400)
        self.assertEqual(curl(base() + "/state", "POST", "{nope", ["Content-Type: application/json"])[0], 400)
        self.assertEqual(hook("Bogus", agent="E2E-A", project="e2e")[0], 400)

    def test_cors_only_for_the_setup_page(self):
        h = curl(base() + "/info", headers=["Origin: https://jor3l.github.io"])[2]
        self.assertEqual(h.get("access-control-allow-origin"), "https://jor3l.github.io")
        h = curl(base() + "/ping", "POST", "{}", ["Content-Type: application/json", "Origin: https://evil.example"])[2]
        self.assertNotIn("access-control-allow-origin", h)


if __name__ == "__main__":
    unittest.main(verbosity=2)
