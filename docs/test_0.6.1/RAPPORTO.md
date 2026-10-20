# TSR Q 0.6.1 · lavoro più profondo: due revisioni esterne, correzioni, CPU

**Base:** 0.6.0 (`55150b4`).

Legenda dei tag: [M] = MISURATO, [C] = LETTO NEL CODICE, [NV] = NON VERIFICATO.

## Cosa non è risolto (prima di tutto)

- **384 kHz Maximum:** la chiamata che chiude una partizione costa da 1,10 a 1,36 volte la scadenza di un blocco da 64 campioni [M].
  - Fino a 192 kHz resta sotto 0,62 volte.
  - Per risolverlo serve una convoluzione a partizioni non uniformi.
- **Rendering offline a 192 kHz Maximum con una banda Mid:** 1,51 volte il tempo reale [M]. Nella 0.6.0 era 36,8.
- **Passaggio statica ↔ dinamica in Linear Phase:** resta uno scarto di 0,40 dB durante la dissolvenza a Medium, 0,05 dB a Maximum [M].
  - Causa: per un momento si sommano la versione a fase lineare e quella a fase minima della stessa banda.
  - Nella 0.6.0 lo scarto era di 12 dB.
- **Automazione in tempo reale in Linear Phase:** bande laterali da −79 a −105 dB, secondo i tempi del thread di progetto [M]. Nella 0.6.0 −61,8 dB.
- **Ritardo dell'automazione in Linear Phase a Maximum:** circa 120–150 ms [M]. In Zero Latency è 10,7 ms.
- **Bande dinamiche in Linear Phase:** restano a fase minima [C]. Pro-Q 4 le fa a fase lineare [NV].
- **Progetto Zero Latency con Q estremi (Q 0,025 / 40) vicino a Nyquist:** fino a 3,4 dB di errore [M]. Non è stato toccato.
- **Latenza riportata all'host dopo un cambio di modo durante la riproduzione:** in pedalboard resta 0 [M]. In Ableton [NV].
- **Non verificati:**
  - tutto sul Mac: CPU su Apple Silicon, Ableton, confronto dal vivo con Pro-Q 4 [NV];
  - Pro-Q 4: grandezze delle finestre, latenze e comportamento in Linear Phase [NV].

## Dati da cui sono partito

1. **Prima revisione esterna** della 0.6.0: 13 difetti misurati con sonde. Le sonde sono in `sonde_revisione_esterna/`.
2. **Profilo CPU** (callgrind) di Zero Latency, 8 bande:
   - 44,8 % del tempo nei biquad, limitati dalla catena di dipendenze;
   - 15 % in due coseni per campione calcolati anche a riposo.
3. **Seconda revisione esterna** della 0.6.1 intermedia: 4 difetti nuovi, uno grave (una corsa tra thread trovata da ThreadSanitizer). Sono stati corretti.

## Correzioni e risultati [M]

### Linear Phase

| Difetto | 0.6.0 | 0.6.1 |
|---|---|---|
| Statica ↔ dinamica (seno 1 kHz, Bell ±12 dB) | ±12 dB | ≤ 0,40 dB |
| Copia vecchia che riappare (banda tolta e rimessa; ritorno a Zero Latency) | +11,6 / +10,4 dB | 0,00 / 0,00 dB |
| Richiesta vecchia dopo tempo reale → offline → tempo reale | 0 dB invece di −12 | −12,00 dB |
| Detector, banda statica dopo la dinamica (Zero Latency / Linear Phase) | +6,74 / 0,00 dB | +6,74 / +6,74 dB |
| Detector, due bande dinamiche in serie (Zero Latency / Linear Phase) | — | +5,24 / +5,24 dB |
| All Pass (fase a 1 kHz) | 0° | 179,8° (come in Zero Latency) |
| Picco di calcolo per chiamata, blocco da 64, 192 kHz Maximum con Mid | 8,32 × la scadenza | 0,62 × |
| Low Cut 96 dB/oct che entra, seno 110 Hz (buco durante la dissolvenza) | 1,14 dB | 1,14 dB (la 0.6.1 intermedia: 20,5 dB) |
| Corsa tra thread durante l'automazione (salti di livello in 2 ms) | — | 0,053 dB (la 0.6.1 intermedia: 3,5 dB); ThreadSanitizer: 0 segnalazioni su 3 esecuzioni |
| Eventi (banda on/off, tipo, collocazione, salto): energia sopra 4 kHz | ≤ −114,6 dBFS | ≤ −114,9 dBFS |
| Automazione offline (spurie): Medium / Maximum | −61,8 / −79,7 dB | −105,6 / −96,9 dB |
| Errore sulle bande strette in basso, Bell 40 Hz Q4 a 48 kHz Medium | 2,94 dB | 1,48 dB |
| Risoluzione a 384 kHz (stessa Bell) | 6,07 dB | 1,50 dB (uguale a 48 kHz) |
| Rendering offline, 192 kHz Maximum con Mid | 36,8 × il tempo reale | 1,51 × |

**Curva nel grafico.** In Linear Phase ora mostra la risposta vera del FIR, cioè la stessa finestra e la stessa lunghezza del motore.
- Con 3 bande, Linear Phase Low a 48 kHz, la curva e il motore C++ coincidono: 40 Hz +7,13 dB, 60 Hz −2,92 dB, 2 kHz −5,99 dB.
- Prima la curva mostrava la forma ideale: +12 dB e un notch profondo.
- Immagine: `gui_curva_linear_phase_low.png`.

**Come è stato fatto** [C]:
- ogni banda agisce solo sulla parte del segnale che non la contiene già (componenti c/f del FIR); le code dei biquad si esauriscono prima di fermarli;
- le bande statiche entrano ed escono solo attraverso il FIR;
- il detector percorre la catena delle bande precedenti, dinamiche comprese;
- le partizioni del FIR sono distribuite sui campioni;
- tre slot, contatore di generazione, slot in uso pubblicati prima di liberare quello pronto;
- finestra di Kaiser β 3 con il tap 0 a zero;
- dissolvenza lineare quando segue da vicino la precedente, coseno rialzato se isolata, lunga quanto l'intervallo tra due progetti;
- modulo di ogni banda in cache.

### Zero Latency: CPU

| 8 bande, 48 kHz, blocco da 512 | 0.6.0 | 0.6.1 |
|---|---|---|
| Media | 27,6 µs | 12,0 µs |
| 99° percentile | 51,2 µs | 32,2 µs |
| Con 2 bande dinamiche | 42,4 µs | 26,8 µs |

- **Uscita identica al bit alla 0.6.0** in 20 scenari difficili (4 frequenze di campionamento × 5 scenari) e in tutte le 151 metriche di qualità non CPU.
- **Come** [C]:
  - L e R in un vettore di due double;
  - bande Stereo statiche consecutive in una sola cascata "a fronte d'onda";
  - uscita a regime senza calcoli superflui;
  - fadeW senza coseno a 0 e 1.
- **Mac:** sulla CPU Apple il confronto era TSR Q 34,1 µs contro Pro-Q 4 16,3 µs. Il nuovo valore sul Mac è [NV].

## Prove [M]

| Prova | Esito |
|---|---|
| dsp_tests | tutti PASS (sezioni 31 e 32 nuove, sezione 29 con soglie più strette) |
| stem | 113/113 |
| parità JS = C++ | forme 20/20, casuale 40/40, master 10/10, stem 26/26 |
| ThreadSanitizer | 0 segnalazioni |
| ASan e UBSan | nessun errore |
| pluginval livello 10 | SUCCESS |

**Plugin VST3 vero** (pedalboard):
- impulso Linear Phase simmetrico (≤ 1,1e-16);
- bypass = ingresso, al bit;
- blocchi da 64 e da 4096 identici a quelli da 512.
