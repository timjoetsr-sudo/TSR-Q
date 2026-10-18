# TSR Q 0.5.0 · movimento dei filtri (revisione esterna + intervento)

**Commit:** `b2eaa83`, `2e763be`.
**Base:** 0.4.9 (`a63c307`).

Legenda dei tag: [M] = MISURATO, [C] = LETTO NEL CODICE, [NV] = NON VERIFICATO.

## Cosa ha trovato la revisione esterna sulla 0.4.9

Un agente indipendente ha analizzato la 0.4.9. I punti principali:
- Quando un parametro cambia, la rampa lineare dei coefficienti biquad (TDF-II) passa per filtri intermedi con un guadagno molto sbagliato [M].
- Caso più grave: High Cut 48 dB/oct da 20 kHz a 200 Hz a 192 kHz. Su un tono a 100 Hz il picco arriva a **+11,5 dB**, su rumore a **+34,8 dB** [M]. È confermato anche sul VST3 vero (−0,52 dBFS contro −12,04 a regime) [M].
- Con automazione casuale a ogni blocco gli eccessi arrivano fino a +87 dB (High Cut) e +137 dB (Band Pass) rispetto ai filtri visitati [M].

## Cosa è cambiato

- **Tipi che si muovono come SVF.** Low/High Cut, Low/High Shelf, Band Pass e Tilt girano come SVF trapezoidale (TPT) durante la rampa. All'inizio e alla fine della rampa lo stato viene convertito in quello equivalente, in modo che l'uscita resti continua [C].
  - Fuori dalle rampe il filtro resta biquad: la CPU non cambia [M].
- **Tipi che restano biquad.** Bell, Notch e All Pass restano biquad anche in rampa. Con questi tipi l'interpolazione è innocua (0,0 dB misurati) e il residuo di modulazione è più basso [M].
- **Sezioni del primo ordine.** Restano biquad: altrimenti, come ha trovato la seconda revisione, davano un click [M].
  - Lo stato non osservabile si converte con la pseudo-inversa [C].
- **Salti grandi in un solo cambio.** Se il cambio supera un'ottava, Q ×2 o 12 dB, vecchio e nuovo filtro si dissolvono uno nell'altro in 15 ms.
  - Esempio [M]: Low Shelf Q40 da +30 a −30 dB, da 100 Hz a 5 kHz, tono a 300 Hz. Con la sola rampa SVF c'era uno scoppio di +38 dB; con la dissolvenza il picco resta −1,5 dB sotto i due regimi.
- **Fine della dissolvenza.** Dove i pesi valgono esattamente 1 e 0, l'uscita non viene più ricalcolata, così nessun arrotondamento dipende dal blocco [C].

## Misure, 0.4.9 → 0.5.0 [M]

**Salti e spazzate (picco oltre il percorso):**

| Prova | 0.4.9 | 0.5.0 |
|---|---|---|
| Salto HC48 20 kHz → 200 Hz @192k, tono | +11,52 dB | +0,02 dB |
| Salto HC48 20 kHz → 200 Hz @192k, rumore | +34,78 dB | −3,1 dB |
| Spazzate di HC48, LC48, BP12 Q10, HS e Bell in 20–1000 ms | fino a +4,67 dB | ≤ +0,11 dB |

**Automazione casuale a ogni blocco** (sonda della revisione: blocchi da 1, 7, 64, 4096 e casuali; 44,1 e 192 kHz; Stereo e Mid), caso peggiore:

| Tipo | 0.4.9 | 0.5.0 |
|---|---|---|
| High Cut | +87,4 dB | +0,5 dB |
| Band Pass | +137,6 dB | +7,2 dB |
| Low Shelf | +62,3 dB | +1,4 dB |
| High Shelf | +33,0 dB | +2,1 dB |
| Low Cut | +78,8 dB | +13,3 dB |
| Flat Tilt | +14,3 dB | +0,5 dB |
| Bell | +28,1 dB | +28,1 dB (invariato: resta biquad) |

Su 160 casi, 2 sono peggiorati di più di 0,5 dB: Low Cut a 44,1 kHz con blocco 4096, da +10,1 a +13,3 dB.

**Altre metriche:**
- Zipper: 16 casi su 20 identici e 4 migliori (High Shelf da −118,3 a −123,8 dBFS; Low Cut).
- Tutte le altre 147 metriche del CSV sono identiche.
- CPU: invariata entro il rumore della macchina.

**Prove di regressione:**

| Prova | Esito |
|---|---|
| dsp_tests | 69 PASS (sezioni 25 e 26 nuove; la 0.4.9 fallisce la 25) |
| stem | 113/113 |
| self-test | 23/23 |
| clip | 8/8 |
| confronto | 15/15 |
| parità JS = C++ | tutte PASS |
| pluginval livello 10 | SUCCESS |

- Plugin VST3 vero: null, blocchi, mono e stato tutti identici al bit; salto HC48 @192k −12,02 dBFS su un regime di −12,04.
- CI macOS (macos-14), commit `2e763be`:
  - AU VALIDATION SUCCEEDED;
  - pluginval AU e VST3 SUCCESS;
  - test DSP tutti PASS.

## Confronto con Pro-Q 4

NON ESEGUITO: in questa sessione il Mac non è raggiungibile.

Lo script `confronto_proq4.py` è pronto. Misura entrambi i plugin con gli stessi segnali e lo stesso host:
- cramping a 48 kHz contro 192 kHz;
- differenza di forma;
- latenza;
- picchi nei salti;
- CPU.

Ho provato il codice dello script mettendo TSR Q in tutti e due i posti [M]. I nomi dei parametri di Pro-Q 4 sono **NON VERIFICATI**: se uno non si trova, lo script si ferma e scrive l'elenco dei parametri in `confronto_proq4_parametri.txt`.

## Non verificato

- Pro-Q 4 (qualunque numero).
- Il Mac di Tim e Ableton.
- Bell in automazione casuale estrema (+28 dB, uguale alla 0.4.9).
- Low Cut con blocco 4096 a 44,1 kHz (+13,3 dB). Non so se sia l'effetto del percorso (risonanza con Q fino a 40) o un difetto.
- Il prototipo e la pagina A/B in JS non hanno la rampa SVF. La parità JS = C++ passa comunque, ma gli scenari di parità non contengono salti grandi.
