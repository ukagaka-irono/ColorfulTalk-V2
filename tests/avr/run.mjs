// AVR シミュレータ（avr8js）で V1 のファームウェアを動かし、出力・RAM・処理時間を測る。
//   node run.mjs firmware.hex out.raw [sram_bytes]
// out.raw には 16bit リトルエンディアンの PCM を書く。
import fs from 'node:fs';
import { CPU, avrInstruction } from 'avr8js';

const [hexPath, outPath, sramArg] = process.argv.slice(2);
const SRAM = Number(sramArg || 16384);
const GPIOR0 = 0x3e, GPIOR1 = 0x4a, GPIOR2 = 0x4b;

function loadHex(text, flash) {
  for (const line of text.split(/\r?\n/)) {
    if (!line.startsWith(':')) continue;
    const n = parseInt(line.substr(1, 2), 16);
    const addr = parseInt(line.substr(3, 4), 16);
    const type = parseInt(line.substr(7, 2), 16);
    if (type !== 0) continue;
    for (let i = 0; i < n; i++) flash[addr + i] = parseInt(line.substr(9 + 2 * i, 2), 16);
  }
}

const bytes = new Uint8Array(128 * 1024);
loadHex(fs.readFileSync(hexPath, 'utf8'), bytes);
const cpu = new CPU(new Uint16Array(bytes.buffer), SRAM);

const samples = [];
let lo = 0, hi = 0, done = false, error = -1;
let lastCycles = 0, maxGap = 0, firstSampleCycles = -1;
let minSP = 0xffff;
cpu.writeHooks[GPIOR1] = (v) => { lo = v; };
cpu.writeHooks[GPIOR2] = (v) => { hi = v; };
cpu.writeHooks[GPIOR0] = (v) => {
  if (v === 1) {
    let s = lo | (hi << 8);
    if (s & 0x8000) s -= 0x10000;
    samples.push(s);
    if (firstSampleCycles < 0) firstSampleCycles = cpu.cycles;
    else maxGap = Math.max(maxGap, cpu.cycles - lastCycles);
    lastCycles = cpu.cycles;
  } else if (v === 2) done = true;
  else if (v === 3) { error = lo; done = true; }
  return false;
};

const limit = 4e10;
while (!done && cpu.cycles < limit) {
  avrInstruction(cpu);
  cpu.tick();
  const sp = cpu.dataView.getUint16(0x5d, true);
  if (sp < minSP) minSP = sp;
}

const out = Buffer.alloc(samples.length * 2);
samples.forEach((s, i) => out.writeInt16LE(s, 2 * i));
fs.writeFileSync(outPath, out);

const total = lastCycles - firstSampleCycles;
console.log(JSON.stringify({
  done, error, samples: samples.length,
  cycles_per_sample_avg: samples.length > 1 ? Math.round(total / (samples.length - 1)) : 0,
  cycles_gap_max: maxGap,
  min_sp: minSP,
  setup_cycles: firstSampleCycles,
}));
