// motore JS del prototipo HTML sugli stem: catene, confronto con il C++ (parità), blocchi, silenzio, NaN, velocità
const vm=require('vm'),fs=require('fs'),cp=require('child_process');
const ctx={Math,Float64Array,Float32Array,Array,Error,Object,console};vm.createContext(ctx);vm.runInContext(fs.readFileSync(process.env.CORE,'utf8')+';this.T=TSRQ;',ctx);const T=ctx.T;
const D='/tmp/claude-0/-home-claude/9256c548-5213-5d65-8a81-a8df32b70ebe/scratchpad/stems/',chains=JSON.parse(fs.readFileSync(__dirname+'/chains.json')),N=1047273,FS=48000;
const TYPES=['bell','lshelf','hshelf','lcut','hcut','notch','bpass','tilt','flat','allpass'];
let pass=0,fail=0;const ok=(c,m)=>{console.log((c?'PASS ':'FAIL ')+m);c?pass++:fail++;};
const load=n=>{const b=fs.readFileSync(D+n+'.f32');const a=new Float32Array(b.buffer,b.byteOffset,b.length/4);return [a.slice(0,N),a.slice(N,2*N)];};
const db=x=>20*Math.log10(Math.max(x,1e-15));
const state=(ch,extra={})=>({bands:ch.map((c,i)=>({id:i,type:c[0],f:c[1],g:c[2],q:c[3],slope:c[4],place:c[5],on:true,dyn:c.length>6?{on:true,thr:c[6],range:c[7],att:5,rel:80,knee:6,det:0}:{on:false}})),inGain:0,out:0,autoGainDb:0,scale:1,character:0,bypass:false,solo:-1,...extra});
function runJS(L0,R0,st,bl){const e=new T.Engine(FS,4096);e.setState(st);const L=new Float32Array(L0),R=new Float32Array(R0);
  for(let i0=0;i0<N;i0+=bl){const n=Math.min(bl,N-i0),l=L.subarray(i0,i0+n),r=R.subarray(i0,i0+n);e.setState(st);e.process(l,r,n);}return [L,R];}
const mdiff=(a,b)=>{let m=0;for(let i=0;i<a.length;i++){const d=Math.abs(a[i]-b[i]);if(d>m)m=d;}return m;};
const finite=a=>{for(const v of a)if(!Number.isFinite(v))return false;return true;};
for(const name of Object.keys(chains)){const [L,R]=load(name),ch=chains[name];
  console.log(`\n== ${name} ==`);
  // trasparenza
  {const [l,r]=runJS(L,R,state([]),512);ok(mdiff(l,L)===0&&mdiff(r,R)===0,`${name}: JS senza bande = ingresso identico al bit`);}
  const [l,r]=runJS(L,R,state(ch),512);ok(finite(l)&&finite(r),`${name}: JS catena da mix senza NaN/Inf`);
  // parità con il C++
  const args=[];for(const c of ch){args.push(TYPES.indexOf(c[0]),c[1],c[2],c[3],c[4],c[5],c.length>6?1:0,c[6]||0,c[7]||0);}
  cp.execFileSync(process.env.DUMP,[D+name+'.f32','/tmp/claude-0/-home-claude-tsr-q/9256c548-5213-5d65-8a81-a8df32b70ebe/scratchpad/tmp/cpp_out_'+(process.env.TAG||'x')+'.f32',String(N),'512',...args.map(String)]);
  const b=fs.readFileSync('/tmp/claude-0/-home-claude-tsr-q/9256c548-5213-5d65-8a81-a8df32b70ebe/scratchpad/tmp/cpp_out_'+(process.env.TAG||'x')+'.f32'),o=new Float32Array(b.buffer,b.byteOffset,b.length/4);
  const dd=Math.max(mdiff(l,o.subarray(0,N)),mdiff(r,o.subarray(N,2*N)));
  ok(db(dd)<-90,`${name}: prototipo HTML (JS) = plugin (C++), differenza max ${db(dd).toFixed(0)} dBFS (< -90)`);
  // blocchi diversi (Web Audio usa 128)
  let w=0;for(const bl of [128,100,1024]){const [l2,r2]=runJS(L,R,state(ch),bl);w=Math.max(w,mdiff(l2,l),mdiff(r2,r));}
  ok(db(w)<-100,`${name}: JS blocchi 128/100/1024 vs 512, differenza max ${db(w).toFixed(0)} dBFS`);
}
{const [L,R]=load('HH-1');const [l,r]=runJS(L,R,state(chains.KICK),128);let m=0;for(const v of l)m=Math.max(m,Math.abs(v));ok(m===0&&finite(r),'HH-1 (silenzio): JS uscita esattamente zero');}
// (prestazioni JS misurate a parte in js_cpu.js, con setState una sola volta come nel browser)

{const [L,R]=load('VOCAL');const [l]=runJS(L,R,state([['bell',1000,0,1,12,0]],{inGain:6}),512);let ml=0,mi=0;for(let i=0;i<N;i++){ml=Math.max(ml,Math.abs(l[i]));mi=Math.max(mi,Math.abs(L[i]));}
 ok(Math.abs(db(ml)-db(mi)-6)<0.05,`JS INPUT +6 dB: picco ${db(mi).toFixed(2)} → ${db(ml).toFixed(2)} dBFS`);}
console.log(`\nRISULTATO JS: ${pass} PASS, ${fail} FAIL`);process.exit(fail?1:0);
