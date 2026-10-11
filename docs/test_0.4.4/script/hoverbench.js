// costo del "fantasma" del clic-per-creare a ogni movimento del mouse: 221 punti della curva totale (12 bande: 8 Bell, 2 tagli 96 dB/oct, 2 Band Pass 96 dB/oct)
const OLD = require('/home/claude/lab/base043/dsp.js'), NEW = require('/home/claude/lab/new/dsp_core.js');
const fs = 48000, bands = [60, 120, 250, 500, 1000, 2000, 4000, 8000].map((f, k) => ({ b: { type: 'bell', f, q: 1, slope: 12, gq: false }, g: k % 2 ? -3 : 3 }))
  .concat([{ b: { type: 'lcut', f: 30, q: 0.71, slope: 96 }, g: 0 }, { b: { type: 'hcut', f: 16000, q: 0.71, slope: 96 }, g: 0 }, { b: { type: 'bpass', f: 700, q: 1, slope: 96 }, g: 0 }, { b: { type: 'bpass', f: 5000, q: 1, slope: 96 }, g: 0 }]);
const F = Array.from({ length: 221 }, (_, k) => 20 * Math.pow(1000, k / 220));
function noMemo(T) { let s = 0; for (const f of F) for (const x of bands) { const c = T.designBand(x.b, x.g, fs); s += 10 * Math.log10(Math.max(T.digMag2(c, f, fs), 1e-12)); } return s; }
function memo(T) { const cs = bands.map(x => T.designBand(x.b, x.g, fs)); let s = 0; for (const f of F) for (const c of cs) s += 10 * Math.log10(Math.max(T.digMag2(c, f, fs), 1e-12)); return s; }
function t(fn, T) { fn(T); const ts = []; for (let r = 0; r < 15; r++) { const t0 = process.hrtime.bigint(); fn(T); ts.push(Number(process.hrtime.bigint() - t0) / 1e6); } ts.sort((a, b) => a - b); return ts[7]; }
const r = { 'UI 0.4.3 + motore 0.4.3 (senza memo)': t(noMemo, OLD), 'UI senza memo + motore 0.4.4 (ipotetico)': t(noMemo, NEW), 'UI 0.4.4 (memo) + motore 0.4.4': t(memo, NEW) };
for (const k in r) console.log(k.padEnd(44), r[k].toFixed(2), 'ms per movimento del mouse');
console.log('stesso risultato memo/no-memo (0.4.4):', noMemo(NEW) === memo(NEW));
