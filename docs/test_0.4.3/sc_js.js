// parita JS = C++ sugli scenari dinamici (stessi di sc_h.cpp) sulla clip ESO - SAFE
const vm=require('vm'),fs=require('fs');
const ctx={Math,Float64Array,Float32Array,Array,Error,Object,console};vm.createContext(ctx);vm.runInContext(fs.readFileSync(process.argv[2],'utf8')+';this.T=TSRQ;',ctx);const T=ctx.T;
const N=48000*12,FS=48000,BL=256,dir=__dirname+'/';
const b=fs.readFileSync(dir+'eso_lp.f32');const a=new Float32Array(b.buffer,b.byteOffset,b.length/4);
const SC=process.argv.slice(3);let fails=0;
for(const sc of SC){
  const L=new Float32Array(a.subarray(0,N)),R=new Float32Array(a.subarray(N,2*N));const e=new T.Engine(FS,4096);
  let bd={type:'bell',f:500,g:6,q:2,slope:12,place:0,byp:false,used:true,dyn:false,thr:-24,range:-6};if(sc==='slope'){bd.type='lcut';bd.f=300;}
  const gl={character:0,bypass:false,out:0,inGain:0,invert:false,pan:0,solo:-1};
  for(let i0=0;i0<N;i0+=BL){const u=i0/N,ph=Math.floor(i0/24000)%2,k=Math.floor(i0/24000);
    if(sc==='drag'||sc==='solo_drag')bd.f=200*Math.pow(10,u);
    if(sc==='solo_q')bd.q=0.3*Math.pow(10/0.3,u);
    gl.solo=sc.indexOf('solo')===0?0:-1; if(sc==='solo_toggle')gl.solo=ph?0:-1;
    if(sc==='bypass_band')bd.byp=!!ph; if(sc==='used')bd.used=!ph;
    if(sc==='type'){bd.type=['bell','lshelf','hshelf','notch'][k%4];bd.f=800;}
    if(sc==='slope')bd.slope=[12,24,48][k%3];
    if(sc==='place')bd.place=[0,3,4][k%3];
    if(sc==='dyn'){bd.dyn=!!ph;bd.thr=-40;bd.range=-1.5;}
    if(sc==='gbypass')gl.bypass=!!ph; if(sc==='character')gl.character=ph?2:0; if(sc==='warm_static')gl.character=2; if(sc==='subtle_toggle')gl.character=ph?1:0; if(sc==='sub_static')gl.character=1;
    if(sc==='outstep')gl.out=ph?-12:0; if(sc==='instep')gl.inGain=ph?-12:0; if(sc==='polarity')gl.invert=!!ph; if(sc==='pan')gl.pan=ph?-1:0;
    const bands=bd.used?[{id:0,on:!bd.byp,type:bd.type,f:bd.f,g:bd.g,q:bd.q,slope:bd.slope,place:bd.place,gq:false,dyn:{on:bd.dyn,thr:bd.thr,range:bd.range,att:5,rel:80,knee:6,det:0}}]:[];
    e.setState({bands,inGain:gl.inGain,out:gl.out,autoGainDb:0,scale:1,character:gl.character,bypass:gl.bypass,solo:gl.solo,pan:gl.pan,panMS:false,invert:gl.invert});
    const n=Math.min(BL,N-i0);e.process(L.subarray(i0,i0+n),R.subarray(i0,i0+n),n);}
  const cb=fs.readFileSync(dir+'o_'+sc+'.f32');const c=new Float32Array(cb.buffer,cb.byteOffset,cb.length/4);
  let m=0,pk=0;for(let i=0;i<N;i++){m=Math.max(m,Math.abs(L[i]-c[i]),Math.abs(R[i]-c[N+i]));pk=Math.max(pk,Math.abs(c[i]));}
  const d=20*Math.log10(Math.max(m,1e-15));const ok=d<-90;if(!ok)fails++;
  console.log((ok?'PASS ':'FAIL ')+sc.padEnd(14)+' JS = C++ differenza max '+d.toFixed(0)+' dBFS (picco uscita '+(20*Math.log10(pk)).toFixed(1)+' dBFS)');}
console.log('RISULTATO parita scenari dinamici: '+(SC.length-fails)+' PASS, '+fails+' FAIL');process.exit(fails?1:0);
