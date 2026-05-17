#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <vector>

class EuclidAudioProcessor : public juce::AudioProcessor,
                             private juce::AudioProcessorValueTreeState::Listener
{
public:
    EuclidAudioProcessor();
    ~EuclidAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int index) override { juce::ignoreUnused(index); }
    const juce::String getProgramName(int index) override { juce::ignoreUnused(index); return {}; }
    void changeProgramName(int index, const juce::String& newName) override { juce::ignoreUnused(index, newName); }

    const juce::String getInputChannelName(int channelIndex) const override;
    const juce::String getOutputChannelName(int channelIndex) const override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    void updatePattern(int grid, int steps, int pulses);
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

    struct PatternSnapshot
    {
        std::vector<int> pattern;
        int currentStep = 0;
        float outputGain = 0.0f;
        bool isActive = false;
    };

    PatternSnapshot getPatternSnapshot() const;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameters();
    std::vector<int> generateEuclideanPattern(int pulses, int steps);

    void updateTiming(int grid, int steps);
    void beginGainRamp(float newTarget);
    void clampPulsesToStepsParam();
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    juce::AudioProcessorValueTreeState apvts;

    float currentGain = 1.0f;
    float targetGain = 1.0f;
    float rampStartGain = 1.0f;
    float rampTargetGain = 1.0f;
    int rampSamplesRemaining = 0;
    int rampTotalSamples = 0;

    float samplesPerStep = 256.0f;
    float sampleCounter = 0.0f;
    std::vector<int> pattern;
    int stepIndex = 0;
    std::atomic<int> uiStepIndex { 0 };
    std::atomic<float> uiOutputGain { 1.0f };

    double currentSampleRate = 44100.0;
    double currentBPM = 120.0;

    juce::AudioParameterFloat* mixParam = nullptr;
    juce::AudioParameterInt* gridParam = nullptr;
    juce::AudioParameterInt* stepsParam = nullptr;
    juce::AudioParameterInt* pulsesParam = nullptr;
    juce::AudioParameterBool* bypassParam = nullptr;
    juce::AudioParameterFloat* smoothMsParam = nullptr;

    int cachedGrid = -1;
    int cachedSteps = -1;
    int cachedPulses = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EuclidAudioProcessor)
};
