#include "PluginEditor.h"
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
#endif

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
        const juce::Colour hubFill        { 0xff0a0e14 };
        const juce::Colour hubLine        { 0xff2a3544 };
        const juce::Colour hubAccent      { 0xff4a5a6e };
        const juce::Colour bypassOn       { 0xff8b4a4a };
        const juce::Colour bypassOff      { 0xff2a3544 };
    }

    void drawEuclideanHub(juce::Graphics& g, juce::Point<float> centre, float hubRadius)
    {
        g.setColour(Theme::hubFill);
        g.fillEllipse(centre.x - hubRadius, centre.y - hubRadius, hubRadius * 2.0f, hubRadius * 2.0f);

        for (int ring = 1; ring <= 3; ++ring)
        {
            const float r = hubRadius * static_cast<float>(ring) / 3.4f;
            g.setColour(Theme::hubLine.withAlpha(0.35f + 0.1f * static_cast<float>(ring)));
            g.drawEllipse(centre.x - r, centre.y - r, r * 2.0f, r * 2.0f, 0.8f);
        }

        for (int i = 0; i < 6; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * static_cast<float>(i) / 6.0f
                            - juce::MathConstants<float>::halfPi;
            const float x = centre.x + std::cos(a) * hubRadius * 0.88f;
            const float y = centre.y + std::sin(a) * hubRadius * 0.88f;
            g.setColour(Theme::hubAccent.withAlpha(0.45f));
            g.drawLine(centre.x, centre.y, x, y, 0.7f);
        }

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

    bool isStandaloneEditorWindow(const juce::Component& editor)
    {
        return juce::StandalonePluginHolder::getInstance() != nullptr
               || editor.findParentComponentOfClass<juce::DocumentWindow>() != nullptr;
    }
}

// -----------------------------------------------------------------------------
ComponentWithParamMenu::ComponentWithParamMenu(juce::AudioProcessorEditor& editorIn,
                                               juce::RangedAudioParameter& paramIn)
    : editor(editorIn),
      param(paramIn)
{
    setOpaque(false);
    setInterceptsMouseClicks(true, true);
}

void ComponentWithParamMenu::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        showHostParameterContextMenu();
}

void ComponentWithParamMenu::mouseUp(const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown() || e.mods.isPopupMenu())
        showHostParameterContextMenu();
}

void ComponentWithParamMenu::showHostParameterContextMenu()
{
    param.beginChangeGesture();

    if (auto* hostContext = editor.getHostContext())
    {
        if (auto menu = hostContext->getContextMenuForParameter(&param))
        {
           #if JucePlugin_Build_VST3
            menu->showNativeMenu(editor.getMouseXYRelative());
           #else
            menu->getEquivalentPopupMenu().showMenuAsync(
                juce::PopupMenu::Options().withTargetComponent(this).withMousePosition());
           #endif
        }
    }

    param.endChangeGesture();
}

int ComponentWithParamMenu::getAttachedParameterIndex() const
{
    return param.getParameterIndex();
}

HostAwareRotaryKnob::HostAwareRotaryKnob(juce::AudioProcessorEditor& editorIn,
                                         juce::AudioProcessorValueTreeState& apvtsIn,
                                         const juce::String& paramId,
                                         juce::RangedAudioParameter& paramIn)
    : ComponentWithParamMenu(editorIn, paramIn),
      attachment(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvtsIn, paramId, slider))
{
    slider.addMouseListener(this, true);
    addAndMakeVisible(slider);
}

void HostAwareRotaryKnob::resized()
{
    slider.setBounds(getLocalBounds());
}

HostAwareChainLink::HostAwareChainLink(juce::AudioProcessorEditor& editorIn,
                                       juce::AudioProcessorValueTreeState& apvtsIn,
                                       juce::RangedAudioParameter& paramIn)
    : ComponentWithParamMenu(editorIn, paramIn),
      attachment(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvtsIn,
                                                                                        "linkGridSteps",
                                                                                        linkButton))
{
    linkButton.addMouseListener(this, true);
    addAndMakeVisible(linkButton);
}

void HostAwareChainLink::resized()
{
    linkButton.setBounds(getLocalBounds());
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
    if (onPieMouseDown)
        onPieMouseDown(e);
}

void PatternPieComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (onPieMouseDrag)
        onPieMouseDrag(e);
}

void PatternPieComponent::mouseUp(const juce::MouseEvent& e)
{
    if (onPieMouseUp)
        onPieMouseUp(e);
}

// -----------------------------------------------------------------------------
EuclidAudioProcessorEditor::EuclidAudioProcessorEditor(EuclidAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      apvts(p.getAPVTS()),
      gridParam(dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter("grid"))),
      stepsParam(dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter("steps"))),
      pulsesParam(dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter("pulses"))),
      gridKnob(*this, apvts, "grid", *gridParam),
      linkGridStepsControl(*this,
                           apvts,
                           *dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter("linkGridSteps"))),
      stepsKnob(*this, apvts, "steps", *stepsParam),
      pulsesKnob(*this, apvts, "pulses", *pulsesParam)
{
    jassert(gridParam != nullptr && stepsParam != nullptr && pulsesParam != nullptr);

    setResizable(true, true);
    setResizeLimits(360, 480, 960, 1280);
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio(static_cast<double>(baseWidth) / static_cast<double>(baseHeight));

    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "mix", mixSlider);
    gainSmoothAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "smoothMs", gainSmoothSlider);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "bypass", bypassButton);

    patternPie.paintPie = [this](juce::Graphics& g, juce::Rectangle<int> b) { drawPatternPie(g, b); };
    patternPie.onPieMouseDown = [this](const juce::MouseEvent& e) { handlePieMouseDown(e); };
    patternPie.onPieMouseDrag = [this](const juce::MouseEvent& e) { handlePieMouseDrag(e); };
    patternPie.onPieMouseUp = [this](const juce::MouseEvent& e) { handlePieMouseUp(e); };
    patternPie.setInterceptsMouseClicks(true, false);
    addAndMakeVisible(patternPie);

    auto configureKnob = [this](juce::Slider& slider, const juce::String& tooltip, bool isInteger)
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 56, 18);
        slider.setTooltip(tooltip);
        applySoothingKnobStyle(slider);
        if (isInteger)
            slider.setNumDecimalPlacesToDisplay(0);
    };

    addAndMakeVisible(gridKnob);
    addAndMakeVisible(stepsKnob);
    addAndMakeVisible(pulsesKnob);
    addAndMakeVisible(linkGridStepsControl);

    configureKnob(gridKnob.getSlider(), "Grid divisions per bar", true);
    configureKnob(stepsKnob.getSlider(), "Pattern length", true);
    configureKnob(pulsesKnob.getSlider(), "Pulses (or drag the hub)", true);
    configureKnob(gainSmoothSlider, "De-click ramp (ms). 0 = off.", false);
    addAndMakeVisible(gainSmoothSlider);

    gainSmoothSlider.setNumDecimalPlacesToDisplay(1);
    gainSmoothSlider.textFromValueFunction = [](double v)
    {
        return v < 0.001 ? juce::String("Off") : juce::String(v, 1) + " ms";
    };

    configureKnob(mixSlider, "Dry/wet mix", false);
    addAndMakeVisible(mixSlider);
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
    setupLabel(stepsLabel, "Step");
    setupLabel(pulsesLabel, "Pulse");
    setupLabel(gainSmoothLabel, "Smooth");
    setupLabel(mixLabel, "Mix");

    titleButton.setTooltip("Click for UI scale");
    titleButton.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    titleButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    titleButton.setColour(juce::TextButton::textColourOffId, juce::Colours::transparentBlack);
    titleButton.setColour(juce::TextButton::textColourOnId, juce::Colours::transparentBlack);
    titleButton.setAlpha(0.0f);
    titleButton.onClick = [this] { showScaleMenu(); };
    addAndMakeVisible(titleButton);

    footerLabel.setText("Uclid  ·  github.com/T3Lem/Uclid-Plugin", juce::dontSendNotification);
    footerLabel.setFont(juce::FontOptions(12.0f));
    footerLabel.setJustificationType(juce::Justification::centred);
    footerLabel.setColour(juce::Label::textColourId, Theme::textLink);
    footerLabel.setTooltip("https://github.com/T3Lem/Uclid-Plugin");
    addAndMakeVisible(footerLabel);

    bypassButton.setClickingTogglesState(true);
    bypassButton.setTooltip("Bypass");
    bypassButton.setColour(juce::TextButton::buttonColourId, Theme::bypassOff);
    bypassButton.setColour(juce::TextButton::buttonOnColourId, Theme::bypassOn);
    bypassButton.setColour(juce::TextButton::textColourOffId, Theme::textMuted);
    bypassButton.setColour(juce::TextButton::textColourOnId, Theme::textPrimary);
    addAndMakeVisible(bypassButton);

    resetPatternButton.setTooltip("Clear manual edits and restore Euclidean rhythm from Pulses");
    applySoothingButtonStyle(resetPatternButton);
    resetPatternButton.onClick = [this]()
    {
        audioProcessor.resetPatternToEuclidean();
        updateResetPatternButtonState();
        patternPie.repaint();
    };
    addAndMakeVisible(resetPatternButton);

    linkGridStepsControl.getButton().setTooltip("Link Grid and Step (Fundamental timing)");
    linkGridStepsControl.getButton().onClick = [this]()
    {
        if (isLinkGridAndSteps())
            syncStepsToGrid();
        updatePulsesSliderRange();
    };

    gridKnob.getSlider().onValueChange = [this]()
    {
        if (suppressSliderCallbacks)
            return;

        if (isLinkGridAndSteps())
            syncStepsToGrid();
        updatePulsesSliderRange();
        audioProcessor.refreshPatternFromParameters();
        patternPie.repaint();
    };

    stepsKnob.getSlider().onValueChange = [this]()
    {
        if (suppressSliderCallbacks)
            return;

        if (isLinkGridAndSteps())
            syncGridToSteps();
        updatePulsesSliderRange();
        audioProcessor.refreshPatternFromParameters();
        patternPie.repaint();
    };

    pulsesKnob.getSlider().onValueChange = [this]()
    {
        if (suppressSliderCallbacks)
            return;

        clampPulsesToSteps();

        if (! audioProcessor.isManualPatternLocked())
            audioProcessor.applyEuclideanFromParameters();
        else
            audioProcessor.refreshPatternFromParameters();

        patternPie.repaint();
    };

    suppressSliderCallbacks = true;

    if (isLinkGridAndSteps() && stepsKnob.getSlider().getValue() != gridKnob.getSlider().getValue())
        syncStepsToGrid();

    updatePulsesSliderRange();
    suppressSliderCallbacks = false;

    audioProcessor.refreshPatternFromParameters();
    updateResetPatternButtonState();

    setUIScale(1.0f);

    if (isStandaloneEditorWindow(*this))
    {
        juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<EuclidAudioProcessorEditor>(this)]
        {
            if (safeThis != nullptr)
                safeThis->centrePluginWindow();
        });
    }
    else
    {
        windowShownFixApplied = true;
    }

    startTimerHz(30);
}

void EuclidAudioProcessorEditor::centrePluginWindow()
{
    if (! isStandaloneEditorWindow(*this))
        return;

    const int winW = juce::jmax(360, getWidth());
    const int winH = juce::jmax(480, getHeight());

    if (auto* holder = juce::StandalonePluginHolder::getInstance())
    {
        if (auto* props = holder->settings.get())
        {
            props->removeValue("windowX");
            props->removeValue("windowY");
        }
    }

    setSize(winW, winH);
    resized();

    if (auto* parent = getParentComponent())
        parent->resized();

    juce::Rectangle<int> targetBounds { 100, 100, winW, winH };

    if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        const auto area = display->userArea;
        targetBounds = { area.getCentreX() - winW / 2,
                         area.getCentreY() - winH / 2,
                         winW,
                         winH };
    }

    for (juce::Component* comp = this; comp != nullptr; comp = comp->getParentComponent())
    {
        if (auto* peer = comp->getPeer())
        {
            auto bounds = targetBounds;

            if (auto* window = comp->findParentComponentOfClass<juce::DocumentWindow>())
                bounds.setHeight(winH + window->getTitleBarHeight());
            else if (auto* doc = dynamic_cast<juce::DocumentWindow*>(comp))
                bounds.setHeight(winH + doc->getTitleBarHeight());

            peer->setVisible(true);

           #if JUCE_WINDOWS
            if (auto hwnd = (HWND) peer->getNativeHandle())
            {
                ShowWindow(hwnd, SW_RESTORE);
                SetWindowPos(hwnd, HWND_TOP, bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(),
                             SWP_SHOWWINDOW | SWP_FRAMECHANGED);
                SetForegroundWindow(hwnd);
            }
           #endif
        }
    }

    if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
    {
        window->setName("Uclid");
        window->setBounds(targetBounds.withHeight(winH + window->getTitleBarHeight()));
        window->setVisible(true);
        window->toFront(true);
        windowShownFixApplied = true;
        return;
    }

    juce::Component* root = this;

    while (root->getParentComponent() != nullptr)
        root = root->getParentComponent();

    root->setName("Uclid");
    root->setSize(winW, winH);
    root->setVisible(true);
    root->toFront(true);

    if (root->getWidth() >= 360 && root->getHeight() >= 480)
        windowShownFixApplied = true;
}

EuclidAudioProcessorEditor::~EuclidAudioProcessorEditor()
{
    if (pulsesHostGestureActive && pulsesParam != nullptr)
    {
        pulsesParam->endChangeGesture();
        pulsesHostGestureActive = false;
    }

    stopTimer();
}

ComponentWithParamMenu* EuclidAudioProcessorEditor::findParentWithParamMenu(juce::Component* component)
{
    if (component == nullptr)
        return nullptr;

    if (auto* menuHost = dynamic_cast<ComponentWithParamMenu*>(component))
        return menuHost;

    return findParentWithParamMenu(component->getParentComponent());
}

int EuclidAudioProcessorEditor::getControlParameterIndex(juce::Component& component)
{
    if (auto* menuHost = findParentWithParamMenu(&component))
        return menuHost->getAttachedParameterIndex();

    return -1;
}

bool EuclidAudioProcessorEditor::isLinkGridAndSteps() const
{
    return linkGridStepsControl.getButton().getToggleState();
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

void EuclidAudioProcessorEditor::setPulsesFromUI(int pulses, bool beginGesture, bool endGesture)
{
    if (pulsesParam == nullptr)
        return;

    const int steps = juce::roundToInt(stepsKnob.getSlider().getValue());
    const int clamped = juce::jlimit(0, steps, pulses);
    const float normalised = pulsesParam->convertTo0to1(static_cast<float>(clamped));

    if (beginGesture && ! pulsesHostGestureActive)
    {
        pulsesParam->beginChangeGesture();
        pulsesHostGestureActive = true;
    }

    pulsesParam->setValueNotifyingHost(normalised);
    pulsesKnob.getSlider().setValue(static_cast<double>(clamped), juce::dontSendNotification);

    if (! audioProcessor.isManualPatternLocked())
        audioProcessor.applyEuclideanFromParameters();

    patternPie.repaint();

    if (endGesture && pulsesHostGestureActive)
    {
        pulsesParam->endChangeGesture();
        pulsesHostGestureActive = false;
    }
}

EuclidAudioProcessorEditor::PieLayout EuclidAudioProcessorEditor::getPieLayout(juce::Rectangle<int> bounds) const
{
    PieLayout layout;
    layout.centre = bounds.getCentre().toFloat();
    layout.outerRadius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f - 6.0f;
    layout.innerRadius = layout.outerRadius * 0.36f;

    const auto snapshot = audioProcessor.getPatternSnapshot();
    layout.numSteps = snapshot.pattern.empty()
                          ? juce::jmax(1, audioProcessor.getPatternStepCount())
                          : static_cast<int>(snapshot.pattern.size());
    layout.gap = juce::jlimit(0.003f, 0.035f, 0.32f / static_cast<float>(layout.numSteps));
    return layout;
}

bool EuclidAudioProcessorEditor::isInHub(juce::Point<float> pos, const PieLayout& layout) const
{
    const float dx = pos.x - layout.centre.x;
    const float dy = pos.y - layout.centre.y;
    const float dist = std::sqrt(dx * dx + dy * dy);
    return dist < layout.innerRadius * 0.55f;
}

int EuclidAudioProcessorEditor::stepIndexAtPoint(juce::Point<float> pos, const PieLayout& layout) const
{
    const float dx = pos.x - layout.centre.x;
    const float dy = pos.y - layout.centre.y;
    const float dist = std::sqrt(dx * dx + dy * dy);

    if (dist < layout.innerRadius || dist > layout.outerRadius * 1.02f)
        return -1;

    for (int i = 0; i < layout.numSteps; ++i)
    {
        const float start = juce::MathConstants<float>::twoPi * static_cast<float>(i) / static_cast<float>(layout.numSteps)
                            - juce::MathConstants<float>::halfPi + layout.gap;
        const float end = juce::MathConstants<float>::twoPi * static_cast<float>(i + 1) / static_cast<float>(layout.numSteps)
                          - juce::MathConstants<float>::halfPi - layout.gap;

        const auto wedge = makeAnnularWedge(layout.centre, layout.innerRadius, layout.outerRadius, start, end);

        if (wedge.contains(pos.x, pos.y))
            return i;
    }

    return -1;
}

void EuclidAudioProcessorEditor::handlePieMouseDown(const juce::MouseEvent& e)
{
    piePointerDown = e.position;
    activeStepIndex = -1;
    selectedStepIndex = -1;
    dragVolumePercent = -1.0f;
    pieGesture = PieGesture::none;

    const auto layout = getPieLayout(patternPie.getLocalBounds());
    const int step = stepIndexAtPoint(e.position, layout);

    if (e.mods.isPopupMenu() || e.mods.isRightButtonDown())
    {
        if (step >= 0)
        {
            audioProcessor.toggleStepEnabled(step);
            patternPie.repaint();
        }
        else if (isInHub(e.position, layout) && pulsesParam != nullptr)
        {
            pulsesParam->beginChangeGesture();

            if (auto* hostContext = getHostContext())
            {
                if (auto menu = hostContext->getContextMenuForParameter(pulsesParam))
                {
                   #if JucePlugin_Build_VST3
                    menu->showNativeMenu(getMouseXYRelative());
                   #else
                    menu->getEquivalentPopupMenu().showMenuAsync(
                        juce::PopupMenu::Options().withTargetComponent(&patternPie).withMousePosition());
                   #endif
                }
            }

            pulsesParam->endChangeGesture();
        }

        return;
    }

    if (isInHub(e.position, layout))
    {
        pieGesture = PieGesture::pulsesDrag;
        setPulsesFromUI(juce::roundToInt(pulsesKnob.getSlider().getValue()), true, false);
        return;
    }

    if (step < 0)
        return;

    activeStepIndex = step;
    selectedStepIndex = step;
    pieGesture = PieGesture::volumeDrag;

    const auto snapshot = audioProcessor.getPatternSnapshot();
    velocityAtDragStart = (step < static_cast<int>(snapshot.velocities.size()))
                              ? snapshot.velocities[static_cast<size_t>(step)]
                              : 1.0f;
    dragVolumePercent = velocityAtDragStart * 100.0f;
}

void EuclidAudioProcessorEditor::handlePieMouseDrag(const juce::MouseEvent& e)
{
    const auto delta = e.position - piePointerDown;

    if (pieGesture == PieGesture::pulsesDrag)
    {
        setPulsesFromUI(pulsesFromPieAngle(e.position), false, false);
        return;
    }

    if (pieGesture == PieGesture::volumeDrag && activeStepIndex >= 0)
    {
        const float velocity = juce::jlimit(0.0f, 1.0f, velocityAtDragStart - delta.y / kVelocityDragRangePx);
        audioProcessor.setStepVelocity(activeStepIndex, velocity);
        dragVolumePercent = velocity * 100.0f;
        patternPie.repaint();
    }
}

void EuclidAudioProcessorEditor::handlePieMouseUp(const juce::MouseEvent&)
{
    if (pieGesture == PieGesture::pulsesDrag)
        setPulsesFromUI(juce::roundToInt(pulsesKnob.getSlider().getValue()), false, true);

    pieGesture = PieGesture::none;
    activeStepIndex = -1;
    dragVolumePercent = -1.0f;
    patternPie.repaint();
}

int EuclidAudioProcessorEditor::pulsesFromPieAngle(juce::Point<float> pos) const
{
    const auto bounds = patternPie.getLocalBounds();
    if (bounds.isEmpty())
        return juce::roundToInt(pulsesKnob.getSlider().getValue());

    const auto layout = getPieLayout(bounds);

    if (! isInHub(pos, layout))
        return juce::roundToInt(pulsesKnob.getSlider().getValue());

    const float dx = pos.x - layout.centre.x;
    const float dy = pos.y - layout.centre.y;

    float angle = std::atan2(dy, dx) + juce::MathConstants<float>::halfPi;
    if (angle < 0.0f)
        angle += juce::MathConstants<float>::twoPi;

    const int steps = juce::jmax(1, juce::roundToInt(stepsKnob.getSlider().getValue()));
    const float ratio = angle / juce::MathConstants<float>::twoPi;

    // Wider top snap zones make the extremes easy to hit: top-right = 0, top-left = full.
    const float snap = juce::jmin(0.18f, 1.15f / static_cast<float>(steps));
    if (ratio <= snap)
        return 0;
    if (ratio >= 1.0f - snap)
        return steps;

    const float remapped = (ratio - snap) / (1.0f - 2.0f * snap);
    const int pulses = 1 + static_cast<int>(std::floor(remapped * static_cast<float>(steps - 1) + 0.5f));
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

void EuclidAudioProcessorEditor::updateResetPatternButtonState()
{
    resetPatternButton.setEnabled(audioProcessor.isManualPatternLocked());
}

void EuclidAudioProcessorEditor::timerCallback()
{
    if (! windowShownFixApplied && windowFixAttempts < 60 && isStandaloneEditorWindow(*this))
    {
        ++windowFixAttempts;
        centrePluginWindow();
    }

    updateResetPatternButtonState();
    patternPie.repaint();
}

void EuclidAudioProcessorEditor::drawTitleLogo(juce::Graphics& g) const
{
    auto area = titleButton.getBounds().toFloat();
    g.setColour(Theme::textPrimary);
    g.setFont(juce::Font(juce::FontOptions(26.0f * currentScale, juce::Font::bold)));
    g.drawText("Uclid", area, juce::Justification::centredLeft);
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

    drawTitleLogo(g);
}

void EuclidAudioProcessorEditor::syncStepsToGrid()
{
    if (updatingLinkedSliders)
        return;
    updatingLinkedSliders = true;
    stepsKnob.getSlider().setValue(gridKnob.getSlider().getValue(), juce::sendNotificationSync);
    updatingLinkedSliders = false;
}

void EuclidAudioProcessorEditor::syncGridToSteps()
{
    if (updatingLinkedSliders)
        return;
    updatingLinkedSliders = true;
    gridKnob.getSlider().setValue(stepsKnob.getSlider().getValue(), juce::sendNotificationSync);
    updatingLinkedSliders = false;
}

void EuclidAudioProcessorEditor::updatePulsesSliderRange()
{
    const int steps = juce::jmax(0, juce::roundToInt(stepsKnob.getSlider().getValue()));
    pulsesKnob.getSlider().setRange(0, steps, 1);
    clampPulsesToSteps();
}

void EuclidAudioProcessorEditor::clampPulsesToSteps()
{
    const int steps = juce::roundToInt(stepsKnob.getSlider().getValue());
    if (pulsesKnob.getSlider().getValue() > steps)
        pulsesKnob.getSlider().setValue(static_cast<double>(steps), juce::sendNotificationSync);
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
        drawEuclideanHub(g, centre, innerRadius - 4.0f);
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
        const float velocity = (i < static_cast<int>(snapshot.velocities.size()))
                                   ? snapshot.velocities[static_cast<size_t>(i)]
                                   : (isOn ? 1.0f : 0.0f);

        auto wedge = makeAnnularWedge(centre, innerRadius, outerRadius, start, end);

        if (isOn)
        {
            const float level = juce::jlimit(0.0f, 1.0f, velocity);
            const auto hi = Theme::stepOnHi.interpolatedWith(Theme::stepOff, 1.0f - level);
            const auto lo = Theme::stepOnLo.interpolatedWith(Theme::stepOff, 1.0f - level * 0.65f);
            juce::ColourGradient fill(hi.brighter(0.08f), centre.x, centre.y - outerRadius,
                                      lo, centre.x, centre.y + outerRadius, false);
            g.setGradientFill(fill);
        }
        else
        {
            g.setColour(Theme::stepOff);
        }

        g.fillPath(wedge);

        if (isOn && velocity > 0.02f)
        {
            const float level = juce::jlimit(0.0f, 1.0f, velocity);
            const float barInner = innerRadius + (outerRadius - innerRadius) * (1.0f - level) * 0.55f;
            auto levelWedge = makeAnnularWedge(centre, barInner, outerRadius - 1.5f, start + gap * 0.25f, end - gap * 0.25f);
            g.setColour(Theme::stepOnHi.withAlpha(0.22f + 0.45f * level));
            g.fillPath(levelWedge);
        }

        g.setColour(isOn ? Theme::stepOnLo.withAlpha(0.35f) : Theme::stepOffEdge);
        g.strokePath(wedge, juce::PathStrokeType(0.9f));

        if (isCurrent)
        {
            g.setColour(Theme::stepPlayhead.withAlpha(0.95f));
            g.strokePath(wedge, juce::PathStrokeType(2.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        if (i == selectedStepIndex && dragVolumePercent >= 0.0f)
        {
            g.setColour(Theme::accentSoft.withAlpha(0.9f));
            g.strokePath(wedge, juce::PathStrokeType(3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    if (dragVolumePercent >= 0.0f && selectedStepIndex >= 0)
    {
        g.setColour(Theme::textPrimary);
        g.setFont(juce::Font(juce::FontOptions(15.0f * currentScale, juce::Font::bold)));
        const auto label = juce::String("Step ") + juce::String(selectedStepIndex + 1)
                           + ": " + juce::String(juce::roundToInt(dragVolumePercent)) + "%";
        g.drawText(label, bounds, juce::Justification::centred);
    }

    const float hubRadius = innerRadius - 4.0f;
    drawEuclideanHub(g, centre, hubRadius);

    if (snapshot.isActive && snapshot.outputGain > 0.001f)
    {
        const float gainAngle = juce::MathConstants<float>::twoPi * snapshot.outputGain;
        juce::Path gainArc;
        gainArc.addCentredArc(centre.x, centre.y, hubRadius * 0.92f, hubRadius * 0.92f, 0.0f,
                              -juce::MathConstants<float>::halfPi,
                              -juce::MathConstants<float>::halfPi + gainAngle, true);
        g.setColour(Theme::stepOnLo.withAlpha(0.55f));
        g.strokePath(gainArc, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

void EuclidAudioProcessorEditor::setUIScale(float scale)
{
    currentScale = juce::jlimit(0.5f, 1.5f, scale);
    setSize(static_cast<int>(baseWidth * currentScale), static_cast<int>(baseHeight * currentScale));
    resized();

    if (isStandaloneEditorWindow(*this))
        centrePluginWindow();
}

void EuclidAudioProcessorEditor::resized()
{
    const int margin = static_cast<int>(12 * currentScale);
    const int headerH = static_cast<int>(48 * currentScale);
    const int knobSize = static_cast<int>(64 * currentScale);
    const int mixKnobSize = static_cast<int>(58 * currentScale);
    const int labelH = static_cast<int>(16 * currentScale);
    const int buttonH = static_cast<int>(24 * currentScale);
    const int buttonW = static_cast<int>(62 * currentScale);
    const int footerH = static_cast<int>(footerHeight * currentScale);
    const int chainSize = static_cast<int>(28 * currentScale);

    auto bounds = getLocalBounds();

    auto footer = bounds.removeFromBottom(footerH);
    footerLabel.setBounds(footer.reduced(margin, 4));

    auto header = bounds.removeFromTop(headerH);
    titleButton.setBounds(header.getX() + margin, header.getY(), static_cast<int>(160 * currentScale), header.getHeight());

    bypassButton.setBounds(header.getRight() - margin - buttonW,
                           header.getY() + (header.getHeight() - buttonH) / 2,
                           buttonW,
                           buttonH);

    const int knobRowH = knobSize + labelH + margin;
    bounds.removeFromBottom(knobRowH);

    const int resetBtnW = static_cast<int>(132 * currentScale);
    const int resetStripH = buttonH + static_cast<int>(6 * currentScale);
    auto resetStrip = bounds.removeFromBottom(resetStripH);
    resetPatternButton.setBounds(resetStrip.withSizeKeepingCentre(resetBtnW, buttonH));

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

    auto placeKnob = [&](juce::Component& knob, juce::Label& label)
    {
        knob.setBounds(x, y, knobSize, knobSize);
        label.setBounds(x, knob.getBottom(), knobSize, labelH);
        x += knobSize + margin;
    };

    placeKnob(gridKnob, gridLabel);
    linkGridStepsControl.setBounds(x, y + (knobSize - chainSize) / 2, chainSize, chainSize);
    x += chainSize + margin / 2;
    placeKnob(stepsKnob, stepsLabel);
    placeKnob(pulsesKnob, pulsesLabel);
    gainSmoothSlider.setBounds(x, y, knobSize, knobSize);
    gainSmoothLabel.setBounds(x, gainSmoothSlider.getBottom(), knobSize, labelH);

    if (isStandaloneEditorWindow(*this))
    {
        juce::Component* root = this;

        while (root->getParentComponent() != nullptr)
            root = root->getParentComponent();

        if (root->getWidth() < getWidth() || root->getHeight() < getHeight())
            root->setSize(getWidth(), getHeight());
    }
}
