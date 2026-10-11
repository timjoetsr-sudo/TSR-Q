# raccoglie TUTTI i numeri del report dai file prodotti dai test (nessun numero scritto a mano): res/final/{base,new}, final_logs, ab/out
import json, csv, re, os, glob, collections, statistics
L = '/home/claude/lab'; F = L + '/res/final'; FL = L + '/final_logs'; OUT = {}
def rj(p): return json.load(open(p))
def prec(fn):
    rows = list(csv.DictReader(open(fn))); agg = collections.defaultdict(float); e = [float(r['err']) for r in rows]
    for r in rows: k = (int(float(r['fs'])), r['type']); agg[k] = max(agg[k], float(r['err']))
    return {'n': len(rows), 'gt1': sum(x > 1 for x in e), 'gt05': sum(x > 0.5 for x in e), 'gt01': sum(x > 0.1 for x in e), 'max': max(e), 'median': statistics.median(e),
            'by': {f'{fs}|{t}': round(v, 4) for (fs, t), v in sorted(agg.items())}}
for v in ('base', 'new'):
    d = {}
    d['precision'] = prec(f'{F}/{v}/precision.csv'); d['precisionx'] = prec(f'{F}/{v}/precisionx.csv')
    rc = list(csv.DictReader(open(f'{F}/{v}/recip.csv'))); d['recip_max'] = max(float(r['dev']) for r in rc)
    for k in ('stability', 'block', 'nan', 'extreme', 'dynacc', 'cpuauto', 'cpuautohf', 'designgrid', 'designgridfast', 'designcost', 'cpuworst', 'denormal', 'sigverify', 'nan_js', 'setstate_js'):
        p = f'{F}/{v}/{k}.json'
        try: d[k] = rj(p)
        except Exception as ex: d[k] = {'ERRORE': str(ex)}
    d['fuzz'] = [json.loads(l) for l in open(f'{F}/{v}/fuzz.json') if l.strip()]
    d['cpu'] = rj(f'{F}/{v}/cpu.json'); d['cpu_js'] = [json.loads(l) for l in open(f'{F}/{v}/cpu_js.jsonl') if l.strip()]
    d['cpuchar'] = open(f'{F}/{v}/cpuchar.txt').read().strip().split('\n')
    d['detector_cal'] = [dict(det=m.group(1).strip(), att=float(m.group(2)), f=float(m.group(3)), lo=float(m.group(4)), hi=float(m.group(5))) for m in re.finditer(r'det (\S+)\s+att\s+([\d.]+) ms f\s+(\d+) Hz: livello letto (-?[\d.]+) \.\. (-?[\d.]+) dB', open(f'{F}/{v}/detector_cal.txt').read())]
    d['detector_band'] = [dict(f=float(m.group(1)), red=float(m.group(2)), lvl=float(m.group(3))) for m in re.finditer(r'tono\s+(\d+) Hz -10 dBFS: riduzione dinamica (-?[\d.]+) dB, livello letto dal detector (-?[\d.]+) dB', open(f'{F}/{v}/detector_band.txt').read())]
    d['dyn_residue'] = open(f'{F}/{v}/dyn_residue.txt').read()
    OUT[v] = d
OUT['hover_ui'] = open(F + '/hover_ui.txt').read()
def cnt(fn, pat=r'^PASS'): t = open(fn).read(); return {'pass': len(re.findall(r'^PASS', t, re.M)), 'fail': len(re.findall(r'^FAIL', t, re.M))}
T = {}
T['dsp_tests'] = cnt(FL + '/dsp_tests_044.log'); T['stem_tests'] = cnt(FL + '/stem_tests_044.log'); T['selftest'] = cnt(FL + '/selftest_044.txt')
T['clip_vst'] = cnt(FL + '/clip_vst_044.log'); T['confronto'] = cnt(FL + '/confronto_044.log')
pv = open(FL + '/pluginval_044.txt').read(); T['pluginval'] = {'ok': 'SUCCESS' in pv and 'exit 0' in pv, 'groups': len(re.findall('Completed tests', pv))}
UI = L + '/web/uitest/log'; T['ui'] = {os.path.basename(f)[:-4]: cnt(f) for f in sorted(glob.glob(UI + '/*.log'))}
P = L + '/par'; T['parity'] = {os.path.basename(f)[:-4]: re.findall(r'RISULTATO[^\n]*', open(f).read())[-1] for f in sorted(glob.glob(P + '/log_new/*.log'))}
T['parity']['scenari'] = re.findall(r'RISULTATO[^\n]*', open(P + '/sc_new.log').read())[-1]
T['coef'] = open(P + '/coef_new.log').read().split('\n')[0]
OUT['tests'] = T
try: OUT['ab'] = rj(L + '/ab/out/summary.json')
except Exception as ex: OUT['ab'] = {'ERRORE': str(ex)}
# residuo per finestre da 50 ms (dai FLAC normalizzati: stesso guadagno per 0.4.4 e DIFF, il rapporto e quello vero)
import numpy as np, subprocess
def dec(fn): return np.frombuffer(subprocess.run(['ffmpeg', '-hide_banner', '-loglevel', 'error', '-i', fn, '-f', 'f32le', '-ac', '2', '-'], capture_output=True, check=True).stdout, np.float32).reshape(-1, 2)
for sc in OUT['ab'].get('scenarios', []):
    k = sc['key']; d = L + '/ab/out/'; y = dec(d + k + '_044.flac')
    for tag, fn in (('diff', d + k + '_diff.flac'), ('diffal', d + k + '_diffal.flac')):
        if not os.path.exists(fn): continue
        r = dec(fn); W = 2400; n = min(len(y), len(r)) // W * W; ys = (y[:n] ** 2).sum(1).reshape(-1, W).mean(1); rs = (r[:n] ** 2).sum(1).reshape(-1, W).mean(1); rel = 10 * np.log10(rs / (ys + 1e-30) + 1e-30)
        sc[tag + '_win50'] = {'median': round(float(np.median(rel)), 1), 'p99': round(float(np.percentile(rel, 99)), 1), 'max': round(float(rel.max()), 1)}
OUT['clip_vst_text'] = open(FL + '/clip_vst_044.log').read(); OUT['confronto_text'] = open(FL + '/confronto_044.log').read()
json.dump(OUT, open(L + '/report/data.json', 'w'), ensure_ascii=False, indent=1)
print('ok', os.path.getsize(L + '/report/data.json'), 'byte')
