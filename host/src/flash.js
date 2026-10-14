// `nebuladeck flash`: writes a prebuilt firmware image with esptool (no PlatformIO needed).
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { SerialPort } from 'serialport';
import { request } from './client.js';

const REPO = 'jor3l/nebuladeck';
const ASSET = 'nebuladeck-sc01plus.bin';

function findEsptool() {
  for (const bin of ['esptool', 'esptool.py']) {
    if (spawnSync(bin, ['version'], { stdio: 'ignore' }).status === 0) return [bin];
  }
  if (spawnSync('python3', ['-m', 'esptool', 'version'], { stdio: 'ignore' }).status === 0) return ['python3', '-m', 'esptool'];
  return null;
}

async function download(tag) {
  const url = tag
    ? `https://github.com/${REPO}/releases/download/${tag}/${ASSET}`
    : `https://github.com/${REPO}/releases/latest/download/${ASSET}`;
  console.log(`downloading ${url}`);
  const res = await fetch(url, { redirect: 'follow' });
  if (!res.ok) throw new Error(`download failed: HTTP ${res.status}`);
  const file = path.join(os.tmpdir(), `nebuladeck-${Date.now()}.bin`);
  fs.writeFileSync(file, Buffer.from(await res.arrayBuffer()));
  return file;
}

export async function flash({ file, tag, port }) {
  const esptool = findEsptool();
  if (!esptool) {
    console.error('esptool not found. Install it with:  pipx install esptool   (or: brew install esptool)');
    process.exit(1);
  }
  port ??= (await SerialPort.list()).find((p) => p.vendorId?.toLowerCase() === '303a')?.path;
  if (!port) {
    console.error('No ESP32-S3 found. Put the board in download mode first:\n' +
      '  jumper BOOT to GND on the DEBUG header, press RST, then run this again.');
    process.exit(1);
  }
  try { await request({ cmd: 'stop' }, { autostart: false }); } catch {} // release the port
  await new Promise((r) => setTimeout(r, 300));
  const image = file ?? (await download(tag));
  console.log(`flashing ${image} to ${port}`);
  const r = spawnSync(esptool[0], [...esptool.slice(1), '--chip', 'esp32s3', '--port', port, '--baud', '921600',
    'write_flash', '-z', '0x0', image], { stdio: 'inherit' });
  if (r.status !== 0) {
    console.error('\nflash failed. If it could not connect, the board is not in download mode (BOOT to GND, press RST).');
    process.exit(r.status ?? 1);
  }
  console.log('\ndone. Remove the BOOT jumper and press RST.');
}
