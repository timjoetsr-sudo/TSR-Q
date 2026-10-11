import base64, os, sys
P = os.path.dirname(os.path.abspath(__file__)); S2 = sys.argv[1]
s = open(sys.argv[2], encoding='utf-8').read()
b64 = base64.b64encode(open(S2 + '/clip/eso_safe.mp3', 'rb').read()).decode()
# 1. spazio per la barra di trasporto: il plugin si adatta all'altezza che resta
a = "s = Math.min((innerWidth - m) / EW, (innerHeight - m) / EH, pl ? 4 : 1.3)"; assert s.count(a) == 1
s = s.replace(a, "s = Math.min((innerWidth - m) / EW, (innerHeight - m - (pl ? 0 : (window.__tsrqBar || 0))) / EH, pl ? 4 : 1.3)")
s = s.replace('<title>TSR Q · Ceramica e platino</title>', '<title>TSR Q · Test ESO – SAFE</title><script>window.__tsrqBar = 64;</script>')
mod = r'''<script>'use strict';
/* ====== pagina di TEST: clip "ESO - SAFE" incorporata (MP3 320 kb/s, 48 kHz, 2:28) e barra di trasporto sotto il plugin.
   PLAY la suona in loop attraverso il motore TSR Q e apre la waveform; AUDIO carica un altro file. ====== */
(() => {
  const NAME = 'ESO - SAFE.mp3', B64 = '%%B64%%';
  const st = document.createElement('style'); st.textContent = 'body{box-sizing:border-box;padding-bottom:64px}' +
    '.clipbar{position:fixed;left:0;right:0;bottom:0;height:64px;display:flex;align-items:center;justify-content:center;gap:12px;background:#0a0b0c;border-top:1px solid rgba(255,255,255,.08);z-index:1000;font:700 12px/1 "Barlow Semi Condensed","Arial Narrow",system-ui,sans-serif;letter-spacing:.08em;color:#c9ced6}' +
    '.clipbar button{background:#fff;border:1px solid rgba(27,27,27,.16);border-radius:3px;color:#1b1b1b;font:inherit;letter-spacing:inherit;height:30px;padding:0 16px;cursor:pointer}.clipbar button.on{background:#ffd23f}.clipbar button:disabled{opacity:.5;cursor:default}' +
    '.clipbar .nm{color:#eef2f6;font-size:13px}.clipbar .tm{font-variant-numeric:tabular-nums;min-width:92px;text-align:center;color:#8b939d}.clipbar .er{color:#ff6b6b}';
  document.head.appendChild(st);
  const bar = document.createElement('div'); bar.className = 'clipbar';
  bar.innerHTML = '<button data-a="play">▶ PLAY</button><span class="nm">' + NAME + ' · 2:28 · loop</span><span class="tm">0:00 / 2:28</span><button data-a="wave">WAVEFORM</button><button data-a="file">AUDIO…</button>';
  document.body.appendChild(bar);
  const bP = bar.querySelector('[data-a=play]'), tm = bar.querySelector('.tm'), bW = bar.querySelector('[data-a=wave]'), bF = bar.querySelector('[data-a=file]'), nm = bar.querySelector('.nm');
  let clip = null, loading = null, t0 = 0;
  const mmss = x => Math.floor(x / 60) + ':' + String(Math.floor(x % 60)).padStart(2, '0');
  const bytes = () => { const s = atob(B64), u = new Uint8Array(s.length); for (let i = 0; i < s.length; i++) u[i] = s.charCodeAt(i); return u.buffer; };
  const loadClip = () => loading || (loading = (async () => { await ensureAudio(); nm.textContent = NAME + ' · decodifica…';
    try { clip = await ctx.decodeAudioData(bytes()); if (!buf) { buf = clip; fileName = NAME; } dirty = true; return clip; }
    finally { nm.textContent = NAME + ' · 2:28 · loop'; } })());
  const go = async () => {
    if (playing) { stopSrc(); return; }
    try { if (!buf) await loadClip(); } catch (e) { nm.innerHTML = '<span class="er">clip non caricata: ' + (e && e.message) + '</span>'; return; }
    await play(); t0 = ctx.currentTime; if (!window.__tsrq.wave.state().view || !document.querySelector('.wpan.on')) { window.__tsrq.wave.open(true); window.__tsrq.wave.set({ view: 'wave' }); }
  };
  bP.onclick = go; bW.onclick = () => { const on = !document.querySelector('.wpan.on'); window.__tsrq.wave.open(on); if (on) window.__tsrq.wave.set({ view: 'wave' }); }; bF.onclick = () => fileIn.click();
  window.__tsrq.play = go; window.__tsrqClip = { name: NAME, load: loadClip, bytes: B64.length };
  const tick = () => { bP.textContent = playing ? '■ STOP' : '▶ PLAY'; bP.classList.toggle('on', playing);
    const d = buf ? buf.duration : 148; const e = playing && ctx ? ((ctx.currentTime - t0) %% d) : 0; tm.textContent = mmss(e) + ' / ' + mmss(d);
    if (buf && fileName !== NAME && !loading) nm.textContent = fileName + ' · ' + mmss(d) + ' · loop'; requestAnimationFrame(tick); };
  requestAnimationFrame(tick);
  if (typeof doFit === 'function') doFit();
  setTimeout(() => { loadClip().catch(() => {}); }, 400);   /* decodifica in anticipo: al primo PLAY la clip e gia pronta */
})();
</script></body></html>'''.replace('%%B64%%', b64).replace('%%', '%')
assert s.count('</script></body></html>') == 1
s = s.replace('</script></body></html>', '</script>\n' + mod)
open(sys.argv[3], 'w', encoding='utf-8').write(s)
print(os.path.getsize(sys.argv[3]) / 1e6, 'MB')
