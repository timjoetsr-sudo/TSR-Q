// parità JS = C++ sulle funzioni nuove: Band Pass 6…96 dB/oct, Low/High Cut senza risonanza (Q ignorato), dinamica -1.5 dB
const vm=require('vm'),fs=require('fs'),cp=require('child_process');
const ctx={Math,Float64Array,Float32Array,Array,Error,Object,console};vm.createContext(ctx);vm.runInContext(fs.readFileSync(process.env.CORE,'utf8')+';this.T=TSRQ;',ctx);const T=ctx.T;
const D='/tmp/claude-0/-home-claude/9256c548-5213-5d65-8a81-a8df32b70ebe/scratchpad/stems/',N=1047273,FS=48000,TYPES=['bell','lshelf','hshelf','lcut','hcut','notch','bpass','tilt','flat','allpass'];
let pass=0,fail=0;const ok=(c,m)=>{console.log((c?'PASS ':'FAIL ')+m);c?pass++:fail++;};
const load=n=>{const b=fs.readFileSync(D+n+'.f32');const a=new Float32Array(b.buffer,b.byteOffset,b.length/4);return [a.slice(0,N),a.slice(N,2*N)];};
const db=x=>20*Math.log10(Math.max(x,1e-15));
const state=ch=>({bands:ch.map((c,i)=>({id:i,type:c[0],f:c[1],g:c[2],q:c[3],slope:c[4],place:c[5],on:true,dyn:c.length>6?{on:true,thr:c[6],range:c[7],att:5,rel:80,knee:6,det:0}:{on:false}})),inGain:0,out:0,autoGainDb:0,scale:1,character:0,bypass:false,solo:-1});
const run=(L0,R0,st)=>{const e=new T.Engine(FS,4096);e.setState(st);const L=new Float32Array(L0),R=new Float32Array(R0);for(let i0=0;i0<N;i0+=512){const n=Math.min(512,N-i0);e.setState(st);e.process(L.subarray(i0,i0+n),R.subarray(i0,i0+n),n);}return [L,R];};
const md=(a,b)=>{let m=0;for(let i=0;i<a.length;i++)m=Math.max(m,Math.abs(a[i]-b[i]));return m;};
const cases={'BP 6':[['bpass',1000,0,1,6,0]],'BP 12':[['bpass',1000,0,1,12,0]],'BP 24':[['bpass',800,0,.5,24,0]],'BP 48':[['bpass',2500,0,2,48,0]],'BP 72':[['bpass',600,0,1,72,0]],'BP 96':[['bpass',1500,0,.7,96,0]],'BP 96 basso':[['bpass',90,0,1,96,0]],
 'LowCut Q4 (Q ignorato)':[['lcut',120,0,4,24,0]],'HighCut Q8 96 dB/oct':[['hcut',6000,0,8,96,0]],'Bell dinamico -1.5':[['bell',1500,3,1,12,0,-30,-1.5]]};
const [L,R]=load('VOCAL');
for(const [nm,ch] of Object.entries(cases)){const [l,r]=run(L,R,state(ch));const args=[];for(const c of ch)args.push(TYPES.indexOf(c[0]),c[1],c[2],c[3],c[4],c[5],c.length>6?1:0,c[6]||0,c[7]||0);
 cp.execFileSync(process.env.DUMP,[D+'VOCAL.f32','/tmp/claude-0/-home-claude-tsr-q/9256c548-5213-5d65-8a81-a8df32b70ebe/scratchpad/tmp/cpp_new_'+(process.env.TAG||'x')+'.f32',String(N),'512',...args.map(String)]);const b=fs.readFileSync('/tmp/claude-0/-home-claude-tsr-q/9256c548-5213-5d65-8a81-a8df32b70ebe/scratchpad/tmp/cpp_new_'+(process.env.TAG||'x')+'.f32'),o=new Float32Array(b.buffer,b.byteOffset,b.length/4);
 const d=Math.max(md(l,o.subarray(0,N)),md(r,o.subarray(N,2*N)));ok(db(d)<-90,`${nm}: JS = C++ su VOCAL, differenza max ${db(d).toFixed(0)} dBFS (< -90)`);}
// Low Cut con Q 4 deve suonare uguale a Q 0.707
{const [a]=run(L,R,state([['lcut',120,0,4,24,0]])),[b2]=run(L,R,state([['lcut',120,0,.707,24,0]]));ok(db(md(a,b2))<-120,`Low Cut: Q 4 = Q 0.707 (nessuna risonanza), differenza ${db(md(a,b2)).toFixed(0)} dBFS`);}
console.log(`RISULTATO nuove funzioni: ${pass} PASS, ${fail} FAIL`);process.exit(fail?1:0);
