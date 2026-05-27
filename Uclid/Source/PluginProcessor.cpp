#include <JuceHeader.h>
#include <vector>
#include <functional>
#include <cmath>
#include <algorithm>
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr float kMinSmoothMs = 0.025f;
    constexpr float kMaxSmoothMs = 10.0f;
    constexpr auto kStepPatternTreeId = "stepPattern";

    float smoothStep(float t)
    {
        return t * t * (3.0f - 2.0f * t);
    }
}

EuclidAudioProcessor::EuclidAudioProcessor()
    : AudioProcessor(juce::AudioProcessor::BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameters())
{
    mixParam = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("mix"));
    gridParam = dynamic_cast<juce::AudioParameterInt*>(apvts.getParameter("grid"));
    stepsParam = dynamic_cast<juce::AudioParameterInt*>(apvts.getParameter("steps"));
    pulsesParam = dynamic_cast<juce::AudioParameterInt*>(apvts.getParameter("pulses"));
    linkGridStepsParam = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("linkGridSteps"));
    bypassParam = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("bypass"));
    smoothMsParam = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("smoothMs"));
    patternLockParam = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("patternLock"));

    const int initialSteps = stepsParam != nullptr ? stepsParam->get() : 16;
    const int initialPulses = pulsesParam != nullptr ? pulsesParam->get() : 4;

    ensureStepArraysSize(initialSteps);
    applyEuclideanToSteps(initialPulses, initialSteps);
    rebuildPatternFromSteps();

    stepIndex = 0;
    uiStepIndex.store(0);
    beginGainRamp(stepGainForIndex(0));

    apvts.addParameterListener("steps", this);
    apvts.addParameterListener("pulses", this);
    apvts.addParameterListener("grid", this);
    clampPulsesToStepsParam();
}

EuclidAudioProcessor::~EuclidAudioProcessor()
{
    apvts.removeParameterListener("steps", this);
    apvts.removeParameterListener("pulses", this);
    apvts.removeParameterListener("grid", this);
}

void EuclidAudioProcessor::parameterChanged(const juce::String& parameterID, float)
{
    if (parameterID == "steps" || parameterID == "pulses" || parameterID == "grid")
        clampPulsesToStepsParam();
}

void EuclidAudioProcessor::clampPulsesToStepsParam()
{
    if (!stepsParam || !pulsesParam || !gridParam)
        return;

    const int grid = juce::jlimit(1, 64, gridParam->get());

    if (stepsParam->get() > grid)
        stepsParam->setValueNotifyingHost(static_cast<float>(grid));

    const int steps = juce::jlimit(1, grid, stepsParam->get());

    if (pulsesParam->get() > steps)
        pulsesParam->setValueNotifyingHost(static_cast<float>(steps));
}

const juce::String EuclidAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool EuclidAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.inputBuses.isEmpty())
        return true;

    const auto in = layouts.getMainInputChannelSet();

    if (in.isDisabled())
        return true;

    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void EuclidAudioProcessor::prepareToPlay(double sampleRate, int)
{
    currentSampleRate = sampleRate;
    stepIndex = 0;
    uiStepIndex.store(0);
    sampleCounter = 0.0f;
    rampSamplesRemaining = 0;

    if (auto* playHead = getPlayHead())
    {
        if (auto pos = playHead->getPosition())
        {
            if (const auto bpm = pos->getBpm(); bpm.hasValue() && *bpm > 0.0)
                currentBPM = *bpm;
        }
    }

    if (gridParam != nullptr && stepsParam != nullptr)
        updateTiming(gridParam->get(), stepsParam->get());
}

void EuclidAudioProcessor::releaseResources() {}

void EuclidAudioProcessor::updateTiming(int grid, int steps)
{
    const int safeGrid = juce::jmax(1, grid);
    const int safeSteps = juce::jmax(1, steps);

    const float samplesPerGridStep = static_cast<float>((currentSampleRate * 60.0 * 4.0)
                                                        / (currentBPM * static_cast<double>(safeGrid)));

    samplesPerStep = juce::jmax(samplesPerGridStep * (static_cast<float>(safeGrid) / static_cast<float>(safeSteps)), 1.0f);
}

void EuclidAudioProcessor::beginGainRamp(float newTarget)
{
    targetGain = newTarget;

    const float smoothMs = smoothMsParam != nullptr
                               ? juce::jlimit(0.0f, kMaxSmoothMs, smoothMsParam->get())
                               : 0.0f;

    if (smoothMs < kMinSmoothMs)
    {
        currentGain = newTarget;
        rampSamplesRemaining = 0;
        return;
    }

    rampStartGain = currentGain;
    rampTargetGain = newTarget;
    rampTotalSamples = juce::jmax(1, static_cast<int>(std::lround(smoothMs * 0.001 * currentSampleRate)));
    rampSamplesRemaining = rampTotalSamples;
}

void EuclidAudioProcessor::ensureStepArraysSize(int steps)
{
    const int safeSteps = juce::jlimit(1, kMaxPatternSteps, steps);
    stepEnabled.assign(static_cast<size_t>(safeSteps), 0);
    stepVelocity.assign(static_cast<size_t>(safeSteps), 1.0f);
}

void EuclidAudioProcessor::resizeStepArraysPreserve(int newSteps)
{
    const int safeSteps = juce::jlimit(1, kMaxPatternSteps, newSteps);
    const auto oldEnabled = stepEnabled;
    const auto oldVelocity = stepVelocity;

    stepEnabled.assign(static_cast<size_t>(safeSteps), 0);
    stepVelocity.assign(static_cast<size_t>(safeSteps), 1.0f);

    const int copyCount = juce::jmin(safeSteps, static_cast<int>(oldEnabled.size()));

    for (int i = 0; i < copyCount; ++i)
    {
        stepEnabled[static_cast<size_t>(i)] = oldEnabled[static_cast<size_t>(i)];
        stepVelocity[static_cast<size_t>(i)] = oldVelocity[static_cast<size_t>(i)];
    }
}

void EuclidAudioProcessor::rebuildPatternFromSteps()
{
    pattern.resize(stepEnabled.size());

    for (size_t i = 0; i < stepEnabled.size(); ++i)
        pattern[i] = stepEnabled[i] != 0 ? 1 : 0;
}

void EuclidAudioProcessor::applyEuclideanToSteps(int pulses, int steps)
{
    const auto euclidean = generateEuclideanPattern(pulses, steps);
    ensureStepArraysSize(steps);

    for (size_t i = 0; i < stepEnabled.size(); ++i)
    {
        const int on = i < euclidean.size() ? euclidean[i] : 0;
        stepEnabled[i] = static_cast<uint8_t>(on != 0 ? 1 : 0);

        if (stepEnabled[i] != 0)
            stepVelocity[i] = 1.0f;
    }

    rebuildPatternFromSteps();
}

void EuclidAudioProcessor::applyEuclideanFromParameters()
{
    if (!stepsParam || !pulsesParam)
        return;

    const int steps = juce::jlimit(1, kMaxPatternSteps, stepsParam->get());
    const int pulses = juce::jlimit(0, steps, pulsesParam->get());
    applyEuclideanToSteps(pulses, steps);
    saveStepStateToValueTree();
}

void EuclidAudioProcessor::refreshPatternFromParameters()
{
    if (!gridParam || !stepsParam || !pulsesParam)
        return;

    updatePattern(gridParam->get(), stepsParam->get(), pulsesParam->get());
}

void EuclidAudioProcessor::resetPatternToEuclidean()
{
    setManualPatternLocked(false);
    cachedGrid = -1;
    cachedSteps = -1;
    cachedPulses = -1;
    refreshPatternFromParameters();
}

void EuclidAudioProcessor::setManualPatternLocked(bool locked)
{
    if (patternLockParam != nullptr)
        patternLockParam->setValueNotifyingHost(locked ? 1.0f : 0.0f);
}

bool EuclidAudioProcessor::isManualPatternLocked() const
{
    return patternLockParam != nullptr && patternLockParam->get();
}

float EuclidAudioProcessor::stepGainForIndex(int index) const
{
    if (index < 0 || index >= static_cast<int>(stepEnabled.size()))
        return 0.0f;

    if (stepEnabled[static_cast<size_t>(index)] == 0)
        return 0.0f;

    return juce::jlimit(0.0f, 1.0f, stepVelocity[static_cast<size_t>(index)]);
}

void EuclidAudioProcessor::loadStepStateFromValueTree()
{
    const auto child = apvts.state.getChildWithName(kStepPatternTreeId);

    if (! child.isValid())
        return;

    const int count = static_cast<int>(child.getProperty("count", 0));

    if (count <= 0 || count > kMaxPatternSteps)
        return;

    resizeStepArraysPreserve(count);

    const auto enabledTokens = juce::StringArray::fromTokens(child.getProperty("enabled").toString(), ",", "");
    const auto velocityTokens = juce::StringArray::fromTokens(child.getProperty("velocities").toString(), ",", "");

    for (int i = 0; i < count; ++i)
    {
        if (i < enabledTokens.size())
            stepEnabled[static_cast<size_t>(i)] = enabledTokens[i].getIntValue() != 0 ? 1 : 0;

        if (i < velocityTokens.size())
            stepVelocity[static_cast<size_t>(i)] = juce::jlimit(0.0f, 1.0f, velocityTokens[i].getFloatValue());
    }

    rebuildPatternFromSteps();
}

void EuclidAudioProcessor::saveStepStateToValueTree()
{
    auto child = apvts.state.getOrCreateChildWithName(kStepPatternTreeId, nullptr);
    child.setProperty("count", static_cast<int>(stepEnabled.size()), nullptr);

    juce::String enabledStr;
    juce::String velocityStr;

    for (size_t i = 0; i < stepEnabled.size(); ++i)
    {
        enabledStr << juce::String(static_cast<int>(stepEnabled[i]));

        if (i + 1 < stepEnabled.size())
            enabledStr << ",";

        velocityStr << juce::String(stepVelocity[i], 4);

        if (i + 1 < stepEnabled.size())
            velocityStr << ",";
    }

    child.setProperty("enabled", enabledStr, nullptr);
    child.setProperty("velocities", velocityStr, nullptr);
}

int EuclidAudioProcessor::getPatternStepCount() const
{
    return static_cast<int>(stepEnabled.size());
}

bool EuclidAudioProcessor::toggleStepEnabled(int stepIndex)
{
    if (stepIndex < 0 || stepIndex >= static_cast<int>(stepEnabled.size()))
        return false;

    // Manual pattern (A): per-step edits override Euclidean until Pulses/steps change without lock.
    setManualPatternLocked(true);

    const size_t idx = static_cast<size_t>(stepIndex);
    stepEnabled[idx] = stepEnabled[idx] == 0 ? 1 : 0;

    if (stepEnabled[idx] != 0 && stepVelocity[idx] <= 0.0f)
        stepVelocity[idx] = 1.0f;

    rebuildPatternFromSteps();
    saveStepStateToValueTree();
    return stepEnabled[idx] != 0;
}

void EuclidAudioProcessor::setStepVelocity(int stepIndex, float velocity)
{
    if (stepIndex < 0 || stepIndex >= static_cast<int>(stepVelocity.size()))
        return;

    setManualPatternLocked(true);

    const float clamped = juce::jlimit(0.0f, 1.0f, velocity);
    stepVelocity[static_cast<size_t>(stepIndex)] = clamped;

    if (clamped > 0.0f && stepEnabled[static_cast<size_t>(stepIndex)] == 0)
        stepEnabled[static_cast<size_t>(stepIndex)] = 1;

    rebuildPatternFromSteps();
    saveStepStateToValueTree();
}

void EuclidAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, numSamples);

    if (!mixParam || !gridParam || !stepsParam || !pulsesParam || !bypassParam)
        return;

    const float mix = juce::jlimit(0.0f, 1.0f, mixParam->get());
    const int grid = juce::jlimit(1, 64, gridParam->get());
    const int steps = juce::jlimit(1, grid, stepsParam->get());
    const int pulses = juce::jlimit(0, steps, pulsesParam->get());
    const bool bypass = bypassParam->get();

    if (bypass || mix <= 0.0f)
        return;

    if (auto* playHead = getPlayHead())
    {
        if (auto pos = playHead->getPosition())
        {
            if (const auto bpm = pos->getBpm(); bpm.hasValue() && *bpm > 0.0)
                currentBPM = *bpm;
        }
    }

    updatePattern(grid, steps, pulses);
    updateTiming(grid, steps);

    if (pattern.empty())
        return;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        sampleCounter += 1.0f;

        if (sampleCounter >= samplesPerStep)
        {
            sampleCounter -= samplesPerStep;
            stepIndex = (stepIndex + 1) % static_cast<int>(pattern.size());
            uiStepIndex.store(stepIndex);
            beginGainRamp(stepGainForIndex(stepIndex));
        }

        if (rampSamplesRemaining > 0)
        {
            const float t = smoothStep(1.0f - static_cast<float>(rampSamplesRemaining)
                                              / static_cast<float>(rampTotalSamples));
            currentGain = rampStartGain + t * (rampTargetGain - rampStartGain);
            --rampSamplesRemaining;
        }
        else
        {
            currentGain = targetGain;
        }

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            float* channelData = buffer.getWritePointer(channel);
            const float dry = channelData[sample];
            const float wet = dry * currentGain;
            channelData[sample] = juce::jmap(mix, dry, wet);
        }
    }

    uiOutputGain.store(currentGain);
}

EuclidAudioProcessor::PatternSnapshot EuclidAudioProcessor::getPatternSnapshot() const
{
    PatternSnapshot snapshot;
    snapshot.pattern = pattern;
    snapshot.velocities.resize(stepVelocity.size());

    for (size_t i = 0; i < stepVelocity.size(); ++i)
        snapshot.velocities[i] = stepEnabled[i] != 0 ? stepVelocity[i] : 0.0f;

    snapshot.currentStep = uiStepIndex.load();
    snapshot.outputGain = uiOutputGain.load();
    snapshot.isActive = bypassParam != nullptr && mixParam != nullptr
                        && ! bypassParam->get() && mixParam->get() > 0.0f;
    snapshot.manualPatternLock = isManualPatternLocked();
    return snapshot;
}

void EuclidAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    saveStepStateToValueTree();
    juce::MemoryOutputStream stream(destData, true);
    apvts.state.writeToStream(stream);
}

void EuclidAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto tree = juce::ValueTree::readFromData(data, static_cast<size_t>(sizeInBytes));

    if (tree.isValid())
    {
        apvts.replaceState(tree);
        loadStepStateFromValueTree();

        if (stepEnabled.empty() && stepsParam != nullptr)
        {
            const int steps = juce::jlimit(1, kMaxPatternSteps, stepsParam->get());
            const int pulses = pulsesParam != nullptr ? juce::jlimit(0, steps, pulsesParam->get()) : 0;
            applyEuclideanToSteps(pulses, steps);
        }

        cachedGrid = -1;
        cachedSteps = -1;
        cachedPulses = -1;

        if (gridParam != nullptr && stepsParam != nullptr && pulsesParam != nullptr)
            updatePattern(gridParam->get(), stepsParam->get(), pulsesParam->get());
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout EuclidAudioProcessor::createParameters()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "mix", 1 },
        "Mix",
        0.0f,
        1.0f,
        1.0f));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "grid", 1 },
        "Grid",
        2,
        64,
        16,
        juce::AudioParameterIntAttributes().withLabel("div/bar")));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "steps", 1 },
        "Step",
        1,
        64,
        16,
        juce::AudioParameterIntAttributes().withLabel("steps")));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "pulses", 1 },
        "Pulse",
        0,
        64,
        4,
        juce::AudioParameterIntAttributes().withLabel("hits")));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "linkGridSteps", 1 },
        "Fundamental Link",
        true,
        juce::AudioParameterBoolAttributes().withMeta(true)));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "bypass", 1 },
        "Bypass",
        false));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "patternLock", 1 },
        "Pattern Lock",
        false,
        juce::AudioParameterBoolAttributes().withMeta(true)));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "smoothMs", 1 },
        "Smooth",
        juce::NormalisableRange<float>(0.0f, kMaxSmoothMs, 0.001f, 1.0f),
        0.1f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    return { params.begin(), params.end() };
}

std::vector<int> EuclidAudioProcessor::generateEuclideanPattern(int pulses, int steps)
{
    std::vector<int> result;

    if (steps <= 0)
        return result;

    result.assign(static_cast<size_t>(steps), 0);

    if (pulses <= 0)
        return result;

    if (pulses >= steps)
    {
        std::fill(result.begin(), result.end(), 1);
        return result;
    }

    // Bucket algorithm (iterative Bjorklund) — avoids deep recursion that could overflow the stack.
    int bucket = 0;

    for (int i = 0; i < steps; ++i)
    {
        bucket += pulses;

        if (bucket >= steps)
        {
            bucket -= steps;
            result[static_cast<size_t>(i)] = 1;
        }
    }

    return result;
}

void EuclidAudioProcessor::updatePattern(int grid, int steps, int pulses)
{
    const int safeSteps = juce::jlimit(1, kMaxPatternSteps, steps);
    const int safePulses = juce::jlimit(0, safeSteps, pulses);
    const bool locked = isManualPatternLocked();
    const bool stepsChanged = safeSteps != cachedSteps;
    const bool pulsesChanged = safePulses != cachedPulses;
    const bool gridChanged = grid != cachedGrid;

    if (stepsChanged)
        resizeStepArraysPreserve(safeSteps);

    if (! locked && (stepsChanged || pulsesChanged || gridChanged))
    {
        applyEuclideanToSteps(safePulses, safeSteps);
        saveStepStateToValueTree();
        stepIndex = 0;
        uiStepIndex.store(0);
        sampleCounter = 0.0f;
        beginGainRamp(stepGainForIndex(0));
    }
    else
    {
        rebuildPatternFromSteps();
    }

    cachedSteps = safeSteps;
    cachedPulses = safePulses;
    cachedGrid = grid;
}

const juce::String EuclidAudioProcessor::getInputChannelName(int channelIndex) const
{
    return juce::String::formatted("Input %d", channelIndex + 1);
}

const juce::String EuclidAudioProcessor::getOutputChannelName(int channelIndex) const
{
    return juce::String::formatted("Output %d", channelIndex + 1);
}

bool EuclidAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* EuclidAudioProcessor::createEditor()
{
    return new EuclidAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EuclidAudioProcessor();
}
