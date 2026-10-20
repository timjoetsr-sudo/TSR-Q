import numpy as np, pedalboard as pb, time
VST="/home/claude/TSRQ/build/TSRQ_artefacts/Release/VST3/TSR Q.vst3"; FS=48000
p=pb.load_plugin(VST); p.band_1_used=True; p.band_1_gain_db=12
x=np.zeros((2,FS),np.float32); x[:,1000]=0.5
y=p(x,FS,buffer_size=512,reset=False); print("ZL picco a",int(np.argmax(np.abs(y[0]))),"lat",p.reported_latency_samples)
p.processing_mode='Linear Phase'
for k in range(5):
    time.sleep(0.3); y=p(x,FS,buffer_size=512,reset=False); print(f"chiamata {k}: picco a {int(np.argmax(np.abs(y[0])))}, latenza riportata {p.reported_latency_samples}")
