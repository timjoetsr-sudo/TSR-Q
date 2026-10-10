# TSR Q — Precision Dynamic EQ · TSR Audio · v0.4.2

Plugin JUCE 8 (C++17). Formati: **AU + VST3 + Standalone su macOS** (universale Apple Silicon + Intel, macOS 11+), VST3 + Standalone su Linux/Windows.
Produttore: **TSR Audio** (codice produttore `Tsra`, codice plugin `Tsrq`, bundle `com.tsraudio.tsrq`).

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
