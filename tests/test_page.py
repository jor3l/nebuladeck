#!/usr/bin/env python3
"""Drives the setup page (site/index.html) in headless Chrome with a fake NebulaDeck behind
Web Bluetooth. The fake behaves like real Chrome: readValue() also fires
'characteristicvaluechanged' (the cause of the "tapping a network does nothing" loop).
No hardware needed. Standard library + Google Chrome / Chromium.
"""
import html, json, os, re, shutil, subprocess, tempfile, unittest

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
with open(os.environ.get("PAGE_FILE") or os.path.join(ROOT, "site", "index.html")) as _f:
    PAGE = _f.read()  # PAGE_FILE: test another version of the page


def chrome():
    for c in [os.environ.get("CHROME"), "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome",
              shutil.which("google-chrome"), shutil.which("google-chrome-stable"), shutil.which("chromium"),
              shutil.which("chromium-browser")]:
        if c and os.path.exists(c):
            return c
    return None


# Wrapped in a function: classic scripts share one global scope with the page's own consts.
FAKE_BLE = """(() => {
const enc = new TextEncoder();
window.__reads = 0; window.__writes = [];
const vals = {
  '6e656275-6c61-6465-636b-000000000002': JSON.stringify(%(nets)s),
  '6e656275-6c61-6465-636b-000000000004': '{"state":"ready","fw":"test"}' };
const ch = (u) => { const l = []; return {
  readValue: async () => { window.__reads++; const v = new DataView(enc.encode(vals[u]).buffer);
                           l.forEach((f) => f({ target: { value: v } })); return v; },  // like Chrome
  writeValueWithResponse: async (v) => { window.__writes.push(new TextDecoder().decode(v)); },
  startNotifications: async () => {}, addEventListener: (_, f) => l.push(f) }; };
const dev = { name: 'NebulaDeck-TEST', id: 't', addEventListener() {},
  gatt: { connect: async () => ({ getPrimaryService: async () => ({ getCharacteristic: async (u) => ch(u) }) }) } };
Object.defineProperty(navigator, 'bluetooth', { value: { requestDevice: async () => dev } });
})();
"""


def run(script, query="", nets=None, budget=8000):
    """Loads the page with the fake device, runs `script` (async, sets window.RESULT) after load."""
    nets = nets if nets is not None else [["Home", -50, 1], ["Cafe", -70, 0]]
    inject = "<script>%s\nconsole.log = () => {};\nwindow.addEventListener('load', async () => {\n" \
             "const wait = (ms) => new Promise((r) => setTimeout(r, ms)); const $ = (id) => document.getElementById(id);\n" \
             "try { %s } catch (e) { window.RESULT = { error: e.message }; }\n" \
             "const pre = document.createElement('pre'); pre.id = 'RESULT'; pre.textContent = JSON.stringify(window.RESULT);\n" \
             "document.body.append(pre); });</script>" % (FAKE_BLE % {"nets": json.dumps(nets)}, script)
    d = tempfile.mkdtemp()
    f = os.path.join(d, "index.html")
    with open(f, "w") as out:
        out.write(PAGE.replace("<head>", "<head>" + inject, 1))
    try:
        dom = subprocess.run([chrome(), "--headless=new", "--disable-gpu", "--no-sandbox", "--virtual-time-budget=%d" % budget,
                              "--dump-dom", "file://" + f + query], capture_output=True, text=True, timeout=30).stdout
    except subprocess.TimeoutExpired:
        dom = ""  # simulated time never settled: the page is busy forever
    shutil.rmtree(d, ignore_errors=True)
    m = re.search(r'<pre id="RESULT">(.*?)</pre>', dom, re.S)
    if not m:
        raise AssertionError("page never finished (an endless loop?)")
    return json.loads(html.unescape(m.group(1)))


@unittest.skipUnless(chrome(), "Chrome/Chromium not found (set CHROME=...)")
class WifiSetup(unittest.TestCase):
    def test_reads_settle_and_tapping_a_network_opens_the_form(self):
        # Regression: reads looped forever and rebuilt the list under the finger.
        r = run("""
          $('connectBtn').click(); await wait(2000);
          const reads = window.__reads;
          const btns = document.querySelectorAll('.nets button');
          btns[0].click(); await wait(300);
          const f = $('joinForm');
          window.RESULT = { reads, nets: btns.length, open: !f.hidden && f.isConnected,
                            under: f.closest('li')?.querySelector('.name')?.textContent };
        """, "?setup=ABCD")
        self.assertNotIn("error", r)
        self.assertLessEqual(r["reads"], 3)
        self.assertEqual(r["nets"], 2)
        self.assertTrue(r["open"])
        self.assertEqual(r["under"], "Home")

    def test_join_sends_ssid_and_password(self):
        r = run("""
          $('connectBtn').click(); await wait(1000);
          document.querySelectorAll('.nets button')[0].click(); await wait(200);
          $('pass').value = 'pa"ss'; $('joinBtn').click(); await wait(300);
          window.RESULT = { writes: window.__writes };
        """)
        self.assertEqual(json.loads(r["writes"][-1]), {"cmd": "join", "ssid": "Home", "pass": 'pa"ss'})

    def test_open_network_needs_no_password(self):
        r = run("""
          $('connectBtn').click(); await wait(1000);
          document.querySelectorAll('.nets button')[1].click(); await wait(200);
          window.RESULT = { passHidden: $('passRow').hidden };
        """)
        self.assertTrue(r["passHidden"])

    def test_long_list_keeps_the_form_on_screen(self):
        nets = [["Network %d" % i, -40 - i, 1] for i in range(15)]
        r = run("""
          $('connectBtn').click(); await wait(1000);
          document.querySelectorAll('.nets button')[12].click(); await wait(300);
          const top = $('joinForm').getBoundingClientRect().top;
          window.RESULT = { top, vh: innerHeight };
        """, nets=nets)
        self.assertGreaterEqual(r["top"], 0)
        self.assertLess(r["top"], r["vh"])


@unittest.skipUnless(chrome(), "Chrome/Chromium not found (set CHROME=...)")
class Navigation(unittest.TestCase):
    def test_choosing_shows_only_that_view_with_its_title_and_back(self):
        r = run("""
          const vis = (id) => !$(id).hidden;
          const home = { title: $('title').textContent, choices: !document.querySelector('.choices').hidden, back: vis('back') };
          $('pickFlash').click(); await wait(50);
          const flash = { title: $('title').textContent, choices: !document.querySelector('.choices').hidden,
                          back: vis('back'), flash: vis('flash'), wifi: vis('wifi'), hash: location.hash };
          $('back').click(); await wait(50);
          const again = { title: $('title').textContent, choices: !document.querySelector('.choices').hidden, back: vis('back'), flash: vis('flash') };
          window.RESULT = { home, flash, again };
        """)
        self.assertEqual(r["home"], {"title": "Set up your screen", "choices": True, "back": False})
        self.assertEqual(r["flash"], {"title": "Flash device", "choices": False, "back": True, "flash": True, "wifi": False, "hash": "#/flash"})
        self.assertEqual(r["again"], {"title": "Set up your screen", "choices": True, "back": False, "flash": False})

    def test_qr_link_opens_wifi(self):
        r = run("window.RESULT = { title: $('title').textContent, wifi: !$('wifi').hidden };", "?setup=ABCD")
        self.assertEqual(r, {"title": "Connect to WiFi", "wifi": True})


@unittest.skipUnless(chrome(), "Chrome/Chromium not found (set CHROME=...)")
class AgentSetup(unittest.TestCase):
    def test_address_flows_into_every_command(self):
        r = run("""
          $('pickAgent').click();
          $('host').value = 'http://192.168.0.201/'; $('host').dispatchEvent(new Event('input'));
          const texts = [...document.querySelectorAll('[data-tpl]')].map((e) => e.textContent);
          window.RESULT = { texts, test: $('testCmd').textContent };
        """)
        self.assertIn("192.168.0.201", r["test"])
        for t in r["texts"]:
            self.assertNotIn("nebuladeck.local", t.replace("nebuladeck\\.local", ""))  # sed pattern may name it
        self.assertTrue(any("sed 's/nebuladeck\\.local/192.168.0.201/g'" in t for t in r["texts"]))

    def test_invalid_address_is_rejected(self):
        r = run("""
          $('host').value = 'bad; rm -rf ~'; $('host').dispatchEvent(new Event('input'));
          const texts = [...document.querySelectorAll('[data-tpl]')].map((e) => e.textContent).join('\\n');
          window.RESULT = { invalid: $('host').getAttribute('aria-invalid'), leaked: texts.includes('rm -rf') };
        """)
        self.assertEqual(r["invalid"], "true")
        self.assertFalse(r["leaked"])

    def test_each_tab_names_its_agent(self):
        r = run("""
          const out = {};
          for (const b of document.querySelectorAll('[data-tab]')) {
            b.click();
            const p = document.querySelector(`[data-panel="${b.dataset.tab}"]`);
            out[b.dataset.tab] = { visible: !p.hidden, text: p.textContent };
          }
          window.RESULT = out;
        """)
        for tab, name in {"codex": "Codex", "gemini": "Gemini", "cursor": "Cursor", "copilot": "Copilot"}.items():
            self.assertTrue(r[tab]["visible"], tab)
            self.assertIn('"agent":"%s"' % name, r[tab]["text"], tab)
        self.assertNotIn('"agent":', r["other"]["text"])  # unknown agents get no name (green dot)
        for tab in r:
            self.assertIn("curl -4", r[tab]["text"].replace("curl -fsSL", "curl -4"), tab)


if __name__ == "__main__":
    unittest.main(verbosity=2)
