#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class ChainLinkButton final : public juce::ToggleButton
{
public:
    ChainLinkButton();

    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

private:
    void drawChain(juce::Graphics& g, juce::Rectangle<float> area, bool linked) const;
};

class PatternPieComponent final : public juce::Component
{
public:
    std::function<void(juce::Graphics&, juce::Rectangle<int>)> paintPie;
    std::function<void(juce::Point<float>, bool isDrag)> onPiePointer;
    std::function<void()> onPieRelease;

private:
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
};

class EuclidAudioProcessorEditor : public juce::AudioProcessorEditor,
                                   private juce::Timer
{
public:
    explicit EuclidAudioProcessorEditor(EuclidAudioProcessor& p);
    ~EuclidAudioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateStatusText();
    void drawPatternPie(juce::Graphics& g, juce::Rectangle<int> bounds);
    void handlePiePointer(juce::Point<float> pos, bool isDrag);
    int pulsesFromPieAngle(juce::Point<float> pos) const;

    void applySoothingKnobStyle(juce::Slider& slider);
    void applySoothingButtonStyle(juce::Button& button);
    void showScaleMenu();
    void syncStepsToGrid();
    void syncGridToSteps();
    void clampPulsesToSteps();
    void updatePulsesSliderRange();
    void setPulsesFromUI(int pulses);

    EuclidAudioProcessor& audioProcessor;
    juce::AudioProcessorValueTreeState& apvts;

    float currentScale = 1.0f;
    int baseWidth = 760;
    int baseHeight = 620;

    bool linkGridAndSteps = true;
    bool updatingLinkedSliders = false;
    bool pieDragActive = false;

    PatternPieComponent patternPie;
    ChainLinkButton linkGridStepsButton;

    juce::Slider gridSlider, stepsSlider, pulsesSlider, mixSlider, gainSmoothSlider;
    juce::Label gridLabel, stepsLabel, pulsesLabel, mixLabel, gainSmoothLabel;
    juce::TextButton titleButton { "Uclid" };
    juce::Label statusLabel;
    juce::Label footerLabel;
    juce::TextButton bypassButton { "Bypass" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gridAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> stepsAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pulsesAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainSmoothAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    juce::Rectangle<int> patternBounds;
    int footerHeight = 36;

    void setUIScale(float scale);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EuclidAudioProcessorEditor)
};
