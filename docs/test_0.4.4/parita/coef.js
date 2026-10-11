// confronto coefficienti JS vs C++ (stesse righe di ingresso)
const fs = require('fs'), cp = require('child_process');
const T = require(process.argv[2]); const TY = ['bell','lshelf','hshelf','lcut','hcut','notch','bpass','tilt','flat','allpass'], GT = { bell: 1, lshelf: 1, hshelf: 1, tilt: 1, flat: 1 };
let s = 987654321; const rnd = () => (s = (s * 1103515245 + 12345) % 2147483648) / 2147483648;
const FSs = [44100, 48000, 88200, 96000, 192000], SL = [6, 12, 18, 24, 30, 36, 48, 60, 72, 84, 96];
const lines = [], spec = []; const N = +process.argv[3] || 3000;
for (let i = 0; i < N; i++) {
  const fsr = FSs[Math.floor(rnd() * 5)], t = Math.floor(rnd() * 10), f = +(10 * Math.pow(2200, rnd())).toFixed(3), g = +(-30 + 60 * rnd()).toFixed(2), q = +(0.1 * Math.pow(400, rnd())).toFixed(4), sl = SL[Math.floor(rnd() * SL.length)], gq = rnd() < 0.2 ? 1 : 0, pr = rnd() < 0.7 ? 1 : 0;
  lines.push([fsr, t, f, g, q, sl, gq, pr].join(' ')); spec.push({ fsr, t, f, g, q, sl, gq, pr });
}
const out = cp.execFileSync(process.argv[4] || __dirname + '/coef_new', { input: lines.join('\n') + '\n', maxBuffer: 1 << 28 }).toString().trim().split('\n');
let worstRel = 0, worstDb = 0, nExact = 0, bad = [];
for (let i = 0; i < N; i++) {
  const { fsr, t, f, g, q, sl, gq, pr } = spec[i]; const b = { type: TY[t], f, q, slope: sl, gq: !!gq };
  const cj = T.designBand(b, GT[TY[t]] ? g : 0, fsr, !!pr), cc = out[i].split(' ').map(Number); const n = cc[0];
  if (cj.length !== 5 * n) { bad.push([i, 'n', cj.length / 5, n]); continue; }
  let rel = 0; for (let k = 0; k < 5 * n; k++) { const a = cj[k], c = cc[1 + k]; const d = Math.abs(a - c) / Math.max(1e-300, Math.abs(c), 1e-12); if (d > rel) rel = d; }
  if (rel === 0) nExact++;
  // differenza di risposta in dB su 200 punti
  let dd = 0; const cv = Float64Array.from(cc.slice(1));
  for (let k = 0; k < 200; k++) { const fr = 20 * Math.pow(fsr / 2 * 0.999 / 20, k / 199), mj = T.digMag2(cj, fr, fsr), mc = T.digMag2(cv, fr, fsr); if (mc > 1e-10) dd = Math.max(dd, Math.abs(10 * Math.log10(mj / mc))); }
  if (rel > worstRel) worstRel = rel; if (dd > worstDb) worstDb = dd; if (dd > 1e-6) bad.push([i, lines[i], rel.toExponential(2), dd.toExponential(2)]);
}
console.log(JSON.stringify({ cases: N, bit_exact: nExact, worst_rel_coef: worstRel, worst_resp_db: worstDb, over_1e6_db: bad.length }));
for (const b of bad.slice(0, 15)) console.log(JSON.stringify(b));
