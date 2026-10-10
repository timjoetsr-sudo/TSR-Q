# test della pagina con la clip "ESO - SAFE": riproduzione reale attraverso il motore, solo che segue la banda, cambi senza errori, fluidita
import asyncio, statistics as st_
from playwright.async_api import async_playwright
res = []
def ok(c, m): res.append(bool(c)); print(('PASS ' if c else 'FAIL ') + m, flush=True)
async def main():
  async with async_playwright() as p:
    br = await p.chromium.launch(executable_path='/opt/pw-browsers/chromium-1194/chrome-linux/chrome', args=['--autoplay-policy=no-user-gesture-required'])
    pg = await br.new_page(viewport={'width': 1400, 'height': 800}); errs = []
    pg.on('pageerror', lambda e: errs.append(str(e))); pg.on('console', lambda m: errs.append(m.text) if m.type == 'error' and 'Failed to load resource' not in m.text else None)   # font di Google e favicon non raggiungibili in sandbox: non sono errori del programma
    await pg.goto('http://localhost:8770/TSR_Q_test_ESO_SAFE.html'); await pg.wait_for_timeout(500)
    ev = pg.evaluate
    r = await ev("window.__tsrqClip && window.__tsrqClip.bytes"); ok(r and r > 7_000_000, f'clip incorporata nella pagina: {r} byte base64')
    await ev("window.__tsrq.play()"); await pg.wait_for_timeout(2500)
    r = await ev("({f: fileName, p: playing, e: engineKind, d: buf && buf.duration, fs: ctx.sampleRate})")
    ok(r['f'] == 'ESO - SAFE.mp3' and r['p'] and 147 < (r['d'] or 0) < 150, f"PLAY: suona {r['f']} ({r['d'] and round(r['d'])} s) con motore {r['e']} a {r['fs']} Hz")
    r = await ev("({pk: meter.peak, pin: meter.pin, fs: ctx.sampleRate})"); FS = r['fs']
    ok(max(r['pk']) > 0.05 and max(r['pin']) > 0.05, f"audio reale nel motore: picco in {max(r['pin']):.2f}, picco out {max(r['pk']):.2f} (lineare)")
    # spettro post in movimento (musica vera)
    a = await ev("Array.from(graph._specPost().slice(100, 300))"); await pg.wait_for_timeout(400); b = await ev("Array.from(graph._specPost().slice(100, 300))")
    ok(sum(abs(x - y) for x, y in zip(a, b)) > 5, 'spettro post cambia nel tempo (musica, non segnale fisso)')
    # banda + solo: il solo segue la banda (energia dell uscita dove sta la banda)
    await ev("graph.createBand(200, 6, 0, 1); P.bands[0].q = 4; solo = 0; dirty = true; flush()"); await pg.wait_for_timeout(1200)
    def E(spec, lo, hi): return st_.mean(spec[lo:hi])
    s1 = await ev("Array.from(graph._specPost())"); n = len(s1); fs = r['pin'] and 48000
    # bin -> frequenza: specPost ha n bin fino a fs/2
    bw = FS / 2 / n; lo200, hi200 = int(150 / bw), int(260 / bw); lo2k, hi2k = int(1600 / bw), int(2600 / bw)
    e200a, e2ka = E(s1, lo200, hi200), E(s1, lo2k, hi2k)
    await ev("P.bands[0].freq = 2000; dirty = true; flush()"); await pg.wait_for_timeout(1500); s2 = await ev("Array.from(graph._specPost())")
    e200b, e2kb = E(s2, lo200, hi200), E(s2, lo2k, hi2k)
    ok(e2kb > e2ka + 6 and e200b < e200a - 6, f'solo segue la banda spostata 200 Hz -> 2 kHz: energia a 2 kHz {e2ka:.1f} -> {e2kb:.1f} dB, a 200 Hz {e200a:.1f} -> {e200b:.1f} dB')
    await ev("P.bands[0].q = 0.3; dirty = true; flush()"); await pg.wait_for_timeout(1200); s3 = await ev("Array.from(graph._specPost())")
    ok(E(s3, lo200, hi200) > e200b + 6, f'solo allargando la banda (Q 4 -> 0.3): energia a 200 Hz {e200b:.1f} -> {E(s3, lo200, hi200):.1f} dB (si sente di piu)')
    await ev("solo = -1; dirty = true; flush()"); await pg.wait_for_timeout(600)
    # cambi di stato in sequenza mentre suona: nessun errore, motore vivo
    for code in ["P.bands[0].type = 5; dirty = true; flush()", "P.bands[0].byp = 1; dirty = true; flush()", "P.bands[0].byp = 0; P.bands[0].dyn = 1; dirty = true; flush()", "P.char = 2; dirty = true; flush()", "P.bypass = 1; dirty = true; flush()", "P.bypass = 0; P.char = 0; dirty = true; flush()", "P.out = -12; dirty = true; flush()", "P.out = 0; dirty = true; flush()"]:
      await ev(code); await pg.wait_for_timeout(350)
    r = await ev("({pk: meter.peak, p: playing, pos: window.__tsrq.wave.state().head})"); ok(r['p'] and max(r['pk']) > 0.05, f"dopo 8 cambi di stato in riproduzione: ancora in play, picco out {max(r['pk']):.2f}")
    # fluidita: waveform a ogni fotogramma, mai indietro
    await ev("window.__tsrq.wave.open(true); window.__tsrq.wave.set({view:'wave'})"); await pg.wait_for_timeout(800)
    w = await ev("new Promise(res=>{const a=[];const f=()=>{a.push([performance.now(),window.__tsrq.wave.state().disp]);if(a.length<90)requestAnimationFrame(f);else res(a)};requestAnimationFrame(f)})")
    inc = [(w[i+1][1]-w[i][1])/max(1e-3,(w[i+1][0]-w[i][0])/1000) for i in range(len(w)-1)]; neg = sum(1 for v in inc if v < 0); m = st_.mean(inc); cv = st_.pstdev(inc)/m if m else 9
    ok(neg == 0 and abs(m - FS / 64) / (FS / 64) < 0.1 and cv < 0.15, f'waveform sulla clip: {neg} passi indietro, {m:.0f} gruppi/s (attesi {FS / 64:.0f} a {FS} Hz), irregolarita {cv*100:.1f} %')
    await ev("window.__tsrq.wave.open(false)")
    ok(not errs, f'nessun errore JavaScript ({len(errs)}): ' + '; '.join(errs[:3]))
    await pg.screenshot(path='clip_page.png'); await br.close()
  print(f'RISULTATO pagina clip: {sum(res)} PASS, {len(res)-sum(res)} FAIL'); raise SystemExit(0 if all(res) else 1)
asyncio.run(main())
