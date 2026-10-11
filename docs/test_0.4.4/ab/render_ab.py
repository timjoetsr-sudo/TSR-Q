# Render A/B con i PLUGIN VERI (VST3 Linux 0.4.3 congelato e 0.4.4 appena compilato, ospitati da pedalboard) sulla clip ESO - SAFE.
# Per ogni scenario: DRY, 0.4.3, 0.4.4 -> loudness integrata (ffmpeg ebur128, BS.1770) -> tutti normalizzati a -18 LUFS -> FLAC 24 bit.
# Inoltre: residuo 0.4.4 - 0.4.3 (prima della normalizzazione) e differenza di loudness tra le versioni.
import numpy as np, pedalboard as pb, subprocess, json, os, re, sys
FS = 48000; L = '/home/claude/lab'
VST = {'0.4.3': L + '/base043/TSR Q.vst3', '0.4.4': '/home/claude/TSRQ/build/TSRQ_artefacts/Release/VST3/TSR Q.vst3'}
OUT = L + '/ab/out'; os.makedirs(OUT, exist_ok=True)
clip = np.fromfile(L + '/eso.f32', np.float32).reshape(-1, 2).T.copy()
T0, DUR, PRE = 40.0, 15.0, 2.0   # estratto da 0:40, 15 s; 2 s di pre-roll scartati (stato dei filtri e del detector a regime)
seg = clip[:, int(FS * (T0 - PRE)): int(FS * (T0 + DUR))].copy()
SL = {6: '6 dB/oct', 12: '12 dB/oct', 24: '24 dB/oct', 48: '48 dB/oct', 96: '96 dB/oct'}
def load(ver, bands, glob):
    p = pb.load_plugin(VST[ver])
    for i, b in enumerate(bands, 1):
        setattr(p, f'band_{i}_used', True); setattr(p, f'band_{i}_type', b['t']); setattr(p, f'band_{i}_freq_hz', b['f'])
        for k, a in (('g', 'gain_db'), ('q', 'q'), ('thr', 'threshold_db'), ('rng', 'range_db'), ('att', 'attack_ms'), ('rel', 'release_ms'), ('knee', 'knee_db')):
            if k in b: setattr(p, f'band_{i}_{a}', b[k])
        if 's' in b: setattr(p, f'band_{i}_slope', SL[b['s']])
        if b.get('dyn'): setattr(p, f'band_{i}_dynamic', True)
        if 'det' in b: setattr(p, f'band_{i}_detection', b['det'])
    for k, v in glob.items(): setattr(p, k, v)
    return p
def render(ver, bands, glob):
    p = load(ver, bands, glob); y = p(seg, FS); return y[:, int(FS * PRE):].astype(np.float32)
def lufs(x):   # loudness integrata BS.1770 (ffmpeg ebur128) + true peak
    fn = OUT + '/_tmp.f32'; x.T.astype(np.float32).tofile(fn)
    r = subprocess.run(['ffmpeg', '-hide_banner', '-nostats', '-f', 'f32le', '-ar', str(FS), '-ac', '2', '-i', fn, '-af', 'ebur128=peak=true', '-f', 'null', '-'], capture_output=True, text=True).stderr
    I = float(re.findall(r'I:\s+(-?[\d.]+) LUFS', r)[-1]); tp = float(re.findall(r'Peak:\s+(-?[\d.]+) dBFS', r)[-1]); return I, tp
def save(x, name):   # FLAC 24 bit (senza perdita)
    fn = OUT + '/_tmp.f32'; x.T.astype(np.float32).tofile(fn)
    subprocess.run(['ffmpeg', '-hide_banner', '-loglevel', 'error', '-y', '-f', 'f32le', '-ar', str(FS), '-ac', '2', '-i', fn, '-c:a', 'flac', '-sample_fmt', 's32', '-bits_per_raw_sample', '24', OUT + '/' + name], check=True)
SCEN = [
 ('dyn_bell_200', 'Bell dinamico 200 Hz (RMS, soglia -30, range -6): il detector ascolta solo la sua banda', [{'t': 'Bell', 'f': 200, 'g': 0, 'q': 1, 'dyn': True, 'thr': -30, 'rng': -6, 'att': 5, 'rel': 80, 'det': 'RMS'}], {}),
 ('dyn_kick_peak', 'Bell dinamico 60 Hz in Peak (soglia -24, range -6, attacco 1 ms): detector calibrato', [{'t': 'Bell', 'f': 60, 'g': 0, 'q': 1.2, 'dyn': True, 'thr': -24, 'rng': -6, 'att': 1, 'rel': 60, 'det': 'Peak'}], {}),
 ('warm_drive', 'Character Warm spinto: INPUT +12 dB (OUTPUT 0: il guadagno d’uscita agisce prima del Character). Oltre 0 dBFS la curva della 0.4.3 ripiega', [], {'input_db': 12.0, 'character': 'Warm'}),
 ('subtle_drive', 'Character Subtle spinto: INPUT +12 dB (OUTPUT 0)', [], {'input_db': 12.0, 'character': 'Subtle'}),
 ('eq_statica', 'EQ statica tipica (Low Cut 30 Hz 24 dB/oct, Bell 250 Hz -3 dB, Bell 3 kHz +2 dB, High Shelf 10 kHz +2,5 dB)',
  [{'t': 'Low Cut', 'f': 30, 's': 24}, {'t': 'Bell', 'f': 250, 'g': -3, 'q': 1}, {'t': 'Bell', 'f': 3000, 'g': 2, 'q': 1}, {'t': 'High Shelf', 'f': 10000, 'g': 2.5, 'q': 0.71}], {}),
]
TARGET = -18.0; summary = {'clip': 'ESO - SAFE, estratto 0:40-0:55 (48 kHz)', 'target_lufs': TARGET, 'scenarios': []}
dry = seg[:, int(FS * PRE):].copy(); Id, tpd = lufs(dry); save(dry * 10 ** ((TARGET - Id) / 20), 'dry.flac')
for key, desc, bands, glob in SCEN:
    y3 = render('0.4.3', bands, glob); y4 = render('0.4.4', bands, glob)
    assert np.isfinite(y3).all() and np.isfinite(y4).all()
    I3, tp3 = lufs(y3); I4, tp4 = lufs(y4); r = y4 - y3
    res_db = float(10 * np.log10((r ** 2).mean() / (y4 ** 2).mean() + 1e-30)); res_pk = float(20 * np.log10(np.abs(r).max() + 1e-30))
    Ir, _ = lufs(r) if np.abs(r).max() > 1e-7 else (-200.0, -200.0)
    save(y3 * 10 ** ((TARGET - I3) / 20), f'{key}_043.flac'); save(y4 * 10 ** ((TARGET - I4) / 20), f'{key}_044.flac')
    if np.abs(r).max() > 1e-7: save(r * 10 ** ((TARGET - I4) / 20), f'{key}_diff.flac')   # differenza con lo STESSO guadagno della 0.4.4 normalizzata: si sente quanto pesa davvero
    # differenza ALLINEATA: la 0.4.3 spostata del ritardo frazionario che minimizza il residuo (Character: i nuovi filtri dell'oversampling anticipano di ~0,6 campioni)
    w = np.fft.rfftfreq(y3.shape[1]) * 2 * np.pi; A3 = np.fft.rfft(y3, axis=1); best = (0.0, (r ** 2).mean())
    for tau in np.arange(-1.5, 1.5001, 0.05):
        sh = np.fft.irfft(A3 * np.exp(-1j * w * tau), y3.shape[1], axis=1); e2 = ((y4 - sh) ** 2).mean()
        if e2 < best[1]: best = (float(tau), float(e2))
    tau = best[0]; ra = y4 - np.fft.irfft(A3 * np.exp(-1j * w * tau), y3.shape[1], axis=1)
    ra_db = float(10 * np.log10((ra ** 2).mean() / (y4 ** 2).mean() + 1e-30))
    if abs(tau) > 0.01 and np.abs(ra).max() > 1e-7: save(ra.astype(np.float32) * 10 ** ((TARGET - I4) / 20), f'{key}_diffal.flac')
    s = {'key': key, 'desc': desc, 'lufs_dry': round(Id, 2), 'lufs_043': round(I3, 2), 'lufs_044': round(I4, 2), 'truepeak_043_dbtp': tp3, 'truepeak_044_dbtp': tp4,
         'gain_applied_043_db': round(TARGET - I3, 2), 'gain_applied_044_db': round(TARGET - I4, 2), 'residuo_rms_rel_db': round(res_db, 1), 'residuo_picco_dbfs': round(res_pk, 1), 'residuo_lufs': round(Ir, 1), 'ritardo_0443_vs_044_campioni': round(-tau, 2), 'residuo_allineato_rms_rel_db': round(ra_db, 1)}
    summary['scenarios'].append(s); print(json.dumps(s, ensure_ascii=False)); sys.stdout.flush()
summary['lufs_dry'] = round(Id, 2); json.dump(summary, open(OUT + '/summary.json', 'w'), ensure_ascii=False, indent=1)
os.remove(OUT + '/_tmp.f32'); print('ok')
