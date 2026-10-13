import asyncio, json, sys
from playwright.async_api import async_playwright
async def main():
  async with async_playwright() as p:
    br = await p.chromium.launch(executable_path='/opt/pw-browsers/chromium-1194/chrome-linux/chrome', args=['--autoplay-policy=no-user-gesture-required'])
    pg = await br.new_page(viewport={'width': 1300, 'height': 860}); await pg.goto(sys.argv[1]); await pg.wait_for_timeout(2500)
    await pg.evaluate("window.__tsrq.play()"); await pg.wait_for_timeout(3000)
    allv = []; tops = []
    for t in range(15):
      r = await pg.evaluate("""() => { const PL = graph.PL, ys = graph.smoothYs(graph._specPost(), PL); return { h: ys.map(y => (PL.y + PL.h - y) / PL.h) }; }""")
      h = sorted(r['h']); allv += h; tops.append(h[-1]); await pg.wait_for_timeout(400)
    allv.sort(); q = lambda f: round(allv[int(f * (len(allv) - 1))], 3)
    print(json.dumps({'p05': q(.05), 'p50': q(.5), 'p95': q(.95), 'picco_medio': round(sum(tops) / len(tops), 3), 'picco_max': round(max(tops), 3)}))
    await br.close()
asyncio.run(main())
