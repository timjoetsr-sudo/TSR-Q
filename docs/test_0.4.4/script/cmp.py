# confronto caso per caso di due CSV di precisione (stesse righe): quanti casi migliorano/peggiorano e di quanto
import csv, sys
a = list(csv.DictReader(open(sys.argv[1]))); b = list(csv.DictReader(open(sys.argv[2]))); assert len(a) == len(b)
key = lambda r: (r['fs'], r['type'], r['f'], r['g'], r['q'], r['slope'])
worse = []; better = 0; n = len(a)
for x, y in zip(a, b):
    assert key(x) == key(y); ea, eb = float(x['err']), float(y['err'])
    if eb > ea + 0.01: worse.append((eb - ea, key(x), ea, eb))
    if eb < ea - 0.01: better += 1
worse.sort(reverse=True)
cnt = lambda rows, t: sum(1 for r in rows if float(r['err']) > t)
print('casi %d | > 1 dB: %d -> %d | > 0.5 dB: %d -> %d | > 0.1 dB: %d -> %d | max %.3f -> %.3f dB' % (n, cnt(a, 1), cnt(b, 1), cnt(a, .5), cnt(b, .5), cnt(a, .1), cnt(b, .1), max(float(r['err']) for r in a), max(float(r['err']) for r in b)))
print('migliori di > 0.01 dB: %d | peggiori di > 0.01 dB: %d (> 0.05: %d)' % (better, len(worse), sum(1 for w in worse if w[0] > 0.05)))
for w in worse[:8]: print('  peggiore +%.3f dB' % w[0], w[1], '%.3f -> %.3f' % (w[2], w[3]))
