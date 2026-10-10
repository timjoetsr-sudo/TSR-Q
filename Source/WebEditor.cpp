#include "WebEditor.h"
#include "BinaryData.h"
using namespace juce;

// autotest: conta inizio/fine dei gesti e i valori di un parametro
struct GestureCounter : AudioProcessorParameter::Listener { int b = 0, e = 0, v = 0; void parameterValueChanged (int, float) override { ++v; } void parameterGestureChanged (int, bool s) override { s ? ++b : ++e; } };

static const char* kBandKeys[] = { "used", "byp", "type", "freq", "gain", "q", "slope", "place", "dyn", "thr", "range", "att", "rel", "knee", "det", "sc" };
// chiave del modello P dell'interfaccia -> chiave del parametro
static const char* kGlobUi[] = { "inp", "out", "scale", "auto", "char", "bypass", "pan", "panMS", "inv", "gq" };
static const char* kGlobId[] = { "in", "out", "scale", "auto", "char", "bypass", "pan", "panmode", "invert", "gq" };
constexpr int kNumGlob = 10;

static std::optional<WebBrowserComponent::Resource> resource (const String& url) {
    const auto path = url.fromFirstOccurrenceOf (WebBrowserComponent::getResourceProviderRoot(), false, false);
    if (path.isEmpty() || path == "/" || path == "index.html" || path == "/index.html") {
        WebBrowserComponent::Resource r; r.mimeType = "text/html; charset=utf-8";
        r.data.resize ((size_t) BinaryData::index_htmlSize);
        std::memcpy (r.data.data(), BinaryData::index_html, (size_t) BinaryData::index_htmlSize);
        return r;
    }
    return std::nullopt;
}

TsrqWebEditor::TsrqWebEditor (TsrqProcessor& p) : AudioProcessorEditor (p), proc (p) {
    setFftOrder (13);
    { const String f = SystemStats::getEnvironmentVariable ("TSRQ_SELFTEST", {}); if (f.isNotEmpty()) { selfTest = std::make_unique<SelfTest>(); selfTest->file = File (f); selfTest->t0 = Time::getMillisecondCounterHiRes(); } }
    auto opts = WebBrowserComponent::Options {}
        .withNativeIntegrationEnabled()
        .withKeepPageLoadedWhenBrowserIsHidden()
        .withResourceProvider (resource)
        .withEventListener ("tsrq_ready", [this] (const var&) { uiReady = true; sendLoad(); if (selfTest) selfTest->t0 = Time::getMillisecondCounterHiRes();
            if (SystemStats::getEnvironmentVariable ("TSRQ_DEMO", {}).isNotEmpty())   // solo per gli screenshot dei test
                Timer::callAfterDelay (800, [this] { web->evaluateJavascript ("graph.createBand(60, 4); graph.createBand(350, -3); graph.createBand(3500, 5, 0, 1); graph.createBand(9000, 0, 4); graph.selectOnly(2); 'ok'"); }); })
        .withEventListener ("tsrq_state", [this] (const var& v) { onUiState (v); })
        .withEventListener ("tsrq_gesture", [this] (const var& v) { if ((bool) v["on"]) gestOpen = true; else endGesture(); })
        .withEventListener ("tsrq_fft", [this] (const var& v) { setFftOrder ((int) v["order"]); })
        .withEventListener ("tsrq_midilearn", [this] (const var& v) { proc.midiLearn (v["id"].toString()); })
        .withEventListener ("tsrq_midiforget", [this] (const var& v) { proc.midiForget (v["id"].toString()); });
   #if JUCE_WINDOWS
    opts = opts.withBackend (WebBrowserComponent::Options::Backend::webview2)
               .withWinWebView2Options (WebBrowserComponent::Options::WinWebView2 {}.withUserDataFolder (File::getSpecialLocation (File::tempDirectory)));
   #endif
    web = std::make_unique<WebBrowserComponent> (opts);
    addAndMakeVisible (*web);
    web->goToURL (WebBrowserComponent::getResourceProviderRoot());
    setResizable (true, true);
    setResizeLimits (708, 372, 2360, 1238);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio (1220.0 / 640.0);
    setSize (1220, 640);
    startTimerHz (30);
}
TsrqWebEditor::~TsrqWebEditor() { stopTimer(); endGesture(); }
void TsrqWebEditor::setFftOrder (int o) {   // thread dei messaggi: nuova risoluzione dell'analizzatore (non tocca l'audio)
    o = jlimit (10, 15, o); fftOrder = o; fftN = 1 << o; fft = std::make_unique<dsp::FFT> (o);
    win.resize ((size_t) fftN); work.assign ((size_t) (2 * fftN), 0.0f);
    for (int i = 0; i < fftN; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * (float) i / (float) (fftN - 1));
}
// chiude una volta sola ogni parametro toccato durante il gesto (coppia inizio/fine bilanciata)
void TsrqWebEditor::endGesture() { for (auto* p : gestParams) { p->endChangeGesture(); ++gestEnds; } gestParams.clear(); gestOpen = false; }

// ---- autotest end-to-end (solo con TSRQ_SELFTEST=<file>): interfaccia ↔ parametri ↔ motore audio ----
void TsrqWebEditor::runSelfTest() {
    auto& st = selfTest; const double t = Time::getMillisecondCounterHiRes() - st->t0;
    auto log = [&st] (bool ok, const String& m) { st->out << (ok ? "PASS " : "FAIL ") << m << "\n"; st->fails += ok ? 0 : 1; };
    auto par = [this] (const String& id) { auto* p = proc.apvts.getParameter (id); return p ? p->convertFrom0to1 (p->getValue()) : -999.0f; };
    auto js = [this, &st] (const String& code, int slot) { web->evaluateJavascript (code, [&st, slot] (WebBrowserComponent::EvaluationResult r) {
        st->res[slot] = r.getResult() != nullptr ? r.getResult()->toString() : String ("ERRORE JS: ") + (r.getError() ? r.getError()->message : String()); }); };
    switch (st->step) {
        case 0: for (auto* p : proc.getParameters()) p->setValueNotifyingHost (p->getDefaultValue());   // stato pulito (lo Standalone ricorda l'ultima sessione)
            for (const auto& id : { "out", "b1_gain", "b1_freq", "b1_q" }) proc.midiForget (id);
            sendLoad(); st->step = 13; st->t0 = Time::getMillisecondCounterHiRes(); break;
        case 13: if (t < 700) break;
            js ("graph.createBand(1000, 6, 0, 1); P.inp = 3; dirty = true; 'ok'", 0); st->step = 1; st->t0 = Time::getMillisecondCounterHiRes(); break;
        case 1: if (t < 1000) break;
            log (st->res[0] == "ok", "UI: creo banda 1 kHz +6 dB con dinamica e INPUT +3 dB (" + st->res[0] + ")");
            log (par ("b1_used") > 0.5f && std::abs (par ("b1_freq") - 1000.0f) < 1.0f && std::abs (par ("b1_gain") - 6.0f) < 0.02f && par ("b1_dyn") > 0.5f,
                 "UI -> parametri: used " + String (par ("b1_used")) + ", freq " + String (par ("b1_freq"), 2) + ", gain " + String (par ("b1_gain"), 2) + ", dyn " + String (par ("b1_dyn")));
            log (std::abs (par ("in") - 3.0f) < 0.02f, "UI -> parametro INPUT: " + String (par ("in"), 2) + " dB");
            if (auto* p = proc.apvts.getParameter ("b1_gain")) p->setValueNotifyingHost (p->convertTo0to1 (-4.5f));     // automazione dalla DAW
            st->step = 2; st->t0 = Time::getMillisecondCounterHiRes(); break;
        case 2: if (t < 1200) break; js ("JSON.stringify({g: P.bands[0].gain, f: P.bands[0].freq, d: P.bands[0].dyn})", 1); st->step = 3; st->t0 = Time::getMillisecondCounterHiRes(); break;
        case 3: { if (t < 600) break; const var r = JSON::parse (st->res[1]);
            log (std::abs ((double) r["g"] + 4.5) < 0.02, "automazione DAW -> interfaccia: gain banda 1 = " + st->res[1]);
            proc.prepareToPlay (48000, 512); AudioBuffer<float> b (2, 512); MidiBuffer mb; double ph = 0; float outPk = 0; bool fin = true;
            for (int k = 0; k < 94; ++k) { for (int i = 0; i < 512; ++i) { const float v = 0.5f * (float) std::sin (ph); ph += 2 * MathConstants<double>::pi * 1000 / 48000; b.setSample (0, i, v); b.setSample (1, i, v); }
                proc.processBlock (b, mb); for (int i = 0; i < 512; ++i) { const float o = b.getSample (0, i); fin = fin && std::isfinite (o); if (k > 60) outPk = jmax (outPk, std::abs (o)); } }
            st->outDb = Decibels::gainToDecibels (outPk);
            log (fin, "motore: 1 s di sinusoide 1 kHz -6 dBFS attraverso il plugin, uscita finita, picco " + String (st->outDb, 2) + " dBFS");
            st->step = 4; st->t0 = Time::getMillisecondCounterHiRes(); break; }
        case 4: if (t < 800) break; js ("JSON.stringify({pkMax: window.__tsrqMax || 0, pinMax: window.__tsrqMaxIn || 0, d: meter.bands && meter.bands[0] ? meter.bands[0].delta : null, spec: graph.specCount || 0})", 2); st->step = 5; st->t0 = Time::getMillisecondCounterHiRes(); break;
        case 5: { if (t < 600) break; const var r = JSON::parse (st->res[2]);
            log (r["spec"].isInt() || r["spec"].isDouble() ? (int) r["spec"] > 0 : false, "spettro plugin -> interfaccia: " + r["spec"].toString() + " fotogrammi ricevuti");
            log ((double) r["pkMax"] > 0.1 && (double) r["pinMax"] > 0.5 && (double) r["d"] < -1.4, "meter plugin -> interfaccia (picco out/in lineare, intervento dinamico dB): " + st->res[2]);
            // atteso: -6 dBFS + 3 (INPUT) - 4.5 (banda) + intervento dinamico (≤ 0, RANGE di default -1.5)
            log (std::abs (st->outDb + 9.0f) < 0.2f, "livello d'uscita esatto: " + String (st->outDb, 2) + " dBFS (atteso -6 +3 INPUT -4.5 banda -1.5 dinamica = -9.0)");
            js ("P.bands[0].byp = 1; dirty = true; 'ok'", 3); st->step = 6; st->t0 = Time::getMillisecondCounterHiRes(); break; }
        case 6: if (t < 800) break; log (par ("b1_byp") > 0.5f, "UI: bypass banda -> parametro b1_byp = " + String (par ("b1_byp")));
            js ("graph.deleteSelected(); 'ok'", 4); st->step = 7; st->t0 = Time::getMillisecondCounterHiRes(); break;
        case 7: if (t < 800) break; log (par ("b1_used") < 0.5f, "UI: elimina banda -> parametro b1_used = " + String (par ("b1_used")));
            js ("graph.createBand(500, 0); flush(); 'ok'", 5); st->step = 8; st->t0 = Time::getMillisecondCounterHiRes(); break;
        case 8: { if (t < 600) break;   // banda creata e gia inviata: ora conto solo i gesti del trascinamento simulato
            static GestureCounter gl; gl.b = gl.e = gl.v = 0; if (auto* p = proc.apvts.getParameter ("b1_gain")) p->addListener (&gl); st->gl = &gl;
            js ("__tsrqGesture(true); for (const v of [1, 2, 3, 4]) { P.bands[0].gain = v; dirty = true; flush(); } P.bands[0].gain = 5; dirty = true; __tsrqGesture(false); 'ok'", 5);
            st->step = 12; st->t0 = Time::getMillisecondCounterHiRes(); break; }
        case 12: { if (t < 800) break; auto* g = static_cast<GestureCounter*> (st->gl); const int gb = g->b, ge = g->e, gv = g->v;
            if (auto* p = proc.apvts.getParameter ("b1_gain")) p->removeListener (g);
            log (gb == 1 && ge == 1 && gv >= 5 && std::abs (par ("b1_gain") - 5.0f) < 0.02f, "gesto di automazione: 5 valori in un trascinamento -> inizio " + String (gb) + ", fine " + String (ge) + ", valori " + String (gv) + ", finale " + String (par ("b1_gain"), 2) + " dB (attesi 1/1/>=5/5)");
            js ("window.__tsrqMidiLearn('out'); graph.fftOrder = 11; window.__tsrqFFT(11); 'ok'", 6); st->step = 9; st->t0 = Time::getMillisecondCounterHiRes(); break; }
        case 9: { if (t < 600) break;
            log (proc.midiLearnTarget() >= 0, "MIDI Learn dall'interfaccia: parametro OUTPUT in ascolto (" + String (proc.midiLearnTarget()) + ")");
            AudioBuffer<float> b (2, 512); b.clear(); MidiBuffer mb; mb.addEvent (MidiMessage::controllerEvent (1, 21, 127), 10); proc.processBlock (b, mb);
            st->step = 10; st->t0 = Time::getMillisecondCounterHiRes(); break; }
        case 10: { if (t < 500) break;
            log (std::abs (par ("out") - 36.0f) < 0.01f && proc.midiLearnTarget() < 0, "CC 21 valore 127 -> OUTPUT = " + String (par ("out"), 2) + " dB (atteso +36, il massimo)");
            MemoryBlock mbk; proc.getStateInformation (mbk); proc.midiForget ("out"); const String before = JSON::toString (proc.midiMapAsVar(), true);
            proc.setStateInformation (mbk.getData(), (int) mbk.getSize()); const String after = JSON::toString (proc.midiMapAsVar(), true);
            log (before.contains ("{}") && after.contains ("\"out\": 21"), "mappa MIDI salvata nello stato e ripristinata: " + after);
            AudioBuffer<float> b (2, 512); b.clear(); MidiBuffer mb; mb.addEvent (MidiMessage::controllerEvent (1, 21, 0), 0); proc.processBlock (b, mb);
            js ("JSON.stringify({midi: window.__tsrqMidi, wave: window.__tsrqWaveCount || 0, head: window.__tsrq.wave.state().head, specN: graph._specPost() ? graph._specPost().length : 0})", 7);
            st->step = 11; st->t0 = Time::getMillisecondCounterHiRes(); break; }
        case 11: { if (t < 700) break; const var r = JSON::parse (st->res[7]);
            log (std::abs (par ("out") + 60.0f) < 0.01f, "CC 21 valore 0 -> OUTPUT = " + String (par ("out"), 2) + " dB (atteso -60 = -inf)");
            log ((int) r["wave"] > 5 && (int) r["head"] > 100, "waveform plugin -> interfaccia: " + r["wave"].toString() + " pacchetti, " + r["head"].toString() + " gruppi da 64 campioni");
            log ((int) r["specN"] == 1024, "risoluzione analizzatore scelta nell'interfaccia (2048 punti) usata dal plugin: " + r["specN"].toString() + " bin");
            log (r["midi"]["map"]["out"].toString() == "21", "interfaccia informata della mappa MIDI: " + st->res[7].substring (0, 60));
            st->out << "RISULTATO " << (st->fails ? "FAIL" : "PASS") << "\n"; st->file.replaceWithText (st->out); st->step = 99;
            MessageManager::callAsync ([] { if (auto* app = JUCEApplicationBase::getInstance()) app->systemRequestedQuit(); }); break; }
        default: break;
    }
}
void TsrqWebEditor::resized() { if (web) web->setBounds (getLocalBounds()); }

var TsrqWebEditor::stateFromParams() const {
    auto val = [this] (const String& id) { auto* p = proc.apvts.getParameter (id); return p ? p->convertFrom0to1 (p->getValue()) : 0.0f; };
    DynamicObject::Ptr P = new DynamicObject(); Array<var> bands;
    for (int i = 0; i < tsrq::kMaxBands; ++i) {
        DynamicObject::Ptr b = new DynamicObject();
        for (auto k : kBandKeys) {
            const float v = val (ids::b (i, k)); const String key (k);
            if (key == "freq") b->setProperty ("freq", v);
            else if (key == "used" || key == "byp" || key == "dyn" || key == "sc") b->setProperty (key, v > 0.5f ? 1 : 0);
            else if (key == "type" || key == "slope" || key == "place" || key == "det") b->setProperty (key, (int) std::lround (v));
            else b->setProperty (key, v);
        }
        bands.add (var (b.get()));
    }
    P->setProperty ("bands", bands);
    for (int g = 0; g < kNumGlob; ++g) { const float v = val (kGlobId[g]); const String k (kGlobUi[g]);
        if (k == "auto" || k == "bypass" || k == "panMS" || k == "inv" || k == "gq") P->setProperty (k, v > 0.5f ? 1 : 0); else if (k == "char") P->setProperty (k, (int) std::lround (v)); else P->setProperty (k, v); }
    return var (P.get());
}

void TsrqWebEditor::sendLoad() {
    DynamicObject::Ptr o = new DynamicObject(); const var P = stateFromParams();
    o->setProperty ("P", P); o->setProperty ("fs", proc.currentSampleRate());
    lastKnown = JSON::toString (P, true);
    web->emitEventIfBrowserIsVisible ("tsrq_load", var (o.get()));
}

void TsrqWebEditor::onUiState (const var& v) {
    const var P = v["P"]; if (! P.isObject()) return;
    auto setP = [this] (const String& id, float plain) {
        if (auto* p = proc.apvts.getParameter (id)) {
            const float n = p->convertTo0to1 (plain);
            if (std::abs (n - p->getValue()) > 1.0e-6f) {
                if (gestOpen) { if (! gestParams.contains (p)) { p->beginChangeGesture(); ++gestBegins; gestParams.add (p); } p->setValueNotifyingHost (n); }   // dentro il gesto: solo valori
                else { p->beginChangeGesture(); p->setValueNotifyingHost (n); p->endChangeGesture(); }                                                      // tastiera/campo: modifica singola
            }
        } };
    if (auto* bands = P["bands"].getArray())
        for (int i = 0; i < jmin (tsrq::kMaxBands, bands->size()); ++i) {
            const var b = (*bands)[i];
            for (auto k : kBandKeys) { const var x = b[k]; if (! x.isVoid()) setP (ids::b (i, k), (float) (double) x); }
        }
    for (int g = 0; g < kNumGlob; ++g) { const var x = P[kGlobUi[g]]; if (! x.isVoid()) setP (kGlobId[g], (float) (double) x); }
    const var so = v["solo"]; proc.solo = so.isVoid() ? -1 : (int) so;
    lastKnown = JSON::toString (stateFromParams(), true);
    lastUi = Time::getMillisecondCounterHiRes();
}

void TsrqWebEditor::sendSpectrum() {
    float tmp[4096];
    for (int pass = 0; pass < 3; ++pass) {   // anelli da 32768 campioni (pre e post scorrono insieme; sidechain a parte)
        auto& fifo = pass == 0 ? proc.scopePre : pass == 1 ? proc.scopePost : proc.scopeSC; auto& ring = pass == 0 ? ringPre : pass == 1 ? ringPost : ringSC;
        int pos = pass == 2 ? ringPosSC : ringPos, got;
        while ((got = fifo.pull (tmp, 4096)) > 0) for (int i = 0; i < got; ++i) { ring[(size_t) pos] = tmp[i]; pos = (pos + 1) % kRing; }
        if (pass == 1) ringPos = pos; if (pass == 2) ringPosSC = pos;
    }
    const bool scOn = Time::getMillisecondCounterHiRes() - proc.scLastMs.load() < 500;   // sidechain presente da poco: altrimenti l'interfaccia mostra "assente"
    DynamicObject::Ptr o = new DynamicObject(); o->setProperty ("n", fftN);
    for (int pass = 0; pass < (scOn ? 3 : 2); ++pass) {
        const auto& ring = pass == 0 ? ringPre : pass == 1 ? ringPost : ringSC; const int p0 = pass == 2 ? ringPosSC : ringPos;
        for (int i = 0; i < fftN; ++i) work[(size_t) i] = ring[(size_t) ((p0 - fftN + i + kRing) % kRing)] * win[(size_t) i];
        std::fill (work.begin() + fftN, work.end(), 0.0f);
        fft->performFrequencyOnlyForwardTransform (work.data(), true);
        MemoryBlock mb ((size_t) fftN / 2 * sizeof (float)); auto* d = static_cast<float*> (mb.getData());
        for (int k = 0; k < fftN / 2; ++k) d[k] = 20.0f * std::log10 (jmax (work[(size_t) k] * 4.0f / (float) fftN, 1.0e-10f));
        o->setProperty (pass == 0 ? "pre" : pass == 1 ? "post" : "sc", Base64::toBase64 (mb.getData(), mb.getSize()));   // base64 standard (atob nel browser)
    }
    web->emitEventIfBrowserIsVisible ("tsrq_spec", var (o.get()));
}
void TsrqWebEditor::sendWave() {   // inviluppo min/max (8 valori per gruppo di kWaveB campioni) + stato del trasporto
    static float tmp[1 << 16]; const int got = proc.wave.pull (tmp, 1 << 16);
    DynamicObject::Ptr o = new DynamicObject(); o->setProperty ("B", kWaveB); o->setProperty ("fs", proc.currentSampleRate()); o->setProperty ("n", got / 8);
    o->setProperty ("drop", proc.wave.dropped.exchange (0)); o->setProperty ("jumps", proc.transportJumps.load()); o->setProperty ("playing", proc.hostPlaying.load());
    if (got > 0) o->setProperty ("w", Base64::toBase64 (tmp, (size_t) got * sizeof (float)));
    web->emitEventIfBrowserIsVisible ("tsrq_wave", var (o.get()));
}

void TsrqWebEditor::timerCallback() {
    if (! uiReady) return;
    ++tick;
    if (selfTest) runSelfTest();
    // meter: picchi IN/OUT e dinamica delle bande
    DynamicObject::Ptr m = new DynamicObject(); Array<var> pin, pk;
    for (int c = 0; c < 2; ++c) { pin.add (proc.pkIn[c].exchange (0.0f)); pk.add (proc.pkOut[c].exchange (0.0f)); }
    m->setProperty ("pin", pin); m->setProperty ("peak", pk); m->setProperty ("fs", proc.currentSampleRate());
    DynamicObject::Ptr bands = new DynamicObject();
    for (int i = 0; i < tsrq::kMaxBands; ++i) { DynamicObject::Ptr b = new DynamicObject();
        b->setProperty ("lvl", proc.engine.meterLevel[i].load()); b->setProperty ("delta", proc.engine.meterDelta[i].load()); bands->setProperty (String (i), var (b.get())); }
    m->setProperty ("bands", var (bands.get()));
    web->emitEventIfBrowserIsVisible ("tsrq_meter", var (m.get()));
    if (tick % 2 == 0) sendSpectrum();
    sendWave();
    if (proc.midiMapVersion.load() != lastMidiVer) { lastMidiVer = proc.midiMapVersion.load(); web->emitEventIfBrowserIsVisible ("tsrq_midi", proc.midiMapAsVar()); }
    // automazione / preset della DAW -> interfaccia (non mentre l'utente sta muovendo qualcosa)
    if (tick % 3 == 0 && Time::getMillisecondCounterHiRes() - lastUi > 300) {
        const String now = JSON::toString (stateFromParams(), true);
        if (now != lastKnown) sendLoad();
    }
}
