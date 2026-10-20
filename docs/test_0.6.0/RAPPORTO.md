# TSR Q 0.6.0 · forme uguali a FabFilter Pro-Q 4

**Base:** 0.5.0 (`7cee633`).

Legenda dei tag: [M] = MISURATO, [C] = LETTO NEL CODICE, [NV] = NON VERIFICATO.

## Cosa non è ancora uguale (prima di tutto)

- **Band Pass:** non è uguale. Scarto da Pro-Q 4 fino a 51 dB [M]. La forma di Pro-Q non è stata ricostruita: con 24 dB/oct la pendenza lontana è 6 dB/oct per lato e vicino al centro è più ripida, una struttura che non ho identificato.
- **Tagli a 6 dB/oct:** scarto 0,47 dB (Low Cut) e 0,18 dB (High Cut) [M]. La causa non è stata trovata.
- **Forme mai misurate su Pro-Q, lasciate come nella 0.5.0** [NV]:
  - tagli a 30 dB/oct (senza Q);
  - Tilt a 6 dB/oct;
  - All Pass.
- **Costanti misurate in un solo punto.** Tutte le costanti vengono da filtri a 1 kHz, misurati a 192 kHz. Non so se lo zero dei tagli di Pro-Q dipenda dalla frequenza o dalla frequenza di campionamento [NV].
  - Shelf e Tilt sono stati misurati con Q da 0,3 a 10; fuori da questo intervallo la legge è estrapolata [NV].
- **Le sessioni salvate con la 0.5.0 suonano diverse.** Non c'è una conversione automatica [C]. Con gli stessi numeri:
  - Bell e Notch sono più larghi (Q effettivo ÷ √2);
  - gli shelf con Q 1 hanno una curva più morbida;
  - il Tilt va da −g a +g (prima da −g/2 a +g/2);
  - il Flat Tilt ha pendenza doppia;
  - i tagli con Q diverso da 1 adesso cambiano (prima il Q era ignorato).
- **Le funzioni di Pro-Q che TSR Q non ha non sono state toccate:** dinamica spettrale, Natural/Linear Phase, pendenze continue, Character.
  - La CPU resta circa il doppio di quella di Pro-Q 4 (misura del 20/10: 34,1 contro 16,3 µs).
- **Il Mac non era raggiungibile in questa sessione.** Il confronto dal vivo con Pro-Q 4 dopo la modifica è [NV]. Il confronto qui sotto usa le curve di Pro-Q 4 registrate il 20/10 (`proq4_192k.npz`).

## Come sono state ricavate le forme

Le curve sono 197 risposte all'impulso di Pro-Q 4, registrate sul Mac di Tim con pedalboard, a 192 kHz, f = 1 kHz, 160 frequenze. Su queste ho adattato un modello analogico (`modello_proq4.py`):

| Forma | Definizione Pro-Q 4 trovata | Scarto del modello |
|---|---|---|
| Bell | standard, Q effettivo = Q / √2 | 0,043 dB |
| Notch | Q effettivo = Q / √2 | 0,006 dB |
| Low/High Shelf 12 dB/oct | Q effettivo = 0,70335 · Q^0,46594 (Q 0,3–10) | 0,028 dB |
| Shelf 6 dB/oct | già uguale nella 0.5.0 | 0,005 dB |
| Tilt Shelf | high shelf con guadagno 2g abbassato di g (da −g a +g), stessa legge di Q degli shelf | 0,028 dB |
| Flat Tilt | pendenza 2g / log2(1000) dB/oct | 0,171 dB |
| Tagli 12–96 dB/oct | vedi sotto | 0,025–0,111 dB |

Definizione dei tagli da 12 a 96 dB/oct:
- Butterworth di ordine pendenza/6.
- Il Q dell'utente moltiplica il Q della sola sezione più risonante; con Q 1 il filtro è Butterworth.
- Il Q di quella sezione non supera 28,28.
- Pro-Q aggiunge uno zero reale a = 0,0871/min(Q; 2,78) a 12 dB/oct, 0,0779 a 18, 0,0843 a 24 e 0,0838 a 36 dB/oct. Da 48 dB/oct in su non c'è nessuno zero.

Lo scarto residuo è quasi tutto quantizzazione dei dati (0,01 dB). Fa eccezione il Flat Tilt: lì la curva di Pro-Q ha ondulazioni di ±0,08 dB.

## Risultato: modulo digitale di TSR Q a 192 kHz contro Pro-Q 4 [M]

Valore riportato: scarto massimo sopra −40 dB e sotto 15 kHz, su tutti i casi di ogni forma. Sonda: `sonda_192k.cpp`.

| Forma | 0.5.0 | 0.6.0 |
|---|---|---|
| Bell | 2,831 dB | 0,053 dB |
| Low/High Shelf 12 | 13,442 dB | 0,028 dB |
| Shelf 6 | 0,005 dB | 0,005 dB |
| Low Cut 12 / 18 / 24 / 36 | 19,68 / 19,58 / 19,32 / 18,63 dB | 0,111 / 0,030 / 0,036 / 0,035 dB |
| Low Cut 48 / 72 / 96 | 17,81 / 14,85 / 12,41 dB | 0,031 / 0,025 / 0,027 dB |
| High Cut 12 / 18 / 24 / 36 | 19,68 / 19,59 / 19,31 / 18,63 dB | 0,110 / 0,031 / 0,035 / 0,035 dB |
| High Cut 48 / 72 / 96 | 17,81 / 14,85 / 12,40 dB | 0,029 / 0,031 / 0,031 dB |
| Notch | 3,012 dB | 0,006 dB |
| Tilt Shelf | 9,745 dB | 0,028 dB |
| Flat Tilt | 7,819 dB | 0,172 dB |
| Low Cut 6 / High Cut 6 | 0,470 / 0,178 dB | 0,470 / 0,178 dB (invariato) |
| Band Pass 6 / 12 / 24 / 48 | 37,0 / 42,2 / 22,5 / 51,1 dB | invariato (non uguale) |

## Seconda modifica: correzione vicino a Nyquist con tre partenze

Le nuove forme (tagli risonanti, Tilt di secondo ordine) mettono più alla prova il progetto vicino a Nyquist.

**Cosa cambia [C]:**
- La sezione di correzione della 0.4.8 ora rifitta la cascata partendo da tre punti diversi: la scelta normale, tutte le sezioni dal matched-z, tutte dal 3 punti. Tiene la migliore.
- La regola per accettare la correzione non cambia.

**Effetto, 4692 progetti a 44,1 e 48 kHz con f0 da 8 a 20 kHz, scarto massimo dall'analogico sopra −30 dB [M]:**

| Tipo | Una partenza | Tre partenze |
|---|---|---|
| Low/High Shelf | 2,371 dB | 0,792 dB |
| Low Cut | 2,623 dB | 1,035 dB |
| High Cut | 1,391 dB | 0,432 dB |
| Tilt | 1,044 dB | 1,044 dB |

- Tempo di progetto: da 0,072 a 0,079 ms (+10 %).
- Il costo c'è solo nei progetti precisi (banda ferma) sopra fs/10.

**Ho provato e annullato** un candidato "passa-alto con zero" per il Low Cut 12 dB/oct a 20 kHz: nessun miglioramento misurato (0,117 dB prima e dopo).

## Prove [M]

| Prova | Esito |
|---|---|
| dsp_tests | 75 PASS, 0 FAIL |
| stem | 113/113 |
| pluginval livello 10, VST3 Linux | SUCCESS |
| parità JS = C++, forme nuove | 20/20 |
| parità JS = C++, casuale | 40/40 (peggiore −163 dBFS) |
| parità JS = C++, master | 10/10 |
| parità JS = C++, stem | 26/26 |

Modifiche ai test `dsp_tests`:
- **Sezione 7.** Prima confrontava i tagli con Q 0,707 con Butterworth. Ora confronta il digitale con il prototipo analogico (Q 1) e verifica che 6, 30, 48, 72 e 96 dB/oct con Q 1 siano Butterworth esatti.
- **Sezione 22.** Shelf con Q analogico 0,5 come prima; il Low Cut 20 kHz ora con Q 1 (il valore predefinito).
- **Sezione 28, nuova.** 10 forme a 19 frequenze contro i valori misurati di Pro-Q 4: scarto massimo 0,121 dB.

Metriche di `quality.csv`:
- 101 sono identiche alla 0.5.0 e 50 sono cambiate (zipper, THD+N, gradino dinamico, null della EQ attiva, estremi).
- Le metriche cambiate dipendono solo dalle nuove forme: con le forme della 0.5.0 inserite nel motore 0.6.0, tutte le 117 metriche controllate sono identiche al bit alla 0.5.0.
- Esempio: zipper Bell ±12 dB, blocco 512, da −115,5 a −113,2 dBFS. Il filtro misurato è diverso (Bell più largo), non è peggiorato il motore.

## Difetti noti (prossimo lavoro)

1. **High Shelf 14,8 kHz +18 dB a 48 kHz.** Con Q analogico 0,505–0,53 lo scarto resta fino a 0,24 dB: il rifit si ferma in un minimo locale, anche con tre partenze [M]. C'era già nella 0.5.0 (0,222 dB con Q 0,51).
   - Con le nuove forme corrisponde a un Q utente di circa 0,49–0,55.
2. **Low Cut 12 dB/oct a 20 kHz, 44,1 kHz, Q 0,71:** 0,117 dB [M].
3. **Band Pass e tagli a 6 dB/oct di Pro-Q:** da ricostruire, con nuove misure sul Mac.
4. **Misure mancanti su Pro-Q 4:** 30 dB/oct, Tilt a 6 dB/oct, All Pass, Q fuori 0,3–10, tagli con f diversa da 1 kHz e con fs di 44,1 e 48 kHz.

## Interfaccia senza bordi laterali (`a5b7b5d`) [M]

- **Display:** va da bordo a bordo della finestra.
- **Meter OUT:** striscia di 26 px dentro il display, a destra.
- **OUTPUT:** nella barra in basso.
- **Finestra:** sempre 1180 × 542. Le grandezze di Pro-Q 4 sono [NV]: il Mac non ha risposto.
- **Dato letto sul Mac:** nelle preferenze di Pro-Q 4 risulta `InterfaceWidth = 760`, `InterfaceHeight = 335`, `InterfaceLayout = 2`. Non so a quale grandezza corrispondano, né se siano in punti o in pixel [NV].

## Linear Phase [M]

**Cosa fa** [C]
- Nuovi parametri:
  - **Processing Mode:** Zero Latency / Linear Phase;
  - **Linear Phase Resolution:** Low, Medium, High, Very High, Maximum.
- **Bande statiche:** diventano un unico FIR a fase lineare, che segue il modulo **analogico** di ogni banda (nessun cramping).
  - Stereo, Left, Right, Mid e Side sono gestite come una matrice 2×2 per frequenza.
  - Il costo è di due prodotti per frequenza con matrice piena, uno solo se tutte le bande sono Stereo.
- **FIR:**
  - finestra di Kaiser β 9;
  - convoluzione partizionata uniforme;
  - nuovo progetto dissolto sul vecchio in 2 partizioni.
- **Progetti:**
  - in tempo reale, in un thread dedicato (consegna con triplo buffer, senza attese nel thread audio);
  - nel rendering offline (`isNonRealtime`), nel thread audio.
- **Bande dinamiche:** restano biquad (fase minima) anche in Linear Phase [C]. Pro-Q 4 le fa a fase lineare: **non è uguale**.
- **Cosa viene ritardato della stessa latenza:** il bypass (segnale dry), il solo, il sidechain esterno e la waveform "prima".

| Risoluzione | FIR a 44,1/48 kHz | Latenza a 48 kHz | Scarto dal modulo analogico (test 29) |
|---|---|---|---|
| Low | 4096 | 2304 | 1,135 dB (a 40 Hz) |
| Medium | 8192 | 4352 | 0,308 dB |
| High | 16384 | 8448 | 0,083 dB |
| Very High | 32768 | 16896 | 0,021 dB |
| Maximum | 65536 | 33792 | 0,005 dB |

- A 88,2/96 kHz il FIR è il doppio e a 176,4/192 kHz il quadruplo, così la risoluzione in Hz resta la stessa.
- Le latenze di Pro-Q 4 non sono state misurate [NV]. Quindi le latenze di TSR Q **non** sono state allineate a quelle di Pro-Q.

**Plugin VST3 vero** (pedalboard, che compensa la latenza riportata):
- risposta all'impulso simmetrica (asimmetria ≤ 3,3e-10);
- bypass = ingresso, al bit;
- blocchi da 64 e da 4096 identici a quelli da 512;
- differenza dal modo Zero Latency, 60 Hz–18 kHz: 0,291 dB (Low), 0,052 (Medium), 0,011 (High), 0,006 (Very High e Maximum).

**CPU a 48 kHz, 8 bande, blocco da 512** (`cpu_linear_phase.cpp`):

| Modo | Tutte Stereo | Con una banda Mid |
|---|---|---|
| Zero Latency | 28 µs | 29 µs |
| Linear Phase Low | 68 µs | 88 µs |
| Linear Phase Medium | 91 µs | 142 µs |
| Linear Phase High | 159 µs | 267 µs |
| Linear Phase Maximum | 158 µs | 261 µs |

- Un progetto costa da 0,6 ms (Low) a 14,7 ms (Maximum con una banda Mid), nel thread dedicato.

**Prove:**

| Prova | Esito |
|---|---|
| dsp_tests | tutti PASS (sezioni 29 e 30 nuove) |
| stem | 113/113 |
| parità JS = C++ (zero latency) | forme 20/20, casuale 40/40, master 10/10, stem 26/26 |
| ThreadSanitizer, 2000 blocchi con cambi continui e cambio di risoluzione | nessuna segnalazione |
| pluginval livello 10 | SUCCESS |

- La sezione 30 verifica il thread dedicato: dopo la consegna l'uscita è uguale al bit a quella con i progetti nel thread audio.
- Su circa 12 esecuzioni di pluginval in questa sessione, 2 hanno dato segmentation fault **dopo** SUCCESS, alla chiusura. Una era già con `a5b7b5d`, prima del Linear Phase. Altre 3 esecuzioni sotto gdb non l'hanno riprodotto. La causa è [NV].
- Le metriche Zero Latency del CSV di qualità sono identiche alla 0.6.0. Il confronto A/B della CPU, eseguito tre volte, dà valori uguali entro il rumore.

**Non verificato:**
- Linear Phase nel prototipo HTML (il motore JS non lo fa: il selettore cambia solo il parametro).
- Curva disegnata in Linear Phase: resta quella del biquad, quasi uguale all'analogico sotto Nyquist.
- Comportamento dentro Ableton.
- Latenze e CPU di Pro-Q 4 in Linear Phase.
