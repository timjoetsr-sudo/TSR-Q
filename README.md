# TSR Q — Precision Dynamic EQ · TSR Audio · v0.3.1

Plugin JUCE 8 (C++17). Formati: **AU + VST3 + Standalone su macOS** (universale Apple Silicon + Intel, macOS 11+), VST3 + Standalone su Linux/Windows.
Produttore: **TSR Audio** (codice produttore `Tsra`, codice plugin `Tsrq`, bundle `com.tsraudio.tsrq`).

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
