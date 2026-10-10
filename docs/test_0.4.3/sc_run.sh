cd $(dirname $0); g++ -O2 -std=c++17 -I/home/claude/tsr-q/Source sc_h.cpp -o sc_h || exit 1; N=$((48000*12))
SC="static drag solo_static solo_drag solo_q solo_toggle bypass_band used type slope place dyn gbypass character outstep instep polarity pan warm_static subtle_toggle sub_static"
for s in $SC; do ./sc_h eso_lp.f32 o_$s.f32 $N $s & done; wait
python3 - $SC <<'PY'
import numpy as np, sys; from scipy.signal import butter,sosfilt
HP=butter(10,6000,'highpass',fs=48000,output='sos'); N=48000*12
x=np.fromfile('eso_lp.f32',np.float32)[:N].astype(float)
def m(fn):
    y=np.fromfile(fn,np.float32)[:N].astype(float); r=sosfilt(HP,y)[4800:]; fr=r[:len(r)//64*64].reshape(-1,64)
    return 20*np.log10(np.sqrt((fr**2).mean(1)).max()/np.sqrt((x**2).mean())+1e-30)
print('%-12s %7.1f dB'%('ingresso',m('eso_lp.f32')))
for s in sys.argv[1:]: print('%-12s %7.1f dB'%(s,m('o_%s.f32'%s)))
PY
