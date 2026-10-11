# prototipo 0.4.4 = prototipo 0.4.3 con il nuovo DSP core (stesso testo che gira nell'AudioWorklet e che disegna le curve)
import sys, hashlib
src, core, dst = sys.argv[1], sys.argv[2], sys.argv[3]
s = open(src, encoding='utf-8').read(); c = open(core, encoding='utf-8').read()
A = '/* ===== TSR Q · DSP CORE'; B = '/* ===== /DSP CORE ===== */'
assert s.count(A) == 1 and s.count(B) == 1, 'marcatori del core non trovati una sola volta'
i, j = s.index(A), s.index(B)
assert c.startswith(A), 'il core non inizia con il marcatore'
s2 = s[:i] + c.rstrip() + '\n' + s[j:]

# interfaccia: progetti delle bande riusati finche lo stato non cambia (il fantasma del clic-per-creare chiamava curveDb su 221 punti a ogni movimento del mouse,
# e ogni chiamata riprogettava TUTTE le bande: con il progetto preciso della 0.4.4 sarebbe diventato lento). Risultato identico al bit (stessi progetti, stesso ordine).
OLD = "const curveDb = f => { const fs = fsNow(); let d = 0; P.bands.forEach((b, i) => { if (!b.used || b.byp) return; const c = TSRQ.designBand(engineBand(i), hasGain(b.type) ? b.gain * P.scale / 100 : 0, fs); d += 10 * Math.log10(Math.max(TSRQ.digMag2(c, f, fs), 1e-12)); }); return d; };"
NEW = ("let dMemo = { k: null, cs: null };\n"
       "  const designsNow = () => { const fs = fsNow(), sc = P.scale / 100, parts = [fs, sc, P.gq ? 1 : 0]; P.bands.forEach((b, i) => { if (b.used && !b.byp) parts.push(i, b.type, b.freq, b.gain, b.q, b.slope, b.place); }); const k = parts.join('|'); if (dMemo.k === k) return dMemo.cs;"
       " const cs = []; P.bands.forEach((b, i) => { if (!b.used || b.byp) return; cs.push(TSRQ.designBand(engineBand(i), hasGain(b.type) ? b.gain * P.scale / 100 : 0, fs)); }); dMemo = { k, cs }; return cs; };   /* progetti riusati finche lo stato non cambia */\n"
       "  const curveDb = f => { const fs = fsNow(), cs = designsNow(); let d = 0; for (const c of cs) d += 10 * Math.log10(Math.max(TSRQ.digMag2(c, f, fs), 1e-12)); return d; };")
assert s2.count(OLD) == 1, 'curveDb non trovata una sola volta'
s2 = s2.replace(OLD, NEW)
open(dst, 'w', encoding='utf-8').write(s2)
print('ok', dst, len(s2), hashlib.sha256(s2.encode()).hexdigest()[:16])
