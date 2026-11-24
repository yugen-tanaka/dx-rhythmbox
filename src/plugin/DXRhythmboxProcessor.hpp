#pragma once

#include "dsp/DXRhythmboxEngine.hpp"
#include "plugin/DXRhythmboxPluginParams.hpp"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace dx701::plugin
{

class DXRhythmboxAudioProcessor final : public juce::AudioProcessor
{
public:
    DXRhythmboxAudioProcessor();
    ~DXRhythmboxAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "DX Rhythmbox"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.5; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& apvts() noexcept { return apvts_; }

    // Lock-free UI <-> Audio bridges
    void triggerVoiceFromUi(dx701::dsp::VoiceId voice) noexcept;
    void triggerFillInFromUi(dx701::dsp::FillInId fillIn) noexcept;
    void setStandaloneStartHeldFromUi(bool held) noexcept;
    void setStandaloneStopHeldFromUi(bool held) noexcept;
    bool isStandaloneMode() const noexcept;
    void setForceStandaloneForTest(bool force) noexcept { forceStandaloneForTest_ = force; }
    bool isRhythmLatched() const noexcept { return telLatched_.load(std::memory_order_relaxed); }
    bool isStopSwitchHeld() const noexcept { return telStopHeld_.load(std::memory_order_relaxed); }
    bool isMidiArmed() const noexcept { return telArmed_.load(std::memory_order_relaxed); }
    std::uint32_t consumeMidiFlashMask() noexcept;
    dx701::dsp::DXRhythmboxTelemetry getTelemetry() const noexcept;

private:
    void renderSlice(int startSample,
                     int numSamples,
                     juce::AudioBuffer<float>& buffer,
                     const dx701::dsp::HostTransportInfo& host) noexcept;

    juce::AudioProcessorValueTreeState apvts_;
    CachedDXRhythmboxParams rawParams_{};
    int currentProgram_ = static_cast<int>(param_ids::kDefaultProgramIndex);

    double sampleRate_ = 48000.0;
    dx701::dsp::DXRhythmboxEngine engine_{};
    std::vector<double> monoScratch_;
    bool prevEnabledParam_ = false;
    bool prevBreakMode_ = false;
    bool prevMidiStartOn_ = false;
    bool prevUiStartHeld_ = false;
    bool prevUiStopHeld_ = false;
    bool prevBreakGate_ = false;
    bool rhythmLatched_ = false;
    bool stopCcHeld_ = false;
    bool forceStandaloneForTest_ = false;
    std::array<std::uint8_t, 128> heldTriggerNotes_{};
    int heldTriggerNoteCount_ = 0;

    std::atomic<std::uint32_t> pendingUiVoices_{0u};
    std::atomic<int> pendingUiFillIn_{-1};
    std::atomic<bool> uiStartHeld_{false};
    std::atomic<bool> uiStopHeld_{false};
    std::atomic<std::uint32_t> midiFlashVoices_{0u};

    std::atomic<float> telRms_{0.0f};
    std::atomic<float> telPeak_{0.0f};
    std::atomic<float> telBpm_{static_cast<float>(dx701::dsp::kDefaultBpm)};
    std::atomic<float> telBeatPhase_{0.0f};
    std::atomic<std::uint32_t> telPacked_{0u};
    std::atomic<bool> telLatched_{false};
    std::atomic<bool> telStopHeld_{false};
    std::atomic<bool> telArmed_{false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DXRhythmboxAudioProcessor)
};

} // namespace dx701::plugin
