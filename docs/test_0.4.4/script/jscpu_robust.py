# CPU del motore JS, robusto al rumore del JIT: per ogni scenario 7 processi separati per motore, alternati; ogni processo = mediana di 5 ripetizioni; si tiene il MINIMO
import subprocess, json
L = '/home/claude/lab'; C = {'base': L + '/base043/dsp.js', 'new': L + '/new/dsp_core.js'}; res = {'base': [], 'new': []}
for i in range(10):
    vals = {'base': [], 'new': []}; name = None
    for r in range(7):
        for v in ('base', 'new'):
            o = json.loads(subprocess.run(['node', L + '/lab.js', C[v], 'cpu1', str(i)], capture_output=True, text=True, check=True).stdout); vals[v].append(o['core_pct']); name = o['scen']
    for v in ('base', 'new'): res[v].append({'scen': name, 'fs': 48000, 'block': 128, 'core_pct': min(vals[v]), 'runs': vals[v], 'method': 'minimo di 7 processi, ciascuno mediana di 5 x 10 s'})
    print(f"{name:<48} {min(vals['base']):.3f} -> {min(vals['new']):.3f}  x{min(vals['base'])/min(vals['new']):.2f}   (base {sorted(vals['base'])[3]:.3f} / new {sorted(vals['new'])[3]:.3f} mediane)", flush=True)
for v in ('base', 'new'):
    with open(f'{L}/res/final/{v}/cpu_js.jsonl', 'w') as f:
        for r in res[v]: f.write(json.dumps(r, ensure_ascii=False) + '\n')
