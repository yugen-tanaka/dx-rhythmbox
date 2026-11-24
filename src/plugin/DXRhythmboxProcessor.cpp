#include "plugin/DXRhythmboxProcessor.hpp"
#include "plugin/DXRhythmboxEditor.hpp"

#include <algorithm>
#include <cmath>

namespace dx701::plugin
{

DXRhythmboxAudioProcessor::DXRhythmboxAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "DXRhythmboxParameters", createDXRhythmboxParameterLayout())
{
    rawParams_.bind(apvts_);
    engine_.prepare(48000.0);
    monoScratch_.assign(512u, 0.0);
}

void DXRhythmboxAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    sampleRate_ = (sampleRate > 1000.0) ? sampleRate : 48000.0;
    const std::size_t cap = static_cast<std::size_t>(std::max(512, samplesPerBlock));
    monoScratch_.assign(cap, 0.0);

    pendingUiVoices_.store(0u, std::memory_order_release);
    pendingUiFillIn_.store(-1, std::memory_order_release);
    uiStartHeld_.store(false, std::memory_order_release);
    uiStopHeld_.store(false, std::memory_order_release);
    midiFlashVoices_.store(0u, std::memory_order_release);
    prevUiStartHeld_ = false;
    prevUiStopHeld_ = false;
    prevBreakGate_ = false;
    stopCcHeld_ = false;
    heldTriggerNotes_.fill(0u);
    heldTriggerNoteCount_ = 0;

    auto rbState = rawParams_.readState();
    prevEnabledParam_ = rbState.enabled;
    prevBreakMode_ = rawParams_.readBreakEnabled();
    prevMidiStartOn_ = rawParams_.readMidiStartEnabled();
    // Red RHYTHM switch only enables/arms the Rhythmer; it never auto-starts rhythm playback.
    rhythmLatched_ = false;
    rbState.enabled = false;

    engine_.prepare(sampleRate_);
    engine_.applyState(rbState);
}

void DXRhythmboxAudioProcessor::releaseResources()
{
}

bool DXRhythmboxAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (!layouts.inputBuses.isEmpty() || layouts.outputBuses.size() != 1)
    {
        return false;
    }
    const auto& mainOut = layouts.getMainOutputChannelSet();
    return mainOut == juce::AudioChannelSet::stereo() || mainOut == juce::AudioChannelSet::mono();
}

void DXRhythmboxAudioProcessor::triggerVoiceFromUi(dx701::dsp::VoiceId voice) noexcept
{
    const auto bit = 1u << static_cast<unsigned>(voice);
    pendingUiVoices_.fetch_or(bit, std::memory_order_release);
}

void DXRhythmboxAudioProcessor::triggerFillInFromUi(dx701::dsp::FillInId fillIn) noexcept
{
    pendingUiFillIn_.store(static_cast<int>(fillIn), std::memory_order_release);
}

void DXRhythmboxAudioProcessor::setStandaloneStartHeldFromUi(bool held) noexcept
{
    uiStartHeld_.store(held, std::memory_order_release);
}

void DXRhythmboxAudioProcessor::setStandaloneStopHeldFromUi(bool held) noexcept
{
    uiStopHeld_.store(held, std::memory_order_release);
}

bool DXRhythmboxAudioProcessor::isStandaloneMode() const noexcept
{
    return forceStandaloneForTest_ || (wrapperType == wrapperType_Standalone);
}

std::uint32_t DXRhythmboxAudioProcessor::consumeMidiFlashMask() noexcept
{
    return midiFlashVoices_.exchange(0u, std::memory_order_acq_rel);
}

dx701::dsp::DXRhythmboxTelemetry DXRhythmboxAudioProcessor::getTelemetry() const noexcept
{
    dx701::dsp::DXRhythmboxTelemetry tel{};
    tel.outputRms = static_cast<double>(telRms_.load(std::memory_order_relaxed));
    tel.outputPeak = static_cast<double>(telPeak_.load(std::memory_order_relaxed));
    tel.lamp.effectiveBpm = static_cast<double>(telBpm_.load(std::memory_order_relaxed));
    tel.lamp.beatPhase = static_cast<double>(telBeatPhase_.load(std::memory_order_relaxed));

    const std::uint32_t packed = telPacked_.load(std::memory_order_relaxed);
    tel.lamp.isRunning = (packed & 0x01u) != 0u;
    tel.lamp.beatFlash = (packed & 0x02u) != 0u;
    tel.lamp.isDownbeat = (packed & 0x04u) != 0u;
    tel.lamp.usingHostBpm = (packed & 0x08u) != 0u;
    tel.lamp.fillInActive = (packed & 0x10u) != 0u;
    const int fillIdxRaw = static_cast<int>((packed >> 5u) & 0x07u);
    tel.lamp.activeFillInIndex = (tel.lamp.fillInActive && fillIdxRaw < static_cast<int>(dx701::dsp::kNumFillIns))
                                     ? fillIdxRaw
                                     : -1;
    tel.lamp.currentStep = static_cast<int>((packed >> 8u) & 0xFFu);
    tel.lamp.totalSteps = static_cast<int>((packed >> 16u) & 0xFFu);
    return tel;
}

void DXRhythmboxAudioProcessor::renderSlice(int startSample,
                                            int numSamples,
                                            juce::AudioBuffer<float>& buffer,
                                            const dx701::dsp::HostTransportInfo& host) noexcept
{
    if (numSamples <= 0)
    {
        return;
    }

    const int numCh = buffer.getNumChannels();
    double* scratchPtr = (numSamples <= static_cast<int>(monoScratch_.size())) ? monoScratch_.data() : nullptr;

    if (numCh == 1)
    {
        engine_.processBlockMono(buffer.getWritePointer(0, startSample), numSamples, host, scratchPtr);
        return;
    }

    float* outL = buffer.getWritePointer(0, startSample);
    float* outR = buffer.getWritePointer(1, startSample);
    engine_.processBlockStereo(outL, outR, numSamples, host, scratchPtr);
    for (int ch = 2; ch < numCh; ++ch)
    {
        buffer.copyFrom(ch, startSample, buffer, 0, startSample, numSamples);
    }
}

void DXRhythmboxAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples <= 0 || numChannels <= 0)
    {
        return;
    }

    auto rbState = rawParams_.readState();
    const bool enabledParam = rbState.enabled;
    const bool breakMode = rawParams_.readBreakEnabled();
    const bool midiStartOn = rawParams_.readMidiStartEnabled();
    const int midiStartCh = rawParams_.readMidiStartChannel();
    const bool stopCcOn = rawParams_.readStopCcEnabled();
    const int stopCcNum = rawParams_.readStopCcNumber();
    const int stopCcCh = rawParams_.readStopCcChannel();

    const bool uiStartHeld = uiStartHeld_.load(std::memory_order_acquire);
    const bool uiStopHeld = uiStopHeld_.load(std::memory_order_acquire);

    if (!stopCcOn)
    {
        stopCcHeld_ = false;
    }
    if (!midiStartOn)
    {
        heldTriggerNotes_.fill(0u);
        heldTriggerNoteCount_ = 0;
    }

    if (uiStopHeld && !prevUiStopHeld_)
    {
        rhythmLatched_ = false;
        engine_.restartSequencer();
    }
    prevUiStopHeld_ = uiStopHeld;
    bool stopActive = stopCcHeld_ || uiStopHeld;

    if (!enabledParam && prevEnabledParam_)
    {
        rhythmLatched_ = false;
        engine_.restartSequencer();
    }

    if (breakMode && !prevBreakMode_)
    {
        rhythmLatched_ = false;
        engine_.restartSequencer();
    }

    prevEnabledParam_ = enabledParam;
    prevBreakMode_ = breakMode;
    prevMidiStartOn_ = midiStartOn;

    if (uiStartHeld && !prevUiStartHeld_)
    {
        if (!breakMode && enabledParam && !stopActive && rbState.buttonMask != 0u)
        {
            rhythmLatched_ = true;
        }
    }
    prevUiStartHeld_ = uiStartHeld;

    // Read DAW playhead transport if available (disabled in Standalone so Host Sync never blocks)
    dx701::dsp::HostTransportInfo host{};
    if (!isStandaloneMode())
    {
        if (auto* ph = getPlayHead())
        {
            if (const auto pos = ph->getPosition())
            {
                host.hasTransport = true;
                host.isPlaying = pos->getIsPlaying();
                if (const auto bpmOpt = pos->getBpm())
                {
                    host.hasBpm = true;
                    host.bpm = *bpmOpt;
                }
                if (const auto ppqOpt = pos->getPpqPosition())
                {
                    host.hasPpqPosition = true;
                    host.ppqPosition = *ppqOpt;
                }
            }
        }
    }

    auto computeEffectiveEnabled = [&]() noexcept -> bool
    {
        const bool stopNow = stopCcHeld_ || uiStopHeld;
        if (stopNow || rbState.buttonMask == 0u)
        {
            return false;
        }
        if (breakMode)
        {
            const bool triggerHeld = uiStartHeld || (midiStartOn && heldTriggerNoteCount_ > 0);
            return enabledParam && triggerHeld;
        }
        const bool hostTransportDrives =
            !isStandaloneMode() && (rbState.clockMode == dx701::dsp::ClockMode::HostSync) &&
            host.hasTransport && !midiStartOn;
        return (hostTransportDrives && enabledParam) || rhythmLatched_;
    };

    const int listenChannel = rawParams_.readMidiChannel();

    // Flush queued UI audition voices at sample 0
    const std::uint32_t uiMask = pendingUiVoices_.exchange(0u, std::memory_order_acq_rel);
    if (uiMask != 0u)
    {
        for (std::size_t v = 0; v < dx701::dsp::kNumVoices; ++v)
        {
            if ((uiMask & (1u << static_cast<unsigned>(v))) != 0u)
            {
                engine_.triggerVoice(static_cast<dx701::dsp::VoiceId>(v), 1.0);
            }
        }
    }

    // Flush queued UI Drum Fill-In trigger at sample 0 (Intro continues if BREAK is OFF & pattern selected)
    const int uiFillIn = pendingUiFillIn_.exchange(-1, std::memory_order_acq_rel);
    if (!stopActive && uiFillIn >= 0 && uiFillIn < static_cast<int>(dx701::dsp::kNumFillIns))
    {
        const bool hasPattern = (rbState.buttonMask != 0u);
        const bool introContinue = hasPattern && !breakMode;
        if (introContinue)
        {
            rhythmLatched_ = true;
        }
        rbState.enabled = computeEffectiveEnabled();
        engine_.applyState(rbState);
        engine_.triggerFillIn(static_cast<dx701::dsp::FillInId>(uiFillIn), introContinue);
    }

    const bool usingHostBpm = (rbState.clockMode != dx701::dsp::ClockMode::Internal) &&
                              host.hasBpm && host.bpm >= 10.0;
    const double sliceBpm = usingHostBpm ? host.bpm : rbState.bpm;
    const double beatsPerSample = std::max(10.0, sliceBpm) / (60.0 * sampleRate_);

    auto syncBreakGate = [&]() noexcept -> bool
    {
        const bool gateNow = computeEffectiveEnabled();
        if (breakMode && gateNow != prevBreakGate_)
        {
            engine_.restartSequencer();
        }
        prevBreakGate_ = breakMode ? gateNow : false;
        return gateNow;
    };

    // Render sample-accurately around incoming MIDI CC (Rhythm Stop Switch) & Note-On/Off (Synchro-Start/Hold + GM Voices)
    int cursor = 0;
    std::uint32_t matchedMidiVoices = 0u;
    for (const auto meta : midiMessages)
    {
        const int eventSample = std::clamp(meta.samplePosition, cursor, numSamples);
        if (eventSample > cursor)
        {
            rbState.enabled = syncBreakGate();
            engine_.applyState(rbState);
            dx701::dsp::HostTransportInfo sliceHost = host;
            if (sliceHost.hasPpqPosition)
            {
                sliceHost.ppqPosition += static_cast<double>(cursor) * beatsPerSample;
            }
            renderSlice(cursor, eventSample - cursor, buffer, sliceHost);
            cursor = eventSample;
        }

        const auto msg = meta.getMessage();
        if (stopCcOn && msg.isController() && msg.getControllerNumber() == stopCcNum &&
            (stopCcCh == 0 || msg.getChannel() == stopCcCh))
        {
            const bool held = (msg.getControllerValue() >= 64);
            if (held)
            {
                stopCcHeld_ = true;
                rhythmLatched_ = false;
                engine_.restartSequencer();
            }
            else
            {
                stopCcHeld_ = false;
            }
        }

        if (midiStartOn && (midiStartCh == 0 || msg.getChannel() == midiStartCh))
        {
            if (msg.isNoteOn())
            {
                const int n = std::clamp(msg.getNoteNumber(), 0, 127);
                if (heldTriggerNotes_[static_cast<std::size_t>(n)] == 0u)
                {
                    heldTriggerNotes_[static_cast<std::size_t>(n)] = 1u;
                    ++heldTriggerNoteCount_;
                }
                if (!breakMode && enabledParam && !rhythmLatched_ && !(stopCcHeld_ || uiStopHeld) && rbState.buttonMask != 0u)
                {
                    rhythmLatched_ = true;
                }
            }
            else if (msg.isNoteOff())
            {
                const int n = std::clamp(msg.getNoteNumber(), 0, 127);
                if (heldTriggerNotes_[static_cast<std::size_t>(n)] != 0u)
                {
                    heldTriggerNotes_[static_cast<std::size_t>(n)] = 0u;
                    heldTriggerNoteCount_ = std::max(0, heldTriggerNoteCount_ - 1);
                }
            }
            else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            {
                heldTriggerNotes_.fill(0u);
                heldTriggerNoteCount_ = 0;
            }
        }

        if (msg.isNoteOn() && listenChannel >= 0 && (listenChannel == 0 || msg.getChannel() == listenChannel))
        {
            const auto voiceOpt = dx701::dsp::mapGmMidiNoteToVoice(msg.getNoteNumber());
            if (voiceOpt.has_value() && engine_.triggerMidiNote(msg.getNoteNumber(), static_cast<int>(msg.getVelocity())))
            {
                matchedMidiVoices |= (1u << static_cast<unsigned>(*voiceOpt));
            }
        }
    }

    if (matchedMidiVoices != 0u)
    {
        midiFlashVoices_.fetch_or(matchedMidiVoices, std::memory_order_release);
    }

    const bool finalEnabled = syncBreakGate();
    rbState.enabled = finalEnabled;
    engine_.applyState(rbState);
    if (cursor < numSamples)
    {
        dx701::dsp::HostTransportInfo tailHost = host;
        if (tailHost.hasPpqPosition)
        {
            tailHost.ppqPosition += static_cast<double>(cursor) * beatsPerSample;
        }
        renderSlice(cursor, numSamples - cursor, buffer, tailHost);
    }

    stopActive = stopCcHeld_ || uiStopHeld;
    telLatched_.store(finalEnabled, std::memory_order_relaxed);
    telStopHeld_.store(stopActive, std::memory_order_relaxed);
    telArmed_.store(enabledParam && !finalEnabled && !stopActive, std::memory_order_relaxed);

    // Publish whole-block telemetry atomics
    const float* ch0 = buffer.getReadPointer(0);
    double sumSq = 0.0;
    double pk = 0.0;
    for (int i = 0; i < numSamples; ++i)
    {
        const double s = static_cast<double>(ch0[i]);
        sumSq += s * s;
        pk = std::max(pk, std::abs(s));
    }

    const auto& tel = engine_.telemetry();
    telRms_.store(static_cast<float>(std::sqrt(sumSq / static_cast<double>(numSamples))), std::memory_order_relaxed);
    telPeak_.store(static_cast<float>(pk), std::memory_order_relaxed);
    telBpm_.store(static_cast<float>(tel.lamp.effectiveBpm), std::memory_order_relaxed);
    telBeatPhase_.store(static_cast<float>(tel.lamp.beatPhase), std::memory_order_relaxed);

    const std::uint32_t fillBits = tel.lamp.fillInActive
                                       ? (0x10u | ((static_cast<std::uint32_t>(std::clamp(tel.lamp.activeFillInIndex, 0, 3)) & 0x07u) << 5u))
                                       : (0x07u << 5u);
    const std::uint32_t packed =
        (tel.lamp.isRunning ? 0x01u : 0u) |
        (tel.lamp.beatFlash ? 0x02u : 0u) |
        (tel.lamp.isDownbeat ? 0x04u : 0u) |
        (tel.lamp.usingHostBpm ? 0x08u : 0u) |
        fillBits |
        ((static_cast<std::uint32_t>(std::clamp(tel.lamp.currentStep, 0, 255)) & 0xFFu) << 8u) |
        ((static_cast<std::uint32_t>(std::clamp(tel.lamp.totalSteps, 0, 255)) & 0xFFu) << 16u);
    telPacked_.store(packed, std::memory_order_relaxed);
}

int DXRhythmboxAudioProcessor::getNumPrograms()
{
    return static_cast<int>(kFactoryPrograms.size());
}

int DXRhythmboxAudioProcessor::getCurrentProgram()
{
    return currentProgram_;
}

void DXRhythmboxAudioProcessor::setCurrentProgram(int index)
{
    if (kFactoryPrograms.empty())
    {
        return;
    }
    const int idx = std::clamp(index, 0, static_cast<int>(kFactoryPrograms.size()) - 1);
    currentProgram_ = idx;
    const auto& prog = kFactoryPrograms[static_cast<std::size_t>(idx)];

    auto setParamNorm = [this](const char* id, float norm)
    {
        if (auto* p = apvts_.getParameter(id))
        {
            p->setValueNotifyingHost(norm);
        }
    };
    auto setParamPlain = [this](const char* id, float plain)
    {
        if (auto* p = apvts_.getParameter(id))
        {
            p->setValueNotifyingHost(p->convertTo0to1(plain));
        }
    };

    setParamNorm(param_ids::dxMultiPattern, 0.0f);
    setParamNorm(param_ids::dxVariation, prog.variation ? 1.0f : 0.0f);
    for (std::size_t b = 0; b < dx701::dsp::kNumButtons; ++b)
    {
        const bool on = (static_cast<int>(b) == prog.buttonIndex);
        setParamNorm(param_ids::kButtonParamIds[b], on ? 1.0f : 0.0f);
    }
    setParamPlain(param_ids::dxBpm, prog.defaultBpm);
}

const juce::String DXRhythmboxAudioProcessor::getProgramName(int index)
{
    if (index >= 0 && index < static_cast<int>(kFactoryPrograms.size()))
    {
        return kFactoryPrograms[static_cast<std::size_t>(index)].name;
    }
    return {};
}

void DXRhythmboxAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    const auto state = apvts_.copyState();
    if (const auto xml = state.createXml())
    {
        xml->setAttribute("currentProgram", currentProgram_);
        copyXmlToBinary(*xml, destData);
    }
}

void DXRhythmboxAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
    {
        return;
    }
    if (const auto xmlState = getXmlFromBinary(data, sizeInBytes))
    {
        if (xmlState->hasTagName(apvts_.state.getType()))
        {
            currentProgram_ = std::clamp(
                xmlState->getIntAttribute("currentProgram", currentProgram_),
                0,
                static_cast<int>(kFactoryPrograms.size()) - 1);
            apvts_.replaceState(juce::ValueTree::fromXml(*xmlState));
        }
    }
}

juce::AudioProcessorEditor* DXRhythmboxAudioProcessor::createEditor()
{
    return new DXRhythmboxAudioProcessorEditor(*this);
}

} // namespace dx701::plugin

#if !defined(DX_RHYTHMBOX_OMIT_CREATE_PLUGIN_FILTER)
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new dx701::plugin::DXRhythmboxAudioProcessor();
}
#endif
