# Test definitivo sul plugin VERO (VST3 compilato) ospitato da pedalboard, confrontato con:
#  - il prototipo analogico ideale (riferimento dei filtri "analog matched", lo stesso obiettivo di Pro-Q in modalita Zero Latency)
#  - il filtro EQ di JUCE (pedalboard PeakFilter/LowShelf/HighShelf/HighpassFilter/LowpassFilter: progetto standard RBJ bilineare)
#  - la formula RBJ "Audio EQ Cookbook" calcolata qui (riferimento usato da gran parte degli EQ digitali)
import numpy as np, pedalboard as pb, time, sys
VST = '/home/claude/TSRQ/build/TSRQ_artefacts/Release/VST3/TSR Q.vst3'
res = []
def ok(c, m): res.append(bool(c)); print(('PASS ' if c else 'FAIL ') + m); sys.stdout.flush()
TYPES = ['Bell', 'Low Shelf', 'High Shelf', 'Low Cut', 'High Cut']
SL = {6: '6 dB/oct', 12: '12 dB/oct', 24: '24 dB/oct', 48: '48 dB/oct'}
def tsrq(fs, bands, **glob):
    p = pb.load_plugin(VST)
    for i, b in enumerate(bands, 1):
        setattr(p, f'band_{i}_used', True); setattr(p, f'band_{i}_type', b['t']); setattr(p, f'band_{i}_freq_hz', b['f'])
        if 'g' in b: setattr(p, f'band_{i}_gain_db', b['g'])
        if 'q' in b: setattr(p, f'band_{i}_q', b['q'])
        if 's' in b: setattr(p, f'band_{i}_slope', SL[b['s']])
    for k, v in glob.items(): setattr(p, k, v)
    return p
def ir(proc, fs, n=1 << 15):
    proc.reset() if hasattr(proc, 'reset') else None
    proc(np.zeros((2, fs), np.float32), fs, reset=False)   # assestamento (rampe dei parametri)
    x = np.zeros((2, n), np.float32); x[:, 0] = 1
    return proc(x, fs, reset=False)[0].astype(np.float64)
def mag(h, fs, f): H = np.fft.rfft(h); fr = np.fft.rfftfreq(len(h), 1 / fs); return 20 * np.log10(np.maximum(np.interp(f, fr, np.abs(H)), 1e-12))
def ana(b, f):
    s = 1j * f / b['f']; t = b['t']
    if t == 'Bell': A = 10 ** (b['g'] / 40); Q = b['q']; H = (s * s + s * A / Q + 1) / (s * s + s / (A * Q) + 1)
    elif t == 'Low Shelf': A = 10 ** (b['g'] / 40); Q = b['q']; H = A * (s * s + np.sqrt(A) / Q * s + A) / (A * s * s + np.sqrt(A) / Q * s + 1)
    elif t == 'High Shelf': A = 10 ** (b['g'] / 40); Q = b['q']; H = A * (A * s * s + np.sqrt(A) / Q * s + 1) / (s * s + np.sqrt(A) / Q * s + A)
    else:
        N = b['s'] // 6; ss = s if t == 'Low Cut' else 1 / s; H = np.ones_like(s)
        poles = [np.exp(1j * np.pi * (2 * k + N + 1) / (2 * N)) for k in range(N)]
        for p_ in poles: H = H * (ss / (ss - p_))
    return 20 * np.log10(np.abs(H))
def juce(b, fs):
    t = b['t']
    if t == 'Bell': return pb.PeakFilter(cutoff_frequency_hz=b['f'], gain_db=b['g'], q=b['q'])
    if t == 'Low Shelf': return pb.LowShelfFilter(cutoff_frequency_hz=b['f'], gain_db=b['g'], q=b['q'])
    if t == 'High Shelf': return pb.HighShelfFilter(cutoff_frequency_hz=b['f'], gain_db=b['g'], q=b['q'])
    if t == 'Low Cut' and b['s'] == 12: return pb.HighpassFilter(cutoff_frequency_hz=b['f'])
    if t == 'High Cut' and b['s'] == 12: return pb.LowpassFilter(cutoff_frequency_hz=b['f'])
    return None
def rbj(b, fs, f):
    if b['t'] != 'Bell': return None
    A = 10 ** (b['g'] / 40); w0 = 2 * np.pi * b['f'] / fs; al = np.sin(w0) / (2 * b['q'])
    B = [1 + al * A, -2 * np.cos(w0), 1 - al * A]; Aa = [1 + al / A, -2 * np.cos(w0), 1 - al / A]
    z = np.exp(-1j * 2 * np.pi * f / fs); return 20 * np.log10(np.abs((B[0] + B[1] * z + B[2] * z * z) / (Aa[0] + Aa[1] * z + Aa[2] * z * z)))
F = np.geomspace(20, 20000, 400)
cases = [{'t': 'Bell', 'f': 1000, 'g': 12, 'q': 2}, {'t': 'Bell', 'f': 5000, 'g': 9, 'q': 1}, {'t': 'Bell', 'f': 10000, 'g': 12, 'q': 2}, {'t': 'Bell', 'f': 15000, 'g': -12, 'q': 4},
         {'t': 'Bell', 'f': 16000, 'g': 6, 'q': 0.7}, {'t': 'Low Shelf', 'f': 100, 'g': 6, 'q': 0.71}, {'t': 'High Shelf', 'f': 8000, 'g': 6, 'q': 0.71}, {'t': 'High Shelf', 'f': 15000, 'g': -9, 'q': 0.71},
         {'t': 'Low Cut', 'f': 50, 's': 24}, {'t': 'High Cut', 'f': 12000, 's': 24}, {'t': 'High Cut', 'f': 16000, 's': 12}, {'t': 'Low Cut', 'f': 30, 's': 12}]
print('== 1. RISPOSTA IN FREQUENZA misurata sul plugin vero: errore massimo rispetto al filtro analogico ideale (20 Hz-20 kHz, sopra -30 dB) ==')
print('%-34s %8s | %10s %10s %10s' % ('banda', 'fs', 'TSR Q', 'JUCE EQ', 'RBJ'))
worst = {44100: 0, 48000: 0, 96000: 0}; worstJ = {44100: 0, 48000: 0, 96000: 0}; better = 0; tot = 0
for fs in (44100, 48000, 96000):
    for b in cases:
        a = ana(b, F); m = a > -30
        t = mag(ir(tsrq(fs, [b]), fs), fs, F); et = np.max(np.abs(t - a)[m])
        j = juce(b, fs); ej = np.max(np.abs(mag(ir(pb.Pedalboard([j]), fs), fs, F) - a)[m]) if j else None
        r = rbj(b, fs, F); er = np.max(np.abs(r - a)[m]) if r is not None else None
        worst[fs] = max(worst[fs], et)
        if ej is not None: worstJ[fs] = max(worstJ[fs], ej); tot += 1; better += et <= ej + 0.05
        print('%-34s %8d | %8.3f dB %10s %10s' % (f"{b['t']} {b['f']} Hz " + (f"{b['g']:+} dB Q{b['q']}" if 'g' in b else f"{b['s']} dB/oct"), fs, et, f'{ej:.3f} dB' if ej is not None else '—', f'{er:.3f} dB' if er is not None else '—'))
for fs in worst: ok(worst[fs] < 1.0, f'TSR Q @{fs}: errore massimo {worst[fs]:.3f} dB (< 1 dB) | JUCE EQ {worstJ[fs]:.2f} dB')
ok(better == tot, f'TSR Q piu vicino all analogico (o uguale) del JUCE EQ in {better} casi su {tot}')
print('== 2. LATENZA, NULL TEST, BYPASS sul plugin vero ==')
fs = 48000; h = ir(tsrq(fs, [{'t': 'Bell', 'f': 1000, 'g': 6, 'q': 1}]), fs); ok(int(np.argmax(np.abs(h))) <= 2, f'latenza: picco della risposta all impulso al campione {int(np.argmax(np.abs(h)))} (0 = nessuna latenza)')
p = pb.load_plugin(VST); rp = p.reported_latency_samples if hasattr(p, 'reported_latency_samples') else 0; ok(rp == 0, f'latenza dichiarata all host: {rp} campioni')
rng = np.random.default_rng(1); x = (rng.standard_normal((2, fs * 2)) * 0.25).astype(np.float32)
p = tsrq(fs, [{'t': 'Bell', 'f': f, 'g': 0, 'q': 1} for f in (100, 1000, 5000)]); y = p(x, fs)
d = np.max(np.abs(y - x)); ok(d < 1e-6, f'null test: 3 bande a 0 dB, residuo {20*np.log10(max(d,1e-30)):.1f} dBFS')
p = tsrq(fs, [{'t': 'Bell', 'f': 1000, 'g': 12, 'q': 2}], bypass=True); p(np.zeros((2, fs), np.float32), fs, reset=False); y = p(x, fs, reset=False); d = np.max(np.abs(y - x)); ok(d < 1e-6, f'bypass: uscita = ingresso, residuo {20*np.log10(max(d,1e-30)):.1f} dBFS')
print('== 3. AUTOMAZIONE: click misurati come energia sopra 4 kHz (il segnale e una sinusoide a 440 Hz: tutto cio che sta sopra 4 kHz e prodotto dal cambio di parametri) ==')
from scipy.signal import lfilter, butter, sosfilt
HP = butter(10, 4000, 'highpass', fs=48000, output='sos')
def clickdb(o, x):   # picco (finestre da 64 campioni) dell'energia sopra 4 kHz, in dB rispetto al segnale
    r = sosfilt(HP, o)[4800:]; fr = r[:len(r) // 64 * 64].reshape(-1, 64); return 20 * np.log10(np.sqrt((fr ** 2).mean(1)).max() / np.sqrt(np.mean(x ** 2)) + 1e-30)
def run_tsrq(setter, f0=440, fs=48000, blk=256):
    p = tsrq(fs, [{'t': 'Bell', 'f': 100, 'g': 0, 'q': 2}]); setter(p, 0.0); p(np.zeros((2, 4800), np.float32), fs, reset=False)
    n = fs * 2; t = np.arange(n) / fs; x = (0.5 * np.sin(2 * np.pi * f0 * t)).astype(np.float32); x2 = np.vstack([x, x]); out = np.zeros(n)
    for i0 in range(0, n, blk): setter(p, i0 / n); out[i0:i0 + blk] = p(x2[:, i0:i0 + blk], fs, reset=False)[0]
    return out, x.astype(np.float64)
def run_rbj(fg, f0=440, fs=48000, blk=256):   # EQ di riferimento senza interpolazione: coefficienti RBJ cambiati di colpo a ogni blocco
    n = fs * 2; t = np.arange(n) / fs; x = 0.5 * np.sin(2 * np.pi * f0 * t); out = np.zeros(n); zi = np.zeros(2)
    for i0 in range(0, n, blk):
        f, g, q = fg(i0 / n); A = 10 ** (g / 40); w0 = 2 * np.pi * f / fs; al = np.sin(w0) / (2 * q)
        B = np.array([1 + al * A, -2 * np.cos(w0), 1 - al * A]); Aa = np.array([1 + al / A, -2 * np.cos(w0), 1 - al / A]); out[i0:i0 + blk], zi = lfilter(B / Aa[0], Aa / Aa[0], x[i0:i0 + blk], zi=zi)
    return out, x
def static_floor():
    p = tsrq(48000, [{'t': 'Bell', 'f': 1000, 'g': 12, 'q': 2}]); p(np.zeros((2, 4800), np.float32), 48000, reset=False)
    t = np.arange(96000) / 48000; x = (0.5 * np.sin(2 * np.pi * 440 * t)).astype(np.float32); return clickdb(p(np.vstack([x, x]), 48000, reset=False)[0].astype(np.float64), x.astype(np.float64))
fl = static_floor(); print(f'   pavimento (parametri fermi): {fl:.1f} dB')
tests = [('frequenza 100 Hz -> 10 kHz in 2 s, Bell +12 dB Q2', lambda p, u: (setattr(p, 'band_1_gain_db', 12), setattr(p, 'band_1_freq_hz', 100 * 100 ** u)), lambda u: (100 * 100 ** u, 12, 2)),
         ('guadagno 0 -> +12 dB a gradino a meta, Bell 440 Hz Q2', lambda p, u: (setattr(p, 'band_1_freq_hz', 440), setattr(p, 'band_1_gain_db', 12 if u >= .5 else 0)), lambda u: (440, 12 if u >= .5 else 0, 2)),
         ('Q 0.3 -> 10 in 2 s, Bell 600 Hz +12 dB', lambda p, u: (setattr(p, 'band_1_freq_hz', 600), setattr(p, 'band_1_gain_db', 12), setattr(p, 'band_1_q', 0.3 * (10 / 0.3) ** u)), lambda u: (600, 12, 0.3 * (10 / 0.3) ** u))]
for name, st, fg in tests:
    o, x = run_tsrq(st); c = clickdb(o, x); o2, x2 = run_rbj(fg); c2 = clickdb(o2, x2)
    print(f'   {name}: TSR Q {c:.1f} dB | EQ a scatti (RBJ) {c2:.1f} dB')
    ok(c < c2 - 20 and c < -80, f'{name}: energia da click TSR Q {c:.1f} dB contro {c2:.1f} dB di un EQ a scatti (< -80 dB e almeno 20 dB meno)')
print('== 4. CHARACTER: aliasing sul plugin vero (tono ~15 kHz centrato su un bin, 0.99 di ampiezza, 44.1 kHz) ==')
fs = 44100; n = 1 << 15; f15 = round(15000 * n / fs) * fs / n; t = np.arange(n + 8192) / fs; x = (0.99 * np.sin(2 * np.pi * f15 * t)).astype(np.float32); x2 = np.vstack([x, x])
for chm in ('Subtle', 'Warm'):
    p = tsrq(fs, [], character=chm); y = p(x2, fs)[0][8192:] * np.blackman(n); Y = np.abs(np.fft.rfft(y)); fr = np.fft.rfftfreq(n, 1 / fs)
    fund = Y[np.argmin(np.abs(fr - f15))]; msk = (np.abs(fr - f15) > 60) & (fr > 30) & (fr < 20000); al = 20 * np.log10(Y[msk].max() / fund)
    ok(al < -80, f'{chm}: componente spuria piu forte sotto 20 kHz {al:.1f} dB rispetto alla fondamentale (aliasing; < -80 dB)')
print('== 5. DINAMICA sul plugin vero: Bell dinamico, range -1.5 dB, tono sopra soglia ==')
fs = 48000; p = tsrq(fs, [{'t': 'Bell', 'f': 1000, 'g': 0, 'q': 1}]); p.band_1_dynamic = True; p.band_1_threshold_db = -40; p.band_1_range_db = -1.5
t = np.arange(fs * 2) / fs; x = (0.5 * np.sin(2 * np.pi * 1000 * t)).astype(np.float32); y = p(np.vstack([x, x]), fs)[0]
g = 20 * np.log10(np.sqrt(np.mean(y[-fs // 2:] ** 2)) / np.sqrt(np.mean(x[-fs // 2:] ** 2))); ok(abs(g + 1.5) < 0.1, f'riduzione misurata {g:.2f} dB (attesa -1.50)')
print('== 6. CPU sul plugin vero: 32 bande (16 dinamiche), stereo 48 kHz, blocchi 128 ==')
p = tsrq(48000, [{'t': ['Bell', 'Low Shelf', 'High Shelf', 'Bell'][i % 4], 'f': 40 * 1.2 ** i, 'g': 3, 'q': 1.5} for i in range(32)])
for i in range(1, 33, 2): setattr(p, f'band_{i}_dynamic', True)
x = (np.random.default_rng(2).standard_normal((2, 48000 * 10)) * 0.2).astype(np.float32); t0 = time.perf_counter()
for i0 in range(0, x.shape[1], 128): p(x[:, i0:i0 + 128], 48000, reset=False)
dt = time.perf_counter() - t0; ok(dt < 10, f'10 s di audio in {dt*1000:.0f} ms = {dt*10:.1f} % di un core (x{10/dt:.0f} tempo reale)')
print(f'\nRISULTATO confronto e plugin vero: {sum(res)} PASS, {len(res)-sum(res)} FAIL'); sys.exit(0 if all(res) else 1)
