#include "MainComponent.h"

MainComponent::MainComponent()
{
    setAudioChannels(0, 2);

    addAndMakeVisible(titleLabel);
    titleLabel.setText("CRSR — JUCE audio starter", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setFont(juce::FontOptions(20.0f, juce::Font::bold));

    addAndMakeVisible(playButton);
    playButton.onClick = [this] { updateTone(); };

    frequencySlider.setRange(110.0, 1760.0, 1.0);
    frequencySlider.setValue(440.0);
    frequencySlider.setTextValueSuffix(" Hz");
    frequencySlider.onValueChange = [this] { updateTone(); };
    addAndMakeVisible(frequencySlider);

    addAndMakeVisible(frequencyLabel);
    frequencyLabel.setText("Frequency", juce::dontSendNotification);
    frequencyLabel.attachToComponent(&frequencySlider, true);

    updateTone();
    setSize(640, 360);
}

MainComponent::~MainComponent()
{
    shutdownAudio();
}

void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    sineTone.prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    sineTone.getNextAudioBlock(bufferToFill);
}

void MainComponent::releaseResources()
{
    sineTone.releaseResources();
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(24);
    titleLabel.setBounds(area.removeFromTop(36));
    area.removeFromTop(16);
    playButton.setBounds(area.removeFromTop(32).withWidth(140));
    area.removeFromTop(24);
    frequencySlider.setBounds(area.removeFromTop(28).withTrimmedLeft(100));
}

void MainComponent::updateTone()
{
    sineTone.setFrequencyHz(frequencySlider.getValue());
    sineTone.setPlaying(playButton.getToggleState());
}
