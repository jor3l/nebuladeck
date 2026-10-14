import os from 'node:os';
import path from 'node:path';
import fs from 'node:fs';

export const DIR = path.join(os.homedir(), '.nebuladeck');
export const SOCK = path.join(DIR, 'daemon.sock');
export const LOG = path.join(DIR, 'daemon.log');
fs.mkdirSync(DIR, { recursive: true });
