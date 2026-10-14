// Owns the USB serial port: handshake, heartbeat, reconnect. CLI clients talk to
// it over a unix socket (one JSON request per line, one JSON reply per line).
import net from 'node:net';
import fs from 'node:fs';
import crypto from 'node:crypto';
import { SerialPort } from 'serialport';
import { SOCK } from './paths.js';

const IDLE_MS = Number(process.env.NEBULADECK_IDLE_MIN ?? 60) * 60_000;
const log = (...a) => console.log(new Date().toISOString(), ...a);

let port = null;       // open SerialPort
let portPath = null;
let ready = false;     // device answered our hello
let active = false;    // an agent session is live: keep the heartbeat going
let lastActivity = 0;
let last = {};         // last state/done, replayed if the device reboots
const pending = new Map(); // ask id -> { reply, timer }
let rx = '';

const sendDev = (o) => { if (port?.isOpen) port.write(JSON.stringify(o) + '\n'); };

async function findPort() {
  if (process.env.NEBULADECK_PORT) return process.env.NEBULADECK_PORT;
  const list = await SerialPort.list();
  return list.find((p) => p.vendorId?.toLowerCase() === '303a')?.path; // Espressif
}

async function connectLoop() {
  if (port) return;
  let p;
  try { p = await findPort(); } catch { return; }
  if (!p) return;
  const sp = new SerialPort({ path: p, baudRate: 115200, autoOpen: false, hupcl: false });
  sp.open((err) => {
    if (err) { log('open failed:', err.message); return; }
    sp.set({ dtr: false, rts: false }, () => {});
    port = sp; portPath = p; ready = false; rx = '';
    log('port open', p);
  });
  sp.on('data', onData);
  sp.on('close', () => { if (port === sp) { log('port closed'); port = null; ready = false; } });
  sp.on('error', (e) => log('port error:', e.message));
}

function onData(buf) {
  rx += buf.toString('utf8');
  let i;
  while ((i = rx.indexOf('\n')) >= 0) {
    const line = rx.slice(0, i).trim();
    rx = rx.slice(i + 1);
    if (!line.startsWith('{')) continue;
    let m;
    try { m = JSON.parse(line); } catch { continue; }
    if (m.t === 'hello') {
      ready = true;
      log('device ready', JSON.stringify(m));
      for (const k of ['presence', 'state', 'done']) if (active && last[k]) sendDev(last[k]);
    } else if (m.t === 'tap' && pending.has(m.id)) {
      const p = pending.get(m.id);
      clearTimeout(p.timer);
      pending.delete(m.id);
      p.reply({ ok: true, delivered: true, choice: m.choice, label: m.label });
    }
  }
}

// 1 Hz: hello until the device answers, then ping. Idle sessions end themselves.
setInterval(() => {
  if (active && IDLE_MS && Date.now() - lastActivity > IDLE_MS) {
    log('idle timeout, ending session');
    endSession();
  }
  if (!port || !active) return;
  sendDev(ready ? { t: 'ping' } : { t: 'hello', v: 1 });
}, 1000);
setInterval(connectLoop, 2000);
connectLoop();

function endSession() {
  sendDev({ t: 'bye' });
  active = false; last = {};
  for (const [id, p] of pending) { clearTimeout(p.timer); p.reply({ ok: true, delivered: false, cancelled: true }); pending.delete(id); }
}

function activate() {
  lastActivity = Date.now();
  if (!active) { active = true; if (ready) sendDev({ t: 'ping' }); }
}

const NEEDS_DEVICE = new Set(['state', 'done', 'msg', 'clear', 'ask']);

// Right after the daemon (or the device) starts, give the handshake a moment.
function waitReady(ms) {
  return new Promise((resolve) => {
    const t0 = Date.now();
    const iv = setInterval(() => {
      if (ready || Date.now() - t0 > ms) { clearInterval(iv); resolve(); }
    }, 100);
  });
}

function handle(req, reply) {
  if (NEEDS_DEVICE.has(req.cmd) && !ready && !req._waited) {
    activate(); // starts the hello/ping loop
    waitReady(3000).then(() => handle({ ...req, _waited: true }, reply));
    return null;
  }
  const delivered = () => ready && port?.isOpen;
  switch (req.cmd) {
    case 'status':
      return reply({ ok: true, port: portPath, open: !!port?.isOpen, ready, active, pid: process.pid });
    case 'stop':
      reply({ ok: true });
      setTimeout(() => process.exit(0), 50);
      return;
    case 'bye':
      endSession();
      return reply({ ok: true });
    case 'presence': { // fire-and-forget: hooks must never wait on the device
      activate();
      const m = { t: 'presence', agent: req.agent, text: req.text, attn: req.attn, new_turn: req.new_turn, end_turn: req.end_turn };
      last.presence = m;
      if (req.new_turn) { delete last.done; delete last.state; }
      if (delivered()) sendDev(m);
      return reply({ ok: true, delivered: delivered() });
    }
    case 'state': {
      activate();
      const m = { t: 'state', project: req.project, status: req.status, title: req.title, progress: req.progress };
      last.state = m; delete last.done;
      if (delivered()) sendDev(m);
      return reply({ ok: true, delivered: delivered() });
    }
    case 'done': {
      activate();
      const m = { t: 'done', summary: req.summary, tokens_in: req.tokens_in, tokens_out: req.tokens_out, duration: req.duration };
      last.done = m; delete last.state;
      if (delivered()) sendDev(m);
      return reply({ ok: true, delivered: delivered() });
    }
    case 'msg':
      activate();
      if (delivered()) sendDev({ t: 'msg', text: req.text, level: req.level, ttl: req.ttl });
      return reply({ ok: true, delivered: delivered() });
    case 'clear':
      activate();
      if (delivered()) sendDev({ t: 'clear' });
      return reply({ ok: true, delivered: delivered() });
    case 'ask': {
      activate();
      if (!delivered()) return reply({ ok: true, delivered: false });
      const id = crypto.randomBytes(4).toString('hex');
      const timer = req.timeout > 0 ? setTimeout(() => {
        pending.delete(id);
        sendDev({ t: 'ask_cancel' });
        reply({ ok: true, delivered: true, timeout: true });
      }, req.timeout * 1000) : null;
      pending.set(id, { reply, timer });
      sendDev({ t: 'ask', id, prompt: req.prompt, options: req.options });
      return id;
    }
    default:
      return reply({ ok: false, error: `unknown cmd ${req.cmd}` });
  }
}

try { fs.unlinkSync(SOCK); } catch {}
const server = net.createServer((sock) => {
  let buf = '';
  let askId = null;
  const reply = (o) => { if (!sock.destroyed) sock.end(JSON.stringify(o) + '\n'); };
  sock.on('data', (d) => {
    buf += d;
    const i = buf.indexOf('\n');
    if (i < 0) return;
    let req;
    try { req = JSON.parse(buf.slice(0, i)); } catch { return reply({ ok: false, error: 'bad json' }); }
    buf = '';
    askId = handle(req, reply) ?? null;
  });
  sock.on('close', () => {
    // Client went away mid-ask: drop the question from the screen.
    if (askId && pending.has(askId)) {
      clearTimeout(pending.get(askId).timer);
      pending.delete(askId);
      sendDev({ t: 'ask_cancel' });
    }
  });
  sock.on('error', () => {});
});
server.listen(SOCK, () => log('daemon listening', SOCK));
for (const s of ['SIGINT', 'SIGTERM']) process.on(s, () => { try { fs.unlinkSync(SOCK); } catch {} process.exit(0); });
