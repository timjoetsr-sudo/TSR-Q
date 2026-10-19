// parità JS = C++ sulle forme 0.6.0 (uguali a Pro-Q 4): Bell, Notch, shelf, tilt, flat tilt, tagli con Q e zero, 30 dB/oct invariato
const vm=require('vm'),fs=require('fs'),cp=require('child_process');
const ctx={Math,Float64Array,Float32Array,Array,Error,Object,console};vm.createContext(ctx);vm.runInContext(fs.readFileSync(__dirname+'/dsp_cur.js','utf8')+';this.T=TSRQ;',ctx);const T=ctx.T;
const D=__dirname+'/../stems/',N=1047273,FS=48000,TYPES=['bell','lshelf','hshelf','lcut','hcut','notch','bpass','tilt','flat','allpass'];
let pass=0,fail=0;const ok=(c,m)=>{console.log((c?'PASS ':'FAIL ')+m);c?pass++:fail++;};
const load=n=>{const b=fs.readFileSync(D+n+'.f32');const a=new Float32Array(b.buffer,b.byteOffset,b.length/4);return [a.slice(0,N),a.slice(N,2*N)];};
const db=x=>20*Math.log10(Math.max(x,1e-15));
const state=ch=>({bands:ch.map((c,i)=>({id:i,type:c[0],f:c[1],g:c[2],q:c[3],slope:c[4],place:c[5],on:true,dyn:c.length>6?{on:true,thr:c[6],range:c[7],att:5,rel:80,knee:6,det:0}:{on:false}})),inGain:0,out:0,autoGainDb:0,scale:1,character:0,bypass:false,solo:-1});
const run=(L0,R0,st)=>{const e=new T.Engine(FS,4096);e.setState(st);const L=new Float32Array(L0),R=new Float32Array(R0);for(let i0=0;i0<N;i0+=512){const n=Math.min(512,N-i0);e.setState(st);e.process(L.subarray(i0,i0+n),R.subarray(i0,i0+n),n);}return [L,R];};
const md=(a,b)=>{let m=0;for(let i=0;i<a.length;i++)m=Math.max(m,Math.abs(a[i]-b[i]));return m;};
const cases={'Bell Q 0,3 +12':[['bell',800,12,.3,12,0]],'Notch Q 2':[['notch',3000,0,2,12,0]],'Low Shelf Q 3 +6':[['lshelf',120,6,3,12,0]],'High Shelf Q 0,3 -9':[['hshelf',6000,-9,.3,12,0]],
 'Tilt 12 dB/oct +6':[['tilt',1000,6,1,12,0]],'Tilt 6 dB/oct +6':[['tilt',1000,6,1,6,0]],'Flat Tilt +6':[['flat',1000,6,1,12,0]],'Low Cut 12 Q 0,5':[['lcut',90,0,.5,12,0]],'Low Cut 18 Q 3':[['lcut',150,0,3,18,0]],
 'High Cut 24 Q 2':[['hcut',8000,0,2,24,0]],'High Cut 36 Q 0,7':[['hcut',12000,0,.7,36,0]],'Low Cut 96 Q 10 (limite 28,28)':[['lcut',60,0,10,96,0]],'Low Cut 30 Q 4 (Q ignorato)':[['lcut',100,0,4,30,0]],
 'High Cut 12 a 20 kHz (correzione)':[['hcut',20000,0,1,12,0]],'High Shelf 15 kHz +18 Q 0,5 (correzione)':[['hshelf',15000,18,.5,12,0]],'High Shelf 14 kHz +18 Q 0,6 (correzione)':[['hshelf',14000,18,.6,12,0]],'High Cut 18 kHz 24 Q 3 (correzione)':[['hcut',18000,0,3,24,0]],'Low Shelf 16 kHz -18 Q 1 (correzione)':[['lshelf',16000,-18,1,12,0]],'Tilt 15 kHz +9 (correzione)':[['tilt',15000,9,.7,12,0]],'Tilt dinamico':[['tilt',2000,3,1,12,0,-30,-6]]};
const [L,R]=load('VOCAL');
for(const [nm,ch] of Object.entries(cases)){const [l,r]=run(L,R,state(ch));const args=[];for(const c of ch)args.push(TYPES.indexOf(c[0]),c[1],c[2],c[3],c[4],c[5],c.length>6?1:0,c[6]||0,c[7]||0);
 cp.execFileSync(__dirname+'/dump',[D+'VOCAL.f32','/tmp/cpp_pq.f32',String(N),'512',...args.map(String)]);const b=fs.readFileSync('/tmp/cpp_pq.f32'),o=new Float32Array(b.buffer,b.byteOffset,b.length/4);
 const d=Math.max(md(l,o.subarray(0,N)),md(r,o.subarray(N,2*N)));ok(db(d)<-90,`${nm}: JS = C++ su VOCAL, differenza max ${db(d).toFixed(0)} dBFS (< -90)`);}
console.log(`RISULTATO forme 0.6.0: ${pass} PASS, ${fail} FAIL`);process.exit(fail?1:0);
