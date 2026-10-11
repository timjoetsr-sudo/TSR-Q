// parita JS = C++ sulla dinamica: detector Peak (tenuta di un periodo) e RMS, attacco 0,5 ms, bande basse e alte, 4 stem
const vm=require('vm'),fs=require('fs'),cp=require('child_process');
const ctx={Math,Float64Array,Float32Array,Array,Error,Object,console};vm.createContext(ctx);vm.runInContext(fs.readFileSync(process.env.CORE,'utf8')+';this.T=TSRQ;',ctx);const T=ctx.T;
const D='/tmp/claude-0/-home-claude/9256c548-5213-5d65-8a81-a8df32b70ebe/scratchpad/stems/',N=1047273,FS=48000,TYPES=['bell','lshelf','hshelf','lcut','hcut','notch','bpass','tilt','flat','allpass'];
const TMP='/tmp/claude-0/-home-claude-tsr-q/9256c548-5213-5d65-8a81-a8df32b70ebe/scratchpad/tmp/cpp_pk_'+(process.env.TAG||'x')+'.f32';
let pass=0,fail=0;const ok=(c,m)=>{console.log((c?'PASS ':'FAIL ')+m);c?pass++:fail++;};
const load=n=>{const b=fs.readFileSync(D+n+'.f32');const a=new Float32Array(b.buffer,b.byteOffset,b.length/4);return [a.slice(0,N),a.slice(N,2*N)];};
const db=x=>20*Math.log10(Math.max(x,1e-15)),md=(a,b)=>{let m=0;for(let i=0;i<a.length;i++)m=Math.max(m,Math.abs(a[i]-b[i]));return m;};
// dynf: 1 = RMS (att 5, rel 80), 2 = Peak (att 0,5, rel 40), 3 = RMS (att 0,5, rel 40)
const state=ch=>({bands:ch.map((c,i)=>({id:i,type:c[0],f:c[1],g:c[2],q:c[3],slope:c[4],place:c[5],on:true,dyn:c[6]?{on:true,thr:c[7],range:c[8],att:c[6]===1?5:0.5,rel:c[6]===1?80:40,knee:6,det:c[6]===2?1:0}:{on:false}})),inGain:0,out:0,autoGainDb:0,scale:1,character:0,bypass:false,solo:-1});
const run=(L0,R0,st)=>{const e=new T.Engine(FS,4096);e.setState(st);const L=new Float32Array(L0),R=new Float32Array(R0);for(let i0=0;i0<N;i0+=512){const n=Math.min(512,N-i0);e.setState(st);e.process(L.subarray(i0,i0+n),R.subarray(i0,i0+n),n);}return [L,R];};
const cases=[['KICK Peak 60 Hz',[['bell',60,3,1.2,12,0,2,-30,-8]]],['BASS Peak 45 Hz shelf',[['lshelf',45,2,0.71,12,0,2,-35,-6]]],['VOCAL Peak 6,5 kHz',[['bell',6500,0,3,12,0,2,-35,-6]]],['MELODY Peak Mid 500 Hz',[['bell',500,-2,1,12,3,2,-40,-4]]],
 ['KICK RMS veloce 60 Hz',[['bell',60,3,1.2,12,0,3,-30,-8]]],['VOCAL Peak + RMS + espansione',[['bell',300,0,1.5,12,0,2,-35,-6],['hshelf',9000,0,0.71,12,0,1,-45,4],['tilt',1000,0,0.71,12,0,3,-30,-3]]]];
for(const [nm,ch] of cases){const stem=nm.split(' ')[0];const [L,R]=load(stem);const [l,r]=run(L,R,state(ch));const args=[];for(const c of ch)args.push(TYPES.indexOf(c[0]),c[1],c[2],c[3],c[4],c[5],c[6],c[7],c[8]);
 cp.execFileSync(process.env.DUMP,[D+stem+'.f32',TMP,String(N),'512',...args.map(String)]);const b=fs.readFileSync(TMP),o=new Float32Array(b.buffer,b.byteOffset,b.length/4);
 const d=Math.max(md(l,o.subarray(0,N)),md(r,o.subarray(N,2*N)));ok(db(d)<-90,`${nm}: JS = C++, differenza max ${db(d).toFixed(0)} dBFS`);}
console.log(`RISULTATO parita dinamica Peak/RMS: ${pass} PASS, ${fail} FAIL`);process.exit(fail?1:0);
