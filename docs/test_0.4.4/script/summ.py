import csv, collections, statistics as st, sys
fn=sys.argv[1]; rows=list(csv.DictReader(open(fn)))
agg=collections.defaultdict(list)
for r in rows: agg[(int(float(r['fs'])), r['type'])].append((float(r['err']), r))
print('%-7s %-10s %5s %8s %8s %9s  peggiore'%('fs','tipo','n','max dB','p99','mediana'))
for fs in (44100,48000,88200,96000,192000):
    for t in ['Bell','LowShelf','HighShelf','TiltShelf','LowCut','HighCut','Notch','BandPass','FlatTilt']:
        v=agg[(fs,t)]; e=sorted(x[0] for x in v); w=max(v,key=lambda x:x[0])[1]
        print('%-7d %-10s %5d %8.4f %8.4f %9.5f  f %s g %s q %s sl %s @%s'%(fs,t,len(e),e[-1],e[int(len(e)*.99)-1],st.median(e),w['f'],w['g'],w['q'],w['slope'],w['worstF']))
big=[r for r in rows if float(r['err'])>1.0]; print('casi > 1 dB:',len(big),' > 0.5 dB:',sum(1 for r in rows if float(r['err'])>0.5),' > 0.1 dB:',sum(1 for r in rows if float(r['err'])>0.1),' su',len(rows))
# casi "musicali": f <= 16 kHz e Q <= 10
mus=[r for r in rows if float(r['f'])<=16000 and float(r['q'])<=10]
print('f <= 16 kHz, Q <= 10: max %.3f dB, casi > 0.5 dB: %d su %d'%(max(float(r['err']) for r in mus), sum(1 for r in mus if float(r['err'])>0.5), len(mus)))
