# Pagina di ascolto A/B: interfaccia TSR Q 0.4.4 + clip ESO - SAFE + DUE motori DSP veri nello stesso AudioWorklet (0.4.3 e 0.4.4) + percorso DRY.
# Ogni percorso e misurato in loudness (BS.1770, K-weighting, gating) mentre suona; con LOUDNESS MATCH i due motori sono riportati alla loudness del DRY.
import sys, re, hashlib
src, core043, dst = sys.argv[1], sys.argv[2], sys.argv[3]
s = open(src, encoding='utf-8').read(); c3 = open(core043, encoding='utf-8').read()
def rep(a, b, cnt=1):
    global s
    assert s.count(a) == cnt, (a[:90], s.count(a)); s = s.replace(a, b)
# 1) motore 0.4.3 come secondo script, con nome globale proprio (TSRQ043): stesso testo del core 0.4.3 congelato
c3 = c3.replace('var TSRQ = (function () {', 'var TSRQ043 = (function () {', 1)
c3 = re.sub(r"if \(typeof module !== ['\"]undefined['\"]\) module\.exports = TSRQ;", '', c3)
assert 'var TSRQ043 = (function () {' in c3 and 'module.exports' not in c3
rep('<script id="dsp">', '<script id="dsp043">' + c3 + '</script>\n<script id="dsp">')
# 2) il modulo dell'AudioWorklet contiene entrambi i core
rep("await ctx.audioWorklet.addModule(URL.createObjectURL(new Blob([DSP + '\\n' + WK], { type: 'application/javascript' })));",
    "await ctx.audioWorklet.addModule(URL.createObjectURL(new Blob([DSP + '\\n' + document.getElementById('dsp043').textContent + '\\n' + WK], { type: 'application/javascript' })));")
# 3) processore A/B (dentro un template literal: niente backtick e niente dollaro-graffa)
i = s.index("\\nclass P extends AudioWorkletProcessor{"); j = s.index("registerProcessor('tsrq',P);`;") + len("registerProcessor('tsrq',P);")
AB = r"""\nclass LM{constructor(fs){var f0=1681.974450955533,G=3.999843853973347,Q=0.7071752369554196,K=Math.tan(Math.PI*f0/fs),Vh=Math.pow(10,G/20),Vb=Math.pow(Vh,0.4996667741545416),a0=1+K/Q+K*K;
this.c1=[(Vh+Vb*K/Q+K*K)/a0,2*(K*K-Vh)/a0,(Vh-Vb*K/Q+K*K)/a0,2*(K*K-1)/a0,(1-K/Q+K*K)/a0];f0=38.13547087602444;Q=0.5003270373238773;K=Math.tan(Math.PI*f0/fs);var a=1+K/Q+K*K;
this.c2=[1,-2,1,2*(K*K-1)/a,(1-K/Q+K*K)/a];this.z=new Float64Array(8);this.sub=Math.round(fs*0.1);this.acc=0;this.cnt=0;this.bl=[];}
reset(){this.bl.length=0;this.acc=0;this.cnt=0;}
add(L,R,n){var c1=this.c1,c2=this.c2,z=this.z;for(var i=0;i<n;i++){var s=0;for(var ch=0;ch<2;ch++){var x=ch?R[i]:L[i],o=ch*4,y=c1[0]*x+z[o];z[o]=c1[1]*x-c1[3]*y+z[o+1];z[o+1]=c1[2]*x-c1[4]*y;
var y2=c2[0]*y+z[o+2];z[o+2]=c2[1]*y-c2[3]*y2+z[o+3];z[o+3]=c2[2]*y-c2[4]*y2;s+=y2*y2;}this.acc+=s;if(++this.cnt===this.sub){this.bl.push(this.acc/this.sub);this.acc=0;this.cnt=0;if(this.bl.length>1800)this.bl.shift();}}}
st(){var n=this.bl.length;if(n<30)return null;var m=0;for(var i=n-30;i<n;i++)m+=this.bl[i];return -0.691+10*Math.log10(m/30+1e-30);}
it(){var n=this.bl.length;if(n<4)return null;var w=[],i,k,m=0;for(i=3;i<n;i++)w.push((this.bl[i]+this.bl[i-1]+this.bl[i-2]+this.bl[i-3])/4);
var a=[];for(k=0;k<w.length;k++)if(-0.691+10*Math.log10(w[k]+1e-30)>-70)a.push(w[k]);if(!a.length)return -Infinity;for(k=0;k<a.length;k++)m+=a[k];m/=a.length;
var thr=-0.691+10*Math.log10(m)-10,b=0,c=0;for(k=0;k<a.length;k++)if(-0.691+10*Math.log10(a[k])>thr){b+=a[k];c++;}return c?-0.691+10*Math.log10(b/c):null;}}
class P extends AudioWorkletProcessor{constructor(){super();var N=4096;this.e=new TSRQ.Engine(sampleRate,N);this.e3=new TSRQ043.Engine(sampleRate,N);this.w=new WT();this.t=0;this.t2=0;
this.pl=new Float32Array(N);this.pr=new Float32Array(N);this.al=new Float32Array(N);this.ar=new Float32Array(N);this.dl=new Float32Array(N);this.dr=new Float32Array(N);
this.sel=2;this.wt=[0,0,1,0];this.w0=[0,0,1,0];this.match=true;this.boost=1;this.g=[1,1,1,1];this.gt=[1,1,1,1];this.lm=[new LM(sampleRate),new LM(sampleRate),new LM(sampleRate),new LM(sampleRate)];this.since=0;this.Iv=null;this.Sv=null;
this.chm=0;this.K=8;this.hb=[new Float64Array(32),new Float64Array(32),new Float64Array(32),new Float64Array(32)];this.hp=0;this.fir=this.mkFir(0.6);
this.port.onmessage=e=>{var d=e.data;if(d&&d.ab){if(d.sel!==undefined)this.sel=d.sel;if(d.match!==undefined)this.match=!!d.match;if(d.boost!==undefined)this.boost=d.boost;if(d.reset)this.resetLM();return;}
this.chm=d&&d.character?d.character:0;try{this.e.setState(d);}catch(x){this.port.postMessage({err:x.message});}try{this.e3.setState(d);}catch(x){this.port.postMessage({err:'0.4.3: '+x.message});}this.resetLM();};}
resetLM(){for(var k=0;k<4;k++)this.lm[k].reset();this.since=0;}
mkFir(fd){var K=this.K,h=new Float64Array(2*K),s=0,i;for(i=0;i<2*K;i++){var t=i-(K+fd),w=0.5-0.5*Math.cos(2*Math.PI*(i+0.5)/(2*K));h[i]=(Math.abs(t)<1e-9?1:Math.sin(Math.PI*t)/(Math.PI*t))*w;s+=h[i];}for(i=0;i<2*K;i++)h[i]/=s;return h;}
process(ins,outs){var o=outs[0];if(!o||!o[0])return true;var n=o[0].length,i=ins[0],L=o[0],R=o[1]||o[0],k,s;if(i&&i[0]){L.set(i[0]);R.set(i[1]||i[0]);}else{L.fill(0);R.fill(0);}
this.pl.set(L.subarray(0,n));this.pr.set(R.subarray(0,n));this.al.set(L.subarray(0,n));this.ar.set(R.subarray(0,n));
this.e3.process(this.al,this.ar,n);this.e.process(L,R,n);
var H=this.hb,h=this.fir,K=this.K,al=this.chm?1:0;for(k=0;k<n;k++){var p=this.hp=(this.hp+1)&31;H[0][p]=L[k];H[1][p]=R[k];H[2][p]=this.al[k];H[3][p]=this.ar[k];if(al){var sl=0,sr=0;for(var j=0;j<2*K;j++){var q=(p-j)&31;sl+=h[j]*H[0][q];sr+=h[j]*H[1][q];}var q2=(p-K)&31;this.dl[k]=sl-H[2][q2];this.dr[k]=sr-H[3][q2];}else{var q3=(p-K)&31;this.dl[k]=H[0][q3]-H[2][q3];this.dr[k]=H[1][q3]-H[3][q3];}}
this.lm[0].add(this.pl,this.pr,n);this.lm[1].add(this.al,this.ar,n);this.lm[2].add(L,R,n);this.lm[3].add(this.dl,this.dr,n);this.since+=n;
if((this.t2+=n)>=sampleRate/4){this.t2=0;var I=[this.lm[0].it(),this.lm[1].it(),this.lm[2].it(),this.lm[3].it()];this.Iv=I;this.Sv=[this.lm[0].st(),this.lm[1].st(),this.lm[2].st(),this.lm[3].st()];
if(this.match){if(this.since>=1.5*sampleRate&&I[0]!==null&&I[1]!==null&&I[2]!==null&&isFinite(I[0])&&isFinite(I[1])&&isFinite(I[2])){this.gt[1]=Math.pow(10,Math.max(-24,Math.min(24,I[0]-I[1]))/20);this.gt[2]=Math.pow(10,Math.max(-24,Math.min(24,I[0]-I[2]))/20);this.gt[3]=this.gt[2];}}
else{this.gt[1]=this.gt[2]=this.gt[3]=1;}}
var a=1-Math.exp(-n/(0.2*sampleRate));for(k=1;k<4;k++)this.g[k]+=(this.gt[k]-this.g[k])*a;
var st=n/(0.02*sampleRate),w=this.wt,w0=this.w0,sel=this.sel,g=this.g,bo=this.boost;for(k=0;k<4;k++){w0[k]=w[k];var tg=k===sel?1:0;w[k]=tg>w[k]?Math.min(tg,w[k]+st):Math.max(tg,w[k]-st);}
var g0=g[0],g1=g[1],g2=g[2],g3=g[3]*bo;
for(s=0;s<n;s++){var u=(s+1)/n,a0=w0[0]+(w[0]-w0[0])*u,a1=w0[1]+(w[1]-w0[1])*u,a2=w0[2]+(w[2]-w0[2])*u,a3=w0[3]+(w[3]-w0[3])*u;
var l=a0*g0*this.pl[s]+a1*g1*this.al[s]+a2*g2*L[s]+a3*g3*this.dl[s],r=a0*g0*this.pr[s]+a1*g1*this.ar[s]+a2*g2*R[s]+a3*g3*this.dr[s];L[s]=l;R[s]=r;}
this.w.add(this.pl,this.pr,L,R,n);
if((this.t+=n)>=sampleRate/30){this.t=0;var m3=this.e3.meter(),m4=this.e.meter(),m=sel===1?m3:m4;m.w=this.w.take();
m.ab={I:this.Iv,S:this.Sv,g:[0,20*Math.log10(g1),20*Math.log10(g2),20*Math.log10(g[3])],sel:sel,match:this.match,boost:bo,since:this.since/sampleRate};this.port.postMessage(m);}
return true;}}
registerProcessor('tsrq',P);"""
assert '`' not in AB and '${' not in AB
s = s[:i] + AB + s[j:]
# 4) letture A/B dal motore
rep("node.port.onmessage = e => { if (e.data.err) flash('errore motore: ' + e.data.err); else onMeter(e.data); }; engineKind = 'AudioWorklet'; }",
    "node.port.onmessage = e => { if (e.data.err) flash('errore motore: ' + e.data.err); else { onMeter(e.data); if (e.data.ab && window.__tsrqAB) window.__tsrqAB(e.data.ab); } }; engineKind = 'AudioWorklet'; }")
# 5) spazio per due barre in basso
rep('window.__tsrqBar = 64;', 'window.__tsrqBar = 64 + 92;')
rep('<title>TSR Q · Test ESO – SAFE</title>', '<title>TSR Q Ascolto A/B</title>')
# 6) barra A/B, scenari, letture
BAR = r'''<script>'use strict';
/* ====== ASCOLTO A/B: DRY / 0.4.3 / 0.4.4 / DIFF sullo stesso segnale, DSP vero (i due core girano insieme nell'AudioWorklet) ====== */
(() => {
  const st = document.createElement('style'); st.textContent = 'body{padding-bottom:156px}' +
    '.abbar{position:fixed;left:0;right:0;bottom:64px;min-height:92px;box-sizing:border-box;padding:8px 12px;display:flex;flex-wrap:wrap;align-items:center;justify-content:center;gap:6px 8px;background:#101215;border-top:1px solid rgba(255,255,255,.10);z-index:1001;font:700 12px/1.2 "Barlow Semi Condensed","Arial Narrow",system-ui,sans-serif;letter-spacing:.05em;color:#c9ced6}' +
    '.abbar .grp{display:flex;align-items:center;gap:6px;flex-wrap:wrap;justify-content:center}.abbar .lb{color:#8b939d;font-size:11px;margin-right:2px}' +
    '.abbar button{background:#1b1f24;border:1px solid rgba(255,255,255,.16);border-radius:3px;color:#eef2f6;font:inherit;letter-spacing:inherit;height:28px;padding:0 10px;cursor:pointer}' +
    '.abbar button.on{background:#ffd23f;color:#1b1b1b;border-color:#ffd23f}.abbar button.pre.on{background:#5ac8fa;border-color:#5ac8fa;color:#0a0b0c}' +
    '.abbar .ro{flex-basis:100%;text-align:center;font-weight:600;font-size:12px;color:#c9ced6;font-variant-numeric:tabular-nums;min-height:15px}.abbar .ro b{color:#eef2f6}.abbar .ro .w{color:#ffd23f}' +
    '.abbar .nt{flex-basis:100%;text-align:center;font-weight:500;font-size:11px;color:#8b939d;letter-spacing:.02em}';
  document.head.appendChild(st);
  const bar = document.createElement('div'); bar.className = 'abbar';
  bar.innerHTML = '<div class="grp"><span class="lb">ASCOLTO</span><button data-s="0">DRY</button><button data-s="1">0.4.3 PRIMA</button><button data-s="2" class="on">0.4.4 DOPO</button><button data-s="3">DIFF 0.4.4 &minus; 0.4.3</button><button data-a="boost">DIFF +20 dB</button><button data-a="match" class="on">LOUDNESS MATCH</button></div>' +
    '<div class="grp"><span class="lb">SCENARI</span><button class="pre" data-p="dyn200">Bell dinamico 200 Hz</button><button class="pre" data-p="kick">Kick 60 Hz Peak</button><button class="pre" data-p="warm">Warm +12 dB</button><button class="pre" data-p="subtle">Subtle +12 dB</button><button class="pre" data-p="eq">EQ statica</button><button class="pre" data-p="free">Libero</button></div>' +
    '<div class="ro">premi PLAY: i tre percorsi suonano insieme, ne senti uno</div><div class="nt"></div>';
  document.body.appendChild(bar);
  const ro = bar.querySelector('.ro'), nt = bar.querySelector('.nt');
  const NOTE = { dyn200: 'Bell 200 Hz, soglia -30, range -6, RMS. 0.4.3: detector passa-alto (reagisce anche a voce e piatti) e +2,9 dB di lettura: comprime di piu e nei momenti sbagliati.',
    kick: 'Bell 60 Hz Q 1,2, soglia -24, range -6, Peak, attacco 1 ms. 0.4.3: Peak letto +6 dB: interviene molto prima.', warm: 'INPUT +12 dB, Character Warm (OUTPUT a 0: agisce prima del Character; il volume lo pareggia il loudness match). 0.4.3: oltre 0 dBFS la curva ripiega (i picchi negativi tornano verso zero): distorsione dura. Con il Character attivo DIFF e allineato: la 0.4.4 e ritardata di 0,6 campioni (i nuovi filtri dell\u2019oversampling anticipano di tanto, inudibile da solo), cosi senti solo la differenza della curva.',
    subtle: 'INPUT +12 dB, Character Subtle (OUTPUT a 0). 0.4.3: il polinomio ripiega oltre -2,2/+2,5. DIFF allineato di 0,6 campioni come per Warm.', eq: 'Low Cut 30 Hz 24 dB/oct, Bell 250 Hz -3, Bell 3 kHz +2, High Shelf 10 kHz +2,5. Qui la differenza e minima: ascolta DIFF.', free: 'Bande libere: tutto quello che cambi nel plugin vale per entrambi i motori.' };
  const post = m => { if (typeof node !== 'undefined' && node && node.port) node.port.postMessage(Object.assign({ ab: 1 }, m)); };   /* node: variabile globale dello script principale */
  let sel = 2, match = true, boost = false;
  const paint = () => { bar.querySelectorAll('[data-s]').forEach(b => b.classList.toggle('on', +b.dataset.s === sel)); bar.querySelector('[data-a=match]').classList.toggle('on', match); bar.querySelector('[data-a=boost]').classList.toggle('on', boost); };
  bar.querySelectorAll('[data-s]').forEach(b => b.onclick = () => { sel = +b.dataset.s; post({ sel }); paint(); });
  bar.querySelector('[data-a=match]').onclick = () => { match = !match; post({ match }); paint(); };
  bar.querySelector('[data-a=boost]').onclick = () => { boost = !boost; post({ boost: boost ? 10 : 1 }); paint(); };
  const preset = k => { const P = window.__tsrq.P(); P.bands.forEach((b, i) => Object.assign(b, defBand(i))); Object.assign(P, { inp: 0, out: 0, char: 0, auto: 0, scale: 100, bypass: 0, pan: 0, panMS: 0, inv: 0, gq: 0 });
    const set = (i, o) => Object.assign(P.bands[i], { used: 1, byp: 0 }, o);
    if (k === 'dyn200') set(0, { type: 0, freq: 200, gain: 0, q: 1, dyn: 1, thr: -30, range: -6, att: 5, rel: 80, knee: 6, det: 0 });
    if (k === 'kick') set(0, { type: 0, freq: 60, gain: 0, q: 1.2, dyn: 1, thr: -24, range: -6, att: 1, rel: 60, knee: 6, det: 1 });
    if (k === 'warm') Object.assign(P, { inp: 12, out: 0, char: 2 });
    if (k === 'subtle') Object.assign(P, { inp: 12, out: 0, char: 1 });
    if (k === 'eq') { set(0, { type: 3, freq: 30, q: 0.71, slope: 3 }); set(1, { type: 0, freq: 250, gain: -3, q: 1 }); set(2, { type: 0, freq: 3000, gain: 2, q: 1 }); set(3, { type: 2, freq: 10000, gain: 2.5, q: 0.71, slope: 1 }); }
    if (k !== 'free') { commit(); if (window.__tsrq.graph.selectOnly) window.__tsrq.graph.selectOnly(P.bands[0].used ? 0 : -1); }
    bar.querySelectorAll('[data-p]').forEach(b => b.classList.toggle('on', b.dataset.p === k)); nt.textContent = NOTE[k] || ''; post({ reset: 1 }); };
  bar.querySelectorAll('[data-p]').forEach(b => b.onclick = () => preset(b.dataset.p));
  const f1 = v => v === null || v === undefined ? '…' : v === -Infinity ? '&lt; -70' : v.toFixed(1), sg = v => (v >= 0 ? '+' : '') + v.toFixed(1);
  window.__tsrqAB = a => { if (!a) return; if (a.sel !== sel || a.match !== match || (a.boost > 1) !== boost) post({ sel, match, boost: boost ? 10 : 1 });   /* il motore nasce al primo PLAY: si allinea qui */
    const I = a.I || [], S = a.S || [], wait = a.since < 1.5;
    ro.innerHTML = 'LUFS integrati dall’ultimo cambio (' + a.since.toFixed(0) + ' s): DRY <b>' + f1(I[0]) + '</b> · 0.4.3 <b>' + f1(I[1]) + '</b> · 0.4.4 <b>' + f1(I[2]) + '</b> · DIFF <b>' + f1(I[3]) + '</b>' +
      (a.match ? ' — compensazione: 0.4.3 <span class="w">' + sg(a.g[1]) + ' dB</span> · 0.4.4 <span class="w">' + sg(a.g[2]) + ' dB</span>' + (wait ? ' (in misura…)' : '') : ' — loudness match spento') +
      (I[3] === -Infinity ? ' · DIFF sotto la soglia assoluta BS.1770 (-70 LUFS): non udibile' : (I[3] !== null && I[3] !== undefined && I[2] !== null && isFinite(I[2])) ? ' · DIFF ' + (I[2] - I[3]).toFixed(1) + ' LU sotto 0.4.4' : ''); };
  const fitBars = () => { window.__tsrqBar = 64 + bar.getBoundingClientRect().height; document.body.style.paddingBottom = window.__tsrqBar + 'px'; if (typeof doFit === 'function') doFit(); };
  addEventListener('resize', fitBars); setTimeout(fitBars, 0); setTimeout(fitBars, 600);
  setTimeout(() => preset('dyn200'), 300);
})();
</script></body></html>'''
rep('</script></body></html>', '</script>\n' + BAR)
open(dst, 'w', encoding='utf-8').write(s)
print('ok', dst, len(s) / 1e6, 'MB', hashlib.sha256(s.encode()).hexdigest()[:16])
