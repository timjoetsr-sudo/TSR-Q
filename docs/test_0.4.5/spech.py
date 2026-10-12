import asyncio, sys, json
from playwright.async_api import async_playwright
URL = sys.argv[1]
async def main():
  async with async_playwright() as p:
    br = await p.chromium.launch(executable_path='/opt/pw-browsers/chromium-1194/chrome-linux/chrome', args=['--autoplay-policy=no-user-gesture-required'])
    pg = await br.new_page(viewport={'width': 1300, 'height': 860}); await pg.goto(URL); await pg.wait_for_timeout(2500)
    await pg.evaluate("window.__tsrq.play()"); await pg.wait_for_timeout(3000)
    res = []
    for t in range(12):
      r = await pg.evaluate("""() => { const PL = graph.PL; let top = 1e9, ys = []; for (let x = PL.x; x < PL.x + PL.w; x += 2) { const d = graph.specDb(x); if (d === null) continue; const y = graph.AY(d); ys.push(y); if (y < top) top = y; }
        ys.sort((a,b)=>a-b); return { top: (PL.y + PL.h - top) / PL.h, med: (PL.y + PL.h - ys[ys.length>>1]) / PL.h, n: ys.length }; }""")
      res.append(r); await pg.wait_for_timeout(500)
    print(json.dumps({'max_top': max(r['top'] for r in res), 'mean_top': sum(r['top'] for r in res)/len(res), 'mean_med': sum(r['med'] for r in res)/len(res)}))
    await br.close()
asyncio.run(main())
