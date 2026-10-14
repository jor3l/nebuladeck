// Maps Claude Code hook events to presence updates, and installs/removes the hooks.
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';

const AGENT = process.env.NEBULADECK_AGENT || 'Claude';

const TOOL_TEXT = [
  [/^Bash/, 'Running a command...'], [/^(Read|NotebookRead)$/, 'Reading...'],
  [/^(Edit|Write|MultiEdit|NotebookEdit)$/, 'Editing...'], [/^(Grep|Glob|LS)$/, 'Searching...'],
  [/^(WebSearch|WebFetch)$/, 'Browsing...'], [/^(Task|Agent)$/, 'Delegating...'],
  [/^(TodoWrite|Plan)/, 'Planning...'],
];

export function presenceFromHook(ev) {
  switch (ev.hook_event_name) {
    case 'SessionStart': return { text: 'Ready', new_turn: true };
    case 'UserPromptSubmit': return { text: 'Thinking...', new_turn: true };
    case 'PreToolUse':
      return { text: TOOL_TEXT.find(([re]) => re.test(ev.tool_name ?? ''))?.[1] ?? 'Working...' };
    case 'Notification': return { text: 'Needs you', attn: true };
    case 'Stop': return { text: 'Ready', end_turn: true };
    case 'SessionEnd': return { bye: true };
    default: return null;
  }
}

export const agentName = () => AGENT;

const EVENTS = ['SessionStart', 'UserPromptSubmit', 'PreToolUse', 'Notification', 'Stop', 'SessionEnd'];
const COMMAND = 'nebuladeck hook';
const SETTINGS = path.join(os.homedir(), '.claude', 'settings.json');

function load() {
  try { return JSON.parse(fs.readFileSync(SETTINGS, 'utf8')); } catch (e) {
    if (e.code === 'ENOENT') return {};
    throw new Error(`cannot parse ${SETTINGS}: ${e.message}`);
  }
}

function save(cfg) {
  fs.mkdirSync(path.dirname(SETTINGS), { recursive: true });
  if (fs.existsSync(SETTINGS)) fs.copyFileSync(SETTINGS, SETTINGS + '.nebuladeck.bak');
  fs.writeFileSync(SETTINGS, JSON.stringify(cfg, null, 2) + '\n');
}

const isOurs = (h) => h?.hooks?.some((x) => x.command === COMMAND);

export function installHooks() {
  const cfg = load();
  cfg.hooks ??= {};
  let added = 0;
  for (const ev of EVENTS) {
    cfg.hooks[ev] ??= [];
    if (cfg.hooks[ev].some(isOurs)) continue;
    cfg.hooks[ev].push({ hooks: [{ type: 'command', command: COMMAND }] });
    added++;
  }
  if (added) save(cfg);
  return { added, file: SETTINGS };
}

export function uninstallHooks() {
  const cfg = load();
  let removed = 0;
  for (const ev of Object.keys(cfg.hooks ?? {})) {
    const keep = cfg.hooks[ev].filter((h) => !isOurs(h));
    removed += cfg.hooks[ev].length - keep.length;
    if (keep.length) cfg.hooks[ev] = keep; else delete cfg.hooks[ev];
  }
  if (cfg.hooks && !Object.keys(cfg.hooks).length) delete cfg.hooks;
  if (removed) save(cfg);
  return { removed, file: SETTINGS };
}
