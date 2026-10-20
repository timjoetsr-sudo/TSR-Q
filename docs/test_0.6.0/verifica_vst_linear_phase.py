# TSR Q 0.6.0 Linear Phase sul plugin VST3 vero (pedalboard): latenza riportata/reale, modulo, null in bypass, blocchi
import numpy as np, pedalboard as pb, sys
VST = sys.argv[1]; FS = 48000
def load(mode, res='Medium'):
    p = pb.load_plugin(VST); p.processing_mode = mode; p.linear_phase_resolution = res
    for i, (t, f, g, q, s) in enumerate([('Bell', 1000, 6, 1, '12 dB/oct'), ('Low Cut', 40, 0, 1, '24 dB/oct'), ('High Shelf', 9000, -4, 1, '12 dB/oct')], 1):
        setattr(p, f'band_{i}_used', True); setattr(p, f'band_{i}_type', t); setattr(p, f'band_{i}_freq_hz', f); setattr(p, f'band_{i}_gain_db', g); setattr(p, f'band_{i}_q', q); setattr(p, f'band_{i}_slope', s)
    return p
for res in ('Low', 'Medium', 'High', 'Very High', 'Maximum'):
    p = load('Linear Phase', res); x = np.zeros((2, FS * 3), np.float32); x[:, FS] = 0.5; y = p(x, FS, buffer_size=512)
    pk = int(np.argmax(np.abs(y[0]))) - FS; lat = p.reported_latency_samples
    h = y[0, FS - lat:FS + lat + 1] / 0.5; sym = float(np.max(np.abs(h - h[::-1])))   # pedalboard compensa la latenza riportata: il centro e al campione dell'impulso
    H = np.abs(np.fft.rfft(y[0, FS - lat:] / 0.5, 1 << 18)); fr = np.fft.rfftfreq(1 << 18, 1 / FS)
    z = load('Zero Latency'); yz = z(x, FS, buffer_size=512); Hz = np.abs(np.fft.rfft(yz[0, FS:] / 0.5, 1 << 18))
    sel = (fr > 60) & (fr < 18000); d = np.max(np.abs(20 * np.log10(H[sel] / Hz[sel])))
    print(f"{res:10s} latenza riportata {lat:6d}  picco misurato {pk:6d}  asimmetria {sym:.1e}  |modulo LP - modulo ZL| 60 Hz-18 kHz max {d:.3f} dB")
# bypass: null con ingresso ritardato
p = load('Linear Phase'); p.bypass = True; rng = np.random.default_rng(1); X = rng.uniform(-.3, .3, (2, FS * 2)).astype(np.float32); y = p(X, FS); lat = p.reported_latency_samples
print("bypass in Linear Phase: residuo vs ingresso (latenza compensata da pedalboard)", float(np.max(np.abs(y[:, FS // 2:] - X[:, FS // 2:]))))
ref = load('Linear Phase')(X, FS, buffer_size=512)
for bs in (64, 4096): print("blocco", bs, "scarto vs 512:", float(np.max(np.abs(load('Linear Phase')(X, FS, buffer_size=bs) - ref))))
