# test funzioni del MASTER (gesti, campi, analizzatore, zoom, viste...): esito PASS/FAIL misurato
import asyncio, sys, math
from playwright.async_api import async_playwright
URL='http://localhost:8770/TSR_Q_09_ceramica_smart.html'; res=[]
def ok(c,m): res.append(bool(c)); print(('PASS ' if c else 'FAIL ')+m)
SECTIONS=sys.argv[1:]  # vuoto = tutte
async def main():
  async with async_playwright() as p:
    br=await p.chromium.launch(executable_path='/opt/pw-browsers/chromium-1194/chrome-linux/chrome',args=['--autoplay-policy=no-user-gesture-required'])
    pg=await br.new_page(viewport={'width':1300,'height':720}); errs=[]; pg.on('pageerror',lambda e:errs.append(str(e)))
    await pg.goto(URL); await pg.wait_for_timeout(700); ev=pg.evaluate
    g=await pg.locator('.graph canvas:not(.ovl)').bounding_box(); S=lambda x,y:(g['x']+x/1160*g['width'],g['y']+y/532*g['height'])
    X=lambda f: ev(f'window.__tsrq.graph.X({f})'); Y=lambda d: ev(f'window.__tsrq.graph.Y({d})')
    bands=lambda: ev("window.__tsrq.P().bands.map((b,i)=>b.used?{i,t:b.type,f:b.freq,g:b.gain,q:b.q,s:b.slope,d:b.dyn,r:b.range,p:b.place,b:b.byp}:null).filter(Boolean)")
    clear=lambda: ev("window.__tsrq.graph.selectAll(); window.__tsrq.graph.deleteSelected()")
    mk=lambda f,gd,t=0: ev(f"window.__tsrq.graph.createBand({f},{gd},{t})")
    async def node(i):
      n=await ev(f"window.__tsrq.graph.nodePos({i})"); return S(n['x'],n['y'])
    want=lambda s: not SECTIONS or s in SECTIONS
    T={'pg':pg,'ev':ev,'S':S,'X':X,'Y':Y,'bands':bands,'clear':clear,'mk':mk,'node':node,'ok':ok,'g':g}
    if want('gesti'):
      print('== Gesti sul grafico ==')
      await clear(); a=await mk(1000,3); sx,sy=await node(a); q0=(await bands())[0]['q']
      await pg.keyboard.down('Control'); await pg.mouse.move(sx,sy); await pg.mouse.down(); await pg.mouse.move(sx,sy-60,steps=6); await pg.mouse.up(); await pg.keyboard.up('Control'); await pg.wait_for_timeout(150)
      b=(await bands())[0]; ok(b['q']>q0*1.3 and abs(b['f']-1000)<1e-6 and abs(b['g']-3)<1e-9,f"Cmd/Ctrl+trascina in su: Q {q0:.2f} → {b['q']:.2f}, freq e guadagno fermi")
      await pg.mouse.move(sx,sy); await pg.keyboard.down('Control'); await pg.mouse.wheel(0,-100); await pg.keyboard.up('Control'); await pg.wait_for_timeout(120)
      ok(abs((await bands())[0]['g']-3.5)<1e-9,f"Cmd/Ctrl+rotella: guadagno 3 → {(await bands())[0]['g']}")
      await ev(f"(()=>{{const b=window.__tsrq.P().bands[{a}];b.dyn=1;b.range=-1;dirty=true}})()"); await pg.keyboard.down('Alt'); await pg.mouse.wheel(0,-100); await pg.keyboard.up('Alt'); await pg.wait_for_timeout(120)
      ok(abs((await bands())[0]['r']+0.9)<1e-9,f"Option/Alt+rotella: range -1 → {(await bands())[0]['r']}")
      await pg.keyboard.down('Control'); await pg.keyboard.down('Alt'); await pg.mouse.wheel(0,-100); await pg.keyboard.up('Alt'); await pg.keyboard.up('Control'); await pg.wait_for_timeout(120); b=(await bands())[0]
      ok(abs(b['g']-4)<1e-9 and abs(b['r']+1.4)<1e-9,f"Cmd+Option+rotella: guadagno +0.5 e range -0.5 (g {b['g']}, r {b['r']})")
      sx,sy=await node(a); await pg.keyboard.down('Control'); await pg.keyboard.down('Alt'); await pg.mouse.click(sx,sy); await pg.keyboard.up('Alt'); await pg.keyboard.up('Control'); await pg.wait_for_timeout(150)
      ok((await bands())[0]['t']==1,f"Cmd+Option+clic: forma successiva (Bell → {(await bands())[0]['t']})")
      s0=(await bands())[0]['s']; await pg.keyboard.down('Alt'); await pg.keyboard.down('Shift'); await pg.mouse.click(*(await node(a))); await pg.keyboard.up('Shift'); await pg.keyboard.up('Alt'); await pg.wait_for_timeout(150)
      ok((await bands())[0]['s']!=s0,f"Option+Shift+clic: pendenza cambia ({s0} → {(await bands())[0]['s']})")
      await clear(); c=await mk(5000,0,4); await ev(f"window.__tsrq.P().bands[{c}].slope=3; dirty=true"); await pg.wait_for_timeout(100); cx,cy=await node(c)
      await pg.mouse.move(cx,cy); await pg.mouse.wheel(0,-100); await pg.wait_for_timeout(120); ok((await bands())[0]['s']==4,f"rotella sul High Cut = pendenza (24 → {[6,12,18,24,30,36,48,72,96][(await bands())[0]['s']]} dB/oct)")
      await clear(); ids=[await mk(f,0) for f in (100,300,900,2700,8000)]; await ev(f"window.__tsrq.graph.selectOnly({ids[1]})")
      await pg.keyboard.down('Shift'); await pg.mouse.click(*(await node(ids[3]))); await pg.keyboard.up('Shift'); await pg.wait_for_timeout(150)
      ok(sorted(await ev("window.__tsrq.graph.selection()"))==ids[1:4],f"Shift+clic: intervallo per frequenza ({sorted(await ev('window.__tsrq.graph.selection()'))})")
      await pg.keyboard.down('Control'); await pg.mouse.click(*(await node(ids[2]))); await pg.keyboard.up('Control'); await pg.wait_for_timeout(150)
      ok(sorted(await ev("window.__tsrq.graph.selection()"))==[ids[1],ids[3]],'Cmd/Ctrl+clic su banda selezionata: la toglie dalla selezione')
      await clear(); x,y=S(await X(2000),await Y(4)); await pg.keyboard.down('Alt'); await pg.mouse.dblclick(x,y); await pg.keyboard.up('Alt'); await pg.wait_for_timeout(200)
      b=await bands(); ok(len(b)==1 and b[0]['d']==1 and abs(b[0]['r'])<=1.5,f"Option+doppio clic: banda creata con dinamica ({b})")
      print('== Campi numerici (eliminati su richiesta) ==')
      await clear(); a=await mk(1000,2); await pg.mouse.dblclick(*(await node(a))); await pg.wait_for_timeout(200)
      ok(await ev("document.querySelectorAll('.numed').length")==0 and len(await bands())==1,'doppio clic sul nodo: nessun riquadro FREQ/GAIN dB/Q, nessuna banda nuova')
      u0=await ev("window.__tsrq.undoLen()"); sx,sy=await node(a)
      await pg.mouse.move(sx,sy); await pg.mouse.down(); await pg.mouse.move(sx+80,sy-40,steps=25); await pg.mouse.up(); await pg.wait_for_timeout(150)
      ok(await ev("window.__tsrq.undoLen()")-u0==1,f"un trascinamento = una sola operazione di undo ({await ev('window.__tsrq.undoLen()')-u0})")
      print('== Manopole ==')
      kq=await ev("(()=>{const k=knobs.find(k=>k.name==='Q');const r=k.cv.getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2}})()")
      await pg.keyboard.down('Control'); await pg.mouse.click(kq['x'],kq['y']); await pg.keyboard.up('Control'); await pg.wait_for_timeout(150)
      ok(abs((await bands())[0]['q']-1)<1e-9,f"Cmd/Ctrl+clic sulla manopola Q: torna al default 1 ({(await bands())[0]['q']})")
      await pg.mouse.move(kq['x'],kq['y']); t=await ev("knobs.find(k=>k.name==='Q').cv.title"); ok(t.startswith('Q: '),f"hover sulla manopola: nome e valore ('{t[:20]}…')")
    if want('extra'):
      import importlib.util
      spec=importlib.util.spec_from_file_location('extra','/tmp/claude-0/-home-claude/9256c548-5213-5d65-8a81-a8df32b70ebe/scratchpad/plug/ui_master_extra.py'); m=importlib.util.module_from_spec(spec); spec.loader.exec_module(m); await m.run(T)
    ok(not errs,f'nessun errore JavaScript ({errs[:3]})')
    await br.close()
  print(f'RISULTATO master UI: {sum(res)} PASS, {len(res)-sum(res)} FAIL'); sys.exit(0 if all(res) else 1)
asyncio.run(main())
