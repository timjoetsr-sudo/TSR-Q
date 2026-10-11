# genera la pagina del report (HTML) dai numeri misurati: data.json + curve dei due motori + riepilogo dei render A/B. Nessun numero scritto a mano.
import json, html, os, sys, re
L = '/home/claude/lab'; R = L + '/report'
D = json.load(open(R + '/data.json')); CB = json.load(open(R + '/curves_base.json')); CN = json.load(open(R + '/curves_new.json'))
AB = D['ab']; B, N = D['base'], D['new']; LISTEN = sys.argv[1] if len(sys.argv) > 1 else ''
e = html.escape
th = lambda n: f'{n:,}'.replace(',', '.')
def f(x, d=2): return ('%.' + str(d) + 'f') % x
def cpu_rows():
    rows = []; idx = {(r['scen'], r['fs'], r['block']): r['core_pct'] for r in B['cpu']}
    for r in N['cpu']:
        k = (r['scen'], r['fs'], r['block']); b = idx.get(k)
        if b is None: continue
        rows.append({'scen': r['scen'], 'fs': r['fs'], 'block': r['block'], 'old': b, 'new': r['core_pct']})
    return rows
CPU = cpu_rows()
JSO = {r['scen']: r['core_pct'] for r in B['cpu_js']}; JSN = {r['scen']: r['core_pct'] for r in N['cpu_js']}
def gm(xs): import math; return math.exp(sum(math.log(x) for x in xs) / len(xs))
speed = [r['old'] / r['new'] for r in CPU]; faster = sum(1 for s in speed if s > 1.0)
pick = lambda scen, fs=48000, bl=512: next((r for r in CPU if r['scen'] == scen and r['fs'] == fs and r['block'] == bl), None)
def det_err(v, det):
    xs = [max(abs(r['lo'] + 20), abs(r['hi'] + 20)) for r in D[v]['detector_cal'] if r['det'] == det]; return max(xs) if xs else float('nan')
db = {x['f']: x for x in B['detector_band']}; dn = {x['f']: x for x in N['detector_band']}
nanb = B['nan']; nann = N['nan']
cw = {r['scen']: r for r in B['cpuworst']}; cwn = {r['scen']: r for r in N['cpuworst']}
hover = D['hover_ui']
hv = re.findall(r'(\d+\.\d+) ms', hover)
T = D['tests']
def tests_rows():
    out = [('Test C++ del motore (tests/dsp_tests.cpp, 21 sezioni)', T['dsp_tests']), ('Test sugli stem reali (tests/stem_tests.cpp)', T['stem_tests']),
           ('Plugin vero sulla clip ESO - SAFE (VST3 ospitato, null test, click, 32 bande)', T['clip_vst']), ('Confronto esterno: plugin vero vs analogico, JUCE, RBJ', T['confronto']),
           ('Self-test end-to-end del plugin (interfaccia, parametri, motore)', T['selftest'])]
    return out
ui = T['ui']; ui_tot = sum(v['pass'] for v in ui.values()); ui_fail = sum(v['fail'] for v in ui.values())
par = T['parity']; par_pass = sum(int(re.search(r'(\d+) PASS', v).group(1)) for v in par.values()); par_fail = sum(int(re.search(r'(\d+) FAIL', v).group(1)) for v in par.values())
coef = json.loads(T['coef'])
# ---- tabella riassuntiva (tutti i numeri dai file) ----
P0, P1 = B['precision'], N['precision']; X0, X1 = B['precisionx'], N['precisionx']
SUM = [
 ('Precisione, 17.880 progetti (5 fs, 9 tipi): casi con errore &gt; 1 dB', P0['gt1'], P1['gt1'], 'errore massimo 20 Hz-20 kHz vs prototipo analogico, maschera -30 dB'),
 ('&nbsp;&nbsp;casi &gt; 0,5 dB / errore massimo', f"{P0['gt05']} / {f(P0['max'])} dB", f"{P1['gt05']} / {f(P1['max'])} dB", ''),
 ('Griglia estesa 19.200 progetti (Bell e shelf, Q fino a 40, &plusmn;30 dB): casi &gt; 1 dB', X0['gt1'], X1['gt1'], ''),
 ('&nbsp;&nbsp;errore massimo', f"{f(X0['max'])} dB", f"{f(X1['max'])} dB", 'High Shelf 16 kHz +18 dB Q 20 a 44,1 kHz era +19 dB a 20 kHz'),
 ('Lettura del detector RMS, sinusoide a -20 dBFS (100 Hz-5 kHz, 44,1-96 kHz)', f"+{f(det_err('base','RMS'))} dB", f"&plusmn;{f(det_err('new','RMS'),3)} dB", 'scarto massimo dal livello vero'),
 ('Lettura del detector Peak', f"+{f(det_err('base','Peak'))} dB", f"&plusmn;{f(det_err('new','Peak'),3)} dB", 'picco - 3,01 dB, tenuto per un periodo della banda'),
 ('Guadagno dinamico a regime vs atteso (162 casi)', f"{f(B['dynacc']['worst_err_db'],3)} dB", f"{f(N['dynacc']['worst_err_db'],4)} dB", 'scarto massimo'),
 ('Bell dinamico 200 Hz, tono a 5 kHz -10 dBFS: riduzione', f"{f(db[5000]['red'])} dB", f"{f(dn[5000]['red'])} dB", 'detector 0.4.3 = passa-alto'),
 ('Un NaN in ingresso: campioni d’uscita non validi (4 s)', nanb[0]['bad_samples'], nann[0]['bad_samples'], '0.4.3: uscita NaN per sempre'),
 ('CPU, 32 bande statiche (48 kHz, blocchi 512)', f"{f(pick('32 statiche')['old'],2)} %", f"{f(pick('32 statiche')['new'],2)} %", '% di un core, mediana di 5'),
 ('CPU, 16 bande dinamiche in movimento', f"{f(pick('16 dinamiche in movimento (range 12, att 1 ms)')['old'],2)} %", f"{f(pick('16 dinamiche in movimento (range 12, att 1 ms)')['new'],2)} %", ''),
 ('CPU, 32 bande automatizzate a ogni blocco da 128', f"{f(B['cpuauto']['core_pct'],2)} %", f"{f(N['cpuauto']['core_pct'],2)} %", ''),
 ('CPU, Character Warm (oversampling 8x, solo uscita)', B['cpuchar'][2].split(': ')[1].split(' %')[0] + ' %', N['cpuchar'][2].split(': ')[1].split(' %')[0] + ' %', ''),
 ('Configurazioni CPU più veloci', '', f"{faster} su {len(CPU)}", f"media geometrica &times;{f(gm(speed),2)}"),
 ('Interfaccia: ricalcolo del fantasma "clic per creare" (12 bande)', f"{hv[0]} ms", f"{hv[2]} ms", 'a ogni movimento del mouse'),
]
def tr(r): return f"<tr><th scope='row'>{r[0]}</th><td class='old'>{r[1]}</td><td class='new'>{r[2]}</td><td class='how'>{r[3]}</td></tr>"
# ---- ascolto A/B ----
def ab_html():
    if 'scenarios' not in AB: return "<p class='warn'>Render A/B non disponibili: " + e(str(AB)) + "</p>"
    out = [f"<p class='lead'>Estratto della clip ESO - SAFE (0:40-0:55, 48 kHz), elaborato dai <b>plugin VST3 compilati</b> (0.4.3 congelato e 0.4.4) ospitati da pedalboard. Ogni file è normalizzato a <b>{AB['target_lufs']:.0f} LUFS</b> integrati (BS.1770, ffmpeg ebur128): nessuna versione vince perché è più forte. DIFF = 0.4.4 - 0.4.3 prima della normalizzazione, con lo stesso guadagno della 0.4.4: si sente quanto pesa davvero. WAV PCM 24 bit, senza perdita (decodificati dai FLAC 24 bit del render).</p>",
           "<div class='abdry'><span>DRY (originale)</span><audio controls preload='none' src='audio/dry.wav'></audio><span class='num'>" + f(AB['lufs_dry'], 1) + " LUFS</span></div>"]
    for s_ in AB['scenarios']:
        k = s_['key']; diff = os.path.exists(L + f'/ab/out/{k}_diff.flac'); tau = s_.get('ritardo_0443_vs_044_campioni', 0); al = abs(tau) >= 0.3 and os.path.exists(L + f'/ab/out/{k}_diffal.flac')
        w = s_.get('diffal_win50' if al else 'diff_win50')
        pl = [f"<div><span class='tag old'>0.4.3 prima</span><audio controls preload='none' src='audio/{k}_043.wav'></audio></div>",
              f"<div><span class='tag new'>0.4.4 dopo</span><audio controls preload='none' src='audio/{k}_044.wav'></audio></div>"]
        if al: pl.append(f"<div><span class='tag dif'>DIFF allineato</span><audio controls preload='none' src='audio/{k}_diffal.wav'></audio></div>")
        elif diff: pl.append(f"<div><span class='tag dif'>DIFF</span><audio controls preload='none' src='audio/{k}_diff.wav'></audio></div>")
        else: pl.append(f"<div><span class='tag dif'>DIFF</span><span class='muted'>residuo a {s_['residuo_picco_dbfs']:.0f} dBFS di picco: arrotondamento, niente da ascoltare</span></div>")
        res = f"{s_['residuo_rms_rel_db']:.1f} dB RMS rispetto al segnale &middot; picco {s_['residuo_picco_dbfs']:.1f} dBFS"
        if al: res = f"grezzo {s_['residuo_rms_rel_db']:.1f} dB (domina un ritardo di {tau:+.2f} campioni dei nuovi filtri dell'oversampling); allineato <b>{s_['residuo_allineato_rms_rel_db']:.1f} dB</b> RMS rispetto al segnale"
        if w: res += f" &middot; finestre da 50 ms: mediana {w['median']:.1f} dB, peggiore {w['max']:.1f} dB"
        out.append(f"<article class='abcase' id='ab-{k}'><h3>{e(s_['desc']).replace(' d uscita', ' d’uscita')}</h3><div class='players'>{''.join(pl)}</div><div class='mini-wrap'><table class='mini'>"
                   f"<tr><th>loudness prima della normalizzazione</th><td>0.4.3 <b class='num'>{f(s_['lufs_043'],1)}</b> LUFS</td><td>0.4.4 <b class='num'>{f(s_['lufs_044'],1)}</b> LUFS</td><td>DRY <span class='num'>{f(s_['lufs_dry'],1)}</span></td></tr>"
                   f"<tr><th>guadagno applicato per arrivare a {AB['target_lufs']:.0f} LUFS</th><td class='num'>{s_['gain_applied_043_db']:+.1f} dB</td><td class='num'>{s_['gain_applied_044_db']:+.1f} dB</td><td></td></tr>"
                   f"<tr><th>DIFF (0.4.4 - 0.4.3)</th><td colspan='3'>{res}</td></tr></table></div></article>")
    return '\n'.join(out)
data_js = {'curves': {'base': CB['curves'], 'new': CN['curves']}, 'character': {'base': CB['character'], 'new': CN['character']},
           'prec': {'base': P0['by'], 'new': P1['by']}, 'precx': {'base': X0['by'], 'new': X1['by']},
           'cpu': CPU, 'js': [{'scen': k, 'old': JSO[k], 'new': JSN.get(k)} for k in JSO],
           'det': {'base': B['detector_cal'], 'new': N['detector_cal']}}
page = open(R + '/template.html', encoding='utf-8').read()
page = page.replace('%%SUMMARY%%', '\n'.join(tr(r) for r in SUM)).replace('%%AB%%', ab_html()).replace('%%DATA%%', json.dumps(data_js, separators=(',', ':')))
page = page.replace('%%LISTEN%%', LISTEN)
# test eseguiti
trs = ''.join(f"<tr><th scope='row'>{e(n)}</th><td class='num'>{c['pass']}</td><td class='num'>{c['fail']}</td></tr>" for n, c in tests_rows())
trs += f"<tr><th scope='row'>Interfaccia (Playwright, Chromium): 5 suite del prototipo + pagina A/B</th><td class='num'>{ui_tot}</td><td class='num'>{ui_fail}</td></tr>"
trs += f"<tr><th scope='row'>Parità JS = C++ (prototipo/artefatto = plugin), soglia -90 dBFS</th><td class='num'>{par_pass}</td><td class='num'>{par_fail}</td></tr>"
trs += f"<tr><th scope='row'>pluginval, livello 10 (VST3 Linux)</th><td class='num'>{T['pluginval']['groups']} gruppi</td><td class='num'>{'0' if T['pluginval']['ok'] else 'ERRORE'}</td></tr>"
trs += f"<tr><th scope='row'>Progetti dei filtri JS vs C++ ({th(coef['cases'])} casi casuali): scarto massimo della risposta {coef['worst_resp_db']:.1e} dB</th><td class='num'>{th(coef['bit_exact'])} identici al bit</td><td class='num'>0</td></tr>"
page = page.replace('%%TESTS%%', trs)
# robustezza
def fz(v): return ', '.join(str(x['nonfinite_samples']) for x in D[v]['fuzz']) + ' campioni non finiti (picco ' + ', '.join(f"{x['peak_dbfs']:.0f}" for x in D[v]['fuzz']) + ' dBFS)'
def blk(v): m = max(x['maxdiff_dbfs'] for x in D[v]['block']); return 'identica al bit' if m <= -600 else f'scarto max {m:.1f} dBFS'
rob = [('Progetti instabili o non finiti', f"{B['stability']['unstable_sections']} su {B['stability']['designs']}", f"{N['stability']['unstable_sections']} su {N['stability']['designs']}"),
       ('Un NaN / +Inf in ingresso (1 campione, 4 s di uscita)', f"{nanb[0]['bad_samples']} / {nanb[1]['bad_samples']} campioni non validi, non si riprende", f"{nann[0]['bad_samples']} / {nann[1]['bad_samples']}"),
       ('Un campione da 1e30 (finito ma assurdo): campioni d’uscita oltre 1e6', f"{nanb[2]['bad_samples']} ({nanb[2]['bad_samples'] / 48000:.2f} s, poi si riprende)", f"{nann[2]['bad_samples']} ({nann[2]['bad_samples'] / 48000:.2f} s, poi si riprende): è la coda del filtro, nessuna protezione"),
       ('Fuzz (3 x 60 s, parametri casuali a ogni blocco)', fz('base'), fz('new')),
       ('Parametri estremi (10 Hz/30 kHz, Q 0,025/40, &plusmn;30 dB, 8-384 kHz)', f"{B['extreme']['nonfinite']} non finiti, {B['extreme']['not_decaying_after_20s']} senza coda dopo 20 s", f"{N['extreme']['nonfinite']} non finiti, {N['extreme']['not_decaying_after_20s']} senza coda dopo 20 s ({e(N['extreme']['note'])})"),
       ('Invarianza al blocco (1, 7, 32, 64, 127, 333, 1024, 4096, casuali)', blk('base'), blk('new')),
       ('Reciprocità &plusmn;g (Bell, shelf, tilt; 44,1-96 kHz)', f"{f(B['recip_max'],3)} dB", f"{f(N['recip_max'],3)} dB")]
page = page.replace('%%ROB%%', ''.join(f"<tr><th scope='row'>{a}</th><td class='old'>{b}</td><td class='new'>{c}</td></tr>" for a, b, c in rob))
worst = ''.join(f"<tr><th scope='row'>{e(k)}</th><td class='old num'>{cw[k]['mean_us']:.1f} / {cw[k]['p999_us']:.0f} / {cw[k]['max_us']:.0f}</td><td class='new num'>{cwn[k]['mean_us']:.1f} / {cwn[k]['p999_us']:.0f} / {cwn[k]['max_us']:.0f}</td></tr>" for k in cw if k in cwn)
page = page.replace('%%WORST%%', worst)
dg = lambda v, k: D[v]['designgrid'].get(k, {}).get('mean_us', float('nan'))
dgf = lambda k: D['new']['designgridfast'].get(k, {}).get('mean_us', float('nan'))
page = page.replace('%%DESIGN%%', ''.join(f"<tr><th scope='row'>{k}</th><td class='old num'>{dg('base',k):.1f}</td><td class='new num'>{dgf(k):.1f}</td><td class='new num'>{dg('new',k):.1f}</td></tr>" for k in ['Bell', 'Shelf', 'Notch', 'LowCut', 'HighCut', 'BandPass', 'FlatTilt']))
# ---- verità scomode: anche queste dai file di misura ----
import csv
def it(x, d=1): s = ('%.' + str(d) + 'f') % x; return s.replace('-', '−').replace('.', ',')
def det_off(v, det, att=None):   # scarto del valore a regime (centro tra min e max) dal livello vero -20 dB
    xs = [(r['lo'] + r['hi']) / 2 + 20 for r in D[v]['detector_cal'] if r['det'] == det and (att is None or r['att'] == att)]
    if not xs: raise SystemExit(f'manca detector_cal {v} {det} {att}')
    return min(xs), max(xs), sum(xs) / len(xs)
rl, rh, _ = det_off('base', 'RMS'); pl, ph, _ = det_off('base', 'Peak'); r5 = det_off('base', 'RMS', 5.0)[2]; p5 = det_off('base', 'Peak', 5.0)[2]
CH = json.load(open(R + '/char_sub0.json'))['Warm']
NM = {'LowShelf': 'Low Shelf', 'HighShelf': 'High Shelf', 'LowCut': 'Low Cut', 'HighCut': 'High Cut', 'BandPass': 'Band Pass', 'Bell': 'Bell', 'Notch': 'Notch', 'TiltShelf': 'Tilt Shelf', 'FlatTilt': 'Flat Tilt'}
def gt1(path):
    rows = [r for r in csv.DictReader(open(path)) if float(r['err']) > 1.0]; g = {}
    for r in rows: g.setdefault(r['type'], []).append(r)
    parts = []
    for t, rs in sorted(g.items(), key=lambda kv: -len(kv[1])):
        fr = sorted(float(r['f']) / 1000 for r in rs); rng = lambda a, b, d: it(a, d) if abs(a - b) < 1e-9 else it(a, d) + '–' + it(b, d)
        if 'Cut' in t: sl = sorted(int(r['slope']) for r in rs); extra = f"{rng(sl[0], sl[-1], 0)} dB/oct"
        else: qs = sorted(float(r['q']) for r in rs); extra = 'Q ' + rng(qs[0], qs[-1], 1).replace(',0', '')
        parts.append(f"{NM.get(t, t)} {len(rs)} ({rng(fr[0], fr[-1], 1)} kHz, {extra})")
    fss = sorted({int(r['fs']) for r in rows}); return len(rows), max(float(r['err']) for r in rows), parts, fss
RES = L + '/res/final/new'
n1, m1, d1, f1 = gt1(RES + '/precision.csv'); nx, mx, dx, fx = gt1(RES + '/precisionx.csv')
def cmpgrid(nm):   # confronto caso per caso 0.4.3 vs 0.4.4 sulla stessa griglia
    ld = lambda q: {tuple(r[k] for k in ('fs', 'type', 'f', 'g', 'q', 'slope')): float(r['err']) for r in csv.DictReader(open(q))}
    b = ld(L + '/res/final/base/' + nm); n = ld(RES + '/' + nm)
    if set(b) != set(n): raise SystemExit('griglie diverse tra 0.4.3 e 0.4.4: ' + nm)
    worse = sorted(((n[k] - b[k], k, b[k], n[k]) for k in n if n[k] > b[k] + 0.005), reverse=True)
    same = sum(1 for k in n if n[k] > 1 and abs(n[k] - b[k]) < 0.005)
    return worse, same
W1, S1 = cmpgrid('precision.csv'); WX, SX = cmpgrid('precisionx.csv')
w0 = W1[0]; wk = w0[1]
k1 = lambda x: it(x, 1).replace(',0', '')
WDESC = f"{NM.get(wk[1], wk[1])} {k1(float(wk[2]) / 1000)} kHz Q {k1(float(wk[4]))} {wk[5]} dB/oct a {k1(int(wk[0]) / 1000)} kHz: {it(w0[2], 3)} → {it(w0[3], 3)} dB"
fsl = lambda fs: ' e '.join(it(x / 1000, 1).replace(',0', '') + ' kHz' for x in fs)
th = lambda n: f'{n:,}'.replace(',', '.')
jsr = {r['scen']: r for r in D['new']['cpu_js']}; jso = {r['scen']: r for r in D['base']['cpu_js']}
sp = lambda k: (min(jsr[k]['runs']), max(jsr[k]['runs']))
TRUTH = [
 f"<b>La dinamica della 0.4.4 interviene meno della 0.4.3 a parità di impostazioni.</b> Non è un peggioramento: la 0.4.3 leggeva il livello più alto del vero, di +{it(rl)}…+{it(rh)} dB in RMS e +{it(pl)}…+{it(ph)} dB in Peak a seconda dell'attacco (con attacco 5 ms: +{it(r5)} e +{it(p5)} dB). Una sessione salvata con la 0.4.3 suonerà diversa: per riavere lo stesso intervento la soglia va abbassata di altrettanto.",
 f"<b>Con Character attivo l'uscita non è identica al bit alla 0.4.3 nemmeno sotto 0 dBFS:</b> i nuovi filtri dell'oversampling (46 passa-tutto calcolati per campione in ogni direzione invece di 84) anticipano il segnale di {it(CH['anticipo_044_campioni'], 2)} campioni ({it(CH['anticipo_044_campioni'] / 48000 * 1e6, 1)} µs a 48 kHz). Sulla clip con Warm e picco sotto 0 dBFS il residuo grezzo è {it(CH['residuo_grezzo_db'])} dB, ma spostando la 0.4.3 dello stesso ritardo scende a {it(CH['residuo_allineato_db'])} dB: è un ritardo, non un cambio di suono. Sopra 0 dBFS invece il suono cambia davvero (la curva della 0.4.3 ripiegava): sezione Character.",
 f"<b>Il progetto preciso dei filtri costa di più</b> (media sulla griglia a 48 kHz: Bell da {it(dg('base','Bell'))} a {it(dg('new','Bell'))} µs, Band Pass da {it(dg('base','BandPass'))} a {it(dg('new','BandPass'))} µs). Per questo il plugin usa il progetto veloce mentre un parametro si muove (Bell {it(dgf('Bell'))} µs, Band Pass {it(dgf('BandPass'))} µs, meno della 0.4.3) e passa al preciso 50 ms dopo l'ultimo cambio, al massimo 2 bande per blocco.",
 f"<b>Restano errori sopra 1 dB</b> in {n1} progetti su {th(P1['n'])} della griglia principale (massimo {it(m1, 2)} dB): {'; '.join(d1)}; e in {nx} su {th(X1['n'])} della griglia estesa (massimo {it(mx, 2)} dB): {'; '.join(dx)}. Tutti a {fsl(sorted(set(f1) | set(fx)))}; {S1} dei {n1} casi della griglia principale sono identici alla 0.4.3: lì la 0.4.4 non migliora. <b>Peggiorati:</b> {len(W1)} progetti su {th(P1["n"])} hanno un errore un po’ più alto che nella 0.4.3, al massimo +{it(w0[0], 3)} dB ({WDESC}); nella griglia estesa {len(WX)}. Sezione Precisione.",
 f"<b>Il detector del Bell è un passa-banda del 2° ordine:</b> fianchi da 6 dB/oct. Un tono forte fuori banda può ancora far intervenire un po' la banda: Bell dinamico a 200 Hz e tono a 5 kHz a −10 dBFS, riduzione {it(dn[5000]['red'], 2)} dB (0.4.3: {it(db[5000]['red'], 2)} dB, tutto il RANGE).",
 f"<b>Nel motore JS (prototipo e pagina di ascolto) le bande statiche non sono più veloci:</b> 1 Bell {it(jso['1 Bell']['core_pct'], 3)} → {it(jsr['1 Bell']['core_pct'], 3)} %, 32 statiche {it(jso['32 statiche']['core_pct'], 2)} → {it(jsr['32 statiche']['core_pct'], 2)} % di un core (minimo di 7 processi; tra un processo e l'altro la 0.4.4 varia da {it(sp('32 statiche')[0], 2)} a {it(sp('32 statiche')[1], 2)} %). Il guadagno nel JS è sulla dinamica, sul Mid/Side e sul Character.",
 "<b>Non ho potuto ascoltare né provare su Mac o in una DAW:</b> i test qui sono su Linux (plugin VST3), in Chromium (interfaccia e motore JS) e sul build Mac fatto dalla CI. Il confronto con FabFilter Pro-Q non è eseguibile qui (licenza e macOS/Windows).",
]
page = page.replace('%%TRUTH%%', '\n      '.join('<li>' + t + '</li>' for t in TRUTH))
open(R + '/TSR_Q_044_prima_dopo.html', 'w', encoding='utf-8').write(page)
print('ok', len(page))
