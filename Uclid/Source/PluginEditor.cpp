#include "PluginEditor.h"

namespace
{
    namespace Theme
    {
        const juce::Colour background     { 0xff0e1218 };
        const juce::Colour panel          { 0xff161d27 };
        const juce::Colour panelBorder    { 0xff2a3544 };
        const juce::Colour textPrimary    { 0xffe8eef4 };
        const juce::Colour textMuted      { 0xff9aa8b8 };
        const juce::Colour textLink       { 0xff8eb8c4 };
        const juce::Colour stepOnHi       { 0xffe8c9a0 };
        const juce::Colour stepOnLo       { 0xffc4956a };
        const juce::Colour stepOff        { 0xff232c38 };
        const juce::Colour stepOffEdge    { 0xff2f3a4a };
        const juce::Colour stepPlayhead   { 0xfff5f0e8 };
        const juce::Colour accentSoft     { 0xff8eb8c4 };
        const juce::Colour chainLinked    { 0xffc9a66b };
        const juce::Colour chainSplit     { 0xff6b7a8c };
        const juce::Colour buttonBg       { 0xff243040 };
        const juce::Colour buttonBgOn     { 0xff3d5168 };
        const juce::Colour knobFill       { 0xffb8956e };
        const juce::Colour knobTrack      { 0xff2c3644 };
    }

    juce::Path makeAnnularWedge(juce::Point<float> centre,
                                float innerR,
                                float outerR,
                                float startRad,
                                float endRad)
    {
        juce::Path p;
        p.addCentredArc(centre.x, centre.y, outerR, outerR, 0.0f, startRad, endRad, true);
        p.addCentredArc(centre.x, centre.y, innerR, innerR, 0.0f, endRad, startRad, false);
        p.closeSubPath();
        return p;
    }
}

// -----------------------------------------------------------------------------
ChainLinkButton::ChainLinkButton()
{
    setClickingTogglesState(true);
    setToggleState(true, juce::dontSendNotification);
    setTooltip("Link Grid and Steps");
}

void ChainLinkButton::paintButton(juce::Graphics& g, bool highlighted, bool)
{
    auto area = getLocalBounds().toFloat().reduced(3.0f);
    drawChain(g, area, getToggleState());

    if (highlighted)
    {
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.0f);
    }
}

void ChainLinkButton::drawChain(juce::Graphics& g, juce::Rectangle<float> area, bool linked) const
{
    const float linkW = area.getWidth() * 0.42f;
    const float linkH = area.getHeight() * 0.62f;
    const float stroke = juce::jmax(1.6f, area.getWidth() * 0.11f);

    auto drawLink = [&](float cx, float cy, float tilt)
    {
        juce::Path link;
        link.addCentredArc(cx, cy, linkW * 0.5f, linkH * 0.5f, tilt, 0.0f, juce::MathConstants<float>::twoPi, true);
        g.strokePath(link, juce::PathStrokeType(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };

    if (linked)
    {
        g.setColour(Theme::chainLinked);
        drawLink(area.getCentreX() - linkW * 0.22f, area.getCentreY(), 0.55f);
        drawLink(area.getCentreX() + linkW * 0.22f, area.getCentreY(), -0.55f);
    }
    else
    {
        g.setColour(Theme::chainSplit);
        drawLink(area.getCentreX() - linkW * 0.28f, area.getCentreY() - linkH * 0.12f, 0.35f);
        drawLink(area.getCentreX() + linkW * 0.28f, area.getCentreY() + linkH * 0.12f, -0.35f);
        g.drawLine(area.getCentreX() - 2.0f, area.getCentreY() + 2.0f,
                   area.getCentreX() + 2.0f, area.getCentreY() - 2.0f, 1.2f);
    }
}

// -----------------------------------------------------------------------------
void PatternPieComponent::paint(juce::Graphics& g)
{
    if (paintPie)
        paintPie(g, getLocalBounds());
}

void PatternPieComponent::mouseDown(const juce::MouseEvent& e)
{
    if (onPiePointer)
        onPiePointer(e.position, false);
}

void PatternPieComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (onPiePointer)
        onPiePointer(e.position, true);
}

void PatternPieComponent::mouseUp(const juce::MouseEvent&)
{
    if (onPieRelease)
        onPieRelease();
}

// -----------------------------------------------------------------------------
EuclidAudioProcessorEditor::EuclidAudioProcessorEditor(EuclidAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      apvts(p.getAPVTS())
{
    setResizable(true, true);
    setResizeLimits(520, 420, 1600, 1000);

    gridAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "grid", gridSlider);
    stepsAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "steps", stepsSlider);
    pulsesAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "pulses", pulsesSlider);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "mix", mixSlider);
    gainSmoothAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "smoothMs", gainSmoothSlider);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "bypass", bypassButton);

    patternPie.paintPie = [this](juce::Graphics& g, juce::Rectangle<int> b) { drawPatternPie(g, b); };
    patternPie.onPiePointer = [this](juce::Point<float> pos, bool isDrag) { handlePiePointer(pos, isDrag); };
    patternPie.onPieRelease = [this]()
    {
        pieDragActive = false;
        updateStatusText();
    };
    addAndMakeVisible(patternPie);

    auto setupKnob = [this](juce::Slider& slider, const juce::String& tooltip, bool isInteger)
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 56, 18);
        slider.setTooltip(tooltip);
        applySoothingKnobStyle(slider);
        if (isInteger)
            slider.setNumDecimalPlacesToDisplay(0);
        addAndMakeVisible(slider);
    };

    setupKnob(gridSlider, "Grid divisions per bar", true);
    setupKnob(stepsSlider, "Pattern length", true);
    setupKnob(pulsesSlider, "Pulses (or drag the circle)", true);
    setupKnob(gainSmoothSlider, "De-click ramp (ms). 0 = off.", false);

    gainSmoothSlider.setNumDecimalPlacesToDisplay(2);
    gainSmoothSlider.textFromValueFunction = [](double v)
    {
        return v < 0.001 ? juce::String("Off") : juce::String(v, 2) + " ms";
    };

    setupKnob(mixSlider, "Dry/wet mix", false);
    mixSlider.textFromValueFunction = [](double v) { return juce::String(juce::roundToInt(v * 100.0)) + "%"; };
    mixSlider.valueFromTextFunction = [](const juce::String& t)
    {
        return t.retainCharacters("0123456789").getIntValue() / 100.0;
    };

    auto setupLabel = [this](juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setFont(juce::FontOptions(12.5f));
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId, Theme::textMuted);
        addAndMakeVisible(label);
    };

    setupLabel(gridLabel, "Grid");
    setupLabel(stepsLabel, "Steps");
    setupLabel(pulsesLabel, "Pulses");
    setupLabel(gainSmoothLabel, "Smooth");
    setupLabel(mixLabel, "Mix");

    titleButton.setButtonText("Uclid");
    titleButton.setTooltip("Click for UI scale");
    titleButton.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    titleButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    titleButton.setColour(juce::TextButton::textColourOffId, Theme::textPrimary);
    titleButton.setColour(juce::TextButton::textColourOnId, Theme::textPrimary);
    titleButton.onClick = [this] { showScaleMenu(); };
    addAndMakeVisible(titleButton);

    statusLabel.setFont(juce::FontOptions(13.0f));
    statusLabel.setJustificationType(juce::Justification::centredLeft);
    statusLabel.setColour(juce::Label::textColourId, Theme::textMuted);
    addAndMakeVisible(statusLabel);

    footerLabel.setText("Uclid  ·  github.com/yodem/uclid-plugin", juce::dontSendNotification);
    footerLabel.setFont(juce::FontOptions(12.0f));
    footerLabel.setJustificationType(juce::Justification::centred);
    footerLabel.setColour(juce::Label::textColourId, Theme::textLink);
    footerLabel.setTooltip("https://github.com/yodem/uclid-plugin");
    addAndMakeVisible(footerLabel);

    bypassButton.setClickingTogglesState(true);
    bypassButton.setTooltip("Bypass");
    applySoothingButtonStyle(bypassButton);
    addAndMakeVisible(bypassButton);

    addAndMakeVisible(linkGridStepsButton);
    linkGridStepsButton.onClick = [this]()
    {
        linkGridAndSteps = linkGridStepsButton.getToggleState();
        if (linkGridAndSteps)
            syncStepsToGrid();
        updatePulsesSliderRange();
        updateStatusText();
    };

    gridSlider.onValueChange = [this]()
    {
        if (linkGridAndSteps)
            syncStepsToGrid();
        updatePulsesSliderRange();
        updateStatusText();
        patternPie.repaint();
    };

    stepsSlider.onValueChange = [this]()
    {
        if (linkGridAndSteps)
            syncGridToSteps();
        updatePulsesSliderRange();
        updateStatusText();
        patternPie.repaint();
    };

    if (linkGridAndSteps && stepsSlider.getValue() != gridSlider.getValue())
        syncStepsToGrid();

    updatePulsesSliderRange();

    pulsesSlider.onValueChange = [this]()
    {
        clampPulsesToSteps();
        updateStatusText();
        patternPie.repaint();
    };

    mixSlider.onValueChange = [this]() { updateStatusText(); };
    bypassButton.onClick = [this]() { updateStatusText(); };

    setUIScale(1.0f);
    startTimerHz(30);
}

EuclidAudioProcessorEditor::~EuclidAudioProcessorEditor()
{
    stopTimer();
}

void EuclidAudioProcessorEditor::showScaleMenu()
{
    juce::PopupMenu menu;
    menu.addItem("150%", [this]() { setUIScale(1.5f); });
    menu.addItem("125%", [this]() { setUIScale(1.25f); });
    menu.addItem("100%", [this]() { setUIScale(1.0f); });
    menu.addItem("75%", [this]() { setUIScale(0.75f); });
    menu.addItem("60%", [this]() { setUIScale(0.6f); });
    menu.addItem("50%", [this]() { setUIScale(0.5f); });
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&titleButton));
}

void EuclidAudioProcessorEditor::setPulsesFromUI(int pulses)
{
    const int steps = juce::roundToInt(stepsSlider.getValue());
    pulsesSlider.setValue(static_cast<double>(juce::jlimit(0, steps, pulses)), juce::sendNotificationSync);
    patternPie.repaint();
}

void EuclidAudioProcessorEditor::handlePiePointer(juce::Point<float> pos, bool isDrag)
{
    pieDragActive = isDrag;
    setPulsesFromUI(pulsesFromPieAngle(pos));
}

int EuclidAudioProcessorEditor::pulsesFromPieAngle(juce::Point<float> pos) const
{
    const auto bounds = patternPie.getLocalBounds();
    if (bounds.isEmpty())
        return juce::roundToInt(pulsesSlider.getValue());

    const auto centre = bounds.getCentre().toFloat();
    const float dx = pos.x - centre.x;
    const float dy = pos.y - centre.y;
    const float dist = std::sqrt(dx * dx + dy * dy);

    const float outerR = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f - 6.0f;
    const float innerR = outerR * 0.36f;

    if (dist < innerR * 0.85f || dist > outerR * 1.05f)
        return juce::roundToInt(pulsesSlider.getValue());

    float angle = std::atan2(dy, dx) + juce::MathConstants<float>::halfPi;
    if (angle < 0.0f)
        angle += juce::MathConstants<float>::twoPi;

    const int steps = juce::jmax(1, juce::roundToInt(stepsSlider.getValue()));
    const int pulses = static_cast<int>(std::round(angle / juce::MathConstants<float>::twoPi * static_cast<float>(steps)));
    return juce::jlimit(0, steps, pulses);
}

void EuclidAudioProcessorEditor::applySoothingKnobStyle(juce::Slider& slider)
{
    slider.setColour(juce::Slider::rotarySliderFillColourId, Theme::knobFill.withAlpha(0.85f));
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, Theme::knobTrack);
    slider.setColour(juce::Slider::thumbColourId, Theme::stepOnHi);
    slider.setColour(juce::Slider::textBoxTextColourId, Theme::textPrimary);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, Theme::panel.withAlpha(0.6f));
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}

void EuclidAudioProcessorEditor::applySoothingButtonStyle(juce::Button& button)
{
    button.setColour(juce::TextButton::buttonColourId, Theme::buttonBg);
    button.setColour(juce::TextButton::buttonOnColourId, Theme::buttonBgOn);
    button.setColour(juce::TextButton::textColourOffId, Theme::textMuted);
    button.setColour(juce::TextButton::textColourOnId, Theme::textPrimary);
}

void EuclidAudioProcessorEditor::timerCallback()
{
    updateStatusText();
    patternPie.repaint();
}

void EuclidAudioProcessorEditor::updateStatusText()
{
    const int pulses = juce::roundToInt(pulsesSlider.getValue());
    const int steps = juce::roundToInt(stepsSlider.getValue());
    const int grid = juce::roundToInt(gridSlider.getValue());
    const auto snapshot = audioProcessor.getPatternSnapshot();

    juce::String status = "E(" + juce::String(pulses) + "," + juce::String(steps) + ")";
    if (linkGridAndSteps)
        status += "  ·  Length " + juce::String(grid);
    else
        status += "  ·  Grid " + juce::String(grid) + "  Steps " + juce::String(steps);

    if (pieDragActive)
        status += "  ·  Drag ring to set pulses";

    if (snapshot.pattern.empty())
        status += "  ·  (empty)";
    else if (bypassButton.getToggleState())
        status += "  ·  Bypassed";
    else if (mixSlider.getValue() <= 0.001)
        status += "  ·  Mix 0%";
    else
        status += "  ·  Step " + juce::String(snapshot.currentStep + 1) + "/" + juce::String(snapshot.pattern.size())
                  + "  ·  " + juce::String(juce::roundToInt(snapshot.outputGain * 100.0f)) + "%";

    statusLabel.setText(status, juce::dontSendNotification);
}

void EuclidAudioProcessorEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient bg(Theme::panel, 0.0f, 0.0f, Theme::background, 0.0f, static_cast<float>(getHeight()), false);
    g.setGradientFill(bg);
    g.fillAll();

    auto header = getLocalBounds().removeFromTop(static_cast<int>(52 * currentScale));
    g.setColour(Theme::panel.withAlpha(0.92f));
    g.fillRect(header);
    g.setColour(Theme::panelBorder.withAlpha(0.5f));
    g.fillRect(header.getX(), header.getBottom() - 1, header.getWidth(), 1);

    auto footer = getLocalBounds().removeFromBottom(static_cast<int>(footerHeight * currentScale));
    g.setColour(Theme::panel.withAlpha(0.85f));
    g.fillRect(footer);
    g.setColour(Theme::panelBorder.withAlpha(0.45f));
    g.fillRect(footer.getX(), footer.getY(), footer.getWidth(), 1);
}

void EuclidAudioProcessorEditor::syncStepsToGrid()
{
    if (updatingLinkedSliders)
        return;
    updatingLinkedSliders = true;
    stepsSlider.setValue(gridSlider.getValue(), juce::sendNotificationSync);
    updatingLinkedSliders = false;
}

void EuclidAudioProcessorEditor::syncGridToSteps()
{
    if (updatingLinkedSliders)
        return;
    updatingLinkedSliders = true;
    gridSlider.setValue(stepsSlider.getValue(), juce::sendNotificationSync);
    updatingLinkedSliders = false;
}

void EuclidAudioProcessorEditor::updatePulsesSliderRange()
{
    const int steps = juce::jmax(0, juce::roundToInt(stepsSlider.getValue()));
    pulsesSlider.setRange(0, steps, 1);
    clampPulsesToSteps();
}

void EuclidAudioProcessorEditor::clampPulsesToSteps()
{
    const int steps = juce::roundToInt(stepsSlider.getValue());
    if (pulsesSlider.getValue() > steps)
        pulsesSlider.setValue(static_cast<double>(steps), juce::sendNotificationSync);
}

void EuclidAudioProcessorEditor::drawPatternPie(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    if (bounds.isEmpty())
        return;

    patternBounds = bounds;

    const auto centre = bounds.getCentre().toFloat();
    const float outerRadius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f - 6.0f;
    const float innerRadius = outerRadius * 0.36f;

    g.setColour(Theme::panel);
    g.fillEllipse(centre.x - outerRadius - 6.0f, centre.y - outerRadius - 6.0f,
                  (outerRadius + 6.0f) * 2.0f, (outerRadius + 6.0f) * 2.0f);
    g.setColour(Theme::panelBorder.withAlpha(0.55f));
    g.drawEllipse(centre.x - outerRadius - 6.0f, centre.y - outerRadius - 6.0f,
                  (outerRadius + 6.0f) * 2.0f, (outerRadius + 6.0f) * 2.0f, 1.2f);

    const auto snapshot = audioProcessor.getPatternSnapshot();
    if (snapshot.pattern.empty())
    {
        g.setColour(Theme::textMuted);
        g.setFont(juce::FontOptions(14.0f));
        g.drawText("No pattern", bounds, juce::Justification::centred);
        return;
    }

    const int numSteps = static_cast<int>(snapshot.pattern.size());
    const float gap = juce::jlimit(0.003f, 0.035f, 0.32f / static_cast<float>(numSteps));

    for (int i = 0; i < numSteps; ++i)
    {
        const float start = juce::MathConstants<float>::twoPi * static_cast<float>(i) / static_cast<float>(numSteps)
                            - juce::MathConstants<float>::halfPi + gap;
        const float end = juce::MathConstants<float>::twoPi * static_cast<float>(i + 1) / static_cast<float>(numSteps)
                          - juce::MathConstants<float>::halfPi - gap;

        const bool isOn = snapshot.pattern[static_cast<size_t>(i)] != 0;
        const bool isCurrent = snapshot.isActive && i == snapshot.currentStep;

        auto wedge = makeAnnularWedge(centre, innerRadius, outerRadius, start, end);

        if (isOn)
        {
            juce::ColourGradient fill(Theme::stepOnHi.brighter(0.1f), centre.x, centre.y - outerRadius,
                                      Theme::stepOnLo, centre.x, centre.y + outerRadius, false);
            g.setGradientFill(fill);
        }
        else
        {
            g.setColour(Theme::stepOff);
        }

        g.fillPath(wedge);
        g.setColour(isOn ? Theme::stepOnLo.withAlpha(0.35f) : Theme::stepOffEdge);
        g.strokePath(wedge, juce::PathStrokeType(0.9f));

        if (isCurrent)
        {
            g.setColour(Theme::stepPlayhead.withAlpha(0.95f));
            g.strokePath(wedge, juce::PathStrokeType(2.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    g.setColour(Theme::background);
    g.fillEllipse(centre.x - innerRadius + 2.0f, centre.y - innerRadius + 2.0f,
                  (innerRadius - 2.0f) * 2.0f, (innerRadius - 2.0f) * 2.0f);
    g.setColour(Theme::panelBorder.withAlpha(0.45f));
    g.drawEllipse(centre.x - innerRadius + 2.0f, centre.y - innerRadius + 2.0f,
                  (innerRadius - 2.0f) * 2.0f, (innerRadius - 2.0f) * 2.0f, 1.0f);

    g.setColour(Theme::textMuted.withAlpha(0.85f));
    g.setFont(juce::FontOptions(11.0f));
    g.drawText("drag ring", juce::Rectangle<int>(static_cast<int>(centre.x - 28), static_cast<int>(centre.y - 6), 56, 14),
               juce::Justification::centred);

    if (snapshot.isActive && snapshot.outputGain > 0.001f)
    {
        const float hubR = innerRadius * 0.7f;
        const float gainAngle = juce::MathConstants<float>::twoPi * snapshot.outputGain;
        juce::Path gainArc;
        gainArc.addCentredArc(centre.x, centre.y, hubR, hubR, 0.0f,
                              -juce::MathConstants<float>::halfPi,
                              -juce::MathConstants<float>::halfPi + gainAngle, true);
        g.setColour(Theme::accentSoft.withAlpha(0.9f));
        g.strokePath(gainArc, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

void EuclidAudioProcessorEditor::setUIScale(float scale)
{
    currentScale = juce::jlimit(0.5f, 1.5f, scale);
    titleButton.setButtonText("Uclid");
    setSize(static_cast<int>(baseWidth * currentScale), static_cast<int>(baseHeight * currentScale));
    resized();
}

void EuclidAudioProcessorEditor::resized()
{
    const int margin = static_cast<int>(14 * currentScale);
    const int headerH = static_cast<int>(52 * currentScale);
    const int knobSize = static_cast<int>(72 * currentScale);
    const int mixKnobSize = static_cast<int>(64 * currentScale);
    const int labelH = static_cast<int>(16 * currentScale);
    const int buttonH = static_cast<int>(28 * currentScale);
    const int footerH = static_cast<int>(footerHeight * currentScale);
    const int chainSize = static_cast<int>(30 * currentScale);

    auto bounds = getLocalBounds();

    auto footer = bounds.removeFromBottom(footerH);
    footerLabel.setBounds(footer.reduced(margin, 4));

    auto header = bounds.removeFromTop(headerH);
    titleButton.setBounds(header.getX() + margin, header.getY(), static_cast<int>(130 * currentScale), header.getHeight());

    bypassButton.setBounds(header.getRight() - margin - static_cast<int>(76 * currentScale),
                           header.getY() + (header.getHeight() - buttonH) / 2,
                           static_cast<int>(76 * currentScale),
                           buttonH);

    statusLabel.setBounds(titleButton.getRight() + margin,
                          header.getY(),
                          bypassButton.getX() - titleButton.getRight() - margin * 2,
                          header.getHeight());

    const int knobRowH = knobSize + labelH + margin;
    bounds.removeFromBottom(knobRowH);

    auto pieArea = bounds.reduced(margin);
    const int pieSize = juce::jmax(140, juce::jmin(pieArea.getWidth(), pieArea.getHeight()));
    patternPie.setBounds(pieArea.withSizeKeepingCentre(pieSize, pieSize));

    auto knobRow = getLocalBounds();
    knobRow.removeFromTop(headerH);
    knobRow.removeFromBottom(footerH);
    knobRow = knobRow.removeFromBottom(knobRowH).reduced(margin, 0);

    mixSlider.setBounds(getWidth() - margin - mixKnobSize,
                        knobRow.getBottom() - mixKnobSize - labelH,
                        mixKnobSize,
                        mixKnobSize);
    mixLabel.setBounds(mixSlider.getX(), mixSlider.getBottom(), mixKnobSize, labelH);

    const auto knobArea = knobRow.withTrimmedRight(mixKnobSize + margin * 2);
    const int mainKnobCount = 4;
    const int totalMainWidth = knobSize * mainKnobCount + chainSize + margin / 2 + margin * (mainKnobCount - 1);
    int x = knobArea.getX() + (knobArea.getWidth() - totalMainWidth) / 2;
    const int y = knobRow.getY();

    auto placeKnob = [&](juce::Slider& slider, juce::Label& label)
    {
        slider.setBounds(x, y, knobSize, knobSize);
        label.setBounds(x, slider.getBottom(), knobSize, labelH);
        x += knobSize + margin;
    };

    placeKnob(gridSlider, gridLabel);
    linkGridStepsButton.setBounds(x, y + (knobSize - chainSize) / 2, chainSize, chainSize);
    x += chainSize + margin / 2;
    placeKnob(stepsSlider, stepsLabel);
    placeKnob(pulsesSlider, pulsesLabel);
    placeKnob(gainSmoothSlider, gainSmoothLabel);
}
