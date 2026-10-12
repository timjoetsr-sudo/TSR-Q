# TSR Q — Precision Dynamic EQ · TSR Audio · v0.4.4

Plugin JUCE 8 (C++17). Formati: **AU + VST3 + Standalone su macOS** (universale Apple Silicon + Intel, macOS 11+), VST3 + Standalone su Linux/Windows.
Produttore: **TSR Audio** (codice produttore `Tsra`, codice plugin `Tsrq`, bundle `com.tsraudio.tsrq`).

## Novità 0.4.5 (interfaccia)
- Clic sulla linea EQ: la banda (Bell; Low Cut/Low Shelf/High Shelf/High Cut vicino ai bordi) nasce e la trascini nello stesso gesto. Fuori dalla linea: doppio clic crea la banda dove clicchi, poi la trascini.
- Tolti: Freeze dell'analizzatore, Spectrum Grab (vista congelata tenendo il mouse fermo sullo spettro), etichette gialle sui picchi e linea gialla "clic: nuova banda" sullo spettro.
- Spettro più alto: la scala dell'analizzatore finisce a -24 dB invece di +6 dB.
- DSP identico alla 0.4.4.

## Novità 0.4.4 (analisi totale del DSP, test estremi, ottimizzazione)
Ogni difetto qui sotto è stato misurato sulla 0.4.3 congelata e ha un test C++ nuovo (`tests/dsp_tests.cpp`, sezioni 14–21) che **fallisce sulla 0.4.3 (13 FAIL) e passa sulla 0.4.4**.

**Attenzione, cambia il suono delle sessioni con dinamica:** la 0.4.3 leggeva il livello **+2,9 dB (RMS) e +6 dB (Peak)** più alto del vero, quindi interveniva prima e di più di quanto impostato.
La 0.4.4 legge il valore vero: a parità di impostazioni interviene meno. Per riavere l'intervento di prima: soglia −3 dB (RMS) o −6 dB (Peak).

Correttezza
- **NaN/Inf in ingresso**: nella 0.4.3 un solo campione NaN rendeva l'uscita NaN per sempre. Ora diventa 0: uscita identica al bit a quella con 0 al suo posto.
- **Detector calibrato**: RMS vero (finestra di 8 periodi della banda, 10–80 ms) e Peak = picco − 3,01 dB tenuto per un periodo della banda. Sinusoide a −20 dBFS, 100 Hz–5 kHz, 44,1–96 kHz: scarto ≤ 0,03 dB (RMS) e ≤ 0,01 dB (Peak); 0.4.3 +2,98 e +5,99 dB.
  Guadagno dinamico a regime = curva soglia/knee/range sul livello vero: scarto massimo 0,018 dB su 162 casi (0.4.3: 4,44 dB).
- **Detector del Bell limitato alla banda**: la 0.4.3 eseguiva solo la prima di due sezioni, cioè un passa-alto: un tono a 5 kHz comprimeva un Bell dinamico a 200 Hz di tutto il RANGE (−6 dB). Ora passa-banda del 2° ordine con 0 dB al centro: −1,5 dB (resta un fianco di 6 dB/oct, −28 dB a 4,6 ottave).
- **Character oltre 0 dBFS**: Warm ripiegava sotto −1,06 (i picchi negativi tornavano verso zero), Subtle oltre −2,2/+2,5. Curve identiche fino a |x| = 1, poi monotone.
  Nota: con Character attivo l'uscita è 0,6 campioni in anticipo sulla 0.4.3 (nuovi filtri dell'oversampling): residuo grezzo −19 dB, −49,6 dB dopo aver compensato il ritardo.
- **Precisione dei filtri** (errore massimo vs prototipo analogico, 20 Hz–20 kHz): 17.880 progetti, casi > 1 dB **83 → 14**, > 0,5 dB 266 → 22, massimo 8,18 → 2,94 dB, nessun caso peggiorato di oltre 0,05 dB.
  Griglia estesa (19.200 progetti, shelf con Q fino a 40, ±30 dB): > 1 dB **324 → 66**, massimo **31,2 → 2,94 dB**, nessun caso peggiorato.
  Casi vicino a Nyquist (44,1/48 kHz, 10–20 kHz): Bell 4,2 → 0,19 dB, Notch 7,3 → 0,40, High Cut 1,5 → 0,22; High Shelf 16 kHz +18 dB Q 20 a 44,1 kHz: **+19 dB in più a 20 kHz → 0,06 dB**.
  Come: modulo valutato in forma φ (niente cancellazione numerica), fit minimax nella banda udibile con pesi di Lawson, partenza anche da poli nulli, numeratore proiettato sul dominio valido,
  punti attorno a poli e zeri, scelta finale su griglia fitta, cascate scelte sull'errore del totale. Reciprocità ±g: 2,58 → 0,14 dB.

Prestazioni (C++, % di un core, 48 kHz, blocchi 512, mediana di 5): **30 configurazioni su 30 più veloci** (da ×1,13 a ×6,2)

| Scenario | 0.4.3 | 0.4.4 | |
|---|---|---|---|
| 32 bande statiche | 1,16 % | 0,69 % | ×1,69 |
| tagli 96 dB/oct | 1,46 % | 0,66 % | ×2,20 |
| 16 bande dinamiche in movimento | 7,69 % | 1,25 % | ×6,15 |
| Character Warm | 2,31 % | 1,78 % | ×1,29 |
| 32 bande automatizzate a ogni blocco da 128 | 22,0 % | 3,9 % | ×5,6 |

- Progetto **veloce** mentre un parametro si muove e **preciso** 50 ms dopo l'ultimo cambio (al massimo 2 bande per blocco); celle della dinamica in tabella (97 livelli) progettate solo quando servono;
  filtro del detector solo per le bande dinamiche; bande Mid/Side consecutive con una sola conversione; oversampling 8x con 46 passa-tutto per direzione invece di 84.
- Caso peggiore per blocco da 64 campioni (1333 µs): cambio di preset con 32 bande 951 → 786 µs; 32 bande dinamiche su raffiche di rumore 333 → 145 µs; 32 bande automatizzate 718 → 184 µs.
- Costo di progetto preciso più alto (Bell circa 4,7 → 7,6 µs, Band Pass 41,6 → 69 µs medi): per questo il preciso arriva solo a parametri fermi.
- Motore JS (prototipo e pagina di ascolto): dinamica in movimento ×5,9–6,0, Mid/Side ×2,0, Character ×1,2–1,3; bande statiche uguali entro il rumore di misura.
- Interfaccia: il "fantasma" del clic-per-creare riprogettava tutte le bande per ognuno dei 221 punti a ogni movimento del mouse: con 12 bande **99 ms → 2 ms**.

Verifiche sulla 0.4.4: test C++ 53/53, stem 113/113, self-test del plugin 23/23, plugin vero sulla clip 8/8, confronto esterno 15/15, pluginval livello 10 SUCCESS,
interfaccia 170/170 (Playwright), parità JS = C++ 114/114 (prototipo = plugin). Log e script in `docs/test_0.4.4/`.

## Novità 0.4.3 (test sulla clip "ESO – SAFE": click eliminati)
- **Dissolvenza di 15 ms** (coseno rialzato) per tutto ciò che non si può interpolare: banda attivata/disattivata o bypass, cambio di tipo/pendenza/collocazione, dinamica on/off, **solo acceso/spento**, cambio di Character.
  Misurato sulla clip (energia sopra 6 kHz prodotta dal cambio, rispetto alla clip ferma): prima solo on/off −15 dB, cambio tipo −17 dB, pendenza −13 dB, attiva/disattiva −25 dB, Character −15 dB; ora tutti al pavimento (−94 dB).
- **Solo senza salto**: il percorso solo lavora su una copia del segnale e le bande continuano a girare sotto; il filtro del solo è interpolato mentre la banda si muove o si allarga.
- **Character**: al cambio il nuovo modo gira 15 ms a vuoto (i filtri dell'oversampling si caricano: senza, transiente a −62 dB misurato) e poi si dissolve sul vecchio.
- **Bypass e polarità** con rampa a coseno rialzato (prima esponenziale: −60 dB di click sull'inversione di polarità, ora al pavimento). **INPUT, OUTPUT e pan** con smoother a due poli (10 + 10 ms): niente spigolo all'inizio della rampa (−76 → −98 dB).
- Test C++ n. 13: 18 scenari di cambio di stato su un segnale senza contenuto sopra 1,5 kHz; **fallisce sul motore 0.4.2 (−10 dB) e passa sul nuovo (−109 dB)**. Il prototipo nel browser è identico al C++ in tutti i 21 scenari dinamici (differenza ≤ −151 dBFS).
- Self-test del plugin: nuovo passo end-to-end **interfaccia → processore → motore** per il solo che segue la banda (solo su 500 Hz: tono a 2,5 kHz a −15,5 dB; banda spostata a 2,5 kHz: +3,0 dB).
- Test sul **plugin VST3 vero con la clip** (pedalboard): null test bit per bit su 148 s, bypass, plugin = motore C++ campione per campione per ogni tipo di banda (≤ −126 dBFS), 16 cambi di stato dall'host senza click, 32 bande (16 dinamiche) su tutta la clip (2,0 % di un core).
- **Correzione di misura**: pedalboard quantizza i parametri (10 000 Hz diventa 10 017 Hz); il confronto con l'analogico ora usa i valori reali letti dal plugin: High Cut 12 kHz 24 dB/oct a 44,1 kHz = 0,98 dB (prima dichiarato 1,10: 0,12 dB erano del banco di prova).

## Novità 0.4.2
- **Solo**: con il solo attivo, spostando o allargando la banda si sente subito la nuova zona (prima il plugin restava sul filtro del momento in cui si era premuto solo; il prototipo nel browser era già corretto). Test C++ che falliva sul motore vecchio e passa sul nuovo.
- **Fluidità**: spettro inviato dal plugin a 60 fotogrammi/s (prima 30) in formato compatto a 16 bit; waveform con orologio continuo (scorre a ogni fotogramma, mai indietro).
- **Doppio clic sul nodo**: tolto il riquadro FREQ/GAIN dB/Q (richiesta). Restano le manopole e la tastiera.
- **Character con oversampling 8x** (prima 4x). Il test esterno sul plugin vero ha trovato a 4x un'armonica ripiegata a 11,4 kHz a −72 dB (Warm, 15 kHz, 0 dBFS, 44,1 kHz). Ora −142 dB. Costo: 3,2 % di un core per 10 s stereo a 48 kHz.
- **Correzione di un test sbagliato**: il test C++ dell'aliasing raddoppiava i dB (rapporto di potenze elevato al quadrato) e a 44,1 kHz era limitato dalla finestra. La cifra "−143 dB" scritta per la 0.4.0 era falsa: il valore reale a 4x era −71 dB.
- **Limite misurato e dichiarato**: High Cut 12 kHz 24 dB/oct a 44,1 kHz scarta 1,1 dB dall'analogico a 20 kHz (−16,7 invece di −17,8 dB). A 48 kHz 0,37 dB, a 96 kHz 0,14 dB. Il filtro EQ di JUCE nello stesso confronto scarta fino a 5,1 dB.

## Novità 0.4.1
- Niente cornice in alto: il display tocca il bordo della finestra (la barra del Mac fa da bordo). In basso la cornice è sottile come a sinistra (10 px). Finestra 1180 × 542.
- L'isola della banda compare subito al suo posto (prima "volava" dall'angolo in alto a sinistra alla prima comparsa).

## Novità 0.4.0 (implementazione dei gap del master)
**Correttezza (P0)**
- Spettro: discesa in dB/s e media legate al tempo reale trascorso (stesso tempo a 30/60/120 fps, anche dopo una pausa della UI); Freeze accumula i massimi.
- Automazione: un gesto per trascinamento/rotellina (inizio e fine bilanciati, chiusura anche su perdita del focus o chiusura dell'editor); tastiera e campi numerici = modifica singola; i valori in arrivo dall'host non creano gesti.

**Controlli (P1)**
- Doppio clic sul nodo: campi FREQ/GAIN/Q (Tab/Shift+Tab, Enter, Esc; Hz, kHz, note come A4 o D#5 +13; valori fuori intervallo segnalati, mai corretti in silenzio).
- Scorciatoie: Cmd/Ctrl+trascina = Q; Cmd/Ctrl+rotella = guadagno; Option/Alt+rotella = range; Cmd+Option+rotella = guadagno e range inversi; rotella sui tagli = pendenza;
  Cmd+Option+clic = forma; Option+Shift+clic = pendenza; Shift+clic = intervallo; Option+doppio clic = banda dinamica; Cmd/Ctrl+clic su manopola = default; Cmd/Ctrl+C/V.
- Zoom/pan dell'asse delle frequenze trascinando sulla scala (ancorato alla frequenza sotto il puntatore), rotella sulla scala, doppio clic = reset. Scala EQ anche ±3 dB.
- Uscita: OUTPUT −∞…+36 dB, pan L/R o M/S (bilanciamento lineare, centro 0 dB), polarità; pannello USCITA dal menu o dal tasto destro su OUTPUT.
- Split L/R e M/S (suono identico per bande statiche, un passo di undo). Gain-Q interaction (Bell: Q × (1 + |dB|/15), max 40).
- Character con oversampling 4x (halfband IIR polifase, > 120 dB fuori banda). [La cifra "−143 dB" qui riportata era sbagliata: vedi 0.4.2.]
- Analizzatore: 1024…32768 punti (anche nel plugin), range 60/90/120 dB, tilt regolabile, Pre tratteggiato, spettro Sidechain, stato "nessun ingresso".
- Spectrum Grab, tastiera con punti delle bande (clic = intona, trascina = semitoni), Sketch che si ridisegna tornando indietro. A/B nel menu. L'isola non copre più i nodi.

**Nuove funzioni (P2)**
- Waveform reale scorrevole (min/max ogni 64 campioni, Pre/Post o L/R, 0,5–10 s, freeze, zoom, auto-scala dichiarata, cursori con Δt, buchi e salti del trasporto segnati, HOLD/LIVE a trasporto fermo).
- Spettrogramma STFT (palette leggibile, legenda dB, memoria limitata). EQ Match (riferimento da ingresso, file o salvato; dettaglio; anteprima; un undo).
- MIDI Learn (tasto destro su FREQ/GAIN/Q/OUTPUT; mappa salvata nello stato). Il plugin AU diventa di tipo "aumf" (effetto che riceve MIDI).

## Novità 0.3.1
- **Menu tasto destro sul nodo**: in più Inverti guadagno, Copia, Incolla (anche Ctrl/Cmd+C, Ctrl/Cmd+V); pendenze dell'elenco secondo il tipo.
- **Dinamica visibile sul knob GAIN**: anello azzurro = range (pieno = 1,5 dB), bianco = riduzione dal vivo; trascinando l'anello cambia il range.
- **Selezione multipla**: rettangolo sul grafico; nell'isola compare "N BANDE" e le manopole cambiano tutte le bande selezionate.
- **Picchi dello spettro**: etichette gialle sulle risonanze; passando sopra = anteprima del Bell; clic = Bell sul picco, trascina giù = taglio.
- **Band Pass fino a 96 dB/oct** (prima 48); Low/High Cut già fino a 96.
- **Zona tagliata rossa** sotto Low/High Cut (più intensa sulla banda selezionata).
- Corretto: dopo un cambio di frequenza di campionamento poteva restare disegnata la curva di una banda appena cancellata.

## Novità 0.3.0
- **Interfaccia nuova dentro il plugin**: è la stessa del prototipo approvato (display a tutto schermo, meter IN/OUT, knob digitali,
  Dynamic Island con FREQ/GAIN/Q, fumetto sul nodo, solo trascinabile, zona dinamica con simbolo RANGE, triplo clic = banda dinamica).
  Gira in una WebView (WKWebView su Mac) collegata ai parametri del plugin: automazione, preset e stato della DAW restano nei parametri.
- **INPUT** (−24…+24 dB) come parametro automatizzabile.
- **Dinamica indipendente dal blocco della DAW**: stesso risultato al campione con blocchi da 32 a 4096 (prima c'era una differenza fino a −53 dBFS).

## Mac: un comando
```bash
./scripts/build_mac.sh          # build universale, test, firma, auval, pluginval (se installato), installer .pkg + zip in dist/
```
Requisiti: Xcode + Command Line Tools, CMake ≥ 3.22, internet al primo build (scarica JUCE 8.0.4).
Senza account Apple Developer la firma è ad-hoc: funziona sul tuo Mac; per distribuirlo ad altri senza avvisi servono
`DEV_ID_APP`, `DEV_ID_INSTALLER`, `NOTARY_PROFILE` (vedi testa dello script).
In alternativa: carica questa cartella su GitHub, il workflow `.github/workflows/mac.yml` fa tutto su un Mac di GitHub e ti lascia i file da scaricare.

## Test
```bash
cmake --build build --target tsrq_dsp_tests tsrq_stem_tests
./build/tsrq_dsp_tests                       # precisione filtri, dinamica, pendenze, prestazioni
./build/tsrq_stem_tests cartella_stem        # 113 controlli sugli stem reali (file .f32 planari L|R, 48 kHz)
TSRQ_SELFTEST=/tmp/esito.txt "build/TSRQ_artefacts/Release/Standalone/TSR Q"   # test interfaccia ↔ parametri ↔ audio
```
Interfaccia: per aggiornarla dal prototipo HTML → `./scripts/build_web.sh percorso/prototipo.html` e ricompila.

## Build manuale (vecchie istruzioni, sempre valide)

Requisiti: Xcode (con Command Line Tools), CMake ≥ 3.22, connessione internet al primo build (scarica JUCE 8.0.4).
```bash
cd TSRQ
cmake -B build -G Xcode                      # oppure: -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target TSRQ_AU TSRQ_VST3 TSRQ_Standalone
```
Binari universali arm64 + x86_64, macOS ≥ 11. Poi copia:
- `build/TSRQ_artefacts/Release/AU/TSR Q.component` → `~/Library/Audio/Plug-Ins/Components/`
- `build/TSRQ_artefacts/Release/VST3/TSR Q.vst3` → `~/Library/Audio/Plug-Ins/VST3/`

Verifica AU: `auval -v aufx Tsrq Tsra`. Per distribuirlo ad altri serve firma + notarizzazione Apple (Developer ID).

## Test del DSP
```bash
cmake --build build --target tsrq_dsp_tests && ./build/tsrq_dsp_tests
```

## Cosa c'è (v0.1.0)
- 32 bande; 10 tipi: Bell, Low/High Shelf (6/12), Low/High Cut (6–96 dB/oct), Notch, Band Pass, Tilt Shelf, Flat Tilt, All Pass.
- Progetto filtri analog-matched (3 candidati per sezione, vince il minor errore vs analogico). Modalità **Zero Latency** (0 campioni).
- Per banda: bypass, Stereo/L/R/M/S, dinamica (Threshold, Range ±, Attack, Release, Knee, detector RMS/Peak, sidechain esterno).
- Tutti i parametri automatizzabili (APVTS), interpolazione coefficienti per campione.
- Analizzatore FFT (2048/4096/8192), Input/Output, Peak Hold, Freeze, media e decadimento regolabili, Spectrum Grab.
- A/B, Undo/Redo, preset `.tsrq` (~/Music/TSR Q Presets), Auto Gain, Gain Scale, Character (Clean/Subtle/Warm), Output.
- Grafico: doppio clic crea banda, drag freq/gain, rotella Q, Shift fine, Option vincolo asse, Option+clic (dinamica o bypass, da Settings),
  selezione rettangolare e Cmd+clic, trascinamento multiplo, Cmd+A, Canc, Esc annulla il trascinamento, Cmd+Z / Cmd+Shift+Z, menu tasto destro.

## NON ancora implementato
Natural Phase, Linear Phase, Spectral Dynamics, TSR Q INSIGHT, Frequency Collision Analysis, Auto Threshold,
visualizzazione stereo dello spettro, colore banda personalizzabile, oversampling del Character.
