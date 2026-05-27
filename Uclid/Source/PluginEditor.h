#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

/** Forwards right-clicks to the VST3 host parameter menu (FL Studio automation, MIDI learn, etc.). */
class ComponentWithParamMenu : public juce::Component
{
public:
    ComponentWithParamMenu(juce::AudioProcessorEditor& editorIn, juce::RangedAudioParameter& paramIn);

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

    int getAttachedParameterIndex() const;

protected:
    void showHostParameterContextMenu();

    juce::AudioProcessorEditor& editor;
    juce::RangedAudioParameter& param;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ComponentWithParamMenu)
};

class HostAwareRotaryKnob final : public ComponentWithParamMenu
{
public:
    HostAwareRotaryKnob(juce::AudioProcessorEditor& editorIn,
                        juce::AudioProcessorValueTreeState& apvtsIn,
                        const juce::String& paramId,
                        juce::RangedAudioParameter& paramIn);

    juce::Slider& getSlider() noexcept { return slider; }
    const juce::Slider& getSlider() const noexcept { return slider; }

    void resized() override;

private:
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HostAwareRotaryKnob)
};

class ChainLinkButton final : public juce::ToggleButton
{
public:
    ChainLinkButton();

    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

private:
    void drawChain(juce::Graphics& g, juce::Rectangle<float> area, bool linked) const;
};

class HostAwareChainLink final : public ComponentWithParamMenu
{
public:
    HostAwareChainLink(juce::AudioProcessorEditor& editorIn,
                       juce::AudioProcessorValueTreeState& apvtsIn,
                       juce::RangedAudioParameter& paramIn);

    ChainLinkButton& getButton() noexcept { return linkButton; }
    const ChainLinkButton& getButton() const noexcept { return linkButton; }

    void resized() override;

private:
    ChainLinkButton linkButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HostAwareChainLink)
};

class PatternPieComponent final : public juce::Component
{
public:
    std::function<void(juce::Graphics&, juce::Rectangle<int>)> paintPie;
    std::function<void(const juce::MouseEvent&)> onPieMouseDown;
    std::function<void(const juce::MouseEvent&)> onPieMouseDrag;
    std::function<void(const juce::MouseEvent&)> onPieMouseUp;

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

    int getControlParameterIndex(juce::Component& component) override;

private:
    struct PieLayout
    {
        juce::Point<float> centre;
        float outerRadius = 0.0f;
        float innerRadius = 0.0f;
        int numSteps = 0;
        float gap = 0.0f;
    };

    enum class PieGesture
    {
        none,
        volumeDrag,
        pulsesDrag
    };

    static ComponentWithParamMenu* findParentWithParamMenu(juce::Component* component);

    void timerCallback() override;
    void drawTitleLogo(juce::Graphics& g) const;
    void drawPatternPie(juce::Graphics& g, juce::Rectangle<int> bounds);
    void handlePieMouseDown(const juce::MouseEvent& e);
    void handlePieMouseDrag(const juce::MouseEvent& e);
    void handlePieMouseUp(const juce::MouseEvent& e);

    PieLayout getPieLayout(juce::Rectangle<int> bounds) const;
    int stepIndexAtPoint(juce::Point<float> pos, const PieLayout& layout) const;
    bool isInHub(juce::Point<float> pos, const PieLayout& layout) const;
    int pulsesFromPieAngle(juce::Point<float> pos) const;
    void setPulsesFromUI(int pulses, bool beginGesture, bool endGesture);

    void applySoothingKnobStyle(juce::Slider& slider);
    void applySoothingButtonStyle(juce::Button& button);
    void showScaleMenu();
    bool isLinkGridAndSteps() const;
    void syncStepsToGrid();
    void syncGridToSteps();
    void clampPulsesToSteps();
    void updatePulsesSliderRange();
    void updateResetPatternButtonState();
    void centrePluginWindow();

    EuclidAudioProcessor& audioProcessor;
    juce::AudioProcessorValueTreeState& apvts;

    juce::RangedAudioParameter* gridParam = nullptr;
    juce::RangedAudioParameter* stepsParam = nullptr;
    juce::RangedAudioParameter* pulsesParam = nullptr;

    float currentScale = 1.0f;
    int baseWidth = 480;
    int baseHeight = 640;

    bool updatingLinkedSliders = false;
    bool suppressSliderCallbacks = false;
    bool pulsesHostGestureActive = false;

    PatternPieComponent patternPie;
    HostAwareRotaryKnob gridKnob;
    HostAwareChainLink linkGridStepsControl;
    HostAwareRotaryKnob stepsKnob;
    HostAwareRotaryKnob pulsesKnob;

    juce::Slider mixSlider, gainSmoothSlider;
    juce::Label gridLabel, stepsLabel, pulsesLabel, mixLabel, gainSmoothLabel;
    juce::TextButton titleButton { "" };
    juce::Label footerLabel;
    juce::TextButton bypassButton { "Bypass" };
    juce::TextButton resetPatternButton { "Reset pattern" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainSmoothAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    juce::Rectangle<int> patternBounds;
    int footerHeight = 36;

    PieGesture pieGesture = PieGesture::none;
    int activeStepIndex = -1;
    int selectedStepIndex = -1;
    juce::Point<float> piePointerDown;
    float velocityAtDragStart = 1.0f;
    float dragVolumePercent = -1.0f;

    static constexpr float kVelocityDragRangePx = 110.0f;

    int windowFixAttempts = 0;
    bool windowShownFixApplied = false;

    void setUIScale(float scale);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EuclidAudioProcessorEditor)
};
