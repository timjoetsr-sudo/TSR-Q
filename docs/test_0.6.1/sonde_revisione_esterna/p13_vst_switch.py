# plugin VST3 vero: cambio di processing_mode tra due chiamate senza nuovo prepare (reset=False), come un'automazione o un clic durante la riproduzione
import numpy as np, pedalboard as pb, time, sys
VST="/home/claude/TSRQ/build/TSRQ_artefacts/Release/VST3/TSR Q.vst3"; FS=48000
p=pb.load_plugin(VST); p.band_1_used=True; p.band_1_type='Bell'; p.band_1_freq_hz=1000; p.band_1_gain_db=12; p.band_1_q=1
t=np.arange(FS)/FS; x=np.stack([0.1*np.sin(2*np.pi*1000*t)]*2).astype(np.float32)
y=p(x,FS,buffer_size=512,reset=False); print("ZL: latenza riportata",p.reported_latency_samples,"livello finale %.2f dB"%(20*np.log10(np.max(np.abs(y[0,-4800:]))/0.1)))
p.processing_mode='Linear Phase'
for k in range(4):
    time.sleep(0.2)
    y=p(x,FS,buffer_size=512,reset=False); print(f"dopo il cambio, chiamata {k}: latenza riportata {p.reported_latency_samples}, livello finale {20*np.log10(np.max(np.abs(y[0,-4800:]))/0.1+1e-12):.2f} dB, primo campione non nullo {int(np.argmax(np.abs(y[0])>1e-6))}")
p2=pb.load_plugin(VST); p2.processing_mode='Linear Phase'; p2.band_1_used=True; p2.band_1_gain_db=12; y=p2(x,FS,buffer_size=512); print("istanza nuova in LP: latenza",p2.reported_latency_samples)
