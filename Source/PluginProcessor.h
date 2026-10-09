#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "TsrqDsp.h"

namespace ids {
    inline juce::String b (int i, const char* n) { return "b" + juce::String (i + 1) + "_" + n; }
    static constexpr const char* bandKeys[] = { "used", "byp", "type", "freq", "gain", "q", "slope", "place", "dyn", "thr", "range", "att", "rel", "knee", "det", "sc" };
    constexpr int numBandKeys = 16;
}
static const int kSlopes[] = { 6, 12, 18, 24, 30, 36, 48, 72, 96 };
constexpr int kNumSlopes = 9;

// FIFO lock-free mono per l'analizzatore (audio -> UI)
struct ScopeFifo {
    juce::AbstractFifo fifo { 1 << 15 };
    std::vector<float> buf = std::vector<float> (1 << 15, 0.0f);
    void push (const float* x, int n) {
        int s1, n1, s2, n2; fifo.prepareToWrite (n, s1, n1, s2, n2);
        if (n1 > 0) std::memcpy (buf.data() + s1, x, sizeof (float) * (size_t) n1);
        if (n2 > 0) std::memcpy (buf.data() + s2, x + n1, sizeof (float) * (size_t) n2);
        fifo.finishedWrite (n1 + n2);
    }
    int pull (float* out, int max) {
        int s1, n1, s2, n2; fifo.prepareToRead (std::min (max, fifo.getNumReady()), s1, n1, s2, n2);
        if (n1 > 0) std::memcpy (out, buf.data() + s1, sizeof (float) * (size_t) n1);
        if (n2 > 0) std::memcpy (out + n1, buf.data() + s2, sizeof (float) * (size_t) n2);
        fifo.finishedRead (n1 + n2); return n1 + n2;
    }
};

class TsrqProcessor : public juce::AudioProcessor, private juce::Timer {
public:
    TsrqProcessor();
    ~TsrqProcessor() override;
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "TSR Q"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    tsrq::BandParams readBand (int i) const;
    juce::RangedAudioParameter* bp (int i, const char* key) const { return apvts.getParameter (ids::b (i, key)); }
    void setBandParam (int i, const char* key, float plainValue);
    int firstFreeBand() const;
    double currentSampleRate() const { return sr.load(); }

    // A/B
    void switchAB (int slot);
    int currentAB() const { return abSlot; }
    void copyCurrentToOther();

    juce::UndoManager undo;
    juce::AudioProcessorValueTreeState apvts;
    tsrq::Engine engine;
    ScopeFifo scopePre, scopePost;
    std::atomic<int> solo { -1 };
    std::atomic<float> pkIn[2] {}, pkOut[2] {};          // picchi per i meter IN/OUT (l'interfaccia li legge e li azzera)
    static void maxInto (std::atomic<float>& a, float v) { float c = a.load (std::memory_order_relaxed); while (v > c && ! a.compare_exchange_weak (c, v, std::memory_order_relaxed)) {} }
    std::atomic<float> autoGainDb { 0.0f };

private:
    void timerCallback() override;      // auto gain sul thread dei messaggi
    std::atomic<float>* raw[tsrq::kMaxBands][ids::numBandKeys] {};
    std::atomic<float>* gIn = nullptr; std::atomic<float>* gOut = nullptr; std::atomic<float>* gScale = nullptr; std::atomic<float>* gAuto = nullptr;
    std::atomic<float>* gChar = nullptr; std::atomic<float>* gByp = nullptr;
    std::atomic<double> sr { 48000.0 };
    std::vector<float> monoTmp;
    juce::ValueTree abState[2]; int abSlot = 0;
    tsrq::Grid uiGrid;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TsrqProcessor)
};
