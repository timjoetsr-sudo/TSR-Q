# test della pagina di ascolto A/B: i due motori nell'AudioWorklet, letture di loudness, cambio percorso, scenari, nessun errore
import asyncio, json
from playwright.async_api import async_playwright
res = []
def ok(c, m): res.append(bool(c)); print(('PASS ' if c else 'FAIL ') + m)
async def main():
    async with async_playwright() as p:
        b = await p.chromium.launch(executable_path='/opt/pw-browsers/chromium-1194/chrome-linux/chrome', args=['--autoplay-policy=no-user-gesture-required'])
        pg = await b.new_page(viewport={'width': 1300, 'height': 860}); errs = []; logs = []
        pg.on('pageerror', lambda e: errs.append(str(e))); pg.on('console', lambda m: logs.append(m.text) if m.type == 'error' else None)
        await pg.goto('http://localhost:8771/TSR_Q_ascolto_AB.html'); await pg.wait_for_timeout(2500)
        ok(await pg.locator('.abbar').count() == 1, 'barra A/B presente')
        r = await pg.evaluate("(()=>{const e=document.querySelector('#ed').getBoundingClientRect(),a=document.querySelector('.abbar').getBoundingClientRect();return {edB:e.bottom,abT:a.top}})()")
        ok(r['edB'] <= r['abT'] + 1, f"il plugin non finisce sotto la barra A/B (fondo plugin {r['edB']:.0f} px, inizio barra {r['abT']:.0f} px)")
        await pg.click('.clipbar [data-a=play]'); await pg.wait_for_timeout(9000)
        eng = await pg.evaluate("window.__tsrq.engine()"); ok(eng == 'AudioWorklet', f'motore: {eng}')
        txt = await pg.inner_text('.abbar .ro'); print('   ', txt)
        import re
        nums = re.findall(r'(DRY|0\.4\.3|0\.4\.4|DIFF) (-?\d+\.\d)', txt); ok(len(nums) >= 4, f'letture LUFS per i 4 percorsi: {nums}')
        ok('compensazione' in txt, 'loudness match attivo con compensazione misurata')
        for s, nm in ((0, 'DRY'), (1, '0.4.3'), (3, 'DIFF'), (2, '0.4.4')):
            await pg.click(f'.abbar [data-s="{s}"]'); await pg.wait_for_timeout(700)
            on = await pg.evaluate(f"document.querySelector('.abbar [data-s=\"{s}\"]').classList.contains('on')"); ok(on, f'percorso {nm} selezionato')
        for k in ('kick', 'warm', 'subtle', 'eq', 'dyn200'):
            await pg.click(f'.abbar [data-p="{k}"]'); await pg.wait_for_timeout(4500); t = await pg.inner_text('.abbar .ro'); print(f'    {k}: {t}')
            ok(t.count('…') == 0 or 'in misura' in t, f'scenario {k}: letture presenti')
        P = await pg.evaluate("JSON.stringify({b:window.__tsrq.P().bands[0],inp:window.__tsrq.P().inp,ch:window.__tsrq.P().char})"); print('   stato dopo dyn200:', P[:200])
        ok(not errs, f'nessun errore JavaScript ({errs[:2]})'); ok(not [l for l in logs if 'errore motore' in l], f'nessun errore motore ({logs[:2]})')
        await pg.screenshot(path='/home/claude/lab/web/ab/ab_page.png')
        await b.close()
    print(f'RISULTATO pagina A/B: {sum(res)} PASS, {len(res) - sum(res)} FAIL')
asyncio.run(main())
