# costo dell'interfaccia: movimento del mouse sul grafico (fantasma del clic-per-creare) e trascinamento di una banda, 0.4.3 (porta 8770) vs 0.4.4 (porta 8771)
import asyncio, json, sys
from playwright.async_api import async_playwright
JS = r"""async () => {
  const T = window.__tsrq, P = T.P(); T.graph.selectAll(); T.graph.deleteSelected();
  const mk = (f, g, t) => T.graph.createBand(f, g, t);
  // 12 bande: 8 Bell, 2 High/Low Cut 96 dB/oct, 2 Band Pass 96 dB/oct
  [60, 120, 250, 500, 1000, 2000, 4000, 8000].forEach((f, k) => mk(f, k % 2 ? -3 : 3, 0));
  const a = mk(30, 0, 3), b = mk(16000, 0, 4); P.bands[a].slope = 8; P.bands[b].slope = 8;   /* 96 dB/oct */
  const c = mk(700, 0, 6), d = mk(5000, 0, 6); P.bands[c].slope = 8; P.bands[d].slope = 8;
  window.dirty = true; await new Promise(r => setTimeout(r, 300));
  const gw = document.querySelector('.graph canvas.ovl') || document.querySelector('.graph canvas'); const R = gw.getBoundingClientRect();
  const fire = (x, y) => gw.dispatchEvent(new PointerEvent('pointermove', { clientX: x, clientY: y, bubbles: true, buttons: 0 }));
  // movimenti sulla curva (a 0 dB fuori dalle bande la curva e vicina al centro): 200 posizioni orizzontali, y sulla curva
  const ts = []; for (let k = 0; k < 200; k++) { const x = R.left + R.width * (0.08 + 0.84 * k / 199), f = T.graph.Fx((x - R.left) / R.width * 1160);
     const yc = R.top + T.graph.Y(0) / 532 * R.height; const t0 = performance.now(); fire(x, yc); ts.push(performance.now() - t0); }
  ts.sort((p, q) => p - q);
  // trascinamento: 120 cambi di frequenza della banda 4 con flush (setState + curve + auto gain)
  const td = []; for (let k = 0; k < 120; k++) { const t0 = performance.now(); P.bands[4].freq = 1000 * Math.pow(2, Math.sin(k / 10)); window.dirty = true; if (window.flush) window.flush(); T.graph.invalidate && T.graph.invalidate(); td.push(performance.now() - t0); }
  td.sort((p, q) => p - q);
  return { hover_median_ms: ts[100], hover_p95_ms: ts[190], hover_max_ms: ts[199], drag_median_ms: td[60], drag_p95_ms: td[114], drag_max_ms: td[119] };
}"""
async def main():
    out = {}
    async with async_playwright() as p:
        b = await p.chromium.launch(executable_path='/opt/pw-browsers/chromium-1194/chrome-linux/chrome')
        for name, port in (('0.4.3', 8770), ('0.4.4', 8771)):
            res = []
            for rep in range(3):
                pg = await b.new_page(viewport={'width': 1300, 'height': 720}); await pg.goto(f'http://localhost:{port}/TSR_Q_09_ceramica_smart.html'); await pg.wait_for_timeout(700)
                res.append(await pg.evaluate(JS)); await pg.close()
            out[name] = {k: round(sorted(r[k] for r in res)[1], 3) for k in res[0]}
        await b.close()
    print(json.dumps(out, indent=1))
asyncio.run(main())
