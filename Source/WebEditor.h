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
    struct SelfTest { juce::File file; int step = 0, fails = 0; double t0 = 0; float outDb = -200; juce::String res[9], out; void* gl = nullptr; int sent0 = 0; double sentT = 0; };
    std::unique_ptr<SelfTest> selfTest;
    TsrqProcessor& proc;
    std::unique_ptr<juce::WebBrowserComponent> web;
    juce::String lastKnown; double lastUi = 0; int tick = 0; bool uiReady = false;
    bool gestOpen = false; juce::Array<juce::RangedAudioParameter*> gestParams;   // gesto di automazione aperto dall'interfaccia (trascinamento/rotellina)
    void endGesture();
  public:
    int gestBegins = 0, gestEnds = 0;   // contatori per l'autotest
  private:
    static constexpr int kRing = 1 << 15;                  // anello che basta per la FFT piu grande (32768)
    int fftOrder = 13, fftN = 1 << 13;                     // risoluzione scelta nell'interfaccia (1024 ... 32768 punti)
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> ringPre = std::vector<float> (kRing, 0.0f), ringPost = std::vector<float> (kRing, 0.0f), ringSC = std::vector<float> (kRing, 0.0f), win, work;
    int ringPos = 0, ringPosSC = 0;
    void setFftOrder (int o);
    void sendWave();
    int lastMidiVer = -1; int specSent = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TsrqWebEditor)
};
