// parità JS = C++ sulle funzioni del master: Character con oversampling 8x, OUTPUT −∞/+36, pan L/R e M/S, polarità, Gain-Q
const vm=require('vm'),fs=require('fs'),cp=require('child_process');
const ctx={Math,Float64Array,Float32Array,Array,Error,Object,console};vm.createContext(ctx);vm.runInContext(fs.readFileSync(__dirname+'/dsp_cur.js','utf8')+';this.T=TSRQ;',ctx);const T=ctx.T;
const D=__dirname+'/../stems/',N=48000*8,FS=48000,TYPES=['bell','lshelf','hshelf','lcut','hcut','notch','bpass','tilt','flat','allpass'];
let pass=0,fail=0;const ok=(c,m)=>{console.log((c?'PASS ':'FAIL ')+m);c?pass++:fail++;};
const load=n=>{const b=fs.readFileSync(D+n+'.f32');const a=new Float32Array(b.buffer,b.byteOffset,b.length/4),M=a.length/2;return [a.slice(0,N),a.slice(M,M+N)];};
const db=x=>20*Math.log10(Math.max(x,1e-15)),md=(a,b)=>{let m=0;for(let i=0;i<a.length;i++)m=Math.max(m,Math.abs(a[i]-b[i]));return m;};
const state=(gl,ch)=>({bands:ch.map((c,i)=>({id:i,type:c[0],f:c[1],g:c[2],q:c[3],slope:c[4],place:c[5],on:true,gq:!!gl.gq,dyn:c.length>6?{on:true,thr:c[6],range:c[7],att:5,rel:80,knee:6,det:0}:{on:false}})),inGain:0,out:gl.out||0,autoGainDb:0,scale:1,character:gl.ch||0,bypass:false,solo:-1,pan:gl.pan||0,panMS:!!gl.ms,invert:!!gl.inv});
const run=(L0,R0,st)=>{const e=new T.Engine(FS,4096);const L=new Float32Array(L0),R=new Float32Array(R0);for(let i0=0;i0<N;i0+=512){const n=Math.min(512,N-i0);e.setState(st);e.process(L.subarray(i0,i0+n),R.subarray(i0,i0+n),n);}return [L,R];};
const cases=[['Character Subtle 8x',{ch:1,out:6},[['bell',3000,6,1,12,0]]],['Character Warm 8x',{ch:2,out:12},[['hshelf',6000,8,.7,12,0]]],['OUTPUT −∞',{out:-60},[['bell',1000,3,1,12,0]]],['OUTPUT +36',{out:36},[]],
 ['pan L/R −60%',{pan:-.6},[['bell',500,-4,2,12,0]]],['pan M/S +40%',{pan:.4,ms:1},[['bell',500,-4,2,12,3]]],['polarità invertita',{inv:1},[['lcut',80,0,.7,24,0]]],['Gain-Q su Bell +12/−9',{gq:1},[['bell',800,12,1,12,0],['bell',4000,-9,2,12,0]]],
 ['Gain-Q + dinamica',{gq:1},[['bell',2000,4,1,12,0,-30,-1.5]]],['tutto insieme',{ch:2,out:3,pan:.3,ms:1,inv:1,gq:1},[['bell',300,6,1.5,12,0],['hcut',9000,0,.7,48,0]]]];
const [L,R]=load('VOCAL');fs.writeFileSync('/tmp/vocal8.f32',Buffer.concat([Buffer.from(L.buffer),Buffer.from(R.buffer)]));
for(const [nm,gl,ch] of cases){const [l,r]=run(L,R,state(gl,ch));const args=[gl.ch||0,gl.out||0,gl.pan||0,gl.ms?1:0,gl.inv?1:0,gl.gq?1:0];for(const c of ch)args.push(TYPES.indexOf(c[0]),c[1],c[2],c[3],c[4],c[5],c.length>6?1:0,c[6]||0,c[7]||0);
 cp.execFileSync(__dirname+'/dump2',['/tmp/vocal8.f32','/tmp/cpp_m.f32',String(N),'512',...args.map(String)]);const b=fs.readFileSync('/tmp/cpp_m.f32'),o=new Float32Array(b.buffer,b.byteOffset,b.length/4);
 const d=Math.max(md(l,o.subarray(0,N)),md(r,o.subarray(N,2*N)));let pk=0;for(let i=0;i<N;i++)pk=Math.max(pk,Math.abs(l[i]),Math.abs(r[i]));
 ok(db(d)<-90,`${nm}: JS = C++ su VOCAL, differenza max ${db(d).toFixed(0)} dBFS (picco uscita ${db(pk).toFixed(1)} dBFS)`);}
console.log(`RISULTATO parità master: ${pass} PASS, ${fail} FAIL`);process.exit(fail?1:0);
