#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "WebEditor.h"

using namespace juce;

static NormalisableRange<float> logRange (float lo, float hi) {
    return NormalisableRange<float> (lo, hi,
        [] (float a, float b, float n) { return a * std::pow (b / a, n); },
        [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); },
        [] (float a, float b, float v) { return jlimit (a, b, v); });
}

AudioProcessorValueTreeState::ParameterLayout TsrqProcessor::createLayout() {
    AudioProcessorValueTreeState::ParameterLayout L;
    const StringArray types { "Bell", "Low Shelf", "High Shelf", "Low Cut", "High Cut", "Notch", "Band Pass", "Tilt Shelf", "Flat Tilt", "All Pass" };
    StringArray slopes; for (int s : kSlopes) slopes.add (String (s) + " dB/oct");
    for (int i = 0; i < tsrq::kMaxBands; ++i) {
        const String n = "Band " + String (i + 1) + " ";
        auto grp = std::make_unique<AudioProcessorParameterGroup> ("band" + String (i + 1), n.trim(), "|");
        auto id = [i] (const char* k) { return ParameterID { ids::b (i, k), 1 }; };
        grp->addChild (std::make_unique<AudioParameterBool> (id ("used"), n + "Used", false));
        grp->addChild (std::make_unique<AudioParameterBool> (id ("byp"), n + "Bypass", false));
        grp->addChild (std::make_unique<AudioParameterChoice> (id ("type"), n + "Type", types, 0));
        grp->addChild (std::make_unique<AudioParameterFloat> (id ("freq"), n + "Freq", logRange (10.f, 30000.f), 1000.f, AudioParameterFloatAttributes().withLabel ("Hz")));
        grp->addChild (std::make_unique<AudioParameterFloat> (id ("gain"), n + "Gain", NormalisableRange<float> (-30.f, 30.f, 0.01f), 0.f, AudioParameterFloatAttributes().withLabel ("dB")));
        grp->addChild (std::make_unique<AudioParameterFloat> (id ("q"), n + "Q", logRange (0.025f, 40.f), 1.f));
        grp->addChild (std::make_unique<AudioParameterChoice> (id ("slope"), n + "Slope", slopes, 3));
        grp->addChild (std::make_unique<AudioParameterChoice> (id ("place"), n + "Placement", StringArray { "Stereo", "Left", "Right", "Mid", "Side" }, 0));
        grp->addChild (std::make_unique<AudioParameterBool> (id ("dyn"), n + "Dynamic", false));
        grp->addChild (std::make_unique<AudioParameterFloat> (id ("thr"), n + "Threshold", NormalisableRange<float> (-60.f, 0.f, 0.1f), -24.f, AudioParameterFloatAttributes().withLabel ("dB")));
        grp->addChild (std::make_unique<AudioParameterFloat> (id ("range"), n + "Range", NormalisableRange<float> (-24.f, 24.f, 0.1f), -1.5f, AudioParameterFloatAttributes().withLabel ("dB")));
        grp->addChild (std::make_unique<AudioParameterFloat> (id ("att"), n + "Attack", logRange (0.1f, 200.f), 5.f, AudioParameterFloatAttributes().withLabel ("ms")));
        grp->addChild (std::make_unique<AudioParameterFloat> (id ("rel"), n + "Release", logRange (5.f, 2000.f), 80.f, AudioParameterFloatAttributes().withLabel ("ms")));
        grp->addChild (std::make_unique<AudioParameterFloat> (id ("knee"), n + "Knee", NormalisableRange<float> (0.f, 24.f, 0.1f), 6.f, AudioParameterFloatAttributes().withLabel ("dB")));
        grp->addChild (std::make_unique<AudioParameterChoice> (id ("det"), n + "Detection", StringArray { "RMS", "Peak" }, 0));
        grp->addChild (std::make_unique<AudioParameterBool> (id ("sc"), n + "External Sidechain", false));
        L.add (std::move (grp));
    }
    L.add (std::make_unique<AudioParameterFloat> (ParameterID { "in", 1 }, "Input", NormalisableRange<float> (-24.f, 24.f, 0.01f), 0.f, AudioParameterFloatAttributes().withLabel ("dB")));
    L.add (std::make_unique<AudioParameterFloat> (ParameterID { "out", 1 }, "Output", NormalisableRange<float> (-24.f, 24.f, 0.01f), 0.f, AudioParameterFloatAttributes().withLabel ("dB")));
    L.add (std::make_unique<AudioParameterFloat> (ParameterID { "scale", 1 }, "Gain Scale", NormalisableRange<float> (-100.f, 200.f, 1.f), 100.f, AudioParameterFloatAttributes().withLabel ("%")));
    L.add (std::make_unique<AudioParameterBool> (ParameterID { "auto", 1 }, "Auto Gain", false));
    L.add (std::make_unique<AudioParameterChoice> (ParameterID { "char", 1 }, "Character", StringArray { "Clean", "Subtle", "Warm" }, 0));
    L.add (std::make_unique<AudioParameterBool> (ParameterID { "bypass", 1 }, "Bypass", false));
    return L;
}

TsrqProcessor::TsrqProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput ("Input", AudioChannelSet::stereo(), true)
                        .withOutput ("Output", AudioChannelSet::stereo(), true)
                        .withInput ("Sidechain", AudioChannelSet::stereo(), false)),
      apvts (*this, &undo, "TSRQ", createLayout()) {
    for (int i = 0; i < tsrq::kMaxBands; ++i)
        for (int k = 0; k < ids::numBandKeys; ++k) raw[i][k] = apvts.getRawParameterValue (ids::b (i, ids::bandKeys[k]));
    gIn = apvts.getRawParameterValue ("in"); gOut = apvts.getRawParameterValue ("out"); gScale = apvts.getRawParameterValue ("scale"); gAuto = apvts.getRawParameterValue ("auto");
    gChar = apvts.getRawParameterValue ("char"); gByp = apvts.getRawParameterValue ("bypass");
    // frequenze di partenza distribuite, così le bande nuove non si sovrappongono
    for (int i = 0; i < tsrq::kMaxBands; ++i) if (auto* p = bp (i, "freq")) p->setValueNotifyingHost (p->convertTo0to1 ((float) (40.0 * std::pow (2.0, i * 9.0 / 31.0))));
    undo.clearUndoHistory();
    startTimerHz (30);
}
TsrqProcessor::~TsrqProcessor() { stopTimer(); }

bool TsrqProcessor::isBusesLayoutSupported (const BusesLayout& l) const {
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (out != AudioChannelSet::stereo() && out != AudioChannelSet::mono()) return false;
    if (in != out) return false;
    if (l.inputBuses.size() > 1) { const auto sc = l.inputBuses[1]; if (! sc.isDisabled() && sc != AudioChannelSet::stereo() && sc != AudioChannelSet::mono()) return false; }
    return true;
}

void TsrqProcessor::prepareToPlay (double sampleRate, int samplesPerBlock) {
    sr = sampleRate;
    engine.prepare (sampleRate, jmax (32, samplesPerBlock));
    monoTmp.assign ((size_t) jmax (32, samplesPerBlock) * 4, 0.0f);
}

tsrq::BandParams TsrqProcessor::readBand (int i) const {
    tsrq::BandParams b; auto v = [&] (int k) { return raw[i][k]->load (std::memory_order_relaxed); };
    b.used = v (0) > 0.5f; b.bypass = v (1) > 0.5f; b.type = jlimit (0, tsrq::NumTypes - 1, (int) v (2)); b.f = v (3); b.gain = v (4); b.q = v (5);
    b.slope = kSlopes[jlimit (0, kNumSlopes - 1, (int) v (6))]; b.place = jlimit (0, 4, (int) v (7)); b.dyn = v (8) > 0.5f;
    b.thr = v (9); b.range = v (10); b.att = v (11); b.rel = v (12); b.knee = v (13); b.det = (int) v (14); b.sc = v (15) > 0.5f;
    if (b.type == tsrq::LowShelf || b.type == tsrq::HighShelf) b.slope = b.slope <= 6 ? 6 : 12;
    return b;
}

void TsrqProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&) {
    ScopedNoDenormals nd;
    auto main = getBusBuffer (buffer, false, 0);
    const int n = main.getNumSamples(), nch = main.getNumChannels(); if (n == 0 || nch == 0) return;
    const double fs = sr.load();

    tsrq::Engine::Global g; g.inDb = gIn->load(); g.outDb = gOut->load(); g.scale = gScale->load() / 100.0; g.character = (int) gChar->load();
    g.bypass = gByp->load() > 0.5f; g.solo = solo.load(); g.autoGainDb = gAuto->load() > 0.5f ? autoGainDb.load() : 0.0;
    engine.setGlobal (g);
    for (int i = 0; i < tsrq::kMaxBands; ++i) { auto b = readBand (i); b.f = std::min (b.f, fs * 0.495); engine.setBand (i, b); }

    const float* scL = nullptr; const float* scR = nullptr;
    if (getBusCount (true) > 1) { auto* scBus = getBus (true, 1);
        if (scBus != nullptr && scBus->isEnabled()) { auto sc = getBusBuffer (buffer, true, 1); if (sc.getNumChannels() > 0) { scL = sc.getReadPointer (0); scR = sc.getReadPointer (jmin (1, sc.getNumChannels() - 1)); } } }

    float* L = main.getWritePointer (0); float* R = nch > 1 ? main.getWritePointer (1) : nullptr;
    { const float gi = Decibels::decibelsToGain (gIn->load()); float a = 0, b = 0; for (int i = 0; i < n; ++i) { a = jmax (a, std::abs (L[i])); if (R) b = jmax (b, std::abs (R[i])); }
      maxInto (pkIn[0], a * gi); maxInto (pkIn[1], (R ? b : a) * gi); }
    // analizzatore: ingresso
    for (int i0 = 0; i0 < n; i0 += 1024) { const int m = jmin (1024, n - i0); float tmp[1024];
        for (int i = 0; i < m; ++i) tmp[i] = R ? 0.5f * (L[i0 + i] + R[i0 + i]) : L[i0 + i]; scopePre.push (tmp, m); }
    if (R) engine.process (L, R, scL, scR, n);
    else { // mono: il motore lavora in stereo su una copia
        float* tmpR = monoTmp.data(); for (int i0 = 0; i0 < n; i0 += (int) monoTmp.size()) { const int m = jmin ((int) monoTmp.size(), n - i0);
            std::memcpy (tmpR, L + i0, sizeof (float) * (size_t) m); engine.process (L + i0, tmpR, scL ? scL + i0 : nullptr, scR ? scR + i0 : nullptr, m); } }
    for (int i0 = 0; i0 < n; i0 += 1024) { const int m = jmin (1024, n - i0); float tmp[1024];
        for (int i = 0; i < m; ++i) tmp[i] = R ? 0.5f * (L[i0 + i] + R[i0 + i]) : L[i0 + i]; scopePost.push (tmp, m); }
    { float a = 0, b = 0; for (int i = 0; i < n; ++i) { a = jmax (a, std::abs (L[i])); if (R) b = jmax (b, std::abs (R[i])); } maxInto (pkOut[0], a); maxInto (pkOut[1], R ? b : a); }
}

void TsrqProcessor::timerCallback() {   // auto gain: media in dB della curva statica 20 Hz-20 kHz, calcolata fuori dal thread audio
    if (gAuto->load() < 0.5f) return;
    const double fs = sr.load(); if (uiGrid.fs != fs) uiGrid.init (fs);
    const double sc = gScale->load() / 100.0; double sum = 0; tsrq::BandCoefs c[tsrq::kMaxBands]; int nb = 0;
    for (int i = 0; i < tsrq::kMaxBands; ++i) { auto b = readBand (i); if (! b.used || b.bypass) continue; b.f = std::min (b.f, fs * 0.495);
        tsrq::designBand (b, tsrq::hasGain (b.type) ? b.gain * sc : 0, uiGrid, c[nb++]); }
    for (int k = 0; k <= 60; ++k) { const double f = 20 * std::pow (1000.0, k / 60.0); double m = 1; for (int i = 0; i < nb; ++i) m *= tsrq::bandMag2 (c[i], f, fs); sum += 10 * std::log10 (std::max (m, 1e-12)); }
    autoGainDb = (float) (-sum / 61.0);
}

void TsrqProcessor::setBandParam (int i, const char* key, float plain) {
    if (auto* p = bp (i, key)) p->setValueNotifyingHost (p->convertTo0to1 (plain));
}
int TsrqProcessor::firstFreeBand() const {
    for (int i = 0; i < tsrq::kMaxBands; ++i) if (raw[i][0]->load() < 0.5f) return i;
    return -1;
}
void TsrqProcessor::switchAB (int slot) {
    if (slot == abSlot) return;
    abState[abSlot] = apvts.copyState();
    if (abState[slot].isValid()) apvts.replaceState (abState[slot].createCopy());
    abSlot = slot;
}
void TsrqProcessor::copyCurrentToOther() { abState[1 - abSlot] = apvts.copyState(); }

void TsrqProcessor::getStateInformation (MemoryBlock& dest) {
    auto st = apvts.copyState(); if (auto xml = st.createXml()) copyXmlToBinary (*xml, dest);
}
void TsrqProcessor::setStateInformation (const void* data, int size) {
    if (auto xml = getXmlFromBinary (data, size)) if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessorEditor* TsrqProcessor::createEditor() { return new TsrqWebEditor (*this); }   // interfaccia TSR Q (HTML) dentro una WebView
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TsrqProcessor(); }
