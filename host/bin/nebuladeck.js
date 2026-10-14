#!/usr/bin/env node
import path from 'node:path';
import { parseArgs } from 'node:util';
import { request } from '../src/client.js';
import { findTranscript, taskUsage } from '../src/transcript.js';

const USAGE = `nebuladeck <command>

  state <working|waiting|done|error|idle> "<title>" [--project P] [--progress 0..1]
  msg "<text>" [--level info|warn|error] [--ttl SECONDS]
  ask "<prompt>" <option> [option...] [--timeout SECONDS]   (prints the tapped option)
  done "<summary>" [--tokens-in N --tokens-out N] [--no-tokens] [--duration S] [--transcript FILE]
  clear | bye | status | stop

Without a device every command exits 0 (add --strict to fail). ask exits
2 on timeout and 3 when no device could be asked.`;

const STATUSES = ['working', 'waiting', 'done', 'error', 'idle'];
const LEVELS = ['info', 'warn', 'error'];

const { values: v, positionals: pos } = parseArgs({
  allowPositionals: true,
  options: {
    project: { type: 'string' }, progress: { type: 'string' }, level: { type: 'string' },
    ttl: { type: 'string' }, timeout: { type: 'string' }, 'tokens-in': { type: 'string' },
    'tokens-out': { type: 'string' }, 'no-tokens': { type: 'boolean' }, duration: { type: 'string' },
    transcript: { type: 'string' }, strict: { type: 'boolean' }, help: { type: 'boolean', short: 'h' },
  },
});

const [cmd, ...args] = pos;
const die = (msg, code = 1) => { console.error(`nebuladeck: ${msg}`); process.exit(code); };
if (!cmd || v.help || cmd === 'help') { console.log(USAGE); process.exit(cmd ? 0 : 1); }
const num = (s) => (s === undefined ? undefined : Number(s));

async function main() {
  let req;
  switch (cmd) {
    case 'state': {
      const [status, ...t] = args;
      if (!STATUSES.includes(status)) die(`status must be one of ${STATUSES.join(', ')}`);
      req = { cmd, status, title: t.join(' '), project: v.project ?? path.basename(process.cwd()), progress: num(v.progress) };
      break;
    }
    case 'msg':
      if (!args.length) die('msg needs text');
      if (v.level && !LEVELS.includes(v.level)) die(`level must be one of ${LEVELS.join(', ')}`);
      req = { cmd, text: args.join(' '), level: v.level ?? 'info', ttl: num(v.ttl) ?? 0 };
      break;
    case 'ask': {
      const [prompt, ...options] = args;
      if (!prompt || options.length < 1 || options.length > 4) die('ask needs a prompt and 1-4 options');
      req = { cmd, prompt, options, timeout: num(v.timeout) ?? 0 };
      break;
    }
    case 'done': {
      req = { cmd, summary: args.join(' '), tokens_in: num(v['tokens-in']), tokens_out: num(v['tokens-out']), duration: num(v.duration) };
      if (!v['no-tokens'] && req.tokens_in === undefined && req.tokens_out === undefined) {
        try {
          const file = v.transcript ?? findTranscript();
          if (file) {
            const u = taskUsage(file);
            req.tokens_in = u.tokensIn; req.tokens_out = u.tokensOut;
            req.duration ??= u.durationS;
          }
        } catch (e) { console.error(`nebuladeck: no token totals (${e.message})`); }
      }
      break;
    }
    case 'clear': case 'bye': case 'stop':
      req = { cmd };
      break;
    case 'status': {
      try {
        const r = await request({ cmd }, { autostart: false });
        console.log(JSON.stringify(r, null, 2));
      } catch { console.log('daemon not running (it starts on the first command)'); }
      return;
    }
    default:
      die(`unknown command "${cmd}"\n\n${USAGE}`);
  }

  let r;
  try {
    r = await request(req, { autostart: cmd !== 'stop' });
  } catch (e) {
    if (cmd === 'stop') return;
    if (v.strict) die(e.message, 3);
    console.error(`nebuladeck: skipped (${e.message})`);
    process.exit(cmd === 'ask' ? 3 : 0);
  }
  if (cmd === 'ask') {
    if (r.timeout) process.exit(2);
    if (!r.delivered) { console.error('nebuladeck: no device to ask'); process.exit(3); }
    console.log(r.label);
    return;
  }
  if (r.delivered === false && v.strict) die('no device connected', 3);
  if (r.delivered === false) console.error('nebuladeck: no device connected (skipped)');
}
main();
