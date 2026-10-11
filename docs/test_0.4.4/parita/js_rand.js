// parità JS = C++ su 40 configurazioni casuali (tutti i tipi, pendenze, Q, guadagni, collocazioni, dinamica)
const vm=require('vm'),fs=require('fs'),cp=require('child_process');
const ctx={Math,Float64Array,Float32Array,Array,Error,Object,console};vm.createContext(ctx);vm.runInContext(fs.readFileSync(process.env.CORE,'utf8')+';this.T=TSRQ;',ctx);const T=ctx.T;
const D='/tmp/claude-0/-home-claude/9256c548-5213-5d65-8a81-a8df32b70ebe/scratchpad/stems/',N=1047273,FS=48000,TYPES=['bell','lshelf','hshelf','lcut','hcut','notch','bpass','tilt','flat','allpass'],SL=[6,12,18,24,30,36,48,72,96];
let s=12345;const rnd=()=>(s=(s*1103515245+12345)%2147483648)/2147483648;
const load=n=>{const b=fs.readFileSync(D+n+'.f32');const a=new Float32Array(b.buffer,b.byteOffset,b.length/4);return [a.slice(0,N),a.slice(N,2*N)];};
const db=x=>20*Math.log10(Math.max(x,1e-15));const md=(a,b)=>{let m=0;for(let i=0;i<a.length;i++)m=Math.max(m,Math.abs(a[i]-b[i]));return m;};
const state=ch=>({bands:ch.map((c,i)=>({id:i,type:c[0],f:c[1],g:c[2],q:c[3],slope:c[4],place:c[5],on:true,dyn:c.length>6?{on:true,thr:c[6],range:c[7],att:5,rel:80,knee:6,det:0}:{on:false}})),inGain:0,out:0,autoGainDb:0,scale:1,character:0,bypass:false,solo:-1});
const run=(L0,R0,st)=>{const e=new T.Engine(FS,4096);e.setState(st);const L=new Float32Array(L0),R=new Float32Array(R0);for(let i0=0;i0<N;i0+=512){const n=Math.min(512,N-i0);e.setState(st);e.process(L.subarray(i0,i0+n),R.subarray(i0,i0+n),n);}return [L,R];};
const stems=['KICK','BASS','VOCAL','MELODY'];let pass=0,fail=0,worst=-999;
for(let k=0;k<40;k++){const t=TYPES[Math.floor(rnd()*10)],f=20*Math.pow(900,rnd()),g=-18+36*rnd(),q=0.3*Math.pow(40,rnd()),sl=SL[Math.floor(rnd()*9)],pl=Math.floor(rnd()*5);
  const c=[t,+f.toFixed(2),+g.toFixed(2),+q.toFixed(3),sl,pl];if(['bell','lshelf','hshelf','tilt'].includes(t)&&rnd()<.5)c.push(-35+20*rnd(),rnd()<.5?-1.5:-6);
  const nm=stems[k%4],[L,R]=load(nm),[l,r]=run(L,R,state([c]));const a=[TYPES.indexOf(c[0]),c[1],c[2],c[3],c[4],c[5],c.length>6?1:0,c[6]||0,c[7]||0];
  cp.execFileSync(process.env.DUMP,[D+nm+'.f32','/tmp/claude-0/-home-claude-tsr-q/9256c548-5213-5d65-8a81-a8df32b70ebe/scratchpad/tmp/cpp_r_'+(process.env.TAG||'x')+'.f32',String(N),'512',...a.map(String)]);const b=fs.readFileSync('/tmp/claude-0/-home-claude-tsr-q/9256c548-5213-5d65-8a81-a8df32b70ebe/scratchpad/tmp/cpp_r_'+(process.env.TAG||'x')+'.f32'),o=new Float32Array(b.buffer,b.byteOffset,b.length/4);
  const d=db(Math.max(md(l,o.subarray(0,N)),md(r,o.subarray(N,2*N))));worst=Math.max(worst,d);if(d<-90)pass++;else{fail++;console.log('FAIL',nm,JSON.stringify(c),d.toFixed(0)+' dBFS');}}
console.log(`RISULTATO parità casuale: ${pass} PASS, ${fail} FAIL, differenza peggiore ${worst.toFixed(0)} dBFS (soglia -90)`);process.exit(fail?1:0);
