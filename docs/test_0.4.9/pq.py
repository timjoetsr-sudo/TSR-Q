# TSR Q · QUALITY PASS · misure sul PLUGIN VST3 vero (pedalboard), CSV: sezione,caso,metrica,valore,unita
import numpy as np, pedalboard as pb, sys
VST = sys.argv[1]
def db(x): return float(20 * np.log10(max(float(x), 1e-20)))
def out(s, c, m, v, u): print(f"{s},{c},{m},{v:.3f},{u}"); sys.stdout.flush()
def load(**kw):
    p = pb.load_plugin(VST)
    for k, v in kw.items(): setattr(p, k, v)
    return p
def eq(p):
    for i, (t, f, g, q) in enumerate([('Low Cut', 30, 0, .71), ('Bell', 250, -3, 1), ('Bell', 3000, 2, 1), ('High Shelf', 10000, 2.5, .71)], 1):
        setattr(p, f'band_{i}_used', True); setattr(p, f'band_{i}_type', t); setattr(p, f'band_{i}_freq_hz', f); setattr(p, f'band_{i}_gain_db', g); setattr(p, f'band_{i}_q', q)
    return p
FS = 48000; rng = np.random.default_rng(3); X = (rng.uniform(-.5, .5, (2, FS * 2))).astype(np.float32)
print("sezione,caso,metrica,valore,unita")
# latenza riportata e reale (impulso a basso livello, dopo 0,5 s di silenzio)
for ch in ('Clean', 'Subtle', 'Warm'):
    p = load(character=ch); x = np.zeros((2, FS), np.float32); x[:, FS // 2] = 0.01; y = p(x, FS)
    out('latenza_plugin', ch, 'riportata_host', p.reported_latency_samples, 'campioni'); out('latenza_plugin', ch, 'picco_impulso', int(np.argmax(np.abs(y[0]))) - FS // 2, 'campioni')
# null
for nm, kw, use_eq in (('neutro', {}, False), ('bypass_con_eq', {'bypass': True}, True), ('bypass_con_eq_e_warm', {'bypass': True, 'character': 'Warm'}, True)):
    p = load(**kw); p = eq(p) if use_eq else p; y = p(X, FS); r = np.abs(y - X).max(); out('null_plugin', nm, 'residuo_max', -999 if r == 0 else db(r), 'dBFS')
# blocchi (buffer_size dell'host simulato) con EQ + Warm
ref = eq(load(character='Warm'))(X, FS, buffer_size=512)
for bs in (7, 64, 512, 4096):
    y = eq(load(character='Warm'))(X, FS, buffer_size=bs); r = np.abs(y - ref).max(); out('blocchi_plugin', str(bs), 'scarto_max_vs_512', -999 if r == 0 else db(r), 'dBFS')
# mono vs stereo (L=R)
m = X[:1].copy(); ys = eq(load())(np.vstack([m, m]), FS); ym = eq(load())(m, FS); r = np.abs(ys[0] - ym[0]).max(); out('mono_plugin', 'mono_vs_stereo_L=R', 'scarto_max', -999 if r == 0 else db(r), 'dBFS')
# stato salvato / ripristinato
p = eq(load(character='Subtle', output_db=-3.0, pan=20.0)); s1 = p.raw_state; q = pb.load_plugin(VST); q.raw_state = s1; s2 = q.raw_state
out('stato_plugin', 'raw_state', 'identico_dopo_ripristino', float(s1 == s2), 'bool'); out('stato_plugin', 'raw_state', 'byte', len(s1), 'n')
r = np.abs(p(X, FS) - q(X, FS)).max(); out('stato_plugin', 'audio', 'scarto_max_originale_vs_ripristinato', -999 if r == 0 else db(r), 'dBFS')
# frequenze di campionamento
for sr in (44100, 48000, 88200, 96000, 176400, 192000):
    p = load(); setattr(p, 'band_1_used', True); setattr(p, 'band_1_type', 'Bell'); setattr(p, 'band_1_freq_hz', 1000.0); setattr(p, 'band_1_gain_db', 6.0)
    t = np.arange(sr) / sr; x = np.tile((0.1 * np.sin(2 * np.pi * 1000 * t)).astype(np.float32), (2, 1)); y = p(x, sr)[0, sr // 2:]
    tt = t[sr // 2:]; a = 2 * np.hypot((y * np.sin(2 * np.pi * 1000 * tt)).mean(), (y * np.cos(2 * np.pi * 1000 * tt)).mean()); out('sample_rate_plugin', str(sr), 'bell1k+6_a_1kHz', db(a / 0.1), 'dB')
