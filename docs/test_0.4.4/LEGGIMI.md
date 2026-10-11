# TSR Q 0.4.4 · prove e misure (prima/dopo rispetto alla 0.4.3)

- `log/` — esiti sulla 0.4.4: test C++ (53/53), stem (113/113), self-test del plugin (23/23), plugin VST3 vero sulla clip ESO - SAFE (8/8),
  confronto esterno con analogico/JUCE/RBJ (15/15), pluginval livello 10, render A/B. `dsp_tests_su_motore_0.4.3_13_FAIL.log`: gli stessi test sul motore 0.4.3 (le sezioni 14–21 nuove falliscono).
- `parita/` — motore JS (prototipo, pagina di ascolto) = motore C++ (plugin): 114 controlli con soglia −90 dBFS, 20.000 progetti di filtri confrontati.
- `interfaccia/` — test Playwright del prototipo 0.4.4 (154) e della pagina di ascolto A/B (16).
- `misure/0.4.3`, `misure/0.4.4` — risultati grezzi delle stesse misure sui due motori (precisione, CPU, caso peggiore per blocco, detector, dinamica, NaN, fuzz, parametri estremi, invarianza al blocco).
- `ab/` — script dei render A/B con i plugin VST3 veri (pedalboard) e riepilogo della loudness (BS.1770, ffmpeg ebur128).
  `char_sub0.py` / `char_sub0.json`: Character sotto 0 dBFS sulla clip, residuo 0.4.4 − 0.4.3 grezzo (−19,3 dB) e dopo l'allineamento di 0,62 campioni (−52,1 dB Warm, −52,2 dB Subtle).
- `mac_*.txt` — CI macOS (macos-14, build universale arm64 + x86_64) sul commit e3209ce: test DSP 53/53 PASS, `auval` AU VALIDATION SUCCEEDED, pluginval AU e VST3 SUCCESS.
- `script/` — laboratorio di misura (`lab.cpp`, `lab.js`), pipeline completa, costruzione del prototipo e delle pagine.

Gli script usano i percorsi della macchina di prova (`/home/claude/lab`, motore 0.4.3 congelato in `base043/`): per rieseguirli altrove vanno adattati.
I tempi di CPU sono misurati su Xeon 2,1 GHz (2 core) Linux, senza altri processi attivi; il caso peggiore per blocco è il minimo su 5 ripetizioni per ogni blocco.
