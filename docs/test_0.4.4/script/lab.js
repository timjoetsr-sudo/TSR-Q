// TSR Q · laboratorio JS (stesso motore del prototipo/artefatto). uso: node lab.js <dsp.js> <sezione>
'use strict';
const fs = require('fs'), path = require('path');
const T = require(path.resolve(process.argv[2])); const sec = process.argv[3];
const raw = fs.readFileSync(path.join(__dirname, 'eso.f32')); const il = new Float32Array(raw.buffer, raw.byteOffset, raw.length / 4);
const NC = il.length / 2, CL = new Float32Array(NC), CR = new Float32Array(NC); for (let i = 0; i < NC; i++) { CL[i] = il[2 * i]; CR[i] = il[2 * i + 1]; }
const TY = ['bell', 'lshelf', 'hshelf', 'lcut', 'hcut', 'notch', 'bpass', 'tilt', 'flat', 'allpass'];
const band = (id, t, f, g, q, slope = 12, place = 0, dyn = null) => ({ id, on: true, type: TY[t], f, g, q, slope, place, gq: false, dyn: dyn || { on: false, thr: -24, range: -6, att: 5, rel: 80, knee: 6, det: 0 } });
const state = (bands, o = {}) => Object.assign({ bands, inGain: 0, out: 0, autoGainDb: 0, scale: 1, character: 0, bypass: false, solo: -1, pan: 0, panMS: false, invert: false }, o);
function scenarios() {
  const v = [];
  v.push(['1 Bell', state([band(0, 0, 1000, 4, 1)])]);
  v.push(['8 Bell', state(Array.from({ length: 8 }, (_, i) => band(i, 0, 60 * Math.pow(1.9, i), i % 2 ? -3 : 3, 1.2)))]);
  const T8 = [0, 1, 2, 0, 5, 0, 7, 0];
  v.push(['32 statiche', state(Array.from({ length: 32 }, (_, i) => band(i, T8[i % 8], 30 * Math.pow(1.22, i), i % 2 ? -3 : 3, 1.5)))]);
  v.push(['32 Mid/Side', state(Array.from({ length: 32 }, (_, i) => band(i, 0, 30 * Math.pow(1.22, i), i % 2 ? -3 : 3, 1.5, 12, i % 2 ? 3 : 4)))]);
  v.push(['tagli 96 dB/oct', state([band(0, 3, 30, 0, .71, 96), band(1, 4, 18000, 0, .71, 96), band(2, 6, 1000, 0, 1, 96)])]);
  v.push(['32 bande, 16 dinamiche (range 1.5)', state(Array.from({ length: 32 }, (_, i) => band(i, i % 2 ? 0 : 2, 30 * Math.pow(1.22, i), 3, 1.5, 12, 0, i % 2 ? null : { on: true, thr: -30, range: -1.5, att: 5, rel: 80, knee: 6, det: 0 })))]);
  v.push(['4 dinamiche in movimento (range 12, att 1 ms)', state(Array.from({ length: 4 }, (_, i) => band(i, 0, 150 * Math.pow(3, i), 0, 1, 12, 0, { on: true, thr: -42, range: -12, att: 1, rel: 30, knee: 6, det: 0 })))]);
  v.push(['16 dinamiche in movimento (range 12, att 1 ms)', state(Array.from({ length: 16 }, (_, i) => band(i, 0, 40 * Math.pow(1.5, i), 0, 1, 12, 0, { on: true, thr: -45, range: -12, att: 1, rel: 30, knee: 6, det: 0 })))]);
  v.push(['Character Subtle', state([band(0, 0, 1000, 3, 1)], { character: 1 })]);
  v.push(['Character Warm', state([band(0, 0, 1000, 3, 1)], { character: 2 })]);
  return v;
}
function runCpu(st, fsr, bl, seconds, reps) {
  const n = Math.floor(fsr * seconds), times = [];
  for (let r = 0; r < reps; r++) {
    const L = new Float32Array(n), R = new Float32Array(n); for (let i = 0; i < n; i++) { L[i] = CL[i % NC]; R[i] = CR[i % NC]; }
    const e = new T.Engine(fsr, 4096); e.setState(st);
    const t0 = process.hrtime.bigint();
    for (let i0 = 0; i0 < n; i0 += bl) { const m = Math.min(bl, n - i0); e.process(L.subarray(i0, i0 + m), R.subarray(i0, i0 + m), m); }   // come nel browser: setState solo quando cambia qualcosa
    times.push(Number(process.hrtime.bigint() - t0) / 1e9);
  }
  times.sort((a, b) => a - b); return times[Math.floor(reps / 2)] / seconds * 100;
}
if (sec === 'cpu') {
  const out = [];
  for (const [name, st] of scenarios()) { const p = runCpu(st, 48000, 128, 10, 3); out.push({ scen: name, fs: 48000, block: 128, core_pct: +p.toFixed(4) }); console.error(name, p.toFixed(3)); }
  console.log(JSON.stringify(out, null, 1));
} else if (sec === 'cpu1') {   // uno scenario per processo (JIT pulito), 5 ripetizioni, mediana: node lab.js <core> cpu1 <indice>
  const [name, st] = scenarios()[+process.argv[4]]; const p = runCpu(st, 48000, 128, 10, 5); console.log(JSON.stringify({ scen: name, fs: 48000, block: 128, core_pct: +p.toFixed(4) }));
} else if (sec === 'setstate') {   // costo di setState con 32 bande (interfaccia: una chiamata per fotogramma durante un trascinamento)
  const T8 = [0, 1, 2, 0, 5, 0, 7, 0]; const e = new T.Engine(48000, 4096);
  const mkst = k => state(Array.from({ length: 32 }, (_, i) => band(i, T8[i % 8], 30 * Math.pow(1.22, i) * (i === 0 ? 1 + 0.001 * k : 1), i % 2 ? -3 : 3, 1.5)));
  let k = 0; const t0 = process.hrtime.bigint(); for (; k < 2000; k++) e.setState(mkst(k)); const us = Number(process.hrtime.bigint() - t0) / 1e3 / k;
  console.log(JSON.stringify({ setstate_32_bands_us: +us.toFixed(1), note: 'una banda cambia, le altre 31 identiche' }));
} else if (sec === 'nan') {
  const res = [];
  for (const [k, bad] of [['NaN', NaN], ['+Inf', Infinity], ['1e30', 1e30]]) {
    const n = 48000 * 4, L = new Float32Array(CL.subarray(48000 * 40, 48000 * 40 + n)), R = new Float32Array(CR.subarray(48000 * 40, 48000 * 40 + n)); L[48000] = bad; R[48000] = bad;
    const e = new T.Engine(48000, 4096); const st = state([band(0, 0, 1000, 4, 1), band(1, 3, 40, 0, .71, 24)]); e.setState(st);
    for (let i0 = 0; i0 < n; i0 += 128) { e.setState(st); e.process(L.subarray(i0, i0 + 128), R.subarray(i0, i0 + 128), 128); }
    let badN = 0, last = -1; for (let i = 0; i < n; i++) if (!isFinite(L[i]) || Math.abs(L[i]) > 1e6) { badN++; last = i; }
    res.push({ input: k + ' al campione 48000', bad_samples: badN, last_bad: last, recovered: last < 96000 });
  }
  console.log(JSON.stringify(res));
} else if (sec === 'render') {   // render della clip con uno stato JSON: node lab.js dsp.js render stato.json out.f32 [start_s dur_s]
  const st = JSON.parse(fs.readFileSync(process.argv[4], 'utf8')), s0 = Math.floor(48000 * (+process.argv[6] || 0)), n = Math.floor(48000 * (+process.argv[7] || NC / 48000));
  const L = new Float32Array(CL.subarray(s0, s0 + n)), R = new Float32Array(CR.subarray(s0, s0 + n)); const e = new T.Engine(48000, 4096); e.setState(st);
  for (let i0 = 0; i0 < L.length; i0 += 128) { const m = Math.min(128, L.length - i0); e.setState(st); e.process(L.subarray(i0, i0 + m), R.subarray(i0, i0 + m), m); }
  const o = new Float32Array(L.length * 2); for (let i = 0; i < L.length; i++) { o[2 * i] = L[i]; o[2 * i + 1] = R[i]; } fs.writeFileSync(process.argv[5], Buffer.from(o.buffer));
  console.log(JSON.stringify({ samples: L.length }));
}
