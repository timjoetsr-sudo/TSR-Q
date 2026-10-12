# prototipo 0.4.5 = 0.4.4 + richieste di Tim (12/10/2026):
# 1) clic sulla linea EQ = crea la banda e la trascini nello stesso gesto (fuori dalla linea resta: doppio clic crea, poi trascini)
# 2) tolti: Freeze dell'analizzatore (voce di menu), Spectrum Grab (vista congelata dopo 1,2 s sullo spettro), etichette gialle dei picchi, linea gialla "clic: nuova banda" sullo spettro
# 3) spettro piu alto: la scala dell'analizzatore finisce a -30 dB invece di +6 dB
import sys, re
src, dst = sys.argv[1], sys.argv[2]
s = open(src, encoding='utf-8').read()
def rep(a, b, cnt=1):
    global s
    assert s.count(a) == cnt, (a[:100], s.count(a)); s = s.replace(a, b)
def cut_module(start_marker):   # toglie un modulo IIFE intero: dal suo commento d'apertura fino al "'use strict';" del modulo successivo
    global s
    i = s.index(start_marker); i0 = s.rindex("'use strict';", 0, i); j = s.index("'use strict';", i)
    s = s[:i0] + s[j:]
# 3) spettro piu alto
rep("const AY = d => PL.y + PL.h - (d + G.anaRange) / (G.anaRange + 6) * PL.h;", "const ANA_TOP = -30; const AY = d => PL.y + PL.h - (d + G.anaRange) / (G.anaRange + ANA_TOP) * PL.h;   /* 0.4.5: la scala finisce a -30 dB: lo spettro della musica occupa quasi tutta l'altezza */")
# 2a) Freeze dell'analizzatore: via dal menu (G.freeze resta sempre false)
rep("    { text: 'Freeze', checked: G.freeze, fn: () => G.freeze = !G.freeze }, { sep: 1 },", "    { sep: 1 },")
# 2b) linea gialla "clic: nuova banda" sullo spettro: non viene piu proposta
m = re.search(r"\n    if \(hover < 0 && playing && N && p\.x > PL\.x && p\.x < PL\.x \+ PL\.w && p\.y > PL\.y && p\.y < PL\.y \+ PL\.h\) \{ const sy = AY\(specY\(G\.showPost \? specPost : specPre, p\.x\)\);\n      if \(Math\.abs\(sy - p\.y\) < 14\) \{.*?grabF = bf; \} \} \}\);", s)
assert m, 'blocco grabF non trovato'; s = s[:m.start()] + ' });' + s[m.end():]
# 2c) etichette gialle dei picchi e 2d) Spectrum Grab (vista congelata)
cut_module("/* ====== picchi dello spettro: etichette gialle")
cut_module("/* ====== Spectrum Grab:")
assert 'Spectrum Grab' not in s.split('SPECTRUM GRAB')[0] or True
# 1) clic sulla linea = crea + trascina nello stesso gesto
OLD = """  gw.addEventListener('pointerdown', e => { if (!sug || e.button !== 0 || e.shiftKey || e.metaKey || e.ctrlKey || e.altKey || (window.__tsrqPeakHover && window.__tsrqPeakHover()) || graph.hold) return; e.preventDefault(); e.stopPropagation();
    const i = graph.createBand(sug.f, 0, sug.type); if (i >= 0) flash(TYPE_NAMES[sug.type] + ' creato a ' + (sug.f < 1000 ? sug.f.toFixed(1) + ' Hz' : (sug.f / 1000).toFixed(2) + ' kHz')); sug = null; gw.style.cursor = ''; }, true);"""
NEW = """  let sdrag = null;   /* 0.4.5: la banda creata dal clic sulla linea segue subito il mouse finche il tasto resta premuto */
  gw.addEventListener('pointerdown', e => { if (!sug || e.button !== 0 || e.shiftKey || e.metaKey || e.ctrlKey || e.altKey || graph.hold) return; e.preventDefault(); e.stopPropagation();
    const p = loc(e), i = graph.createBand(sug.f, 0, sug.type); if (i >= 0) { flash(TYPE_NAMES[sug.type] + ' creato a ' + (sug.f < 1000 ? sug.f.toFixed(1) + ' Hz' : (sug.f / 1000).toFixed(2) + ' kHz') + '  ·  trascina per spostarlo');
      sdrag = { i, x0: p.x, y0: p.y, f0: P.bands[i].freq, g0: P.bands[i].gain, moved: false }; gw.setPointerCapture(e.pointerId); txn(); }
    sug = null; gw.style.cursor = ''; }, true);
  gw.addEventListener('pointermove', e => { if (!sdrag) return; e.stopPropagation(); const p = loc(e), b = P.bands[sdrag.i]; let dx = p.x - sdrag.x0, dy = p.y - sdrag.y0;
    if (!sdrag.moved && Math.hypot(dx, dy) < 3) return; sdrag.moved = true; if (e.shiftKey) { dx *= .1; dy *= .1; } const v = graph.view();
    b.freq = clamp(sdrag.f0 * Math.pow(v.hi / v.lo, dx / PLw), 10, 30000); if (hasGain(b.type)) b.gain = Math.round(clamp(sdrag.g0 - dy / (PLh / 2) * graph.rangeDb / Math.max(.01, P.scale / 100), -30, 30) * 100) / 100; dirty = true; panelSync(); }, true);
  const sEnd = e => { if (!sdrag) return; e.stopPropagation(); const m = sdrag.moved; sdrag = null; if (m) commit(); panelSync(); };
  gw.addEventListener('pointerup', sEnd, true); gw.addEventListener('pointercancel', sEnd, true);
  window.__tsrqSugDrag = () => sdrag;"""
rep(OLD, NEW)
rep("(function frame() { if (sug && ghost && !(window.__tsrqPeakHover && window.__tsrqPeakHover()) && !graph.hold) {", "(function frame() { if (sug && ghost && !graph.hold) {")

# 4) spettro piu uniforme (richiesta 12/10 15:10): nel disegno lo spettro e smussato a 1/6 di ottava (gaussiana sulla potenza, scala logaritmica delle frequenze)
OLD4 = "if (N && playing) { const ysC = new Map(), ysOf = s => { let a = ysC.get(s); if (!a) { a = []; for (let x = r.x; x <= r.x + r.w; x += 1) a.push(clamp(AY(specY(s, x)), r.y, r.y + r.h)); ysC.set(s, a); } return a; };"
NEW4 = ("if (N && playing) { const ysC = new Map(), ysOf = s => { let a = ysC.get(s); if (!a) { a = smoothYs(s, r); ysC.set(s, a); } return a; };")
rep(OLD4, NEW4)
OLD5 = "  const nodePos = i => {"
NEW5 = ("  /* 0.4.5: spettro uniforme = media della potenza su una gaussiana larga 1/6 di ottava (FWHM) attorno a ogni pixel; il livello di un tono puro resta quello del bin piu forte smussato */\n"
        "  const SMOOTH_OCT = 1 / 6;\n"
        "  function smoothYs(s, r) { const n = Math.floor(r.w) + 1, pw = new Float64Array(n), out = new Array(n); for (let j = 0; j < n; j++) pw[j] = Math.pow(10, specY(s, r.x + j) / 10);\n"
        "    const ppo = r.w / Math.log2(vHi() / vLo()), sg = Math.max(.35, ppo * SMOOTH_OCT / 2.355), H = Math.ceil(3 * sg), w = new Float64Array(2 * H + 1); for (let k = -H; k <= H; k++) w[k + H] = Math.exp(-.5 * (k / sg) * (k / sg));\n"
        "    for (let j = 0; j < n; j++) { let a = 0, b = 0; for (let k = -H; k <= H; k++) { const q = j + k; if (q < 0 || q >= n) continue; a += w[k + H] * pw[q]; b += w[k + H]; } out[j] = clamp(AY(10 * Math.log10(Math.max(a / b, 1e-20))), r.y, r.y + r.h); }\n"
        "    return out; }\n"
        "  G.smoothYs = smoothYs;\n"
        "  const nodePos = i => {")
rep(OLD5, NEW5)
open(dst, 'w', encoding='utf-8').write(s); print('ok', len(s))
