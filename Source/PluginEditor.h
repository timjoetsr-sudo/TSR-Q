#pragma once
#include "PluginProcessor.h"
#include <set>

struct TsrqLnf : juce::LookAndFeel_V4 {
    TsrqLnf();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

class EqGraph : public juce::Component, private juce::Timer {
public:
    explicit EqGraph (TsrqProcessor&);
    ~EqGraph() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

    std::function<void (int)> onPrimaryChanged;
    int primary = -1;
    std::set<int> sel;
    // impostazioni analizzatore / vista
    int fftOrder = 12; float decayDbPerSec = 40.f; float avgAlpha = 0.5f; bool peakHold = false, freeze = false, showPre = true, showPost = true;
    int rangeDb = 12; bool optionClickDynamic = true;
    void resetPeaks() { std::fill (peak.begin(), peak.end(), -200.f); }
    void selectOnly (int i);
    void selectAll();
    void deleteSelected();
    int createBand (double f, double g, int type = tsrq::Bell);
    static juce::Colour bandColour (int i);

private:
    void timerCallback() override;
    void updateSpectrum();
    juce::Rectangle<float> plot() const;
    float X (double f) const; double Fx (float x) const; float Y (double db) const; double Dy (float y) const; float AY (double d) const;
    double fMax() const;
    int hitNode (juce::Point<float> p) const;
    juce::Point<float> nodePos (int i) const;
    void refreshCurves();
    void showBandMenu (int band);
    void showEmptyMenu (juce::Point<float> p);
    void applyToSelected (const char* key, float v);
    float specAt (float x) const;

    TsrqProcessor& proc;
    tsrq::Grid grid;
    tsrq::BandParams cacheP[tsrq::kMaxBands]; float cacheDelta[tsrq::kMaxBands] {}; double cacheScale = -999, cacheFs = 0;
    std::vector<float> curveDb[tsrq::kMaxBands], totalDb, liveDb, colF; std::vector<double> cw, c2w;   // coseni in double: in float la curva sotto ~300 Hz e' rumorosa
    bool anyDyn = false;
    // spettro
    std::unique_ptr<juce::dsp::FFT> fft; int fftN = 0;
    std::vector<float> ringPre, ringPost, work, win, specPre, specPost, peak, pullTmp; int ringPos = 0, newSamples = 0;
    // interazione
    struct DragStart { int band; double f, g; };
    std::vector<DragStart> dragStart; bool dragging = false, moved = false; juce::Point<float> downPos;
    bool rectSel = false; juce::Rectangle<float> rect; std::set<int> rectKeep;
    int hover = -1; double grabF = -1; juce::Point<float> mouse { -1, -1 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EqGraph)
};

class BandPanel : public juce::Component {
public:
    explicit BandPanel (TsrqProcessor&);
    void bind (int band);
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    TsrqProcessor& proc; int band = -1;
    juce::Slider freq, gain, q, thr, range, att, rel, knee, scale, out;
    juce::ComboBox type, slope, place, det, character;
    juce::TextButton dyn { "DYN" }, sc { "SIDECHAIN" }, byp { "BYPASS" }, autoGain { "AUTO GAIN" };
    std::unique_ptr<SA> aFreq, aGain, aQ, aThr, aRange, aAtt, aRel, aKnee; std::unique_ptr<CA> aType, aSlope, aPlace, aDet; std::unique_ptr<BA> aDyn, aSc, aByp;
    std::unique_ptr<SA> aScale, aOut; std::unique_ptr<CA> aChar; std::unique_ptr<BA> aAuto;
    juce::Label title;
    void knob (juce::Slider&, const juce::String& name);
    juce::StringArray knobNames;
    std::vector<juce::Slider*> knobs;
};

class TsrqEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit TsrqEditor (TsrqProcessor&);
    ~TsrqEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
private:
    void timerCallback() override;
    void showPresets(); void showSettings();
    TsrqProcessor& proc; TsrqLnf lnf;
    EqGraph graph; BandPanel panel;
    juce::TextButton bA { "A" }, bB { "B" }, bCopy { "A>B" }, bUndo { "UNDO" }, bRedo { "REDO" }, bPresets { "PRESET" }, bSettings { "SETTINGS" }, bBypass { "BYPASS" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> aBypass;
    std::unique_ptr<juce::FileChooser> chooser;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TsrqEditor)
};
