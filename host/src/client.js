import net from 'node:net';
import fs from 'node:fs';
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { SOCK, LOG } from './paths.js';

const tryConnect = () => new Promise((resolve, reject) => {
  const s = net.connect(SOCK);
  s.once('connect', () => resolve(s));
  s.once('error', reject);
});

async function ensureDaemon() {
  try { return await tryConnect(); } catch {}
  const out = fs.openSync(LOG, 'a');
  const child = spawn(process.execPath, [fileURLToPath(new URL('./daemon.js', import.meta.url))], {
    detached: true, stdio: ['ignore', out, out],
  });
  child.unref();
  for (let i = 0; i < 30; i++) {
    await new Promise((r) => setTimeout(r, 100));
    try { return await tryConnect(); } catch {}
  }
  throw new Error(`daemon did not start (see ${LOG})`);
}

// Sends one request; resolves with the daemon's reply (may take a while for `ask`).
export async function request(req, { autostart = true } = {}) {
  const sock = autostart ? await ensureDaemon() : await tryConnect();
  return new Promise((resolve, reject) => {
    let buf = '';
    sock.on('data', (d) => (buf += d));
    sock.on('end', () => { try { resolve(JSON.parse(buf)); } catch { reject(new Error('bad reply')); } });
    sock.on('error', reject);
    sock.write(JSON.stringify(req) + '\n');
  });
}
