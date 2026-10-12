import asyncio, json
from playwright.async_api import async_playwright
async def main():
  async with async_playwright() as p:
    br = await p.chromium.launch(executable_path='/opt/pw-browsers/chromium-1194/chrome-linux/chrome', args=['--autoplay-policy=no-user-gesture-required'])
    pg = await br.new_page(viewport={'width': 1300, 'height': 860}); errs = []; pg.on('pageerror', lambda e: errs.append(str(e)))
    await pg.goto('http://localhost:8771/TSR_Q_test_ESO_SAFE.html'); await pg.wait_for_timeout(2500)
    await pg.evaluate("window.__tsrq.play()"); await pg.wait_for_timeout(1000)
    await pg.evaluate("window.__tsrq.wave && window.__tsrq.wave.open && window.__tsrq.wave.open(false)"); await pg.wait_for_timeout(2500)
    out = []
    for t in range(10):
      r = await pg.evaluate("""() => { const PL = graph.PL, sp = graph._specPost(), raw = [], sm = graph.smoothYs(sp, PL); for (let j = 0; j < sm.length; j++) { const d = graph.specDb(PL.x + j); raw.push(Math.min(PL.y + PL.h, Math.max(PL.y, graph.AY(d)))); }
        const rough = a => { let s = 0, d = []; for (let j = 1; j < a.length; j++) { const v = Math.abs(a[j] - a[j-1]); s += v; d.push(v); } d.sort((x,y)=>x-y); return [s / (a.length - 1), d[Math.floor(d.length * .99)]]; };
        const top = a => (PL.y + PL.h - Math.min(...a)) / PL.h; return { raw: rough(raw), sm: rough(sm), topRaw: top(raw), topSm: top(sm) }; }""")
      out.append(r); await pg.wait_for_timeout(400)
    m = lambda f: sum(f(r) for r in out) / len(out)
    print(json.dumps({'ruvidita_media_px_grezzo': round(m(lambda r: r['raw'][0]), 2), 'ruvidita_media_px_smussato': round(m(lambda r: r['sm'][0]), 2),
      'salto_p99_px_grezzo': round(m(lambda r: r['raw'][1]), 2), 'salto_p99_px_smussato': round(m(lambda r: r['sm'][1]), 2),
      'altezza_picco_grezzo': round(m(lambda r: r['topRaw']), 3), 'altezza_picco_smussato': round(m(lambda r: r['topSm']), 3), 'errori_js': errs}, ensure_ascii=False))
    await pg.screenshot(path='spec_smooth_045.png', clip={'x': 0, 'y': 0, 'width': 1300, 'height': 640})
    await br.close()
asyncio.run(main())
