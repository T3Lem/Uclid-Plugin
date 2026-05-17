#pragma once

#include <JuceHeader.h>
#include "SineTone.h"

class MainComponent final : public juce::AudioAppComponent
{
public:
    MainComponent();
    ~MainComponent() override;

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void updateTone();

    SineTone sineTone;

    juce::Label titleLabel;
    juce::ToggleButton playButton { "Play tone" };
    juce::Slider frequencySlider;
    juce::Label frequencyLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
