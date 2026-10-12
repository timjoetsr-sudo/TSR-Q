import asyncio, sys
from playwright.async_api import async_playwright
URL = sys.argv[1]; R = []
def ok(c, m): R.append(bool(c)); print(('PASS ' if c else 'FAIL ') + m)
async def main():
  async with async_playwright() as p:
    br = await p.chromium.launch(executable_path='/opt/pw-browsers/chromium-1194/chrome-linux/chrome', args=['--autoplay-policy=no-user-gesture-required'])
    pg = await br.new_page(viewport={'width': 1300, 'height': 860}); errs = []; pg.on('pageerror', lambda e: errs.append(str(e)))
    await pg.goto(URL); await pg.wait_for_timeout(2500); ev = pg.evaluate
    # coordinate schermo di un punto del grafico (spazio 1160x532 del canvas)
    async def scr(x, y):
      return await ev(f"(() => {{ const r = document.querySelector('.graph .ovl').getBoundingClientRect(); return [r.left + {x} / 1160 * r.width, r.top + {y} / 532 * r.height]; }})()")
    used = "P.bands.filter(b => b.used).length"
    await ev("graph.selectAll(); graph.deleteSelected()"); await pg.wait_for_timeout(200)   # la pagina A/B parte con lo scenario Bell 200 Hz
    n0 = await ev(used); ok(n0 == 0, f"partenza senza bande ({n0})")
    # 1) clic sulla linea (0 dB, 1 kHz) e trascinamento nello stesso gesto
    x1k = await ev("graph.X(1000)"); y0 = await ev("graph.Y(0)")
    sx, sy = await scr(x1k, y0); await pg.mouse.move(sx - 5, sy); await pg.mouse.move(sx, sy); await pg.wait_for_timeout(100)
    sug = await ev("window.__tsrqSug && window.__tsrqSug() && window.__tsrqSug().type"); ok(sug == 0, f"sulla linea a 1 kHz la proposta e un Bell (tipo {sug})")
    await pg.mouse.down(); await pg.wait_for_timeout(50)
    tx, ty = await scr(await ev("graph.X(2000)"), await ev("graph.Y(6)"))
    for k in range(1, 11): await pg.mouse.move(sx + (tx - sx) * k / 10, sy + (ty - sy) * k / 10); await pg.wait_for_timeout(15)
    mid = await ev("(() => { const b = P.bands.find(b => b.used); return b && { f: b.freq, g: b.gain, t: b.type }; })()")
    await pg.mouse.up(); await pg.wait_for_timeout(100)
    b = await ev("(() => { const b = P.bands.find(b => b.used); return b && { f: b.freq, g: b.gain, t: b.type }; })()")
    ok(await ev(used) == 1 and b['t'] == 0, f"un solo gesto: Bell creato ({await ev(used)} banda)")
    ok(abs(b['f'] - 2000) / 2000 < 0.03 and abs(b['g'] - 6) < 0.3, f"durante lo stesso trascinamento la banda segue il mouse: {b['f']:.0f} Hz {b['g']:+.2f} dB (attesi 2000 Hz +6 dB)")
    await pg.keyboard.press('Control+z'); await pg.wait_for_timeout(100); un1 = await ev(used)
    await pg.keyboard.press('Control+z'); await pg.wait_for_timeout(100); un2 = await ev(used)
    ok(un2 == 0, f"annulla: dopo 2 passi nessuna banda (dopo il 1o: {un1}, dopo il 2o: {un2})")
    await ev("P.bands.forEach(b => b.used = 0); dirty = true; panelSync && panelSync()"); await pg.wait_for_timeout(100)
    # 2) fuori dalla linea: un clic non crea, il trascinamento non crea
    fx, fy = await scr(await ev("graph.X(300)"), await ev("graph.Y(-7)"))
    await pg.mouse.move(fx, fy); await pg.wait_for_timeout(80); s2 = await ev("window.__tsrqSug && window.__tsrqSug()")
    ok(not s2, "a -7 dB, lontano dalla linea: nessuna proposta")
    await pg.mouse.click(fx, fy); await pg.wait_for_timeout(400); ok(await ev(used) == 0, "fuori dalla linea un clic singolo non crea bande")
    await pg.mouse.dblclick(fx, fy); await pg.wait_for_timeout(150)
    b = await ev("(() => { const b = P.bands.find(b => b.used); return b && { f: b.freq, g: b.gain }; })()")
    ok(b and abs(b['f'] - 300) / 300 < 0.03 and abs(b['g'] + 7) < 0.3, f"doppio clic fuori dalla linea crea la banda dove clicchi: {b and round(b['f'])} Hz {b and round(b['g'], 2)} dB")
    nx, ny = await scr(await ev("graph.nodePos(P.bands.findIndex(b => b.used)).x"), await ev("graph.nodePos(P.bands.findIndex(b => b.used)).y"))
    tx, ty = await scr(await ev("graph.X(600)"), await ev("graph.Y(-3)"))
    await pg.mouse.move(nx, ny); await pg.mouse.down()
    for k in range(1, 11): await pg.mouse.move(nx + (tx - nx) * k / 10, ny + (ty - ny) * k / 10); await pg.wait_for_timeout(15)
    await pg.mouse.up(); await pg.wait_for_timeout(100)
    b = await ev("(() => { const b = P.bands.find(b => b.used); return b && { f: b.freq, g: b.gain }; })()")
    ok(abs(b['f'] - 600) / 600 < 0.03 and abs(b['g'] + 3) < 0.3, f"poi la trascini: {b['f']:.0f} Hz {b['g']:+.2f} dB (attesi 600 Hz -3 dB)")
    # 3) niente freeze, niente segnalatori gialli
    await ev("window.__tsrq.play()"); await pg.wait_for_timeout(3000)
    ok(await ev("typeof window.__tsrq.peaks === 'undefined' && typeof window.__tsrq.grab === 'undefined' && typeof window.__tsrqPeakHover === 'undefined'"), "moduli tolti: etichette gialle dei picchi e Spectrum Grab")
    # mouse fermo 2,5 s sullo spettro: prima entrava la vista congelata (graph.hold)
    px, py = await scr(await ev("graph.X(150)"), await ev("graph.PL.y + graph.PL.h - 20")); await pg.mouse.move(px, py); await pg.mouse.move(px + 1, py); await pg.wait_for_timeout(2500)
    ok(not await ev("!!graph.hold") and not await ev("G0 = graph.freeze || false"), "mouse fermo 2,5 s sullo spettro: nessuna vista congelata")
    # menu dell'analizzatore senza Freeze
    await ev("bSettings.onclick()"); await pg.wait_for_timeout(200)
    txt = await ev("[...document.querySelectorAll('.pm')].map(m => m.textContent).join(' | ')"); await pg.keyboard.press('Escape')
    print('   menu impostazioni:', txt[:160])
    ok('Freeze' not in txt and 'Peak Hold' in txt, f"menu del grafico senza 'Freeze' ({len(txt)} caratteri)")
    sub = await ev("(() => { const t = []; document.querySelectorAll('.pm-i').forEach(e => t.push(e.textContent)); return t.join(','); })()")
    # spettro piu alto
    top = await ev("(() => { const PL = graph.PL; let m = 0; for (let x = PL.x; x < PL.x + PL.w; x += 2) { const d = graph.specDb(x); if (d !== null) m = Math.max(m, (PL.y + PL.h - graph.AY(d)) / PL.h); } return m; })()")
    ok(top > 0.6, f"spettro: il picco arriva al {top*100:.0f}% dell'altezza")
    await pg.screenshot(path='shot_045.png')
    ok(not errs, f"nessun errore JS ({errs[:2]})")
    print(f"RISULTATO 0.4.5: {sum(R)} PASS, {len(R) - sum(R)} FAIL"); await br.close()
asyncio.run(main())
