# TSR Q 0.4.9 · DSP QUALITY PASS · rapporto

**Contesto.** Nel template il campo "Plugin target" era vuoto. Ho preso come plugin target TSR Q, il plugin di questa sessione.
- Repo: `timjoetsr-sudo/tsr-q` @ `main`. Il pass parte dal commit `7f5cc16` (0.4.8) e arriva al commit `af98d88` (0.4.9).
- File toccati: `Source/TsrqDsp.h`, `Source/PluginProcessor.cpp`, `CMakeLists.txt`, `tests/dsp_tests.cpp`, `tests/quality_csv.cpp` (nuovo).
- GUI (Editor, WebEditor, `web/index.html`, asset): **non toccata**.
- Riferimento di mercato: FabFilter Pro-Q 4.
- Uso reale: non indicato nel template.

Legenda dei tag: [M] = MISURATO, [C] = LETTO NEL CODICE, [L] = DA LETTERATURA, [NV] = NON VERIFICATO.

---

## Fase A — Inventario [C]

**Catena del segnale** (`Engine::process` → `processChunk`), in ordine:
1. **Ingresso.** Ogni campione NaN/Inf o oltre ±1000 (+60 dBFS) diventa 0.
2. **INPUT.** Guadagno con smoother a 2 poli (10 + 10 ms). Il segnale `dL/dR` resta il dry usato dal bypass.
3. **32 bande.** Cascate di biquad TDF-II in double.
   - Posizione della banda: Stereo, L, R, Mid o Side.
   - Bande statiche: rampa dei coefficienti di 512 campioni fissi.
   - Bande dinamiche: frame da 32 campioni, detector RMS o Peak, sidechain esterno opzionale.
4. **Solo** (monitor), con dissolvenza di 15 ms.
5. **OUTPUT.** Smoother a 2 poli, −∞ sotto −60 dB.
6. **Character.** Clean, Subtle o Warm. Subtle e Warm usano oversampling 8× con halfband IIR, poi saturazione e DC blocker a 5 Hz.
7. **Pan** (L/R o M/S), smoother a 2 poli.
8. **Polarità**, dissolvenza a coseno rialzato di 15 ms.
9. **Bypass** verso il dry, dissolvenza a coseno rialzato di 15 ms.

**Parametri** (`createLayout`): 32 bande × 16 parametri, più 10 globali.
- Freq: 10–30000 Hz, scala logaritmica. In processBlock è limitata a 0,495·fs.
- Gain: ±30 dB.
- Q: 0,025–40, scala logaritmica.
- Thr: −60…0 dB. Range: ±24 dB.
- Attack: 0,1–200 ms, scala logaritmica. Release: 5–2000 ms, scala logaritmica.
- Knee: 0–24 dB.
- Input: ±24 dB. Output: −60 (= −∞)…+36 dB.
- Pan: ±100 %. Scale: −100…200 %.
- Smoothing: elencato nella catena qui sopra. La soglia dinamica è levigata in 10 ms.

**Latenza dichiarata.** Nessuna chiamata a `setLatencySamples`, quindi l'host riceve 0. `getTailLengthSeconds` restituisce 0.

**Sample rate.** `prepare` ricalcola griglia, dissolvenze, costanti del detector e oversampling.

**Allocazioni e lock in processBlock.** Non ne ho trovati leggendo il codice: solo array su stack da 1024 float, FIFO lock-free e atomici.
- Nota: il progetto dei filtri (`designBand`) gira nel thread audio dentro `setBand`.
- `ScopedNoDenormals`: presente.

**Stato salvato.** XML dell'APVTS, più la mappa MIDI CC e `tsrqStateVersion` = 2.

---

## Fase C — Diagnosi (in ordine di impatto udibile)

| # | Difetto | Prova | Tipo |
|---|---|---|---|
| 1 | Al primo blocco dopo `prepare`, OUTPUT, INPUT, pan, bypass, polarità e Character partono dai valori di default e poi scivolano verso quelli impostati. Effetti: OUTPUT −∞ fa uscire audio a −4,2 dBFS per 161 ms; OUTPUT −12 dB parte da 0 dB (157 ms); un'istanza in bypass fa uscire audio processato per 15 ms; Warm parte Clean per 30 ms. | [M] sezione `avvio` del CSV; [C] `primed = true` impostato prima dello stadio d'uscita | **difetto** |
| 2 | Lo stato salvato/ripristinato non è identico al bit: Freq e Q derivano di 1–3 ulp a ogni ripristino VST3. | [M] uscita diversa a −138,5 dBFS; [C] scala logaritmica in float, non idempotente (14–18 % dei valori, `fixpt.cpp`) | **difetto** (inudibile) |
| 3 | Character Subtle/Warm ha un ritardo reale di 8 campioni (picco dell'impulso) e un ritardo di gruppo di 7,4 campioni a 1 kHz (11 a 100 Hz, 19 a 20 kHz). L'host riceve 0. Il bypass dissolve tra wet ritardato e dry non ritardato. | [M] `latenza` nel CSV, `gd5.cpp` | **dubbio / decisione** (vedi punto 6 della sezione "Prossimo difetto") |
| 4 | CPU della 0.4.8 rispetto alla 0.4.7. Automazione continua di 32 bande: +8 %. Primo blocco con 32 progetti precisi: +14 %. | [M] lab `cpuworst` | **compromesso** (decisione tua) |
| 5 | Residuo di cramping vicino a Nyquist: massimo 1,593 dB (Low Cut 24 dB/oct a 20 kHz, 44,1 kHz). È 1 caso su 17.880. | [M] griglia di precisione della 0.4.8 | **dubbio** |
| — | Attacco della dinamica: con attacco 10 ms il guadagno arriva al 63 % in 25 ms (gradino da −40 a −10 dBFS). | [M] grafico | **carattere**: il detector lavora in dB, il tempo include l'attraversamento della soglia |

**Nota su un errore di misura della baseline.** Nella prima baseline la latenza di Subtle/Warm risultava 0. Era un artefatto del difetto 1: nei primi 30 ms passava il percorso Clean. Misurata dopo 100 ms, anche la 0.4.8 ha 8 campioni [M]. I rapporti precedenti non riportavano questo ritardo assoluto.

---

## Fase D — Interventi (un difetto per commit, ri-misurati)

| Commit | Intervento | Misura prima → dopo |
|---|---|---|
| `d6546a7` | Rampa dei coefficienti incrementale con stato (`runInc`/`runInc2`) al posto della moltiplicazione per campione (`runFrame`). | Metriche del CSV invariate alla terza cifra [M]. Process con automazione lenta: 24,0 → 22,5 µs/blocco (−6 %) [M]. Istruzioni (callgrind): 847 → 828 M [M]. |
| `a8790b3` | `snapGlobals()` al primo blocco dopo `prepare`, più il test 24. | Avvio: scarto −4,2…+1,6 dBFS per 15–165 ms → identico al bit [M]. Test 24: 7 FAIL sul motore precedente, 7 PASS sul nuovo [M]. |
| `e67707d` | Scale logaritmiche dei parametri calcolate in double (valore normalizzato → in chiaro → normalizzato idempotente). | Stato: XML diverso, audio a −138,5 dBFS → XML identico in 8/8 stati casuali e audio identico [M]. Valori non fissi: float 14–18 % → double 0 su 10⁶ [M]. |
| `af98d88` | Harness `tsrq_quality_csv` e versione 0.4.9. | — |

**Tentativi annullati.**
- `kAlign` = 512: l'invarianza al blocco del test 22 falliva (−18,3 dBFS) [M].
- Salvare nello stato il valore normalizzato esatto: il wrapper VST3 lo ri-arrotondava comunque [M]. Sostituito dal commit `e67707d`.

**Specchio JS** (lab, prototipo e pagina A/B): ho aggiunto lo stesso snap. Parità JS = C++ dopo la modifica:
- master 10/0, nuove 11/0, Peak 6/0, casuale 40/0 (−151 dBFS), stem 26/0, scenari 21/0 [M].
- Prima della modifica JS gli scenari erano 19/2 [M].
- `web/index.html` (GUI del plugin) **non** è stato ricostruito.

---

## Tabella metriche (motore C++, harness `tsrq_quality_csv`; prima = 0.4.8 `7f5cc16`, dopo = 0.4.9 `af98d88`)

| Metrica | Prima | Dopo | Soglia | Esito |
|---|---|---|---|---|
| Aliasing Warm, sinusoide 0 dBFS 20 Hz–20 kHz, picco non armonico peggiore | −130,7 dBFS | −130,7 dBFS | ≤ −100 | PASS [M] |
| Aliasing Subtle | −155,2 dBFS | −155,2 dBFS | ≤ −100 | PASS [M] |
| THD+N neutro (100 Hz / 1 kHz / 10 kHz, −6 dBFS) | −153,6 / −153,7 / −153,7 dB | uguale | ≤ −110 | PASS [M] |
| THD+N con EQ tipica a 8 bande | −149,5…−150,0 dB | uguale | (riferimento) | [M] |
| Null, nessuna banda | identico al bit | identico al bit | ≤ −140 | PASS [M] |
| Null, bypass globale | identico al bit | identico al bit | ≤ −140 | PASS [M] |
| Null, bypass con EQ e Warm, dal primo campione | **−15,5 dBFS** | identico al bit | ≤ −140 | **PASS dopo D2** [M] |
| Latenza Clean, riportata / reale | 0 / 0 | 0 / 0 | uguali | PASS [M] |
| Latenza Subtle e Warm, riportata / reale (picco impulso) | 0 / 8 | 0 / 8 | uguali | **FAIL, non corretto** (decisione) [M] |
| Zipper, picco peggiore 2–20 kHz (Bell ±12 dB, blocco 512) | −115,5 dBFS | −115,5 dBFS | ≤ −90 | PASS [M] |
| Zipper, tutti i 9 parametri automatizzati, blocchi 64 e 512 | da −115,5 a −146,8 | uguale | ≤ −90 | PASS [M] |
| CPU 48 kHz, blocco 512, EQ a 8 bande, media | 32,7 µs (0.4.7: 32,0) | 32,2 µs | riportare | [M] |
| CPU 48 kHz, 8 bande + 2 dinamiche | 48,9 µs | 48,7 µs | riportare | [M] |
| CPU 48 kHz, 8 bande + Warm | 254,3 µs | 253,0 µs | riportare | [M] |
| CPU 96 kHz, EQ a 8 bande | 33,3 µs | 32,4 µs | riportare | [M] |
| CPU, caso peggiore: primo blocco da 64 con 32 bande, progetto preciso | 1089,5 µs (0.4.7: 959,8) | 1097,6 µs | riportare | [M] |
| CPU, 32 bande automatizzate a ogni blocco da 64, media | 105,1 µs (0.4.7: 96,1) | 104,1 µs | riportare | [M] |
| Dinamica: curva statica e risposta a gradino | — | `dinamica_049.png` | graficate | FATTO [M] |
| Blocchi 1 / 7 / 64 / 512 / 4096 / variabili, con automazione | identico al bit (0.4.7: −26,9 dBFS) | identico al bit | identico | PASS [M] |
| Sample rate 44,1–192 kHz: Bell 1 kHz +6 dB / attacco 10 ms al 63 % | 6,000 dB / 25 ms ovunque | uguale | identico | PASS [M] |
| Ingressi estremi (silenzio, DC, 0 dBFS, +12 dBFS sinusoide e rumore), campioni NaN/Inf | 0 | 0 | 0 | PASS [M] |
| `prepare` ripetuto (96 kHz → 48 kHz, doppio) | identico al nuovo | identico | identico | PASS [M] |
| Avvio dopo `prepare` (7 configurazioni) | scarto fino a +1,6 dBFS, 15–165 ms | identico al bit | nessuno stato sporco | **PASS dopo D2** [M] |
| Sidechain attivo con bus assente | 0 NaN, −6,00 dB | uguale | — | PASS [M] |

**Metriche sul plugin VST3 vero** (pedalboard, `plugin_quality_0.4.9.csv`).
- "Prima" qui è la build con D1 e D2 ma senza la correzione dello stato: il codice dello stato era quello della 0.4.8.
- Null neutro / bypass con EQ / bypass con EQ e Warm: identici al bit [M].
- Blocchi 7, 64, 512 e 4096: identici al bit [M].
- Mono contro stereo con L = R: identico [M].
- Stato: XML identico e audio identico dopo la correzione. Prima: XML diverso, audio a −138,5 dBFS [M].
- Latenza riportata 0; reale 0 in Clean e 8 in Subtle/Warm [M].

---

## Output dei comandi di verifica (estratti; log completi in `docs/test_0.4.9/log/`)

```
cmake --build build --config Release                   -> exit 0
./build/tsrq_dsp_tests                                 -> 66 PASS, "RISULTATO: tutti i test PASS", exit 0
./build/tsrq_stem_tests                                -> "RISULTATO: 113 PASS, 0 FAIL", exit 0
./build/tsrq_quality_csv                               -> 151 righe di misura in CSV (misure/quality_0.4.9.csv), exit 0
pluginval --strictness-level 10 --validate-in-process "TSR Q.vst3"
   ... Completed tests in pluginval / Fuzz parameters
   SUCCESS
   exit 0
selftest del plugin                                    -> 23 PASS, "RISULTATO PASS"
plugin vero sulla clip ESO - SAFE                      -> "RISULTATO plugin vero sulla clip: 8 PASS, 0 FAIL"
confronto esterno                                      -> "RISULTATO confronto e plugin vero: 15 PASS, 0 FAIL"
CI macOS (macos-14, universale), commit af98d88:
   auval -v aumf Tsrq Tsra  -> AU VALIDATION SUCCEEDED.
   pluginval AU             -> SUCCESS
   pluginval VST3           -> SUCCESS
   test DSP                 -> RISULTATO: tutti i test PASS (compresa la sezione 24)
```

Nota sui percorsi del template:
- `./build/tests/tsr_dsp_tests` non esiste in questo repo. I target reali sono `./build/tsrq_dsp_tests` e `./build/tsrq_quality_csv`.
- Il tipo AU è `aumf`, non `aufx` (letto in `scripts/build_mac.sh`).

---

## Confronto con FabFilter Pro-Q 4

Non ho usato numeri pubblicati né misure di Pro-Q 4 fatte da te. Per tutte le metriche (aliasing, THD+N, null, latenza, zipper, CPU, precisione vicino a Nyquist): **NON CONFRONTABILE SENZA MISURA**.

---

## NON VERIFICATO

1. Mac di Tim, **DA VERIFICARE SU MAC**:
   ```
   ./scripts/build_mac.sh
   auval -v aumf Tsrq Tsra
   pluginval --strictness-level 10 --validate "/Library/Audio/Plug-Ins/VST3/TSR Q.vst3"
   pluginval --strictness-level 10 --validate "/Library/Audio/Plug-Ins/Components/TSR Q.component"
   ```
   Ableton Live 12: caricamento, automazione di Freq/Gain/Q, bypass, salvataggio e riapertura del set.
   - La CI macos-14 **non** è il tuo Mac.
2. Assenza di allocazioni e lock in processBlock: verificata solo leggendo il codice, non con strumenti.
3. Set salvati con la 0.4.8: Freq e Q possono ricaricarsi diversi di ≤ 1 ulp, perché la scala ora è calcolata in double. Non misurato in una DAW.
4. Sidechain con bus presente, a livello di plugin: provato solo sul motore.
5. Blocco da 1 campione a livello di plugin: provato solo sul motore.
6. CPU su Apple Silicon: misure fatte su Xeon 2,1 GHz Linux. Il p99 per blocco oscilla di ±10 % fra un'esecuzione e l'altra.
7. AAX: non costruito.
8. Prototipo e pagina A/B pubblicati come artifact: non ripubblicati con lo snap JS.

---

## Prossimo difetto (solo elencato, non eseguito)

1. **Latenza di Character non riportata.** Reale 8 campioni, riportata 0 (sezione C, punto 3). Bypass e passaggio Clean↔Warm dissolvono tra percorsi sfasati. Servono una scelta tua tra due opzioni:
   - riportare 8 campioni e ritardare dry e Clean;
   - oversampling a fase lineare con latenza fissa.
2. **Costo del primo blocco con 32 bande.** 1,1 ms su un blocco da 64 campioni (83 % del tempo disponibile), dovuto al progetto preciso più la correzione vicino a Nyquist.
3. **Residuo di 1,593 dB** del Low Cut 24 dB/oct a 20 kHz, 44,1 kHz.
