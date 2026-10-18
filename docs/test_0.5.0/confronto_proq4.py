#!/usr/bin/env python3
# TSR Q contro FabFilter Pro-Q 4: misure reali, stessi segnali, stessi host (pedalboard), sul Mac di Tim.
# Uso:  python3 confronto_proq4.py            (cerca i due plugin nelle cartelle standard)
#       python3 confronto_proq4.py --tsrq PERCORSO.vst3 --proq PERCORSO.vst3
# Scrive confronto_proq4.csv e confronto_proq4_parametri.txt accanto allo script. Nessun valore stimato:
# se un parametro di Pro-Q 4 non si trova, lo script si ferma e lo dice.
import argparse, os, re, sys, time
try:
    import numpy as np, pedalboard as pb
except ImportError:
    sys.exit("Mancano numpy/pedalboard: python3 -m pip install --user numpy pedalboard")

ap = argparse.ArgumentParser()
ap.add_argument("--tsrq", default="/Library/Audio/Plug-Ins/VST3/TSR Q.vst3")
ap.add_argument("--proq", default="/Library/Audio/Plug-Ins/VST3/FabFilter Pro-Q 4.vst3")
a = ap.parse_args()
HERE = os.path.dirname(os.path.abspath(__file__))
for p in (a.tsrq, a.proq):
    if not os.path.exists(p): sys.exit(f"ERRORE: plugin non trovato: {p}")

# ---------- mappa dei parametri ----------
def keys(p): return list(p.parameters.keys())
def find(p, band, words, what):
    ks = [k for k in keys(p) if re.search(rf"(^|_)band_?{band}(_|$)", k) and all(w in k for w in words)]
    if len(ks) != 1: sys.exit(f"ERRORE: parametro '{what}' della banda {band} non univoco in {p.name}: {ks or 'nessuno'} (vedi confronto_proq4_parametri.txt)")
    return ks[0]
proq0 = pb.load_plugin(a.proq)
with open(os.path.join(HERE, "confronto_proq4_parametri.txt"), "w") as f:
    for k, v in proq0.parameters.items(): f.write(f"{k}\t{v}\n")
MAP = {
    "tsrq": dict(used=lambda b: f"band_{b}_used", type=lambda b: f"band_{b}_type", freq=lambda b: f"band_{b}_freq_hz", gain=lambda b: f"band_{b}_gain_db", q=lambda b: f"band_{b}_q", slope=lambda b: f"band_{b}_slope"),
    "proq": dict(used=lambda b: find(proq0, b, ["used"], "used"), type=lambda b: find(proq0, b, ["shape"], "shape"), freq=lambda b: find(proq0, b, ["freq"], "frequency"),
                 gain=lambda b: find(proq0, b, ["gain"], "gain"), q=lambda b: find(proq0, b, ["q"], "q"), slope=lambda b: find(proq0, b, ["slope"], "slope")),
}
for k in ("used", "type", "freq", "gain", "q", "slope"): MAP["proq"][k](1)   # verifica subito: si ferma qui se manca qualcosa
TYPES = {"tsrq": {"Bell": "Bell", "Low Shelf": "Low Shelf", "High Shelf": "High Shelf", "Low Cut": "Low Cut", "High Cut": "High Cut", "Notch": "Notch", "Band Pass": "Band Pass"},
         "proq": {"Bell": "Bell", "Low Shelf": "Low Shelf", "High Shelf": "High Shelf", "Low Cut": "Low Cut", "High Cut": "High Cut", "Notch": "Notch", "Band Pass": "Band Pass"}}

def load(which, bands, fs_note=""):
    p = pb.load_plugin(a.tsrq if which == "tsrq" else a.proq); m = MAP[which]
    for i, b in enumerate(bands, 1):
        setattr(p, m["used"](i), True); setattr(p, m["type"](i), TYPES[which][b["t"]]); setattr(p, m["freq"](i), float(b["f"]))
        if "g" in b: setattr(p, m["gain"](i), float(b["g"]))
        if "q" in b: setattr(p, m["q"](i), float(b["q"]))
        if "s" in b: setattr(p, m["slope"](i), f"{b['s']} dB/oct")
    return p

def ir(p, fs, n=1 << 16):
    x = np.zeros((2, n), np.float32); x[:, 256] = 1.0; y = p(x, fs); return y[0]
def mag_db(h, fs, freqs):
    H = np.fft.rfft(h); fr = np.fft.rfftfreq(len(h), 1 / fs); return 20 * np.log10(np.abs(np.interp(freqs, fr, np.abs(H))) + 1e-20)

rows = []
def out(sez, caso, metrica, t, q, unita):
    rows.append((sez, caso, metrica, t, q, unita)); print(f"{sez:12s} {caso:46s} {metrica:34s} TSR Q {t:10.3f}   Pro-Q 4 {q:10.3f}  {unita}"); sys.stdout.flush()

# ---------- 1. cramping vicino a Nyquist: risposta a 48 kHz contro la stessa a 192 kHz (riferimento senza Nyquist vicino) ----------
F = np.geomspace(20, 20000, 400)
CASES = [("Bell 10 kHz +12 Q2", [dict(t="Bell", f=10000, g=12, q=2)]), ("Bell 16 kHz -9 Q4", [dict(t="Bell", f=16000, g=-9, q=4)]),
         ("High Shelf 8 kHz +6", [dict(t="High Shelf", f=8000, g=6, q=0.71)]), ("High Cut 15 kHz 24 dB", [dict(t="High Cut", f=15000, q=0.71, s=24)]),
         ("Low Cut 30 Hz 48 dB", [dict(t="Low Cut", f=30, q=0.71, s=48)]), ("Bell 1 kHz +6 Q1", [dict(t="Bell", f=1000, g=6, q=1)])]
for nome, bands in CASES:
    r = []
    for w in ("tsrq", "proq"):
        h48 = mag_db(ir(load(w, bands), 48000), 48000, F); h192 = mag_db(ir(load(w, bands), 192000, 1 << 18), 192000, F)
        sel = h192 > -30; r.append((float(np.max(np.abs(h48 - h192)[sel])), h48, h192))
    out("cramping", nome, "max |48k - 192k| sopra -30 dB", r[0][0], r[1][0], "dB")
    sel = r[1][2] > -30
    out("forma", nome, "max |TSR Q - Pro-Q| a 192 kHz", float(np.max(np.abs(r[0][2] - r[1][2])[sel])), 0.0, "dB (definizioni di Q possono differire)")

# ---------- 2. latenza ----------
for w in ("tsrq", "proq"): pass
pt, pq = load("tsrq", CASES[0][1]), load("proq", CASES[0][1])
out("latenza", "riportata all'host", "campioni", pt.reported_latency_samples, pq.reported_latency_samples, "campioni")
out("latenza", "picco dell'impulso (Bell 1 kHz)", "campioni dopo l'impulso", float(np.argmax(np.abs(ir(load("tsrq", CASES[5][1]), 48000))) - 256), float(np.argmax(np.abs(ir(load("proq", CASES[5][1]), 48000))) - 256), "campioni")

# ---------- 3. movimento: salto di frequenza a regime su un tono, a blocchi da 64 (picco nei 30 ms dopo il salto oltre il regime) ----------
def jump(w, fs, b0, f1, tf):
    p = load(w, [b0]); n = int(fs * 1.5); t = np.arange(n) / fs; x = np.tile((0.25 * np.sin(2 * np.pi * tf * t)).astype(np.float32), (2, 1))
    y1 = p.process(x[:, :fs], fs, buffer_size=64, reset=False); setattr(p, MAP[w]["freq"](1), float(f1)); y2 = p.process(x[:, fs:], fs, buffer_size=64, reset=False)
    reg = np.abs(y1[0, fs // 2:]).max(); fin = np.abs(y2[0, -fs // 4:]).max(); pk = np.abs(y2[0, :int(0.03 * fs)]).max()
    return 20 * np.log10(pk / max(reg, fin))
for fs in (48000, 192000):
    for nome, b0, f1, tf in (("High Cut 48 dB 20 kHz -> 200 Hz, tono 100 Hz", dict(t="High Cut", f=20000, q=0.71, s=48), 200, 100),
                             ("Low Cut 48 dB 30 -> 300 Hz, tono 2 kHz", dict(t="Low Cut", f=30, q=0.71, s=48), 300, 2000),
                             ("High Shelf +12 Q2 1 -> 15 kHz, tono 6 kHz", dict(t="High Shelf", f=1000, g=12, q=2), 15000, 6000)):
        out("movimento", f"{nome} @{fs // 1000}k", "picco oltre i regimi", jump("tsrq", fs, b0, f1, tf), jump("proq", fs, b0, f1, tf), "dB")

# ---------- 4. CPU: 10 s stereo, 8 bande, blocco 512 a 48 kHz (miglior tempo su 5) ----------
EQ8 = [dict(t="Low Cut", f=30, q=0.71, s=24), dict(t="Bell", f=250, g=-3, q=1), dict(t="Bell", f=3000, g=2, q=1), dict(t="High Shelf", f=10000, g=2.5, q=0.71),
       dict(t="Bell", f=80, g=2, q=0.8), dict(t="Bell", f=1200, g=-1.5, q=2), dict(t="Low Shelf", f=120, g=1, q=0.71), dict(t="High Cut", f=18000, q=0.71, s=12)]
x = (np.random.default_rng(1).uniform(-.3, .3, (2, 480000))).astype(np.float32); res = []
for w in ("tsrq", "proq"):
    p = load(w, EQ8); best = 1e9
    for _ in range(5): t0 = time.perf_counter(); p(x, 48000, buffer_size=512); best = min(best, time.perf_counter() - t0)
    res.append(best / (480000 / 512) * 1e6)
out("cpu", "8 bande, blocco 512 @48k", "us per blocco (miglior tempo)", res[0], res[1], "us")

with open(os.path.join(HERE, "confronto_proq4.csv"), "w") as f:
    f.write("sezione,caso,metrica,tsrq,proq4,unita\n"); [f.write(",".join(str(v) for v in r) + "\n") for r in rows]
print("\nScritto confronto_proq4.csv")
