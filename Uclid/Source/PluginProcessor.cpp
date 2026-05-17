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
    bypassParam = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("bypass"));
    smoothMsParam = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("smoothMs"));

    pattern = generateEuclideanPattern(4, 16);
    stepIndex = 0;
    uiStepIndex.store(0);
    beginGainRamp(pattern.empty() ? 0.0f : (pattern[0] != 0 ? 1.0f : 0.0f));

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

    // Spread `steps` pattern hits evenly across one bar (grid divisions).
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
            beginGainRamp(pattern[static_cast<size_t>(stepIndex)] != 0 ? 1.0f : 0.0f);
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
    snapshot.currentStep = uiStepIndex.load();
    snapshot.outputGain = uiOutputGain.load();
    snapshot.isActive = bypassParam != nullptr && mixParam != nullptr
                        && ! bypassParam->get() && mixParam->get() > 0.0f;
    return snapshot;
}

void EuclidAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream stream(destData, true);
    apvts.state.writeToStream(stream);
}

void EuclidAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto tree = juce::ValueTree::readFromData(data, static_cast<size_t>(sizeInBytes));
    if (tree.isValid())
        apvts.replaceState(tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout EuclidAudioProcessor::createParameters()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>("mix", "Mix", 0.0f, 1.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterInt>("grid", "Grid", 2, 64, 16));
    params.push_back(std::make_unique<juce::AudioParameterInt>("steps", "Steps", 1, 64, 16));
    params.push_back(std::make_unique<juce::AudioParameterInt>("pulses", "Pulses", 0, 64, 4));
    params.push_back(std::make_unique<juce::AudioParameterBool>("bypass", "Bypass", false));

    // 0 = instant gate (no smoothing). 0.025–10 ms = short de-click ramp only on step edges.
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "smoothMs", 1 },
        "Smooth",
        juce::NormalisableRange<float>(0.0f, kMaxSmoothMs, 0.001f, 1.0f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    return { params.begin(), params.end() };
}

std::vector<int> EuclidAudioProcessor::generateEuclideanPattern(int pulses, int steps)
{
    std::vector<int> result;

    if (steps <= 0)
        return result;

    if (pulses <= 0)
    {
        result.assign(static_cast<size_t>(steps), 0);
        return result;
    }

    if (pulses >= steps)
    {
        result.assign(static_cast<size_t>(steps), 1);
        return result;
    }

    std::vector<int> counts;
    std::vector<int> remainders;

    int divisor = steps - pulses;
    remainders.push_back(pulses);
    int level = 0;

    while (true)
    {
        counts.push_back(divisor / remainders[static_cast<size_t>(level)]);
        const int r = divisor % remainders[static_cast<size_t>(level)];
        remainders.push_back(r);
        divisor = remainders[static_cast<size_t>(level)];
        ++level;

        if (remainders[static_cast<size_t>(level)] <= 1)
            break;
    }

    counts.push_back(divisor);

    std::function<void(int)> build = [&](int lvl)
    {
        if (lvl == -1)
            result.push_back(0);
        else if (lvl == -2)
            result.push_back(1);
        else
        {
            for (int i = 0; i < counts[static_cast<size_t>(lvl)]; ++i)
                build(lvl - 1);

            if (remainders[static_cast<size_t>(lvl)] != 0)
                build(lvl - 2);
        }
    };

    build(level);

    while (result.size() > static_cast<size_t>(steps))
        result.pop_back();

    while (result.size() < static_cast<size_t>(steps))
        result.push_back(0);

    return result;
}

void EuclidAudioProcessor::updatePattern(int grid, int steps, int pulses)
{
    juce::ignoreUnused(grid);

    if (steps != cachedSteps || pulses != cachedPulses)
    {
        cachedSteps = steps;
        cachedPulses = pulses;
        cachedGrid = grid;

        pattern = generateEuclideanPattern(pulses, steps);

        if (pattern.size() != static_cast<size_t>(steps))
            pattern.resize(static_cast<size_t>(steps), 0);

        stepIndex = 0;
        uiStepIndex.store(0);
        sampleCounter = 0.0f;

        const float initialGain = pattern.empty() ? 0.0f : (pattern[0] != 0 ? 1.0f : 0.0f);
        beginGainRamp(initialGain);
    }
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
