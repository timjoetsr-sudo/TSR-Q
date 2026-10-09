// TSR Q · editor del plugin: la stessa interfaccia del prototipo HTML, dentro una WebView (WKWebView su Mac)
#pragma once
#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>

class TsrqWebEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit TsrqWebEditor (TsrqProcessor&);
    ~TsrqWebEditor() override;
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff0a0b0c)); }
private:
    void timerCallback() override;
    void onUiState (const juce::var& v);          // l'interfaccia ha cambiato qualcosa → parametri
    juce::var stateFromParams() const;            // parametri → modello P dell'interfaccia
    void sendLoad();
    void sendSpectrum();
    void runSelfTest();
    struct SelfTest { juce::File file; int step = 0, fails = 0; double t0 = 0; float outDb = -200; juce::String res[8], out; };
    std::unique_ptr<SelfTest> selfTest;
    TsrqProcessor& proc;
    std::unique_ptr<juce::WebBrowserComponent> web;
    juce::String lastKnown; double lastUi = 0; int tick = 0; bool uiReady = false;
    static constexpr int fftOrder = 13, fftN = 1 << fftOrder;
    juce::dsp::FFT fft { fftOrder };
    std::vector<float> ringPre = std::vector<float> (fftN, 0.0f), ringPost = std::vector<float> (fftN, 0.0f), win, work;
    int ringPos = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TsrqWebEditor)
};
