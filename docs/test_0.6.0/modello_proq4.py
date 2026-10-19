# Modello analogico delle forme di FabFilter Pro-Q 4, ricavato dalle risposte misurate a 192 kHz (proq4_192k.npz: 197 casi x 160 frequenze, f = 1 kHz).
# Uso: python3 modello_proq4.py  -> scarto massimo modello - Pro-Q 4 per forma (sopra -40 e -80 dB)
import numpy as np
from scipy.optimize import minimize
import os
d=np.load(os.path.join(os.path.dirname(os.path.abspath(__file__)),'proq4_192k.npz')); A=d['A']; F=d['F']; C=[c.split('|') for c in d['cases']]
x=1j*F/1000.0
def bq(N): return sorted([1/(2*np.cos(np.pi*(2*k+1)/(2*N))) for k in range(N//2)] if N%2==0 else [1/(2*np.cos(np.pi*k/N)) for k in range(1,(N-1)//2+1)])
P=dict(sc=0.70335,sp=0.46594,c12=0.0871,q12=2.78,a18=0.0779,a24=0.0843,a36=0.0838,qmax=28.2843)
def shelfQ(Q,P): return P['sc']*Q**P['sp']
def model(sh,g,Q,sl,P):
    A_=10**(g/40); sA=np.sqrt(A_)
    if sh=='Bell': Qe=Q/np.sqrt(2); return (x*x+x*A_/Qe+1)/(x*x+x/(A_*Qe)+1)
    if sh=='Notch': Qe=Q/np.sqrt(2); return (x*x+1)/(x*x+x/Qe+1)
    if sh in('Low Shelf','High Shelf'):
        if sl<=6: return (A_*A_+A_*x)/(1+A_*x) if sh=='Low Shelf' else (A_+A_*A_*x)/(A_+x)
        q=shelfQ(Q,P)
        if sh=='Low Shelf': return (A_*A_+A_*sA/q*x+A_*x*x)/(1+sA/q*x+A_*x*x)
        return (A_+A_*sA/q*x+A_*A_*x*x)/(A_+sA/q*x+x*x)
    if sh=='Tilt Shelf':
        T=10**(g/20); sT=np.sqrt(T); q=shelfQ(Q,P)
        return (T+T*sT/q*x+T*T*x*x)/(T+sT/q*x+x*x)/T
    if sh=='Flat Tilt':
        S=2*g/np.log2(1000); D=1.5; r=10**(S*D/20); H=np.ones_like(x); z=4.0
        while z<60000: H*=(1+1j*F/z)/(1+1j*F/(z*r)); z*=2**D
        H/=np.interp(1000,F,np.abs(H)); return H
    if sh in('Low Cut','High Cut'):
        hp=sh=='Low Cut'; N=int(sl)//6; qs=bq(N); H=np.ones_like(x)
        if N%2: H*=(x/(1+x)) if hp else 1/(1+x)
        a={12:P['c12']/min(Q,P['q12']),18:P['a18'],24:P['a24'],36:P['a36']}.get(int(sl),0.0)
        top=max(qs) if qs else None
        for j,qi in enumerate(qs):
            qe=min(qi*Q,P['qmax']) if qi==top else qi
            if j==0: num=(x*x+a*x) if hp else (1+a*x)
            else: num=x*x if hp else 1
            H*=num/(1+x/qe+x*x)
        return H
    return None
def errs(P,lim=-40,show=False,only=None):
    out=[]
    for i,(sh,g,q,sl) in enumerate(C):
        if only and sh not in only: continue
        H=model(sh,float(g),float(q),int(sl),P)
        if H is None: continue
        m=(A[i]>lim)&(F<15000)
        e=np.max(np.abs(20*np.log10(np.abs(H)+1e-300)-A[i])[m]); out.append((sh,g,q,sl,e))
    return out
if __name__=='__main__':
    for lim in (-40,-80):
        r=errs(P,lim); w={}
        for sh,g,q,sl,e in r: k=sh+' '+sl; w[k]=max(w.get(k,0),e)
        print('soglia',lim); [print('  %-16s %.3f'%(k,v)) for k,v in w.items()]
