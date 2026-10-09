#include "WebEditor.h"
#include "BinaryData.h"
using namespace juce;

static const char* kBandKeys[] = { "used", "byp", "type", "freq", "gain", "q", "slope", "place", "dyn", "thr", "range", "att", "rel", "knee", "det", "sc" };
// chiave del modello P dell'interfaccia -> chiave del parametro
static const char* kGlobUi[] = { "inp", "out", "scale", "auto", "char", "bypass" };
static const char* kGlobId[] = { "in", "out", "scale", "auto", "char", "bypass" };

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
    win.resize (fftN); work.resize (2 * fftN);
    { const String f = SystemStats::getEnvironmentVariable ("TSRQ_SELFTEST", {}); if (f.isNotEmpty()) { selfTest = std::make_unique<SelfTest>(); selfTest->file = File (f); selfTest->t0 = Time::getMillisecondCounterHiRes(); } }
    for (int i = 0; i < fftN; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * (float) i / (float) (fftN - 1));
    auto opts = WebBrowserComponent::Options {}
        .withNativeIntegrationEnabled()
        .withKeepPageLoadedWhenBrowserIsHidden()
        .withResourceProvider (resource)
        .withEventListener ("tsrq_ready", [this] (const var&) { uiReady = true; sendLoad(); if (selfTest) selfTest->t0 = Time::getMillisecondCounterHiRes();
            if (SystemStats::getEnvironmentVariable ("TSRQ_DEMO", {}).isNotEmpty())   // solo per gli screenshot dei test
                Timer::callAfterDelay (800, [this] { web->evaluateJavascript ("graph.createBand(60, 4); graph.createBand(350, -3); graph.createBand(3500, 5, 0, 1); graph.createBand(9000, 0, 4); graph.selectOnly(2); 'ok'"); }); })
        .withEventListener ("tsrq_state", [this] (const var& v) { onUiState (v); });
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
TsrqWebEditor::~TsrqWebEditor() { stopTimer(); }

// ---- autotest end-to-end (solo con TSRQ_SELFTEST=<file>): interfaccia ↔ parametri ↔ motore audio ----
void TsrqWebEditor::runSelfTest() {
    auto& st = selfTest; const double t = Time::getMillisecondCounterHiRes() - st->t0;
    auto log = [&st] (bool ok, const String& m) { st->out << (ok ? "PASS " : "FAIL ") << m << "\n"; st->fails += ok ? 0 : 1; };
    auto par = [this] (const String& id) { auto* p = proc.apvts.getParameter (id); return p ? p->convertFrom0to1 (p->getValue()) : -999.0f; };
    auto js = [this, &st] (const String& code, int slot) { web->evaluateJavascript (code, [&st, slot] (WebBrowserComponent::EvaluationResult r) {
        st->res[slot] = r.getResult() != nullptr ? r.getResult()->toString() : String ("ERRORE JS: ") + (r.getError() ? r.getError()->message : String()); }); };
    switch (st->step) {
        case 0: js ("graph.createBand(1000, 6, 0, 1); P.inp = 3; dirty = true; 'ok'", 0); st->step = 1; st->t0 = Time::getMillisecondCounterHiRes(); break;
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
            st->out << "RISULTATO " << (st->fails ? "FAIL" : "PASS") << "\n"; st->file.replaceWithText (st->out); st->step = 99;
            MessageManager::callAsync ([] { if (auto* app = JUCEApplicationBase::getInstance()) app->systemRequestedQuit(); }); break;
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
    for (int g = 0; g < 6; ++g) { const float v = val (kGlobId[g]); const String k (kGlobUi[g]);
        if (k == "auto" || k == "bypass") P->setProperty (k, v > 0.5f ? 1 : 0); else if (k == "char") P->setProperty (k, (int) std::lround (v)); else P->setProperty (k, v); }
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
            if (std::abs (n - p->getValue()) > 1.0e-6f) { p->beginChangeGesture(); p->setValueNotifyingHost (n); p->endChangeGesture(); }
        } };
    if (auto* bands = P["bands"].getArray())
        for (int i = 0; i < jmin (tsrq::kMaxBands, bands->size()); ++i) {
            const var b = (*bands)[i];
            for (auto k : kBandKeys) { const var x = b[k]; if (! x.isVoid()) setP (ids::b (i, k), (float) (double) x); }
        }
    for (int g = 0; g < 6; ++g) { const var x = P[kGlobUi[g]]; if (! x.isVoid()) setP (kGlobId[g], (float) (double) x); }
    const var so = v["solo"]; proc.solo = so.isVoid() ? -1 : (int) so;
    lastKnown = JSON::toString (stateFromParams(), true);
    lastUi = Time::getMillisecondCounterHiRes();
}

void TsrqWebEditor::sendSpectrum() {
    float tmp[4096];
    for (int pass = 0; pass < 2; ++pass) {   // tiene gli ultimi fftN campioni in un anello (pre e post scorrono insieme)
        auto& fifo = pass == 0 ? proc.scopePre : proc.scopePost; auto& ring = pass == 0 ? ringPre : ringPost; int pos = ringPos, got;
        while ((got = fifo.pull (tmp, 4096)) > 0) for (int i = 0; i < got; ++i) { ring[(size_t) pos] = tmp[i]; pos = (pos + 1) % fftN; }
        if (pass == 1) ringPos = pos;
    }
    DynamicObject::Ptr o = new DynamicObject(); o->setProperty ("n", fftN);
    for (int pass = 0; pass < 2; ++pass) {
        const auto& ring = pass == 0 ? ringPre : ringPost;
        for (int i = 0; i < fftN; ++i) work[(size_t) i] = ring[(size_t) ((ringPos + i) % fftN)] * win[(size_t) i];
        std::fill (work.begin() + fftN, work.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform (work.data(), true);
        MemoryBlock mb ((size_t) fftN / 2 * sizeof (float)); auto* d = static_cast<float*> (mb.getData());
        for (int k = 0; k < fftN / 2; ++k) d[k] = 20.0f * std::log10 (jmax (work[(size_t) k] * 4.0f / (float) fftN, 1.0e-9f));
        o->setProperty (pass == 0 ? "pre" : "post", Base64::toBase64 (mb.getData(), mb.getSize()));   // base64 standard (atob nel browser)
    }
    web->emitEventIfBrowserIsVisible ("tsrq_spec", var (o.get()));
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
    // automazione / preset della DAW -> interfaccia (non mentre l'utente sta muovendo qualcosa)
    if (tick % 3 == 0 && Time::getMillisecondCounterHiRes() - lastUi > 300) {
        const String now = JSON::toString (stateFromParams(), true);
        if (now != lastKnown) sendLoad();
    }
}
