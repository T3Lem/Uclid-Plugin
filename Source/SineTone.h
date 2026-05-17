#pragma once

#include <JuceHeader.h>

class SineTone final : public juce::AudioSource
{
public:
    void prepareToPlay(int, double sampleRate) override
    {
        sampleRateHz = sampleRate;
    }

    void releaseResources() override {}

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override
    {
        if (! playing)
        {
            bufferToFill.clearActiveBufferRegion();
            return;
        }

        const auto frequency = frequencyHz.load();
        const auto level = gain.load();
        const auto phaseIncrement = juce::MathConstants<double>::twoPi * frequency / sampleRateHz;

        for (int sample = 0; sample < bufferToFill.numSamples; ++sample)
        {
            const auto value = static_cast<float>(std::sin(phase) * level);
            phase += phaseIncrement;

            for (int channel = 0; channel < bufferToFill.buffer->getNumChannels(); ++channel)
                bufferToFill.buffer->setSample(channel, bufferToFill.startSample + sample, value);
        }
    }

    void setFrequencyHz(double hz) { frequencyHz.store(hz); }
    void setGain(float g) { gain.store(juce::jlimit(0.0f, 1.0f, g)); }
    void setPlaying(bool shouldPlay) { playing = shouldPlay; }

private:
    double sampleRateHz = 44100.0;
    double phase = 0.0;
    std::atomic<double> frequencyHz { 440.0 };
    std::atomic<float> gain { 0.2f };
    std::atomic<bool> playing { false };
};
