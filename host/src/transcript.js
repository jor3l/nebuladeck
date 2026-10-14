// Token totals for the current task, read from Claude Code's session transcript.
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';

// The agent's shell may sit in a subfolder of the project, so walk up from cwd.
export function findTranscript(cwd = process.cwd()) {
  for (let d = cwd; ; d = path.dirname(d)) {
    const dir = path.join(os.homedir(), '.claude', 'projects', d.replace(/[^a-zA-Z0-9]/g, '-'));
    if (fs.existsSync(dir)) return newestIn(dir);
    if (path.dirname(d) === d) return null;
  }
}

function newestIn(dir) {
  let best = null;
  for (const f of fs.readdirSync(dir)) {
    if (!f.endsWith('.jsonl')) continue;
    const p = path.join(dir, f);
    const m = fs.statSync(p).mtimeMs;
    if (!best || m > best.m) best = { p, m };
  }
  return best?.p ?? null;
}

// "Task" = everything since the last prompt the user typed.
// in = fresh input + cache writes (cache reads are counted separately: they re-send
// the same context every turn and would drown the number).
export function taskUsage(file) {
  const entries = [];
  for (const line of fs.readFileSync(file, 'utf8').split('\n')) {
    if (!line) continue;
    try { entries.push(JSON.parse(line)); } catch {}
  }
  let start = 0;
  entries.forEach((e, i) => {
    if (e.type === 'user' && !e.isMeta && !e.isSidechain && typeof e.message?.content === 'string') start = i;
  });
  const byId = new Map(); // streamed duplicates share an id: keep the last
  for (const e of entries.slice(start)) {
    if (e.type === 'assistant' && e.message?.usage) byId.set(e.message.id ?? e.uuid, e.message.usage);
  }
  let tokensIn = 0, tokensOut = 0, cacheRead = 0;
  for (const u of byId.values()) {
    tokensIn += (u.input_tokens ?? 0) + (u.cache_creation_input_tokens ?? 0);
    tokensOut += u.output_tokens ?? 0;
    cacheRead += u.cache_read_input_tokens ?? 0;
  }
  const t0 = Date.parse(entries[start]?.timestamp ?? '');
  const durationS = Number.isFinite(t0) ? Math.max(0, Math.round((Date.now() - t0) / 1000)) : undefined;
  return { tokensIn, tokensOut, cacheRead, durationS };
}
