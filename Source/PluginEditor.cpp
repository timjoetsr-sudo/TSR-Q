#include "PluginEditor.h"
using namespace juce;

namespace col {
    const Colour graphite (0xff121417), panelTop (0xff2c3035), panelBot (0xff1b1e22), edge (0xff3d434a), silver (0xffc9ced6),
                 ink (0xffeef2f6), dim (0xff8b939d), screen (0xff0f1012), grid (0x14ffffff), accent (0xffffffff), dyn (0xff7cc4ff), spec (0xff9aa1aa);
}
// tema "Ceramica e platino": telaio ceramica bianca, manopole in ceramica, testo grafite
namespace cer {
    const Colour top (0xfff7f6f3), bot (0xffe6e4df), edge (0xffcfccc5), ink (0xff1b1b1b), title (0xff111111), dim (0xff7a7873), btn (0xffffffff), btnDown (0xffefeeea),
                 btnEdge (0x291b1b1b), on (0xff1b1b1b), onInk (0xfff7f6f3), dis (0x401b1b1b), sep (0x1a1b1b1b),
                 kArcBg (0x1a1b1b1b), kBody0 (0xffffffff), kBody1 (0xffd5d3ce), kFace0 (0xfffbfaf8), kFace1 (0xffe6e4df), kRim (0x0f000000), kShadow (0x383c372d);
}
static const char* kTypeNames[] = { "Bell", "Low Shelf", "High Shelf", "Low Cut", "High Cut", "Notch", "Band Pass", "Tilt Shelf", "Flat Tilt", "All Pass" };
static String fmtF (double f) { return f < 1000 ? String (f, f < 100 ? 1 : 0) + " Hz" : String (f / 1000.0, f < 10000 ? 2 : 1) + " kHz"; }
static String fmtDb (double d) { return (d > 0.05 ? "+" : "") + String (d, 1) + " dB"; }

// ================= Look & Feel: grafite, titanio satinato, argento =================
TsrqLnf::TsrqLnf() {
    setColour (Slider::textBoxTextColourId, cer::ink); setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (ComboBox::textColourId, cer::ink); setColour (ComboBox::arrowColourId, cer::dim);
    setColour (PopupMenu::backgroundColourId, Colour (0xff181b1f)); setColour (PopupMenu::textColourId, col::ink);
    setColour (PopupMenu::highlightedBackgroundColourId, Colour (0xff2f353c)); setColour (PopupMenu::highlightedTextColourId, Colours::white);
    setColour (TextButton::textColourOffId, cer::ink); setColour (TextButton::textColourOnId, cer::onInk);
    setColour (Label::textColourId, cer::ink); setColour (TextEditor::backgroundColourId, cer::btn); setColour (TextEditor::textColourId, cer::ink);
    setColour (TextEditor::highlightColourId, Colour (0x337cc4ff)); setColour (CaretComponent::caretColourId, cer::ink);
}
void TsrqLnf::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, Slider& s) {
    const float d = (float) jmin (w, h) - 6.f; const auto c = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
    const auto r = Rectangle<float> (d, d).withCentre (c); const float ang = a0 + pos * (a1 - a0); const bool en = s.isEnabled();
    // scala
    Path arcBg; arcBg.addCentredArc (c.x, c.y, d / 2 + 1, d / 2 + 1, 0, a0, a1, true);
    g.setColour (cer::kArcBg); g.strokePath (arcBg, PathStrokeType (2.f));
    Path arc; arc.addCentredArc (c.x, c.y, d / 2 + 1, d / 2 + 1, 0, a0, ang, true);
    g.setColour (en ? cer::ink : cer::dis); g.strokePath (arc, PathStrokeType (2.f));
    // corpo in ceramica
    const auto body = r.reduced (d * 0.12f);
    g.setColour (cer::kShadow); g.fillEllipse (body.translated (0, 2.5f));
    g.setGradientFill (ColourGradient (cer::kBody0, body.getX(), body.getY(), cer::kBody1, body.getRight(), body.getBottom(), false)); g.fillEllipse (body);
    const auto face = body.reduced (body.getWidth() * 0.12f);
    g.setGradientFill (ColourGradient (cer::kFace0, face.getCentreX(), face.getY(), cer::kFace1, face.getCentreX(), face.getBottom(), false)); g.fillEllipse (face);
    g.setColour (cer::kRim); g.drawEllipse (face, 1.f);
    // indicatore
    const float r0 = face.getWidth() * 0.12f, r1 = face.getWidth() * 0.45f;
    g.setColour (en ? cer::ink : cer::dis); g.drawLine (c.x + r0 * std::sin (ang), c.y - r0 * std::cos (ang), c.x + r1 * std::sin (ang), c.y - r1 * std::cos (ang), 2.f);
}
void TsrqLnf::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down) {
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f); const bool on = b.getToggleState();
    g.setColour (on ? cer::on : (down ? cer::btnDown : cer::btn)); g.fillRoundedRectangle (r, 3.f);
    g.setColour (on ? cer::on : (over ? cer::ink.withAlpha (0.45f) : cer::btnEdge)); g.drawRoundedRectangle (r, 3.f, 1.f);
}
void TsrqLnf::drawButtonText (Graphics& g, TextButton& b, bool, bool) {
    g.setFont (getTextButtonFont (b, b.getHeight())); g.setColour (b.getToggleState() ? cer::onInk : (b.isEnabled() ? cer::ink : cer::dis));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds(), Justification::centred, 1);
}
Font TsrqLnf::getTextButtonFont (TextButton&, int h) { return Font (FontOptions (jmin (12.f, h * 0.5f), Font::bold)).withExtraKerningFactor (0.08f); }
void TsrqLnf::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& c) {
    const auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (cer::btn); g.fillRoundedRectangle (r, 3.f); g.setColour (cer::btnEdge); g.drawRoundedRectangle (r, 3.f, 1.f);
    Path p; const float ax = (float) w - 12, ay = h * 0.5f; p.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (c.isEnabled() ? cer::dim : cer::dis); g.fillPath (p);
}
Font TsrqLnf::getComboBoxFont (ComboBox&) { return Font (FontOptions (12.f, Font::bold)); }
Label* TsrqLnf::createSliderTextBox (Slider& s) {
    auto* l = LookAndFeel_V4::createSliderTextBox (s); l->setFont (Font (FontOptions (12.5f, Font::bold))); l->setJustificationType (Justification::centred);
    l->setColour (Label::outlineColourId, Colours::transparentBlack); l->setColour (Label::backgroundColourId, Colours::transparentBlack);
    l->setColour (Label::textColourId, cer::ink); l->setColour (Label::textWhenEditingColourId, cer::ink); return l;
}

// ================= Grafico EQ + analizzatore =================
Colour EqGraph::bandColour (int i) { return Colour::fromHSV (std::fmod (0.13f + i * 0.618034f, 1.f), 0.55f, 0.98f, 1.f); }

EqGraph::EqGraph (TsrqProcessor& p) : proc (p) {
    setWantsKeyboardFocus (true); setMouseClickGrabsKeyboardFocus (true);
    resetPeaks(); startTimerHz (60);
}
EqGraph::~EqGraph() { stopTimer(); }
Rectangle<float> EqGraph::plot() const { return getLocalBounds().toFloat().withTrimmedLeft (44).withTrimmedRight (38).withTrimmedTop (10).withTrimmedBottom (22); }
double EqGraph::fMax() const { return jmin (30000.0, proc.currentSampleRate() / 2); }
float EqGraph::X (double f) const { const auto r = plot(); return r.getX() + (float) (std::log (f / 10.0) / std::log (fMax() / 10.0)) * r.getWidth(); }
double EqGraph::Fx (float x) const { const auto r = plot(); return 10.0 * std::pow (fMax() / 10.0, (x - r.getX()) / r.getWidth()); }
float EqGraph::Y (double db) const { const auto r = plot(); return r.getCentreY() - (float) (db / rangeDb) * r.getHeight() / 2; }
double EqGraph::Dy (float y) const { const auto r = plot(); return (r.getCentreY() - y) / (r.getHeight() / 2) * rangeDb; }
float EqGraph::AY (double d) const { const auto r = plot(); return r.getBottom() - (float) ((d + 96.0) / 102.0) * r.getHeight(); }

void EqGraph::resized() { cacheFs = 0; }

void EqGraph::refreshCurves() {
    const double fs = proc.currentSampleRate(); const auto r = plot(); const int W = jmax (2, (int) r.getWidth() / 2);
    bool geom = false;
    if (cacheFs != fs || (int) colF.size() != W) {
        cacheFs = fs; grid.init (fs); geom = true; colF.resize ((size_t) W); cw.resize ((size_t) W); c2w.resize ((size_t) W);
        for (int i = 0; i < W; ++i) { colF[(size_t) i] = (float) Fx (r.getX() + r.getWidth() * i / (W - 1)); const double w = 2 * tsrq::kPi * colF[(size_t) i] / fs; cw[(size_t) i] = std::cos (w); c2w[(size_t) i] = std::cos (2 * w); }
    }
    const double sc = proc.apvts.getRawParameterValue ("scale")->load() / 100.0; bool any = false, totalDirty = geom || sc != cacheScale; cacheScale = sc;
    auto mag = [&] (const tsrq::BandCoefs& c, std::vector<float>& out) {
        out.resize ((size_t) W);
        for (int i = 0; i < W; ++i) { double m = 1; const double a = cw[(size_t) i], b = c2w[(size_t) i];
            for (int k = 0; k < c.n; ++k) { const auto& q = c.c[k];
                m *= (q.b0 * q.b0 + q.b1 * q.b1 + q.b2 * q.b2 + 2 * (q.b0 * q.b1 + q.b1 * q.b2) * a + 2 * q.b0 * q.b2 * b) / (1 + q.a1 * q.a1 + q.a2 * q.a2 + 2 * (q.a1 + q.a1 * q.a2) * a + 2 * q.a2 * b); }
            out[(size_t) i] = (float) (10 * std::log10 (std::max (m, 1e-12))); } };
    anyDyn = false; bool liveDirty = false;
    for (int i = 0; i < tsrq::kMaxBands; ++i) {
        auto b = proc.readBand (i); b.f = std::min (b.f, fs * 0.495);
        const bool active = b.used && ! b.bypass; const bool dyn = active && b.dyn && tsrq::canDyn (b.type);
        const float d = dyn ? proc.engine.meterDelta[i].load() : 0.f; if (dyn) anyDyn = true;
        const auto& o = cacheP[i];
        const bool changed = geom || sc != cacheScale || o.used != b.used || o.bypass != b.bypass || o.type != b.type || o.f != b.f || o.gain != b.gain || o.q != b.q || o.slope != b.slope;
        if (changed || totalDirty) { cacheP[i] = b; totalDirty = true;
            if (active) { tsrq::BandCoefs c; tsrq::designBand (b, tsrq::hasGain (b.type) ? b.gain * sc : 0, grid, c); mag (c, curveDb[i]); } else curveDb[i].clear(); }
        if (std::abs (d - cacheDelta[i]) > 0.05f || changed) { cacheDelta[i] = d; liveDirty = true; }
        any = any || active;
    }
    if (totalDirty) { totalDb.assign ((size_t) W, 0.f); for (int i = 0; i < tsrq::kMaxBands; ++i) if (! curveDb[i].empty()) for (int k = 0; k < W; ++k) totalDb[(size_t) k] += curveDb[i][(size_t) k]; }
    if (anyDyn && (liveDirty || totalDirty)) {
        liveDb = totalDb;
        for (int i = 0; i < tsrq::kMaxBands; ++i) { const auto& b = cacheP[i];
            if (b.used && ! b.bypass && b.dyn && tsrq::canDyn (b.type) && std::abs (cacheDelta[i]) > 0.01f) {
                tsrq::BandCoefs c; tsrq::designBand (b, b.gain * sc + cacheDelta[i], grid, c); std::vector<float> tmp; mag (c, tmp);
                for (int k = 0; k < W; ++k) liveDb[(size_t) k] += tmp[(size_t) k] - curveDb[i][(size_t) k]; } }
    }
    ignoreUnused (any);
}

void EqGraph::updateSpectrum() {
    if (fftN != (1 << fftOrder)) {
        fftN = 1 << fftOrder; fft = std::make_unique<dsp::FFT> (fftOrder);
        ringPre.assign ((size_t) fftN, 0.f); ringPost.assign ((size_t) fftN, 0.f); work.assign ((size_t) fftN * 2, 0.f); win.resize ((size_t) fftN);
        for (int i = 0; i < fftN; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (2 * MathConstants<float>::pi * i / (fftN - 1));
        specPre.assign ((size_t) fftN / 2, -200.f); specPost.assign ((size_t) fftN / 2, -200.f); peak.assign ((size_t) fftN / 2, -200.f); ringPos = 0;
    }
    pullTmp.resize (8192);
    int got = 0, gotPost = 0, pos = ringPos;
    while ((got = proc.scopePre.pull (pullTmp.data(), 8192)) > 0) { for (int i = 0; i < got; ++i) { ringPre[(size_t) pos] = pullTmp[(size_t) i]; pos = (pos + 1) % fftN; } newSamples += got; }
    pos = ringPos;
    while ((gotPost = proc.scopePost.pull (pullTmp.data(), 8192)) > 0) { for (int i = 0; i < gotPost; ++i) { ringPost[(size_t) pos] = pullTmp[(size_t) i]; pos = (pos + 1) % fftN; } }
    ringPos = pos;
    if (freeze || newSamples < fftN / 8) return;
    const float dt = 1.f / 60.f, fall = decayDbPerSec * dt, norm = 4.f / fftN;
    for (int which = 0; which < 2; ++which) {
        auto& ring = which ? ringPost : ringPre; auto& spec = which ? specPost : specPre;
        for (int i = 0; i < fftN; ++i) work[(size_t) i] = ring[(size_t) ((ringPos + i) % fftN)] * win[(size_t) i];
        std::fill (work.begin() + fftN, work.end(), 0.f);
        fft->performFrequencyOnlyForwardTransform (work.data());
        for (int k = 0; k < fftN / 2; ++k) {
            const float v = 20.f * std::log10 (std::max (work[(size_t) k] * norm, 1e-9f));
            float& s = spec[(size_t) k]; const float avg = s + (v - s) * (1.f - avgAlpha);
            s = v > s ? avg : std::max (avg, s - fall);
            if (which && peakHold) peak[(size_t) k] = std::max (peak[(size_t) k], s);
        }
    }
    newSamples = 0;
}

void EqGraph::timerCallback() { updateSpectrum(); refreshCurves(); repaint(); }

float EqGraph::specAt (float x) const {
    if (fftN == 0) return -200.f; const auto& sp = showPost ? specPost : specPre; const double fs = proc.currentSampleRate();
    const int k0 = jmax (1, (int) (Fx (x - 1) * fftN / fs)), k1 = jmax (k0, (int) std::ceil (Fx (x + 1) * fftN / fs)); float m = -200.f;
    for (int k = k0; k <= k1 && k < fftN / 2; ++k) m = std::max (m, sp[(size_t) k]);
    return m + 4.5f * (float) std::log2 (Fx (x) / 1000.0);
}

Point<float> EqGraph::nodePos (int i) const {
    const auto b = proc.readBand (i); const double sc = proc.apvts.getRawParameterValue ("scale")->load() / 100.0;
    const double gdb = tsrq::hasGain (b.type) ? jlimit (-(double) rangeDb, (double) rangeDb, b.gain * sc) : 0.0;
    return { X (std::min (b.f, fMax())), Y (gdb) };
}
int EqGraph::hitNode (Point<float> p) const {
    for (int i = tsrq::kMaxBands - 1; i >= 0; --i) if (proc.readBand (i).used && nodePos (i).getDistanceFrom (p) < 10.f) return i;
    return -1;
}

void EqGraph::paint (Graphics& g) {
    const auto r = plot(); const double fs = proc.currentSampleRate();
    g.setColour (col::screen); g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.f);
    // griglia: sottodivisioni log, decadi, dB maggiori/minori, cornice
    for (int dec = 10; dec <= 10000; dec *= 10) for (int m = 1; m <= 9; ++m) { const double f = dec * m; if (f < 10 || f > fMax()) continue;
        const float x = std::round (X (f)) + 0.5f; g.setColour (Colours::white.withAlpha (m == 1 ? 0.16f : 0.05f)); g.drawVerticalLine ((int) x, r.getY(), r.getBottom()); }
    const int stp = rangeDb <= 6 ? 2 : rangeDb <= 12 ? 3 : rangeDb <= 18 ? 6 : 10;
    for (int d = -rangeDb; d <= rangeDb; d += stp) { g.setColour (Colours::white.withAlpha (d == 0 ? 0.32f : 0.11f)); g.drawHorizontalLine ((int) std::round (Y (d)), r.getX(), r.getRight());
        g.setColour (col::dim); g.setFont (Font (FontOptions (10.f, Font::bold))); g.drawText (d == 0 ? "0 dB" : (d > 0 ? "+" : "") + String (d), 0, (int) Y (d) - 6, 40, 12, Justification::centredRight); }
    g.setColour (Colours::white.withAlpha (0.18f)); g.drawRect (r, 1.f);
    const std::pair<double, const char*> fl[] = { { 20, "20" }, { 50, "50" }, { 100, "100" }, { 200, "200" }, { 500, "500" }, { 1000, "1k" }, { 2000, "2k" }, { 5000, "5k" }, { 10000, "10k" }, { 20000, "20k" } };
    g.setFont (Font (FontOptions (10.f, Font::bold)));
    for (auto& [f, t] : fl) if (f <= fMax()) { g.setColour (f == 100 || f == 1000 || f == 10000 ? col::ink : col::dim); g.drawText (t, (int) X (f) - 20, (int) r.getBottom() + 4, 40, 12, Justification::centred); }
    g.setColour (col::spec); g.setFont (Font (FontOptions (9.f, Font::bold)));
    for (int d = -90; d <= 0; d += 15) { const float y = AY (d); if (y > r.getY() + 4 && y < r.getBottom() - 4) g.drawText (String (d), (int) r.getRight() + 4, (int) y - 6, 32, 12, Justification::centredLeft); }
    g.drawText ("dBFS", (int) r.getRight() + 4, (int) r.getBottom() + 4, 34, 12, Justification::centredLeft);

    g.saveState(); g.reduceClipRegion (r.toNearestInt());
    // analizzatore (scala dBFS, inclinazione 4,5 dB/oct)
    auto specPath = [&] (const std::vector<float>& sp, bool fill) {
        Path p; bool first = true;
        for (float x = r.getX(); x <= r.getRight(); x += 2.f) {
            const int k0 = jmax (1, (int) (Fx (x - 1) * fftN / fs)), k1 = jmax (k0, (int) std::ceil (Fx (x + 1) * fftN / fs)); float m = -200.f;
            for (int k = k0; k <= k1 && k < fftN / 2; ++k) m = std::max (m, sp[(size_t) k]);
            const float y = jlimit (r.getY(), r.getBottom(), AY (m + 4.5f * (float) std::log2 (Fx (x) / 1000.0)));
            if (first) { p.startNewSubPath (x, fill ? r.getBottom() : y); if (fill) p.lineTo (x, y); first = false; } else p.lineTo (x, y); }
        if (fill) { p.lineTo (r.getRight(), r.getBottom()); p.closeSubPath(); }
        return p; };
    if (fftN > 0) {
        if (showPre) { g.setColour (col::spec.withAlpha (0.22f)); g.fillPath (specPath (specPre, true)); }
        if (showPost) { g.setColour (col::spec.withAlpha (0.30f)); g.fillPath (specPath (specPost, true)); g.setColour (col::spec.withAlpha (0.9f)); g.strokePath (specPath (specPost, false), PathStrokeType (1.f)); }
        if (peakHold) { g.setColour (Colours::white.withAlpha (0.5f)); g.strokePath (specPath (peak, false), PathStrokeType (0.8f)); }
        if (freeze) { g.setColour (col::dyn); g.setFont (Font (FontOptions (10.f, Font::bold))); g.drawText ("FREEZE", r.reduced (6).toNearestInt(), Justification::topRight); }
    }
    // curve delle bande + curva globale
    const int W = (int) colF.size();
    auto curvePath = [&] (const std::vector<float>& c) { Path p; for (int i = 0; i < W; ++i) { const float x = r.getX() + r.getWidth() * i / (W - 1), y = Y (c[(size_t) i]); if (i) p.lineTo (x, y); else p.startNewSubPath (x, y); } return p; };
    if (W > 1) {
        for (int i = 0; i < tsrq::kMaxBands; ++i) if (! curveDb[i].empty()) {
            const bool s = sel.count (i) > 0; auto p = curvePath (curveDb[i]);
            if (s) { Path f = p; f.lineTo (r.getRight(), Y (0)); f.lineTo (r.getX(), Y (0)); f.closeSubPath(); g.setColour (bandColour (i).withAlpha (0.16f)); g.fillPath (f); }
            g.setColour (bandColour (i).withAlpha (s ? 0.9f : 0.45f)); g.strokePath (p, PathStrokeType (s ? 1.4f : 1.f));
        }
        if (! totalDb.empty()) {
            const bool byp = proc.apvts.getRawParameterValue ("bypass")->load() > 0.5f;
            if (anyDyn && ! liveDb.empty()) { g.setColour (col::dyn); g.strokePath (curvePath (liveDb), PathStrokeType (2.2f)); }
            g.setColour (byp ? col::dim : col::accent); g.strokePath (curvePath (totalDb), PathStrokeType (2.f));
        }
    }
    g.restoreState();
    // nodi
    for (int i = 0; i < tsrq::kMaxBands; ++i) { const auto b = proc.readBand (i); if (! b.used) continue;
        const auto p = nodePos (i); const auto c = bandColour (i); const bool s = sel.count (i) > 0;
        if (b.dyn && tsrq::canDyn (b.type) && ! b.bypass) { const float d = proc.engine.meterDelta[i].load();
            g.setColour (col::dyn); g.drawLine (p.x, p.y, p.x, Y (jlimit (-(double) rangeDb, (double) rangeDb, b.gain * cacheScale + d)), 2.f);
            Path a; a.addCentredArc (p.x, p.y, 12, 12, 0, 0, MathConstants<float>::twoPi * jmin (1.f, std::abs (d) / jmax (0.1f, (float) std::abs (b.range))), true);
            g.strokePath (a, PathStrokeType (2.f)); }
        const auto e = Rectangle<float> (16, 16).withCentre (p);
        if (b.bypass) { g.setColour (col::screen); g.fillEllipse (e); g.setColour (c.withAlpha (0.45f)); g.drawEllipse (e, 1.2f); }
        else { g.setColour (c.darker (0.2f)); g.fillEllipse (e); g.setColour (Colours::white.withAlpha (0.35f)); g.drawEllipse (e.reduced (1), 0.8f); }
        if (s) { g.setColour (Colours::white); g.drawEllipse (e.expanded (3), i == primary ? 1.8f : 1.f); }
        g.setColour (b.bypass ? c.withAlpha (0.6f) : Colours::black); g.setFont (Font (FontOptions (9.5f, Font::bold))); g.drawText (String (i + 1), e.toNearestInt(), Justification::centred);
        const char* pl[] = { "", "L", "R", "M", "S" }; if (b.place) { g.setColour (col::ink); g.drawText (pl[b.place], (int) p.x + 9, (int) p.y - 18, 12, 10, Justification::centred); }
    }
    // etichetta vicino al nodo (hover / trascinamento)
    const int lb = dragging ? primary : hover;
    if (lb >= 0 && proc.readBand (lb).used) { const auto b = proc.readBand (lb); const auto p = nodePos (lb);
        String t = String (kTypeNames[b.type]) + "  " + fmtF (b.f) + (tsrq::hasGain (b.type) ? "  " + fmtDb (b.gain) : "") + "  Q " + String (b.q, 2);
        if (b.dyn && tsrq::canDyn (b.type)) t << "  DYN " << String (proc.engine.meterDelta[lb].load(), 1) << " dB";
        const float w = GlyphArrangement::getStringWidth (Font (FontOptions (11.f, Font::bold)), t) + 16;
        auto box = Rectangle<float> (w, 20).withCentre ({ p.x, p.y - 26 }); if (box.getY() < 2) box.setY (p.y + 16);
        box = box.constrainedWithin (getLocalBounds().toFloat().reduced (2));
        g.setColour (Colour (0xf0181b1f)); g.fillRoundedRectangle (box, 4); g.setColour (bandColour (lb)); g.drawRoundedRectangle (box, 4, 1);
        g.setColour (col::ink); g.setFont (Font (FontOptions (11.f, Font::bold))); g.drawText (t, box.toNearestInt(), Justification::centred); }
    // spectrum grab
    if (grabF > 0 && hover < 0 && ! dragging && ! rectSel) { const float x = X (grabF);
        g.setColour (col::accent.withAlpha (0.8f)); g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
        g.setFont (Font (FontOptions (11.f, Font::bold))); g.drawText (fmtF (grabF) + "  clic: nuova banda", (int) x + 6, (int) r.getY() + 4, 170, 14, Justification::centredLeft); }
    if (rectSel) { g.setColour (col::dyn.withAlpha (0.10f)); g.fillRect (rect); g.setColour (col::dyn); g.drawRect (rect, 1.f); }
    bool none = true; for (int i = 0; i < tsrq::kMaxBands; ++i) if (proc.readBand (i).used) { none = false; break; }
    if (none) { g.setColour (col::dim); g.setFont (Font (FontOptions (13.f, Font::bold))); g.drawText ("Doppio clic sul grafico per creare una banda", r.toNearestInt(), Justification::centred); }
    if (proc.apvts.getRawParameterValue ("auto")->load() > 0.5f) { g.setColour (col::accent); g.setFont (Font (FontOptions (10.f, Font::bold)));
        g.drawText ("AUTO GAIN " + fmtDb (proc.autoGainDb.load()), r.reduced (6).toNearestInt(), Justification::topLeft); }
}

// ---------- selezione / bande ----------
void EqGraph::selectOnly (int i) { sel.clear(); if (i >= 0) sel.insert (i); primary = i; if (onPrimaryChanged) onPrimaryChanged (primary); }
void EqGraph::selectAll() { sel.clear(); for (int i = 0; i < tsrq::kMaxBands; ++i) if (proc.readBand (i).used) sel.insert (i); if (! sel.empty() && ! sel.count (primary)) { primary = *sel.begin(); if (onPrimaryChanged) onPrimaryChanged (primary); } }
void EqGraph::deleteSelected() { if (sel.empty()) return; proc.undo.beginNewTransaction ("Elimina bande"); for (int i : sel) proc.setBandParam (i, "used", 0); selectOnly (-1); }
int EqGraph::createBand (double f, double gdb, int type) {
    const int i = proc.firstFreeBand(); if (i < 0) return -1;
    proc.undo.beginNewTransaction ("Nuova banda");
    proc.setBandParam (i, "type", (float) type); proc.setBandParam (i, "freq", (float) jlimit (10.0, 30000.0, f)); proc.setBandParam (i, "gain", tsrq::hasGain (type) ? (float) jlimit (-30.0, 30.0, gdb) : 0.f);
    proc.setBandParam (i, "q", type == tsrq::LowCut || type == tsrq::HighCut ? 0.71f : 1.f); proc.setBandParam (i, "slope", 3); proc.setBandParam (i, "place", 0);
    proc.setBandParam (i, "dyn", 0); proc.setBandParam (i, "byp", 0); proc.setBandParam (i, "used", 1);
    selectOnly (i); return i;
}
void EqGraph::applyToSelected (const char* key, float v) { proc.undo.beginNewTransaction(); for (int i : sel) proc.setBandParam (i, key, v); }

// ---------- mouse ----------
void EqGraph::mouseDown (const MouseEvent& e) {
    grabKeyboardFocus(); const auto p = e.position; downPos = p; moved = false; const int h = hitNode (p);
    if (e.mods.isPopupMenu()) { if (h >= 0) { if (! sel.count (h)) selectOnly (h); else { primary = h; if (onPrimaryChanged) onPrimaryChanged (h); } showBandMenu (h); } else showEmptyMenu (p); return; }
    if (h >= 0) {
        if (e.mods.isCommandDown()) { if (sel.count (h) && sel.size() > 1) { sel.erase (h); if (primary == h) { primary = *sel.begin(); if (onPrimaryChanged) onPrimaryChanged (primary); } return; } sel.insert (h); primary = h; if (onPrimaryChanged) onPrimaryChanged (h); }
        else if (! (sel.count (h) && sel.size() > 1)) selectOnly (h);
        else { primary = h; if (onPrimaryChanged) onPrimaryChanged (h); }
        proc.undo.beginNewTransaction ("Sposta bande"); dragStart.clear();
        for (int i : sel) { const auto b = proc.readBand (i); dragStart.push_back ({ i, b.f, b.gain }); proc.bp (i, "freq")->beginChangeGesture(); proc.bp (i, "gain")->beginChangeGesture(); }
        dragging = true; return;
    }
    if (grabF > 0) { createBand (grabF, 0.0); proc.setBandParam (primary, "q", 4.f); grabF = -1; return; }
    rectSel = true; rect = Rectangle<float> (p, p); rectKeep = e.mods.isCommandDown() ? sel : std::set<int> {};
}
void EqGraph::mouseDrag (const MouseEvent& e) {
    const auto p = e.position; if (p.getDistanceFrom (downPos) > 3) moved = true;
    if (rectSel) { rect = Rectangle<float> (downPos, p); std::set<int> s = rectKeep; for (int i = 0; i < tsrq::kMaxBands; ++i) if (proc.readBand (i).used && rect.contains (nodePos (i))) s.insert (i);
        sel = s; if (! sel.empty() && ! sel.count (primary)) { primary = *sel.begin(); if (onPrimaryChanged) onPrimaryChanged (primary); } return; }
    if (! dragging || ! moved) return;
    float dx = p.x - downPos.x, dy = p.y - downPos.y;
    if (e.mods.isShiftDown()) { dx *= 0.1f; dy *= 0.1f; }
    if (e.mods.isAltDown()) { if (std::abs (dx) > std::abs (dy)) dy = 0; else dx = 0; }          // Option: vincolo su un asse
    const auto r = plot(); const double ratio = std::pow (fMax() / 10.0, dx / r.getWidth()), ddb = -dy / (r.getHeight() / 2) * rangeDb / jmax (0.01, cacheScale);
    for (auto& d : dragStart) { proc.setBandParam (d.band, "freq", (float) jlimit (10.0, 30000.0, d.f * ratio)); if (tsrq::hasGain (proc.readBand (d.band).type)) proc.setBandParam (d.band, "gain", (float) jlimit (-30.0, 30.0, d.g + ddb)); }
}
void EqGraph::mouseUp (const MouseEvent& e) {
    if (rectSel) { rectSel = false; if (! moved && ! e.mods.isCommandDown()) selectOnly (-1); repaint(); return; }
    if (dragging) { for (auto& d : dragStart) { proc.bp (d.band, "freq")->endChangeGesture(); proc.bp (d.band, "gain")->endChangeGesture(); } dragging = false;
        if (! moved && e.mods.isAltDown() && primary >= 0) {          // Option+clic: azione rapida configurabile
            const auto b = proc.readBand (primary); proc.undo.beginNewTransaction();
            if (optionClickDynamic && tsrq::canDyn (b.type)) proc.setBandParam (primary, "dyn", b.dyn ? 0.f : 1.f); else proc.setBandParam (primary, "byp", b.bypass ? 0.f : 1.f); } }
}
void EqGraph::mouseMove (const MouseEvent& e) {
    mouse = e.position; hover = hitNode (e.position); grabF = -1;
    if (hover < 0 && plot().contains (e.position) && fftN > 0) {             // spectrum grab: vicino alla linea dello spettro, cerca il picco locale
        const float sy = AY (specAt (e.position.x));
        if (std::abs (sy - e.position.y) < 14.f) { const double f0 = Fx (e.position.x); double bf = f0; float bv = -1e9f;
            for (int k = -12; k <= 12; ++k) { const double f = f0 * std::pow (2.0, k / 72.0); const float v = specAt (X (f)); if (v > bv) { bv = v; bf = f; } } grabF = bf; } }
    setMouseCursor (hover >= 0 ? MouseCursor::NormalCursor : MouseCursor::NormalCursor);
}
void EqGraph::mouseExit (const MouseEvent&) { hover = -1; grabF = -1; }
void EqGraph::mouseDoubleClick (const MouseEvent& e) {
    if (hitNode (e.position) >= 0 || ! plot().contains (e.position)) return;
    createBand (Fx (e.position.x), Dy (e.position.y) / jmax (0.01, cacheScale));
}
void EqGraph::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w) {
    const int h = hitNode (e.position); if (h >= 0 && ! sel.count (h)) selectOnly (h); if (sel.empty()) return;
    const double k = std::pow (1.12, (w.deltaY > 0 ? 1 : -1) * (e.mods.isShiftDown() ? 0.25 : 1.0));
    for (int i : sel) proc.setBandParam (i, "q", (float) jlimit (0.025, 40.0, proc.readBand (i).q * k));
}
bool EqGraph::keyPressed (const KeyPress& k) {
    if (k == KeyPress::escapeKey) {
        if (dragging) { for (auto& d : dragStart) { proc.setBandParam (d.band, "freq", (float) d.f); proc.setBandParam (d.band, "gain", (float) d.g); proc.bp (d.band, "freq")->endChangeGesture(); proc.bp (d.band, "gain")->endChangeGesture(); } dragging = false; return true; }
        if (rectSel) { rectSel = false; sel = rectKeep; return true; }
        selectOnly (-1); return true; }
    if (k == KeyPress::deleteKey || k == KeyPress::backspaceKey) { deleteSelected(); return true; }
    if (k == KeyPress ('a', ModifierKeys::commandModifier, 0)) { selectAll(); return true; }
    return false;
}

// ---------- menu contestuali ----------
void EqGraph::showBandMenu (int band) {
    const auto b = proc.readBand (band); PopupMenu m, types, slopes, place;
    m.addSectionHeader ("BANDA " + String (band + 1) + "  " + fmtF (b.f) + (sel.size() > 1 ? "  (" + String ((int) sel.size()) + " selezionate)" : ""));
    for (int t = 0; t < tsrq::NumTypes; ++t) types.addItem (kTypeNames[t], true, b.type == t, [this, t] { applyToSelected ("type", (float) t); });
    m.addSubMenu ("Tipo di filtro", types);
    for (int s = 0; s < kNumSlopes; ++s) slopes.addItem (String (kSlopes[s]) + " dB/oct", true, kSlopes[s] == b.slope, [this, s] { applyToSelected ("slope", (float) s); });
    m.addSubMenu ("Pendenza", slopes, tsrq::hasSlope (b.type));
    const char* pn[] = { "Stereo", "Left", "Right", "Mid", "Side" };
    for (int p = 0; p < 5; ++p) place.addItem (pn[p], true, b.place == p, [this, p] { applyToSelected ("place", (float) p); });
    m.addSubMenu ("Lavora su", place);
    m.addSeparator();
    m.addItem ("Dinamica", tsrq::canDyn (b.type), b.dyn, [this, b] { applyToSelected ("dyn", b.dyn ? 0.f : 1.f); });
    m.addItem ("Bypass banda", true, b.bypass, [this, b] { applyToSelected ("byp", b.bypass ? 0.f : 1.f); });
    m.addItem ("Solo banda", true, proc.solo.load() == band, [this, band] { proc.solo = proc.solo.load() == band ? -1 : band; });
    m.addSeparator();
    m.addItem ("Elimina", [this] { deleteSelected(); });
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this).withMousePosition());
}
void EqGraph::showEmptyMenu (Point<float> p) {
    PopupMenu m, types; const double f = Fx (p.x), gdb = Dy (p.y);
    for (int t = 0; t < tsrq::NumTypes; ++t) types.addItem (kTypeNames[t], [this, f, gdb, t] { createBand (f, gdb, t); });
    m.addSectionHeader ("NUOVA BANDA  " + fmtF (f)); m.addSubMenu ("Crea qui", types);
    m.addItem ("Seleziona tutte", [this] { selectAll(); });
    m.addItem ("Elimina tutte", [this] { selectAll(); deleteSelected(); });
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this).withMousePosition());
}

// ================= Pannello banda =================
void BandPanel::knob (Slider& s, const String& name) {
    s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag); s.setTextBoxStyle (Slider::TextBoxBelow, false, 78, 18);
    s.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    s.setTextBoxIsEditable (true); s.setScrollWheelEnabled (true);
    s.setColour (Slider::textBoxTextColourId, cer::ink); s.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    s.setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack); s.setColour (Slider::textBoxHighlightColourId, Colour (0x337cc4ff));
    addAndMakeVisible (s); knobs.push_back (&s); knobNames.add (name);
}
BandPanel::BandPanel (TsrqProcessor& p) : proc (p) {
    knob (freq, "FREQ"); knob (gain, "GAIN"); knob (q, "Q"); knob (thr, "THRESH"); knob (range, "RANGE"); knob (att, "ATTACK"); knob (rel, "RELEASE"); knob (knee, "KNEE"); knob (scale, "SCALE"); knob (out, "OUTPUT");
    StringArray tn; for (auto* t : kTypeNames) tn.add (t); type.addItemList (tn, 1);
    StringArray sl; for (int s : kSlopes) sl.add (String (s) + " dB/oct"); slope.addItemList (sl, 1);
    place.addItemList ({ "Stereo", "Left", "Right", "Mid", "Side" }, 1); det.addItemList ({ "RMS", "Peak" }, 1); character.addItemList ({ "Clean", "Subtle", "Warm" }, 1);
    for (auto* c : { &type, &slope, &place, &det, &character }) addAndMakeVisible (*c);
    for (auto* b : { &dyn, &sc, &byp, &autoGain }) { b->setClickingTogglesState (true); addAndMakeVisible (*b); }
    title.setFont (Font (FontOptions (13.f, Font::bold))); addAndMakeVisible (title);
    aScale = std::make_unique<SA> (proc.apvts, "scale", scale); aOut = std::make_unique<SA> (proc.apvts, "out", out);
    aChar = std::make_unique<CA> (proc.apvts, "char", character); aAuto = std::make_unique<BA> (proc.apvts, "auto", autoGain);
    scale.textFromValueFunction = [] (double v) { return String ((int) std::lround (v)) + " %"; }; scale.updateText();
    out.textFromValueFunction = [] (double v) { return fmtDb (v); }; out.updateText();
    bind (-1);
}
void BandPanel::bind (int b) {
    band = b;
    aFreq.reset(); aGain.reset(); aQ.reset(); aThr.reset(); aRange.reset(); aAtt.reset(); aRel.reset(); aKnee.reset(); aType.reset(); aSlope.reset(); aPlace.reset(); aDet.reset(); aDyn.reset(); aSc.reset(); aByp.reset();
    const bool on = b >= 0;
    for (auto* c : std::initializer_list<Component*> { &freq, &gain, &q, &thr, &range, &att, &rel, &knee, &type, &slope, &place, &det, &dyn, &sc, &byp }) c->setEnabled (on);
    if (! on) { for (auto* k : { &freq, &gain, &q, &thr, &range, &att, &rel, &knee }) { k->textFromValueFunction = [] (double) { return String (CharPointer_UTF8 ("\xe2\x80\x94")); }; k->updateText(); }
        title.setText ("NESSUNA BANDA SELEZIONATA", dontSendNotification); title.setColour (Label::textColourId, cer::dim); repaint(); return; }
    auto id = [b] (const char* k) { return ids::b (b, k); };
    aFreq = std::make_unique<SA> (proc.apvts, id ("freq"), freq); aGain = std::make_unique<SA> (proc.apvts, id ("gain"), gain); aQ = std::make_unique<SA> (proc.apvts, id ("q"), q);
    aThr = std::make_unique<SA> (proc.apvts, id ("thr"), thr); aRange = std::make_unique<SA> (proc.apvts, id ("range"), range); aAtt = std::make_unique<SA> (proc.apvts, id ("att"), att);
    aRel = std::make_unique<SA> (proc.apvts, id ("rel"), rel); aKnee = std::make_unique<SA> (proc.apvts, id ("knee"), knee);
    aType = std::make_unique<CA> (proc.apvts, id ("type"), type); aSlope = std::make_unique<CA> (proc.apvts, id ("slope"), slope); aPlace = std::make_unique<CA> (proc.apvts, id ("place"), place);
    aDet = std::make_unique<CA> (proc.apvts, id ("det"), det); aDyn = std::make_unique<BA> (proc.apvts, id ("dyn"), dyn); aSc = std::make_unique<BA> (proc.apvts, id ("sc"), sc); aByp = std::make_unique<BA> (proc.apvts, id ("byp"), byp);
    freq.textFromValueFunction = [] (double v) { return fmtF (v); };
    gain.textFromValueFunction = range.textFromValueFunction = [] (double v) { return fmtDb (v); };
    thr.textFromValueFunction = knee.textFromValueFunction = [] (double v) { return String (v, 1) + " dB"; };
    q.textFromValueFunction = [] (double v) { return String (v, v < 1 ? 3 : 2); };
    att.textFromValueFunction = [] (double v) { return String (v, v < 10 ? 1 : 0) + " ms"; };
    rel.textFromValueFunction = [] (double v) { return v < 1000 ? String (v, 0) + " ms" : String (v / 1000.0, 2) + " s"; };
    for (auto* k : { &freq, &gain, &q, &thr, &range, &att, &rel, &knee }) k->updateText();
    title.setText ("BANDA " + String (b + 1), dontSendNotification); title.setColour (Label::textColourId, EqGraph::bandColour (b).darker (1.2f)); repaint();
}
void BandPanel::resized() {
    auto r = getLocalBounds().reduced (12, 8);
    auto left = r.removeFromLeft (150); title.setBounds (left.removeFromTop (20));
    left.removeFromTop (6); type.setBounds (left.removeFromTop (24)); left.removeFromTop (6); slope.setBounds (left.removeFromTop (24)); left.removeFromTop (6); place.setBounds (left.removeFromTop (24));
    left.removeFromTop (8); byp.setBounds (left.removeFromTop (24));
    r.removeFromLeft (14);
    const int kw = jmax (64, (r.getWidth() - 260) / 10);
    auto put = [&] (Slider& s) { auto c = r.removeFromLeft (kw); c.removeFromTop (16); s.setBounds (c.withHeight (jmin (c.getHeight(), kw + 22))); };
    put (freq); put (gain); put (q); r.removeFromLeft (16);
    auto dynCol = r.removeFromLeft (100); dynCol.removeFromTop (16); dyn.setBounds (dynCol.removeFromTop (24)); dynCol.removeFromTop (6); det.setBounds (dynCol.removeFromTop (24)); dynCol.removeFromTop (6); sc.setBounds (dynCol.removeFromTop (24));
    r.removeFromLeft (8); put (thr); put (range); put (att); put (rel); put (knee); r.removeFromLeft (16);
    auto g2 = r.removeFromLeft (100); g2.removeFromTop (16); autoGain.setBounds (g2.removeFromTop (24)); g2.removeFromTop (6); character.setBounds (g2.removeFromTop (24));
    put (scale); put (out);
}
void BandPanel::paint (Graphics& g) {
    g.setFont (Font (FontOptions (10.f, Font::bold)));
    for (size_t i = 0; i < knobs.size(); ++i) { auto* s = knobs[i]; g.setColour (s->isEnabled() ? cer::ink : cer::dis);
        g.drawText (knobNames[(int) i], s->getX() - 10, s->getY() - 16, s->getWidth() + 20, 14, Justification::centred); }
    g.setColour (cer::sep); g.drawVerticalLine (q.getRight() + 8, 10.f, (float) getHeight() - 10); g.drawVerticalLine (knee.getRight() + 8, 10.f, (float) getHeight() - 10);
}

// ================= Editor =================
TsrqEditor::TsrqEditor (TsrqProcessor& p) : AudioProcessorEditor (p), proc (p), graph (p), panel (p) {
    setLookAndFeel (&lnf);
    addAndMakeVisible (graph); addAndMakeVisible (panel);
    graph.onPrimaryChanged = [this] (int b) { panel.bind (b); };
    for (auto* b : { &bA, &bB, &bCopy, &bUndo, &bRedo, &bPresets, &bSettings, &bBypass }) addAndMakeVisible (*b);
    bA.setToggleState (true, dontSendNotification);
    bA.onClick = [this] { proc.switchAB (0); bA.setToggleState (true, dontSendNotification); bB.setToggleState (false, dontSendNotification); graph.selectOnly (-1); };
    bB.onClick = [this] { proc.switchAB (1); bB.setToggleState (true, dontSendNotification); bA.setToggleState (false, dontSendNotification); graph.selectOnly (-1); };
    bCopy.onClick = [this] { proc.copyCurrentToOther(); };
    bUndo.onClick = [this] { proc.undo.undo(); }; bRedo.onClick = [this] { proc.undo.redo(); };
    bPresets.onClick = [this] { showPresets(); }; bSettings.onClick = [this] { showSettings(); };
    bBypass.setClickingTogglesState (true); aBypass = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "bypass", bBypass);
    setResizable (true, true); setResizeLimits (900, 560, 2400, 1500); setSize (1180, 700);
    setWantsKeyboardFocus (true); startTimerHz (10);
}
TsrqEditor::~TsrqEditor() { setLookAndFeel (nullptr); }
void TsrqEditor::timerCallback() { bUndo.setEnabled (proc.undo.canUndo()); bRedo.setEnabled (proc.undo.canRedo()); }
void TsrqEditor::paint (Graphics& g) {
    g.setGradientFill (ColourGradient (cer::top, 0, 0, cer::bot, 0, (float) getHeight(), false)); g.fillAll();   // ceramica: niente texture
    g.setColour (cer::edge); g.drawRect (getLocalBounds(), 1);
    // logo TSR AUDIO + nome
    g.setColour (cer::ink); g.fillRoundedRectangle (18, 14, 4, 26, 2); g.fillRoundedRectangle (13, 27, 14, 5, 1.5f);
    g.setFont (Font (FontOptions (19.f, Font::bold))); g.drawText ("TSR", 34, 10, 60, 20, Justification::left);
    g.setFont (Font (FontOptions (9.5f, Font::bold)).withExtraKerningFactor (0.25f)); g.setColour (cer::dim); g.drawText ("AUDIO", 34, 29, 60, 12, Justification::left);
    g.setColour (cer::edge); g.fillRect (92, 14, 1, 26);
    g.setColour (cer::title); g.setFont (Font (FontOptions (17.f, Font::bold)).withExtraKerningFactor (0.3f)); g.drawText ("TSR Q", 104, 10, 120, 20, Justification::left);
    g.setColour (cer::dim); g.setFont (Font (FontOptions (9.5f, Font::bold)).withExtraKerningFactor (0.15f)); g.drawText ("PRECISION DYNAMIC EQ", 104, 29, 200, 12, Justification::left);
    g.setColour (cer::sep); g.drawHorizontalLine (52, 10, (float) getWidth() - 10);
}
void TsrqEditor::resized() {
    auto r = getLocalBounds(); auto top = r.removeFromTop (52).reduced (12, 12);
    auto put = [&] (Button& b, int w) { b.setBounds (top.removeFromRight (w)); top.removeFromRight (6); };
    put (bBypass, 76); put (bSettings, 86); put (bPresets, 74); top.removeFromRight (10); put (bRedo, 58); put (bUndo, 58); top.removeFromRight (10); put (bCopy, 48); put (bB, 30); put (bA, 30);
    auto bottom = r.removeFromBottom (170); panel.setBounds (bottom);
    graph.setBounds (r.reduced (10, 6));
}
bool TsrqEditor::keyPressed (const KeyPress& k) {
    if (k == KeyPress ('z', ModifierKeys::commandModifier, 0)) { proc.undo.undo(); return true; }
    if (k == KeyPress ('z', ModifierKeys::commandModifier | ModifierKeys::shiftModifier, 0)) { proc.undo.redo(); return true; }
    return graph.keyPressed (k);
}
void TsrqEditor::showPresets() {
    PopupMenu m;
    m.addItem ("Salva preset...", [this] {
        chooser = std::make_unique<FileChooser> ("Salva preset TSR Q", File::getSpecialLocation (File::userMusicDirectory).getChildFile ("TSR Q Presets"), "*.tsrq");
        chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles, [this] (const FileChooser& fc) {
            auto f = fc.getResult(); if (f == File()) return; f = f.withFileExtension ("tsrq"); f.getParentDirectory().createDirectory();
            if (auto xml = proc.apvts.copyState().createXml()) xml->writeTo (f); }); });
    m.addItem ("Carica preset...", [this] {
        chooser = std::make_unique<FileChooser> ("Carica preset TSR Q", File::getSpecialLocation (File::userMusicDirectory).getChildFile ("TSR Q Presets"), "*.tsrq");
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this] (const FileChooser& fc) {
            auto f = fc.getResult(); if (! f.existsAsFile()) return;
            if (auto xml = XmlDocument::parse (f)) if (xml->hasTagName (proc.apvts.state.getType())) { proc.undo.beginNewTransaction ("Preset"); proc.apvts.replaceState (ValueTree::fromXml (*xml)); graph.selectOnly (-1); } }); });
    m.addSeparator();
    m.addItem ("Init (tutte le bande off)", [this] { graph.selectAll(); graph.deleteSelected(); });
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&bPresets));
}
void TsrqEditor::showSettings() {
    PopupMenu m, res, dec, avg, rng, opt;
    for (int o : { 11, 12, 13 }) res.addItem (String (1 << o) + " punti", true, graph.fftOrder == o, [this, o] { graph.fftOrder = o; });
    m.addSubMenu ("Risoluzione FFT", res);
    for (auto [n, v] : { std::pair<const char*, float> { "Lento", 12.f }, { "Medio", 40.f }, { "Veloce", 120.f } }) dec.addItem (String (n) + " (" + String ((int) v) + " dB/s)", true, graph.decayDbPerSec == v, [this, v = v] { graph.decayDbPerSec = v; });
    m.addSubMenu ("Decadimento", dec);
    for (auto [n, v] : { std::pair<const char*, float> { "Nessuna", 0.f }, { "Breve", 0.5f }, { "Lunga", 0.85f } }) avg.addItem (n, true, graph.avgAlpha == v, [this, v = v] { graph.avgAlpha = v; });
    m.addSubMenu ("Media temporale", avg);
    m.addItem ("Spettro Input", true, graph.showPre, [this] { graph.showPre = ! graph.showPre; });
    m.addItem ("Spettro Output", true, graph.showPost, [this] { graph.showPost = ! graph.showPost; });
    m.addItem ("Peak Hold", true, graph.peakHold, [this] { graph.peakHold = ! graph.peakHold; graph.resetPeaks(); });
    m.addItem ("Freeze", true, graph.freeze, [this] { graph.freeze = ! graph.freeze; });
    m.addSeparator();
    for (int d : { 6, 12, 18, 30 }) rng.addItem ("±" + String (d) + " dB", true, graph.rangeDb == d, [this, d] { graph.rangeDb = d; });
    m.addSubMenu ("Scala grafico", rng);
    opt.addItem ("Attiva/disattiva dinamica", true, graph.optionClickDynamic, [this] { graph.optionClickDynamic = true; });
    opt.addItem ("Bypass banda", true, ! graph.optionClickDynamic, [this] { graph.optionClickDynamic = false; });
    m.addSubMenu ("Option + clic sul nodo", opt);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&bSettings));
}
