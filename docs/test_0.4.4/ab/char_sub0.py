# Character sotto 0 dBFS (INPUT 0, OUTPUT 0) con i plugin VST3 veri: residuo 0.4.4-0.4.3 grezzo e dopo l'allineamento frazionario (stesso metodo di render_ab.py)
import numpy as np, pedalboard as pb, json
FS = 48000; L = '/home/claude/lab'
VST = {'0.4.3': L + '/base043/TSR Q.vst3', '0.4.4': '/home/claude/TSRQ/build/TSRQ_artefacts/Release/VST3/TSR Q.vst3'}
clip = np.fromfile(L + '/eso.f32', np.float32).reshape(-1, 2).T.copy()
T0, DUR, PRE = 40.0, 15.0, 2.0
seg = clip[:, int(FS * (T0 - PRE)): int(FS * (T0 + DUR))].copy()
print('picco ingresso estratto: %.2f dBFS' % (20 * np.log10(np.abs(seg).max())))
def render(ver, ch):
    p = pb.load_plugin(VST[ver]); p.character = ch; y = p(seg, FS); return y[:, int(FS * PRE):].astype(np.float64)
out = {}
for ch in ['Warm', 'Subtle']:
    y3 = render('0.4.3', ch); y4 = render('0.4.4', ch); r = y4 - y3
    raw = 10 * np.log10((r ** 2).mean() / (y4 ** 2).mean())
    w = np.fft.rfftfreq(y3.shape[1]) * 2 * np.pi; A3 = np.fft.rfft(y3, axis=1); best = (0.0, (r ** 2).mean())
    for tau in np.arange(-1.5, 1.5001, 0.01):
        e2 = ((y4 - np.fft.irfft(A3 * np.exp(-1j * w * tau), y3.shape[1], axis=1)) ** 2).mean()
        if e2 < best[1]: best = (float(tau), float(e2))
    al = 10 * np.log10(best[1] / (y4 ** 2).mean())
    out[ch] = {'picco_out_043_dbfs': round(float(20 * np.log10(np.abs(y3).max())), 2), 'residuo_grezzo_db': round(float(raw), 1), 'anticipo_044_campioni': round(-best[0], 2), 'residuo_allineato_db': round(float(al), 1)}
    print(ch, json.dumps(out[ch]))
json.dump(out, open('char_sub0.json', 'w'), indent=1)
