#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "Equalizer.h"

class EQ1Processor final : public juce::AudioProcessor, private juce::AsyncUpdater,
                                    private juce::AudioProcessorValueTreeState::Listener {
public:
    EQ1Processor();
    ~EQ1Processor() override;
    void prepareToPlay(double,int) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "EQ-01"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.25; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*,int) override;
    void updateTrackProperties(const TrackProperties&) override;
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    static juce::String id(int,const char*);
    void setValue(const juce::String&,float,bool gesture=true);
    float value(const juce::String&) const;
    curveshaper::Band band(int) const;
    int addBand(float frequency,float gain,curveshaper::Shape=curveshaper::Shape::Bell);
    void clearBands();
    void applyPreset(int);
    void copyBand(int);
    int pasteBand();
    bool pullSpectrum(float&,float&,float&,float&);
    struct Peer { uint64_t id; juce::String name; int bands; };
    std::vector<Peer> peers() const;
    bool copyFromPeer(uint64_t);
    juce::AudioProcessorValueTreeState parameters;
    std::array<std::atomic<float>,curveshaper::maxBands> reduction{};
    std::atomic<float> inputPeak{0},outputPeak{0};
    std::atomic<bool> sidechainConnected{false};
    std::atomic<int> audioMode{0};
    std::atomic<int> auditionBand{-1};
    std::atomic<bool> auditionDetector{false};
    double analysisSampleRate() const { return getSampleRate()>0?getSampleRate():48000; }
private:
    void handleAsyncUpdate() override;
    void parameterChanged(const juce::String&,float) override;
    static constexpr int fields=21;
    std::array<std::array<std::atomic<float>*,fields>,curveshaper::maxBands> cached{};
    std::array<std::atomic<float>*,8> globals{};
    std::unique_ptr<curveshaper::Engine> engine;
    std::unique_ptr<curveshaper::SpectralEngine> spectral;
    juce::dsp::Oversampling<float> oversampling{2,1,juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,true,true};
    juce::AudioBuffer<float> stereoWork,auditionWork;
    juce::LinearSmoothedValue<float> outputGain,bypassMix,width,drive,auditionMix;
    std::array<curveshaper::Biquad,2> auditionFilter{};
    int currentAudition=-1;
    std::array<std::array<float,8192>,2> delay{};
    int delayCursor=0,baseLatency=0;
    int currentMode=0,currentCharacter=0;
    int requestedLatency() const;
    std::array<float,16384> spectrumPre{},spectrumPost{},spectrumSideLeft{},spectrumSideRight{};
    std::atomic<unsigned> spectrumWrite{0},spectrumRead{0};
    void pushSpectrum(float,float,float,float);
    void render(juce::AudioBuffer<float>&,bool);
    juce::String trackName{"Untitled"};
    uint64_t instanceId=0;
    static juce::CriticalSection registryLock;
    static std::vector<EQ1Processor*> registry;
    static std::atomic<uint64_t> nextId;
    static curveshaper::Band clipboard;
    static bool clipboardValid;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EQ1Processor)
};
