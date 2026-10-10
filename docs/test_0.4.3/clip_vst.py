# TEST SUL PLUGIN VERO (VST3 compilato, ospitato da pedalboard) CON LA CLIP "ESO - SAFE" (48 kHz stereo, 2:28)
import numpy as np, pedalboard as pb, subprocess, time, sys, os
VST = '/home/claude/TSRQ/build/TSRQ_artefacts/Release/VST3/TSR Q.vst3'; D = os.path.dirname(os.path.abspath(__file__)); FS = 48000
res = []
def ok(c, m): res.append(bool(c)); print(('PASS ' if c else 'FAIL ') + m); sys.stdout.flush()
def dbfs(v): return 20 * np.log10(max(float(v), 1e-30))
clip = np.fromfile(D + '/eso.f32', np.float32).reshape(-1, 2).T.copy(); NC = clip.shape[1]
print(f'clip: {NC / FS:.1f} s, picco {dbfs(np.abs(clip).max()):.2f} dBFS, RMS {dbfs(np.sqrt((clip ** 2).mean())):.1f} dBFS')
TYPES = ['Bell', 'Low Shelf', 'High Shelf', 'Low Cut', 'High Cut', 'Notch', 'Band Pass', 'Tilt Shelf', 'Flat Tilt', 'All Pass']
TNUM = {t: i for i, t in enumerate(TYPES)}; SL = {6: '6 dB/oct', 12: '12 dB/oct', 18: '18 dB/oct', 24: '24 dB/oct', 48: '48 dB/oct', 96: '96 dB/oct'}; PL = {0: 'Stereo', 1: 'Left', 2: 'Right', 3: 'Mid', 4: 'Side'}
def load(bands, **glob):
    p = pb.load_plugin(VST)
    for i, b in enumerate(bands, 1):
        setattr(p, f'band_{i}_used', True); setattr(p, f'band_{i}_type', b['t']); setattr(p, f'band_{i}_freq_hz', b['f'])
        if 'g' in b: setattr(p, f'band_{i}_gain_db', b['g'])
        if 'q' in b: setattr(p, f'band_{i}_q', b['q'])
        if 's' in b: setattr(p, f'band_{i}_slope', SL[b['s']])
        if 'pl' in b: setattr(p, f'band_{i}_placement', PL[b['pl']])
        if b.get('dyn'): setattr(p, f'band_{i}_dynamic', True); setattr(p, f'band_{i}_threshold_db', b['thr']); setattr(p, f'band_{i}_range_db', b['rng'])
    for k, v in glob.items(): setattr(p, k, v)
    return p
def actual(p, bands):   # valori REALI dei parametri nel plugin (pedalboard quantizza il valore normalizzato a 3 decimali: 10000 Hz diventa 10017 Hz)
    out = []
    for i, b in enumerate(bands, 1):
        a = dict(b); a['f'] = float(np.float32(getattr(p, f'band_{i}_freq_hz')))
        if 'g' in b: a['g'] = float(np.float32(getattr(p, f'band_{i}_gain_db')))
        if 'q' in b: a['q'] = float(np.float32(getattr(p, f'band_{i}_q')))
        if b.get('dyn'): a['thr'] = float(np.float32(getattr(p, f'band_{i}_threshold_db'))); a['rng'] = float(np.float32(getattr(p, f'band_{i}_range_db')))
        out.append(a)
    return out
def settle(p, s=1.0): p(np.zeros((2, int(FS * s)), np.float32), FS, reset=False)
def engine(x, bands, character=0, out=0.0, pan=0.0, panMS=0, inv=0, gq=0, blk=512):   # stesso segnale nel motore C++ nudo (dump2)
    n = x.shape[1]; fi = D + '/_in.f32'; fo = D + '/_out.f32'; np.concatenate([x[0], x[1]]).astype(np.float32).tofile(fi)
    args = [D + '/dump2', fi, fo, str(n), str(blk), str(character), str(out), str(pan), str(panMS), str(inv), str(gq)]
    for b in bands: args += [str(TNUM[b['t']]), str(b['f']), str(b.get('g', 0)), str(b.get('q', 1)), str(b.get('s', 12)), str(b.get('pl', 0)), '1' if b.get('dyn') else '0', str(b.get('thr', -24)), str(b.get('rng', -6))]
    subprocess.run(args, check=True); y = np.fromfile(fo, np.float32); return np.vstack([y[:n], y[n:]])
seg = clip[:, FS * 40: FS * 52].copy()   # 12 s dal secondo 40 (parte piena)

print('== 1. NULL TEST sull intera clip: plugin appena caricato (nessuna banda) ==')
p = pb.load_plugin(VST); y = p(clip, FS); d = np.abs(y - clip).max(); ok(d == 0, f'uscita = ingresso bit per bit su {NC / FS:.0f} s: differenza massima {dbfs(d):.1f} dBFS'); ok(np.isfinite(y).all(), 'nessun NaN/inf')
print('== 2. BYPASS sull intera clip con 3 bande forti ==')
p = load([{'t': 'Bell', 'f': 1000, 'g': 12, 'q': 2}, {'t': 'Low Shelf', 'f': 120, 'g': 9, 'q': 0.75}, {'t': 'High Cut', 'f': 8000, 's': 48}], bypass=True); settle(p); y = p(clip, FS, reset=False)
d = np.abs(y - clip).max(); ok(d < 1e-6, f'bypass: uscita = ingresso, differenza massima {dbfs(d):.1f} dBFS')
print('== 3. PLUGIN = MOTORE C++: ogni tipo di banda sulla clip (12 s), confronto campione per campione (parametri letti dal plugin) ==')
cases = [[{'t': 'Bell', 'f': 1000, 'g': 6, 'q': 2}], [{'t': 'Low Shelf', 'f': 150, 'g': -6, 'q': 0.75}], [{'t': 'High Shelf', 'f': 6000, 'g': 4, 'q': 0.75}], [{'t': 'Low Cut', 'f': 80, 's': 24}], [{'t': 'High Cut', 'f': 10000, 's': 48}],
         [{'t': 'Notch', 'f': 3000, 'q': 8}], [{'t': 'Band Pass', 'f': 2000, 'q': 1, 's': 24}], [{'t': 'Tilt Shelf', 'f': 1000, 'g': 3, 'q': 0.75}], [{'t': 'Flat Tilt', 'f': 1000, 'g': -3}], [{'t': 'All Pass', 'f': 500, 'q': 1}],
         [{'t': 'Bell', 'f': 400, 'g': -4, 'q': 1.5, 'pl': 3}, {'t': 'Bell', 'f': 4000, 'g': 3, 'q': 1, 'pl': 4}], [{'t': 'Bell', 'f': 800, 'g': 5, 'q': 1.5, 'pl': 1}, {'t': 'High Shelf', 'f': 5000, 'g': -3, 'q': 0.75, 'pl': 2}],
         [{'t': 'Bell', 'f': 200, 'g': 0, 'q': 1, 'dyn': True, 'thr': -30, 'rng': -1.5}], [{'t': 'Low Shelf', 'f': 100, 'g': 3, 'q': 0.75, 'dyn': True, 'thr': -36, 'rng': 1.5}]]
worst = -999
for c in cases:
    p = load(c); ca = actual(p, c); settle(p); y = p(seg, FS, reset=False); e = engine(seg, ca)
    d = dbfs(np.abs(y[:, FS // 2:] - e[:, FS // 2:]).max()); worst = max(worst, d); name = ' + '.join(f"{b['t']} {b['f']} Hz" + (f" {b['g']:+} dB" if 'g' in b else '') + (f" {PL[b['pl']]}" if 'pl' in b else '') + (' dinamico' if b.get('dyn') else '') for b in c)
    print(f'   {name:<60} plugin - motore: {d:7.1f} dBFS   picco uscita {dbfs(np.abs(y).max()):.1f} dBFS')
    if not np.isfinite(y).all(): ok(False, f'{name}: NaN/inf in uscita')
ok(worst < -100, f'plugin = motore C++ in {len(cases)} configurazioni (differenza peggiore {worst:.1f} dBFS, soglia -100)')
print('== 4. CLICK sul plugin vero: cambi di stato dall host mentre la clip suona (blocchi da 256), energia sopra 6 kHz vs clip con parametri fermi ==')
from scipy.signal import butter, sosfilt
HP = butter(10, 6000, 'highpass', fs=FS, output='sos'); lp = butter(16, 2500, 'lowpass', fs=FS, output='sos')
from scipy.signal import sosfiltfilt
seglp = sosfiltfilt(lp, seg, axis=1).astype(np.float32)   # clip senza contenuto sopra 2,5 kHz: tutto cio che compare sopra 6 kHz e prodotto dal plugin
xr = np.sqrt((seglp ** 2).mean())
def hf(y): r = sosfilt(HP, y[0])[4800:]; fr = r[:len(r) // 64 * 64].reshape(-1, 64); return dbfs(np.sqrt((fr ** 2).mean(1)).max() / xr)
def automate(bands, setter, **glob):
    p = load(bands, **glob); setter(p, 0, 0); settle(p, 0.5); n = seglp.shape[1]; out = np.zeros_like(seglp)
    for i0 in range(0, n, 256): setter(p, i0 / n, (i0 // 24000) % 2); out[:, i0:i0 + 256] = p(seglp[:, i0:i0 + 256], FS, reset=False)
    return out
B1 = [{'t': 'Bell', 'f': 500, 'g': 6, 'q': 2}]
tests = [('parametri fermi', B1, lambda p, u, ph: None),
         ('frequenza 200 Hz -> 2 kHz', B1, lambda p, u, ph: setattr(p, 'band_1_freq_hz', 200 * 10 ** u)),
         ('guadagno -12 -> +12 dB', B1, lambda p, u, ph: setattr(p, 'band_1_gain_db', -12 + 24 * u)),
         ('Q 0.3 -> 10', B1, lambda p, u, ph: setattr(p, 'band_1_q', 0.3 * (10 / 0.3) ** u)),
         ('banda bypass on/off ogni 0,5 s', B1, lambda p, u, ph: setattr(p, 'band_1_bypass', bool(ph))),
         ('banda used on/off ogni 0,5 s', B1, lambda p, u, ph: setattr(p, 'band_1_used', not ph)),
         ('tipo Bell/Notch ogni 0,5 s', B1, lambda p, u, ph: setattr(p, 'band_1_type', 'Notch' if ph else 'Bell')),
         ('pendenza 12/48 (Low Cut 300 Hz)', [{'t': 'Low Cut', 'f': 300, 's': 12}], lambda p, u, ph: setattr(p, 'band_1_slope', '48 dB/oct' if ph else '12 dB/oct')),
         ('collocazione Stereo/Side', B1, lambda p, u, ph: setattr(p, 'band_1_placement', 'Side' if ph else 'Stereo')),
         ('dinamica on/off', B1, lambda p, u, ph: (setattr(p, 'band_1_threshold_db', -40), setattr(p, 'band_1_range_db', -1.5), setattr(p, 'band_1_dynamic', bool(ph)))),
         ('bypass globale on/off', B1, lambda p, u, ph: setattr(p, 'bypass', bool(ph))),
         ('Character Clean/Subtle', B1, lambda p, u, ph: setattr(p, 'character', 'Subtle' if ph else 'Clean')),
         ('OUTPUT 0/-12 dB a gradino', B1, lambda p, u, ph: setattr(p, 'output_db', -12 if ph else 0)),
         ('INPUT 0/-12 dB a gradino', B1, lambda p, u, ph: setattr(p, 'input_db', -12 if ph else 0)),
         ('polarita', B1, lambda p, u, ph: setattr(p, 'polarity_invert', bool(ph))),
         ('pan 0/-100', B1, lambda p, u, ph: setattr(p, 'output_pan', -100 if ph else 0))]
floor = hf(automate(B1, lambda p, u, ph: None)); worstc = -999
sub_static = hf(automate(B1, lambda p, u, ph: None, character='Subtle'))
for name, bands, st_ in tests:
    v = hf(automate(bands, st_)); ref = sub_static if 'Subtle' in name else floor; worstc = max(worstc, v - ref)
    print(f'   {name:<34} {v:7.1f} dB   (riferimento fermo {ref:.1f} dB)')
ok(worstc < 3, f'nessun click sul plugin vero: ogni cambio di stato resta entro {worstc:.1f} dB dal pavimento della clip ferma ({floor:.1f} dB); soglia +3 dB')
print('== 5. STRESS: 32 bande (16 dinamiche) sull intera clip, plugin = motore, nessun NaN, CPU ==')
rng = np.random.default_rng(7); big = []
for i in range(32):
    t = ['Bell', 'Low Shelf', 'High Shelf', 'Notch', 'Bell', 'Tilt Shelf', 'Bell', 'Bell'][i % 8]; b = {'t': t, 'f': float(np.round(30 * 1.22 ** i, 1)), 'q': float(rng.choice([0.5, 1, 2, 4])), 'pl': int(rng.choice([0, 0, 3, 4]))}
    if t != 'Notch': b['g'] = float(rng.choice([-6, -3, 3, 6]))
    if i % 2 == 0 and t != 'Notch': b.update(dyn=True, thr=float(rng.choice([-30, -40])), rng=float(rng.choice([-1.5, 1.5])))
    big.append(b)
p = load(big); settle(p); t0 = time.perf_counter(); y = np.concatenate([p(clip[:, i0:i0 + 512], FS, reset=False) for i0 in range(0, NC, 512)], axis=1); dt = time.perf_counter() - t0
ok(np.isfinite(y).all(), f'32 bande su {NC / FS:.0f} s: uscita finita, picco {dbfs(np.abs(y).max()):.1f} dBFS')
ok(dt < NC / FS * 0.5, f'CPU: {NC / FS:.0f} s di clip in {dt:.2f} s = {dt / (NC / FS) * 100:.1f} % di un core (blocchi da 512)')
e = engine(clip[:, :FS * 20], actual(p, big)); d = dbfs(np.abs(y[:, FS: FS * 20] - e[:, FS: FS * 20]).max()); ok(d < -100, f'32 bande, 20 s: plugin = motore C++ (differenza max {d:.1f} dBFS)')
print(f'\nRISULTATO plugin vero sulla clip: {sum(res)} PASS, {len(res) - sum(res)} FAIL'); sys.exit(0 if all(res) else 1)
