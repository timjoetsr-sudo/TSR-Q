# test generale dell'interfaccia TSR Q (prototipo HTML = interfaccia del plugin), con esito PASS/FAIL per ogni controllo
import asyncio, math, sys
from playwright.async_api import async_playwright
URL = 'http://localhost:8770/TSR_Q_09_ceramica_smart.html'
res = []
def ok(c, m): res.append(bool(c)); print(('PASS ' if c else 'FAIL ') + m)

async def main():
    async with async_playwright() as p:
        brw = b = await p.chromium.launch(executable_path='/opt/pw-browsers/chromium-1194/chrome-linux/chrome', args=['--autoplay-policy=no-user-gesture-required'])
        pg = await brw.new_page(viewport={'width': 1300, 'height': 720}); errs = []
        pg.on('pageerror', lambda e: errs.append(str(e)))
        await pg.goto(URL); await pg.wait_for_timeout(600)
        ev = pg.evaluate
        g = await pg.locator('.graph canvas:not(.ovl)').bounding_box()
        S = lambda x, y: (g['x'] + x / 1160 * g['width'], g['y'] + y / 532 * g['height'])
        X = lambda f: ev(f'window.__tsrq.graph.X({f})'); Y = lambda d: ev(f'window.__tsrq.graph.Y({d})')
        bands = lambda: ev("window.__tsrq.P().bands.map((b,i)=>b.used?{i,t:b.type,f:b.freq,g:b.gain,q:b.q,s:b.slope,d:b.dyn,r:b.range}:null).filter(Boolean)")
        clear = lambda: ev("window.__tsrq.graph.selectAll(); window.__tsrq.graph.deleteSelected()")

        print('== 1. Struttura ==')
        r = await ev("(()=>{const e=document.querySelector('#ed').getBoundingClientRect();return {w:e.width,h:e.height}})()")
        ok(abs(r['w'] / r['h'] - 1180 / 600) < 0.01, f"proporzioni del plugin 1180×600 (rapporto {r['w']/r['h']:.3f})")
        out = await ev("""(()=>{const E=document.querySelector('#ed').getBoundingClientRect();return [...document.querySelectorAll('#ed *')].filter(x=>{const s=getComputedStyle(x);if(s.display==='none'||s.visibility==='hidden'||x.offsetParent===null)return false;const r=x.getBoundingClientRect();return r.width>0&&(r.bottom>E.bottom+1||r.right>E.right+1||r.left<E.left-1)}).length})()""")
        ok(out == 0, f'nessun elemento fuori dal plugin ({out})')
        vis = await ev("[...document.querySelectorAll('#ed>button.tb')].filter(b=>getComputedStyle(b).display!=='none').map(b=>b.textContent)")
        ok(not any(t in vis for t in ['AUDIO', 'PLAY', 'A', 'B', 'A>B', 'UNDO', 'REDO', 'PRESET', 'SETTINGS', 'BYPASS']), f'vecchi tasti nascosti (visibili: {vis})')
        ok(await pg.locator('.tbar [data-t="name"]').inner_text() == 'Default Setting', 'barra preset: "Default Setting"')
        kn = await ev("[...document.querySelectorAll('#ed>canvas.knob')].filter(c=>getComputedStyle(c).display!=='none').length")
        ok(kn == 1, f'solo il knob OUTPUT visibile ({kn})')
        ok(await pg.locator('.lglass').count() == 0, 'nessun vetro sotto il nome')
        kc = await ev("(()=>{const c=[...document.querySelectorAll('#ed>canvas.knob')].find(c=>getComputedStyle(c).display!=='none').getBoundingClientRect();return c.x+c.width/2})()")
        mc = g['x'] + (1160 - 43) * g['width'] / 1160
        ok(abs(kc - mc) < 0.5, f'meter OUT in asse con il knob OUTPUT (scarto {kc-mc:.2f} px)')

        print('== 2. Bande: creazione, isola, trascinamento ==')
        await clear(); y0 = await Y(0)
        await pg.mouse.dblclick(*S(await X(1000), await Y(6))); await pg.wait_for_timeout(250)
        bs = await bands(); ok(len(bs) == 1 and bs[0]['t'] == 0 and abs(bs[0]['f'] - 1000) < 15 and abs(bs[0]['g'] - 6) < 0.3, f'doppio clic → Bell 1 kHz +6 dB ({bs})')
        isl = await pg.locator('.bisl.bp').bounding_box(); x20 = g['x'] + await X(20) / 1160 * g['width']; x10k = g['x'] + await X(10000) / 1160 * g['width']
        ok(isl and isl['x'] >= x20 - 1 and isl['x'] + isl['width'] <= x10k + 1, 'isola della banda tra 20 Hz e 10 kHz')
        n = await ev("window.__tsrq.graph.nodePos(window.__tsrq.graph.primary)"); sx, sy = S(n['x'], n['y'])
        # nel Chromium headless (senza GPU) un evento può arrivare >350 ms dopo la pressione e diventare "tieni premuto": riprovo fino a 3 volte e lo dichiaro
        for att in range(1, 4):
            await ev(f"(()=>{{const b=window.__tsrq.P().bands.find(b=>b.used);b.freq=1000;b.gain=6;b.q={bs[0]['q']};dirty=true}})()"); await pg.wait_for_timeout(300)
            n = await ev("window.__tsrq.graph.nodePos(window.__tsrq.graph.primary)"); sx, sy = S(n['x'], n['y'])
            await pg.mouse.move(sx, sy); await pg.mouse.down(); await pg.mouse.move(sx + 60, sy - 30, steps=6); await pg.mouse.up(); await pg.wait_for_timeout(150)
            b1 = (await bands())[0]
            if b1['f'] > 1100 and b1['g'] > 6.5 and abs(b1['q'] - bs[0]['q']) < 1e-9: break
        ok(b1['f'] > 1100 and b1['g'] > 6.5 and abs(b1['q'] - bs[0]['q']) < 1e-9, f"trascinamento: frequenza e guadagno cambiano, Q no ({b1['f']:.0f} Hz {b1['g']:.2f} dB Q {b1['q']:.2f}, tentativo {att})")
        n = await ev("window.__tsrq.graph.nodePos(window.__tsrq.graph.primary)"); sx, sy = S(n['x'], n['y']); q0 = b1['q']
        await pg.mouse.move(sx, sy); await pg.mouse.down(); await pg.wait_for_timeout(500); await pg.mouse.move(sx + 100, sy, steps=8); await pg.mouse.up(); await pg.wait_for_timeout(150)
        b2 = (await bands())[0]; ok(b2['q'] < q0 * 0.7 and abs(b2['f'] - b1['f']) < 1e-6, f"tieni premuto + destra: Bell più largo (Q {q0:.2f} → {b2['q']:.2f}), frequenza ferma")
        await pg.mouse.move(sx, sy); await pg.mouse.wheel(0, -100); await pg.wait_for_timeout(150)
        b3 = (await bands())[0]; ok(b3['q'] > b2['q'], f"rotella sul Bell: Q {b2['q']:.2f} → {b3['q']:.2f}")
        await pg.locator('.tbar [data-t="undo"]').click(); await pg.wait_for_timeout(150)
        ok(abs((await bands())[0]['q'] - b2['q']) < 1e-9, 'undo dalla barra')
        await pg.locator('.tbar [data-t="redo"]').click(); await pg.wait_for_timeout(150)
        ok(abs((await bands())[0]['q'] - b3['q']) < 1e-9, 'redo dalla barra')

        print('== 3. Tipi, pendenze, Low/High Cut ==')
        await pg.locator('.nbub .ti').click(); await pg.wait_for_timeout(200)
        ok(not await ev("document.querySelector('.tmenu').hidden"), 'clic sull\'icona del fumetto → elenco verticale dei tipi')
        await pg.locator('.tmenu .tm-i', has_text='Low Cut').click(); await pg.wait_for_timeout(250)
        b = (await bands())[0]; ok(b['t'] == 3 and abs(b['q'] - math.sqrt(.5)) < 1e-9, f"Low Cut: Q bloccato a 0.707 ({b['q']:.4f})")
        ok(not await ev('KG.en') and not await ev('KQ.en'), 'Low Cut: manopole GAIN e Q spente')
        await ev("window.__tsrq.graph.apply('q', 8)"); await pg.wait_for_timeout(150)
        pk = await ev("""(()=>{const P=window.__tsrq.P(),i=P.bands.findIndex(b=>b.used),o=[];for(let s=0;s<SLOPES.length;s++){P.bands[i].slope=s;const c=TSRQ.designBand(engineBand(i),0,48000);let m=-1e9;for(let k=0;k<1500;k++){const f=10*Math.pow(2000,k/1499);m=Math.max(m,10*Math.log10(TSRQ.digMag2(c,f,48000)));}o.push(m);}P.bands[i].slope=3;return Math.max(...o)})()""")
        ok(pk <= 0.001, f'Low Cut con Q 8 tentato: la curva non supera mai 0 dB (picco {pk:.4f} dB, tutte le pendenze)')
        await pg.locator('.bp .slo .sv').click(); await pg.wait_for_timeout(150)
        it = await ev("[...document.querySelectorAll('.pm .pm-i')].map(e=>e.textContent.trim())")
        ok(len(it) == 9 and '96 dB/oct' in it[-1], f'clic sulla pendenza → elenco verticale 6…96 dB/oct ({len(it)} voci)')
        await pg.locator('.pm .pm-i', has_text='48 dB/oct').click(); await pg.wait_for_timeout(150)
        ok(await pg.locator('.bp .slo .sv').inner_text() == '48 dB/oct', 'pendenza scelta dall\'elenco: 48 dB/oct')
        await ev("window.__tsrq.graph.apply('type', 6)"); await pg.wait_for_timeout(150)
        b = (await bands())[0]; ok(b['s'] == 0, 'Band Pass: parte da 6 dB/oct')
        n = await ev("window.__tsrq.graph.nodePos(window.__tsrq.graph.primary)"); await pg.mouse.move(*S(n['x'], n['y']))
        for _ in range(3): await pg.mouse.wheel(0, -100); await pg.wait_for_timeout(120)
        b = (await bands())[0]; ok(b['s'] == 3, f'Band Pass: rotella → 24 dB/oct (indice {b["s"]})')
        sl = await ev("""(()=>{const P=window.__tsrq.P(),i=P.bands.findIndex(b=>b.used);P.bands[i].freq=1000;P.bands[i].q=1;const c=TSRQ.designBand(engineBand(i),0,48000),d=f=>10*Math.log10(TSRQ.digMag2(c,f,48000));return [d(1000),d(4000)-d(8000)]})()""")
        ok(abs(sl[0]) < 0.05 and abs(sl[1] - 24) < 1, f'Band Pass 24 dB/oct: picco {sl[0]:.2f} dB, fianco {sl[1]:.1f} dB/oct')
        await ev("window.__tsrq.graph.apply('type', 0)"); await pg.wait_for_timeout(150)
        ok((await bands())[0]['t'] == 0, 'tornando a Bell dal Band Pass')

        print('== 4. Dinamica ==')
        await pg.locator('.bp [data-k="dyn"]').click(); await pg.wait_for_timeout(150)
        b = (await bands())[0]; ok(b['d'] == 1 and abs(b['r'] + 1.5) < 1e-9, f"DYN acceso → range -1.5 dB ({b['r']})")
        await ev("window.__tsrq.P().bands.find(b=>b.used).range=-8"); await pg.locator('.bp [data-k="dyn"]').click(); await pg.locator('.bp [data-k="dyn"]').click(); await pg.wait_for_timeout(150)
        ok(abs((await bands())[0]['r'] + 1.5) < 1e-9, 'range -8 → DYN spento e riacceso → -1.5 dB')
        await clear(); await pg.wait_for_timeout(300); await pg.mouse.click(*S(await X(3000), await Y(-6)), click_count=3); await pg.wait_for_timeout(300)
        bs = await bands(); ok(len(bs) == 1 and bs[0]['d'] == 1 and abs(bs[0]['r'] + 1.5) < 1e-9, f'triplo clic → banda con dinamica, range -1.5 ({bs})')

        print('== 5. Proposte sulla linea ==')
        await clear(); await pg.mouse.move(10, 10); await pg.wait_for_timeout(100)
        for f, t in [(15, 'Low Cut'), (30, 'Low Shelf'), (800, 'Bell'), (17000, 'High Shelf'), (22000, 'High Cut')]:
            await pg.mouse.move(*S(await X(f), await Y(0))); await pg.wait_for_timeout(120)
            s = await ev("(()=>{const s=window.__tsrqSug();return s?TYPE_NAMES[s.type]:null})()"); ok(s == t, f'passando sulla linea a {f} Hz → proposta {s}')
        await pg.mouse.move(*S(await X(800), await Y(0) - 60)); await pg.wait_for_timeout(100)
        ok(await ev('window.__tsrqSug()') is None, 'lontano dalla linea → nessuna proposta')
        await pg.mouse.move(*S(await X(15), await Y(0))); await pg.wait_for_timeout(120); await pg.mouse.down(); await pg.mouse.up(); await pg.wait_for_timeout(200)
        bs = await bands(); ok(len(bs) == 1 and bs[0]['t'] == 3 and abs(bs[0]['f'] - 10) < 1e-6, f'clic sulla proposta (mouse a 15 Hz) → Low Cut a 10 Hz ({bs})')

        print('== 6. Tastiera e matita ==')
        await clear(); await pg.locator('.tool[title^="Tastiera"]').click(); await pg.wait_for_timeout(150)
        ky = 532 - 22 - 11
        await pg.mouse.click(*S(await X(440), ky)); await pg.wait_for_timeout(200)
        await pg.mouse.click(*S(await X(440), ky), click_count=2); await pg.wait_for_timeout(250)
        await pg.mouse.click(*S(await X(440), ky), click_count=3); await pg.wait_for_timeout(250)
        bs = await bands(); ok(len(bs) == 1 and bs[0]['t'] == 0 and abs(bs[0]['f'] - 440) < 1e-6 and bs[0]['g'] == 0 and bs[0]['d'] == 0, f'tastiera: 1/2/3 clic su A4 → un solo Bell 440 Hz, 0 dB, senza dinamica ({bs})')
        await pg.mouse.click(*S(await X(15), ky)); await pg.mouse.click(*S(await X(16000), ky)); await pg.wait_for_timeout(200)
        ok(len(await bands()) == 1, 'tastiera: fuori da 20 Hz–10 kHz nessuna banda')
        await pg.locator('.tool[title^="Tastiera"]').click(); await clear(); await pg.locator('.tool[title^="Matita"]').click()
        xa, xb, xc = await X(100), await X(30), await X(12000); dbpx = (await Y(0) - await Y(1))
        pts = [xb + (xc - xb) * k / 120 for k in range(121)]
        tgt = lambda x: 6 * math.exp(-((x - xa) / 40) ** 2)
        await pg.mouse.move(*S(pts[0], await Y(0))); await pg.mouse.down()
        for x in pts[1:]: await pg.mouse.move(*S(x, y0 - tgt(x) * dbpx))
        await pg.mouse.up(); await pg.wait_for_timeout(300); await pg.keyboard.press('Escape')
        bs = await bands(); ok(len(bs) >= 1 and any(abs(math.log2(x['f'] / 100)) < 0.2 and abs(x['g'] - 6) < 0.6 for x in bs), f"matita: disegno +6 dB a 100 Hz → Bell creato ({[(round(x['f']), round(x['g'], 1)) for x in bs]})")

        print('== 7. Preset, impostazioni, scala ==')
        await pg.locator('.tbar [data-t="name"]').click(); await pg.locator('.pm .pm-i', has_text='Salva in').hover(); await pg.wait_for_timeout(150)
        await pg.locator('.pm .pm-i', has_text='Slot 3').first.click(); await pg.wait_for_timeout(150)
        ok(await pg.locator('.tbar [data-t="name"]').inner_text() == 'Preset 3', 'preset salvato → "Preset 3"')
        nb = len(await bands()); await pg.locator('.tbar [data-t="name"]').click(); await pg.locator('.pm .pm-i', has_text='Init').click(); await pg.wait_for_timeout(150)
        ok(len(await bands()) == 0 and await pg.locator('.tbar [data-t="name"]').inner_text() == 'Default Setting', 'Init → nessuna banda, "Default Setting"')
        await pg.locator('.tbar [data-t="next"]').click(); await pg.wait_for_timeout(150)
        ok(len(await bands()) == nb, f'freccia › → ricarica Preset 3 ({nb} bande)')
        r0 = await ev('window.__tsrq.graph.rangeDb'); await pg.locator('.tb.rng').click(); await pg.wait_for_timeout(100)
        ok(await ev('window.__tsrq.graph.rangeDb') != r0, 'tasto scala cambia la scala del grafico')
        await pg.locator('.tbar [data-t="name"]').click(); await pg.locator('.pm .pm-i', has_text='Peak Hold').click(); await pg.wait_for_timeout(100)
        ok(await ev('window.__tsrq.graph.peakHold'), 'impostazioni dal menu: Peak Hold attivato')

        print('== 8. Audio (motore nel browser) ==')
        await clear(); await ev('window.__tsrq.play()'); await pg.wait_for_timeout(1500)
        async def peak(ms=1500):
            m = 0
            for _ in range(ms // 100):
                await pg.wait_for_timeout(100); m = max(m, *(await ev('window.__tsrq.meter().peak')))
            return 20 * math.log10(max(m, 1e-9))
        p0 = await peak(); ok(p0 > -60, f'audio in riproduzione: picco d\'uscita {p0:.1f} dBFS')
        kb = await pg.locator('#ed>canvas.knob').filter(has=None).evaluate_all("l=>l.filter(c=>getComputedStyle(c).display!=='none').map(c=>{const r=c.getBoundingClientRect();return [r.x,r.y,r.width,r.height]})")
        kx, ky2, kw, kh = kb[0]; await pg.mouse.move(kx + kw / 2, ky2 + kh / 2); await pg.mouse.down(); await pg.mouse.move(kx + kw / 2, ky2 + kh / 2 + 60, steps=6); await pg.mouse.up(); await pg.wait_for_timeout(200)
        o = await ev('window.__tsrq.P().out'); ok(o < -0.5, f'knob OUTPUT trascinato in giù → P.out {o:.2f} dB')
        await pg.keyboard.press('Control+z'); await pg.wait_for_timeout(100); ok(abs(await ev('window.__tsrq.P().out')) < 1e-9, 'Ctrl+Z riporta OUTPUT a 0 dB')
        g6 = await ev("""(()=>{const fs=48000,n=fs,e=new TSRQ.Engine(fs,n);e.setState({inGain:0,bands:[],out:-6,scale:1,autoGainDb:0,character:0,bypass:false,solo:-1});const L=new Float32Array(n),R=new Float32Array(n);for(let i=0;i<n;i++)L[i]=R[i]=0.5*Math.sin(2*Math.PI*1000*i/fs);e.process(L,R,n);let pk=0;for(let i=n/2;i<n;i++)pk=Math.max(pk,Math.abs(L[i]));return 20*Math.log10(pk/0.5)})()""")
        ok(abs(g6 + 6) < 0.05, f'motore con OUTPUT -6 dB: sinusoide misurata {g6:.2f} dB')
        ok(not await ev('window.__tsrq.meter().peak.some(v=>!Number.isFinite(v))'), 'meter: valori finiti')

        ok(not errs, f'nessun errore JavaScript ({errs[:2]})')
        await brw.close()
    n = len(res); f = res.count(False)
    print(f'\nRISULTATO interfaccia: {n - f} PASS, {f} FAIL'); sys.exit(1 if f else 0)
asyncio.run(main())
