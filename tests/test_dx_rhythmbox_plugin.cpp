// Catch2 unit, AudioPlayHead sync, MIDI, Fill-In, BREAK, and GUI tests for DX Rhythmbox plugin.
#include "dsp/Biquad.hpp"
#include "plugin/DXRhythmboxEditor.hpp"
#include "plugin/DXRhythmboxPluginParams.hpp"
#include "plugin/DXRhythmboxProcessor.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace
{

using namespace dx701::dsp;
using namespace dx701::plugin;

void setParamUnnorm(juce::AudioProcessorValueTreeState& apvts, const char* id, float unnormalizedValue)
{
    auto* p = apvts.getParameter(id);
    REQUIRE(p != nullptr);
    p->setValueNotifyingHost(p->convertTo0to1(unnormalizedValue));
}

float bufferMaxAbs(const juce::AudioBuffer<float>& buf, int startSample = 0, int numSamples = -1) noexcept
{
    const int n = (numSamples < 0) ? (buf.getNumSamples() - startSample) : numSamples;
    float m = 0.0f;
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    {
        const float* ptr = buf.getReadPointer(ch);
        for (int i = 0; i < n; ++i)
        {
            m = std::max(m, std::abs(ptr[startSample + i]));
        }
    }
    return m;
}

bool bufferAllFinite(const juce::AudioBuffer<float>& buf) noexcept
{
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    {
        const float* ptr = buf.getReadPointer(ch);
        for (int i = 0; i < buf.getNumSamples(); ++i)
        {
            if (!std::isfinite(ptr[i]))
            {
                return false;
            }
        }
    }
    return true;
}

class MockPlayHead final : public juce::AudioPlayHead
{
public:
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setIsPlaying(playing);
        info.setBpm(bpm);
        info.setPpqPosition(ppq);
        return info;
    }

    bool playing = false;
    double bpm = 120.0;
    double ppq = 0.0;
};

void saveImageToPng(const juce::Image& img, const juce::File& targetFile)
{
    targetFile.getParentDirectory().createDirectory();
    targetFile.deleteFile();
    juce::FileOutputStream stream(targetFile);
    REQUIRE(stream.openedOk());
    juce::PNGImageFormat pngFormat;
    REQUIRE(pngFormat.writeImageToStream(img, stream));
    stream.flush();
    REQUIRE(targetFile.existsAsFile());
    REQUIRE(targetFile.getSize() > 15000);
}

bool shouldExportScreenshots() noexcept
{
    const char* env = std::getenv("DX_EXPORT_GUI_PNG");
    return (env != nullptr) && (std::strcmp(env, "1") == 0);
}

} // namespace

TEST_CASE("DX Plugin 1: Stereo & Mono bus layouts, polite default silence, block edge safety", "[dx_plugin][buses]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    DXRhythmboxAudioProcessor proc;

    REQUIRE(proc.getName() == "DX Rhythmbox");
    REQUIRE(proc.getBusCount(true) == 0);
    REQUIRE(proc.getBusCount(false) == 1);
    REQUIRE(proc.getChannelCountOfBus(false, 0) == 2);
    REQUIRE(proc.acceptsMidi());
    REQUIRE_FALSE(proc.producesMidi());

    // Supported & rejected bus layouts
    juce::AudioProcessor::BusesLayout stereoLayout;
    stereoLayout.outputBuses.add(juce::AudioChannelSet::stereo());
    REQUIRE(proc.isBusesLayoutSupported(stereoLayout));

    juce::AudioProcessor::BusesLayout monoLayout;
    monoLayout.outputBuses.add(juce::AudioChannelSet::mono());
    REQUIRE(proc.isBusesLayoutSupported(monoLayout));

    juce::AudioProcessor::BusesLayout quadLayout;
    quadLayout.outputBuses.add(juce::AudioChannelSet::quadraphonic());
    REQUIRE_FALSE(proc.isBusesLayoutSupported(quadLayout));

    juce::AudioProcessor::BusesLayout withInputLayout;
    withInputLayout.inputBuses.add(juce::AudioChannelSet::stereo());
    withInputLayout.outputBuses.add(juce::AudioChannelSet::stereo());
    REQUIRE_FALSE(proc.isBusesLayoutSupported(withInputLayout));

    // Default state: dx_enabled == false -> polite silence on load
    proc.prepareToPlay(48000.0, 512);
    {
        juce::AudioBuffer<float> buf(2, 512);
        juce::MidiBuffer midi;
        proc.processBlock(buf, midi);
        REQUIRE(bufferAllFinite(buf));
        REQUIRE(bufferMaxAbs(buf) == 0.0f);
    }

    // Mono bus vs Stereo bus bit-identical L/R rendering
    auto& apvts = proc.apvts();
    setParamUnnorm(apvts, param_ids::dxSyncMode, 0.0f); // Internal
    setParamUnnorm(apvts, param_ids::dxEnabled, 1.0f);
    setParamUnnorm(apvts, param_ids::dxAnalogDrift, 0.0f);
    setParamUnnorm(apvts, param_ids::dxVolume, 1.0f);

    proc.setBusesLayout(monoLayout);
    proc.prepareToPlay(48000.0, 512);
    proc.setStandaloneStartHeldFromUi(true);
    juce::AudioBuffer<float> monoBuf(1, 512);
    juce::MidiBuffer emptyMidi;
    proc.processBlock(monoBuf, emptyMidi);
    proc.setStandaloneStartHeldFromUi(false);
    REQUIRE(bufferAllFinite(monoBuf));
    REQUIRE(monoBuf.getRMSLevel(0, 0, 512) > 1e-3f);

    proc.setBusesLayout(stereoLayout);
    proc.prepareToPlay(48000.0, 512);
    proc.setStandaloneStartHeldFromUi(true);
    juce::AudioBuffer<float> stereoBuf(2, 512);
    proc.processBlock(stereoBuf, emptyMidi);
    proc.setStandaloneStartHeldFromUi(false);
    REQUIRE(bufferAllFinite(stereoBuf));
    for (int i = 0; i < 512; ++i)
    {
        REQUIRE(stereoBuf.getSample(0, i) == monoBuf.getSample(0, i));
        REQUIRE(stereoBuf.getSample(1, i) == monoBuf.getSample(0, i));
    }

    // 0-sample and oversize-block (2048 > prepared 512) edge safety
    juce::AudioBuffer<float> zeroBuf(2, 0);
    proc.processBlock(zeroBuf, emptyMidi);

    juce::AudioBuffer<float> bigBuf(2, 2048);
    proc.processBlock(bigBuf, emptyMidi);
    REQUIRE(bufferAllFinite(bigBuf));
    REQUIRE(bufferMaxAbs(bigBuf) < 1.5f);
}

TEST_CASE("DX Plugin 2: All 32 patterns (16 buttons x VARIATION) non-silent, bounded & distinct", "[dx_plugin][patterns]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    DXRhythmboxAudioProcessor proc;
    auto& apvts = proc.apvts();
    setParamUnnorm(apvts, param_ids::dxSyncMode, 0.0f);
    setParamUnnorm(apvts, param_ids::dxEnabled, 1.0f);
    setParamUnnorm(apvts, param_ids::dxAnalogDrift, 0.0f);
    setParamUnnorm(apvts, param_ids::dxVolume, 0.90f);

    juce::MidiBuffer midi;
    for (std::size_t btn = 0; btn < kNumButtons; ++btn)
    {
        for (std::size_t j = 0; j < kNumButtons; ++j)
        {
            setParamUnnorm(apvts, param_ids::kButtonParamIds[j], (j == btn) ? 1.0f : 0.0f);
        }

        auto renderVar = [&](bool varOn)
        {
            setParamUnnorm(apvts, param_ids::dxVariation, varOn ? 1.0f : 0.0f);
            proc.prepareToPlay(48000.0, 512);
            proc.setStandaloneStartHeldFromUi(true);
            constexpr int kTotal = 76800; // 1.6s covers > 3 beats at 112.2 BPM
            juce::AudioBuffer<float> out(2, kTotal);
            juce::AudioBuffer<float> chunk(2, 512);
            for (int offset = 0; offset < kTotal; offset += 512)
            {
                proc.processBlock(chunk, midi);
                if (offset == 0)
                {
                    proc.setStandaloneStartHeldFromUi(false);
                }
                REQUIRE(bufferAllFinite(chunk));
                REQUIRE(bufferMaxAbs(chunk) < 1.5f);
                out.copyFrom(0, offset, chunk, 0, 0, 512);
                out.copyFrom(1, offset, chunk, 1, 0, 512);
            }
            REQUIRE(out.getRMSLevel(0, 0, kTotal) > 5e-4f);
            return out;
        };

        auto offBuf = renderVar(false);
        auto onBuf = renderVar(true);

        double diffSq = 0.0;
        for (int i = 0; i < 76800; ++i)
        {
            const double d = static_cast<double>(offBuf.getSample(0, i)) - static_cast<double>(onBuf.getSample(0, i));
            diffSq += d * d;
        }
        REQUIRE(std::sqrt(diffSq / 76800.0) > 1e-4);
    }
}

TEST_CASE("DX Plugin 3: Wired-OR Multi-Pattern & 4-pattern hardware cap", "[dx_plugin][wired_or]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    DXRhythmboxAudioProcessor proc;
    auto& apvts = proc.apvts();

    CachedDXRhythmboxParams cache;
    cache.bind(apvts);

    for (std::size_t j = 0; j < kNumButtons; ++j)
    {
        // Enable 6 buttons simultaneously: indices 0, 2, 5, 8, 11, 14
        const bool on = (j == 0 || j == 2 || j == 5 || j == 8 || j == 11 || j == 14);
        setParamUnnorm(apvts, param_ids::kButtonParamIds[j], on ? 1.0f : 0.0f);
    }

    // Single mode keeps only lowest bit (bit 0)
    setParamUnnorm(apvts, param_ids::dxMultiPattern, 0.0f);
    REQUIRE(std::popcount(cache.readState().buttonMask) == 1);
    REQUIRE(cache.readState().buttonMask == (1u << 0u));

    // Multi-pattern mode caps to first 4 active buttons (0, 2, 5, 8)
    setParamUnnorm(apvts, param_ids::dxMultiPattern, 1.0f);
    const auto st = cache.readState();
    REQUIRE(st.multiPatternEnabled);
    REQUIRE(std::popcount(st.buttonMask) == 4);
    REQUIRE(st.buttonMask == ((1u << 0u) | (1u << 2u) | (1u << 5u) | (1u << 8u)));
}

TEST_CASE("DX Plugin 4: Sample-accurate GM MIDI triggers across all 16 voices & channel filter", "[dx_plugin][midi]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    DXRhythmboxAudioProcessor proc;
    auto& apvts = proc.apvts();

    setParamUnnorm(apvts, param_ids::dxEnabled, 0.0f);
    setParamUnnorm(apvts, param_ids::dxVolume, 1.0f);
    setParamUnnorm(apvts, param_ids::dxMidiCh, 0.0f); // Omni

    for (std::size_t v = 0; v < kNumVoices; ++v)
    {
        const int note = kPrimaryGmMidiNotes[v];
        proc.prepareToPlay(48000.0, 512);
        juce::AudioBuffer<float> buf(2, 512);
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(10, note, 1.0f), 200);
        proc.processBlock(buf, midi);
        REQUIRE(bufferAllFinite(buf));
        REQUIRE(bufferMaxAbs(buf, 0, 200) == 0.0f);
        REQUIRE(bufferMaxAbs(buf, 200, 312) > 5e-4f);

        const std::uint32_t flashMask = proc.consumeMidiFlashMask();
        REQUIRE((flashMask & (1u << static_cast<unsigned>(v))) != 0u);
    }

    // Ch 10 ignores Ch 1 and accepts Ch 10
    setParamUnnorm(apvts, param_ids::dxMidiCh, 10.0f);
    proc.prepareToPlay(48000.0, 512);
    {
        juce::AudioBuffer<float> bufReject(2, 512);
        juce::MidiBuffer midiCh1;
        midiCh1.addEvent(juce::MidiMessage::noteOn(1, 36, 1.0f), 0);
        proc.processBlock(bufReject, midiCh1);
        REQUIRE(bufferMaxAbs(bufReject) == 0.0f);

        juce::AudioBuffer<float> bufAccept(2, 512);
        juce::MidiBuffer midiCh10;
        midiCh10.addEvent(juce::MidiMessage::noteOn(10, 36, 1.0f), 0);
        proc.processBlock(bufAccept, midiCh10);
        REQUIRE(bufferMaxAbs(bufAccept) > 0.02f);
    }

    // Off (index 17) blocks all MIDI triggers
    setParamUnnorm(apvts, param_ids::dxMidiCh, 17.0f);
    proc.prepareToPlay(48000.0, 512);
    {
        juce::AudioBuffer<float> bufOff(2, 512);
        juce::MidiBuffer midiCh10;
        midiCh10.addEvent(juce::MidiMessage::noteOn(10, 36, 1.0f), 0);
        proc.processBlock(bufOff, midiCh10);
        REQUIRE(bufferMaxAbs(bufOff) == 0.0f);
    }
}

TEST_CASE("DX Plugin 5: AudioPlayHead HostSync (PPQ/BPM/Loop), TempoSync & Internal fallback", "[dx_plugin][sync]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    DXRhythmboxAudioProcessor proc;
    MockPlayHead playHead;
    proc.setPlayHead(&playHead);
    proc.prepareToPlay(48000.0, 512);
    auto& apvts = proc.apvts();

    proc.setCurrentProgram(static_cast<int>(PatternId::March1));
    setParamUnnorm(apvts, param_ids::dxEnabled, 1.0f);
    setParamUnnorm(apvts, param_ids::dxSyncMode, 2.0f); // Host Sync
    setParamUnnorm(apvts, param_ids::dxVolume, 1.0f);

    // 1) Host stopped -> silent
    playHead.playing = false;
    playHead.bpm = 120.0;
    playHead.ppq = 0.0;
    juce::AudioBuffer<float> buf(2, 512);
    juce::MidiBuffer midi;
    proc.processBlock(buf, midi);
    REQUIRE(bufferMaxAbs(buf) == 0.0f);
    REQUIRE_FALSE(proc.getTelemetry().lamp.isRunning);

    // 2) Host playing at 150 BPM -> running + downbeat trigger
    playHead.playing = true;
    playHead.bpm = 150.0;
    playHead.ppq = 0.0;
    proc.prepareToPlay(48000.0, 512);
    proc.processBlock(buf, midi);
    REQUIRE(proc.getTelemetry().lamp.isRunning);
    REQUIRE(proc.getTelemetry().lamp.usingHostBpm);
    REQUIRE(proc.getTelemetry().lamp.effectiveBpm == Catch::Approx(150.0).margin(0.05));
    REQUIRE(bufferMaxAbs(buf, 0, 256) > 0.02f);

    // 3) DAW loop-back to PPQ = 0.0 triggers Beat 1 cleanly at sample 0
    playHead.ppq = 3.5;
    proc.processBlock(buf, midi);
    playHead.ppq = 0.0;
    proc.processBlock(buf, midi);
    REQUIRE(bufferMaxAbs(buf, 0, 256) > 0.02f);

    // 4) Host Tempo mode (index 1) follows DAW BPM (142.0) even while transport stopped (once started)
    setParamUnnorm(apvts, param_ids::dxSyncMode, 1.0f);
    playHead.playing = false;
    playHead.bpm = 142.0;
    proc.setStandaloneStartHeldFromUi(true);
    proc.processBlock(buf, midi);
    proc.setStandaloneStartHeldFromUi(false);
    REQUIRE(proc.getTelemetry().lamp.isRunning);
    REQUIRE(proc.getTelemetry().lamp.effectiveBpm == Catch::Approx(142.0).margin(0.05));

    // 5) Internal mode ignores host BPM & transport
    setParamUnnorm(apvts, param_ids::dxSyncMode, 0.0f);
    setParamUnnorm(apvts, param_ids::dxBpm, 98.5f);
    proc.processBlock(buf, midi);
    REQUIRE(proc.getTelemetry().lamp.isRunning);
    REQUIRE_FALSE(proc.getTelemetry().lamp.usingHostBpm);
    REQUIRE(proc.getTelemetry().lamp.effectiveBpm == Catch::Approx(98.5).margin(0.05));
}

TEST_CASE("DX Plugin 6: Drum Fill-In Intro (auto-continue into rhythm), empty-pattern stop & latching BREAK",
          "[dx_plugin][fillin][break]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    DXRhythmboxAudioProcessor proc;
    auto& apvts = proc.apvts();

    setParamUnnorm(apvts, param_ids::dxSyncMode, 0.0f);
    setParamUnnorm(apvts, param_ids::dxBpm, 120.0f); // 1 bar (4 beats) = 2.0s = 96000 samples
    setParamUnnorm(apvts, param_ids::dxVolume, 1.0f);
    setParamUnnorm(apvts, param_ids::dxEnabled, 0.0f);

    juce::MidiBuffer midi;
    // 1) Owner's Manual Intro: rhythm pattern selected (Rock I) + stopped -> Fill-In plays 1 bar and continues into rhythm!
    for (int f = 0; f < static_cast<int>(kNumFillIns); ++f)
    {
        setParamUnnorm(apvts, param_ids::kButtonParamIds[param_ids::kDefaultButtonIndex], 1.0f);
        setParamUnnorm(apvts, param_ids::dxEnabled, 0.0f);
        proc.prepareToPlay(48000.0, 512);
        proc.triggerFillInFromUi(static_cast<FillInId>(f));

        juce::AudioBuffer<float> buf(2, 512);
        proc.processBlock(buf, midi);
        const auto telStart = proc.getTelemetry();
        REQUIRE(telStart.lamp.fillInActive);
        REQUIRE(telStart.lamp.activeFillInIndex == f);
        REQUIRE(telStart.lamp.isRunning);
        REQUIRE(proc.isRhythmLatched());
        REQUIRE(bufferMaxAbs(buf) > 1e-3f);

        // Render full 2.2s (> 2.0s bar): Fill-In transitions seamlessly into selected rhythm
        for (int b = 0; b < 210; ++b)
        {
            proc.processBlock(buf, midi);
        }
        const auto telEnd = proc.getTelemetry();
        REQUIRE_FALSE(telEnd.lamp.fillInActive);
        REQUIRE(telEnd.lamp.isRunning);
        REQUIRE(proc.isRhythmLatched());
    }

    // 2) If NO rhythm pattern is selected (all pattern buttons OFF), Fill-In stops cleanly after 1 bar
    for (std::size_t bIdx = 0; bIdx < kNumButtons; ++bIdx)
    {
        setParamUnnorm(apvts, param_ids::kButtonParamIds[bIdx], 0.0f);
    }
    setParamUnnorm(apvts, param_ids::dxEnabled, 0.0f);
    proc.prepareToPlay(48000.0, 512);
    proc.triggerFillInFromUi(FillInId::FillIn1);
    juce::AudioBuffer<float> soloBuf(2, 512);
    proc.processBlock(soloBuf, midi);
    REQUIRE(proc.getTelemetry().lamp.fillInActive);
    for (int b = 0; b < 210; ++b)
    {
        proc.processBlock(soloBuf, midi);
    }
    REQUIRE_FALSE(proc.getTelemetry().lamp.fillInActive);
    REQUIRE_FALSE(proc.getTelemetry().lamp.isRunning);
    REQUIRE_FALSE(proc.isRhythmLatched());

    // Restore Rock I for BREAK test
    setParamUnnorm(apvts, param_ids::kButtonParamIds[param_ids::kDefaultButtonIndex], 1.0f);

    // 3) BREAK latching mode: plays from Beat 1 only while Standalone START or MIDI trigger key is held
    proc.prepareToPlay(48000.0, 512);
    setParamUnnorm(apvts, param_ids::dxBreak, 0.0f);
    setParamUnnorm(apvts, param_ids::dxEnabled, 1.0f);
    proc.setStandaloneStartHeldFromUi(true);
    juce::AudioBuffer<float> rbuf(2, 512);
    proc.processBlock(rbuf, midi);
    proc.setStandaloneStartHeldFromUi(false);
    REQUIRE(proc.getTelemetry().lamp.isRunning);

    setParamUnnorm(apvts, param_ids::dxBreak, 1.0f);
    for (int b = 0; b < 80; ++b)
    {
        proc.processBlock(rbuf, midi);
    }
    REQUIRE_FALSE(proc.getTelemetry().lamp.isRunning);
    REQUIRE(bufferMaxAbs(rbuf) < 1e-3f);

    // Holding Standalone START in BREAK mode plays from Beat 1, releasing stops cleanly
    proc.setStandaloneStartHeldFromUi(true);
    proc.processBlock(rbuf, midi);
    REQUIRE(proc.getTelemetry().lamp.isRunning);
    REQUIRE(bufferMaxAbs(rbuf) > 0.02f);

    proc.setStandaloneStartHeldFromUi(false);
    for (int b = 0; b < 80; ++b)
    {
        proc.processBlock(rbuf, midi);
    }
    REQUIRE_FALSE(proc.getTelemetry().lamp.isRunning);

    // Holding MIDI key on trigger channel in BREAK mode also plays while held and stops on Note-Off
    setParamUnnorm(apvts, param_ids::dxMidiStartEn, 1.0f);
    setParamUnnorm(apvts, param_ids::dxMidiStartCh, 2.0f);
    juce::MidiBuffer holdMidi;
    holdMidi.addEvent(juce::MidiMessage::noteOn(2, 48, 0.8f), 0);
    proc.processBlock(rbuf, holdMidi);
    REQUIRE(proc.getTelemetry().lamp.isRunning);
    REQUIRE(bufferMaxAbs(rbuf) > 0.02f);

    holdMidi.clear();
    holdMidi.addEvent(juce::MidiMessage::noteOff(2, 48), 0);
    proc.processBlock(rbuf, holdMidi);
    REQUIRE_FALSE(proc.getTelemetry().lamp.isRunning);
    setParamUnnorm(apvts, param_ids::dxBreak, 0.0f);
    setParamUnnorm(apvts, param_ids::dxMidiStartEn, 0.0f);
}

TEST_CASE("DX Plugin 7: TONE tilt, 32 factory programs & XML state round-trip + invalid data safety", "[dx_plugin][state]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    DXRhythmboxAudioProcessor proc;
    proc.prepareToPlay(48000.0, 512);
    auto& apvts = proc.apvts();

    REQUIRE(proc.getNumPrograms() == 32);
    for (int i = 0; i < 32; ++i)
    {
        REQUIRE(proc.getProgramName(i).isNotEmpty());
    }

    auto measureToneRatioDb = [&](float toneVal)
    {
        proc.prepareToPlay(48000.0, 512);
        proc.setCurrentProgram(static_cast<int>(PatternId::Rock1));
        setParamUnnorm(apvts, param_ids::dxSyncMode, 0.0f);
        setParamUnnorm(apvts, param_ids::dxEnabled, 1.0f);
        setParamUnnorm(apvts, param_ids::dxTone, toneVal);
        setParamUnnorm(apvts, param_ids::dxVolume, 1.0f);

        BiquadCascade<1> lp500;
        BiquadCascade<1> hp3000;
        const FilterSectionSpec lpS{FilterType::LP2, 500.0, 0.707, 0.0};
        const FilterSectionSpec hpS{FilterType::HP2, 3000.0, 0.707, 0.0};
        lp500.design(&lpS, 1u, 0.0, 48000.0);
        hp3000.design(&hpS, 1u, 0.0, 48000.0);

        double eLow = 0.0;
        double eHigh = 0.0;
        juce::AudioBuffer<float> buf(2, 512);
        juce::MidiBuffer midi;
        proc.setStandaloneStartHeldFromUi(true);
        for (int b = 0; b < 94; ++b)
        {
            proc.processBlock(buf, midi);
            if (b == 0)
            {
                proc.setStandaloneStartHeldFromUi(false);
            }
            const float* p = buf.getReadPointer(0);
            for (int i = 0; i < 512; ++i)
            {
                const double lo = lp500.processSample(p[i]);
                const double hi = hp3000.processSample(p[i]);
                eLow += lo * lo;
                eHigh += hi * hi;
            }
        }
        return 10.0 * std::log10((eLow + 1e-24) / (eHigh + 1e-24));
    };

    const double rDrum = measureToneRatioDb(0.0f);
    const double rFlat = measureToneRatioDb(0.5f);
    const double rCym = measureToneRatioDb(1.0f);
    REQUIRE(rDrum > rFlat + 4.0);
    REQUIRE(rCym < rFlat - 4.0);

    // XML round-trip with multi-pattern state
    proc.setCurrentProgram(static_cast<int>(PatternId::ChaCha));
    REQUIRE(proc.getCurrentProgram() == static_cast<int>(PatternId::ChaCha));
    setParamUnnorm(apvts, param_ids::dxMultiPattern, 1.0f);
    setParamUnnorm(apvts, param_ids::kButtonParamIds[5], 1.0f); // + Rock
    setParamUnnorm(apvts, param_ids::dxTone, 0.75f);
    setParamUnnorm(apvts, param_ids::dxBpm, 128.4f);

    juce::MemoryBlock stateBlob;
    proc.getStateInformation(stateBlob);
    REQUIRE(stateBlob.getSize() > 100);

    proc.setCurrentProgram(0);
    REQUIRE(apvts.getParameter(param_ids::dxMultiPattern)->getValue() < 0.5f);

    proc.setStateInformation(stateBlob.getData(), static_cast<int>(stateBlob.getSize()));
    REQUIRE(proc.getCurrentProgram() == static_cast<int>(PatternId::ChaCha));
    REQUIRE(apvts.getParameter(param_ids::dxMultiPattern)->getValue() > 0.5f);
    REQUIRE(apvts.getParameter(param_ids::kButtonParamIds[11])->getValue() > 0.5f);
    REQUIRE(apvts.getParameter(param_ids::kButtonParamIds[5])->getValue() > 0.5f);
    const auto* bpmParam = apvts.getParameter(param_ids::dxBpm);
    REQUIRE(bpmParam->convertFrom0to1(bpmParam->getValue()) == Catch::Approx(128.4f).margin(0.15f));

    // Corrupt / empty buffer must not crash or corrupt valid state
    proc.setStateInformation(nullptr, 0);
    const char garbage[] = "not_valid_xml_at_all";
    proc.setStateInformation(garbage, static_cast<int>(sizeof(garbage)));
    REQUIRE(proc.getCurrentProgram() == static_cast<int>(PatternId::ChaCha));
}

TEST_CASE("DX Plugin 8: Hardware GUI rendering, Multi-Pattern 4-cap, Fill-In click, resize & env-gated PNG export",
          "[dx_plugin][gui]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    DXRhythmboxAudioProcessor proc;
    proc.prepareToPlay(48000.0, 512);
    auto& apvts = proc.apvts();

    proc.setCurrentProgram(static_cast<int>(PatternId::Rock1));
    setParamUnnorm(apvts, param_ids::dxSyncMode, 0.0f);
    setParamUnnorm(apvts, param_ids::dxEnabled, 1.0f);
    setParamUnnorm(apvts, param_ids::dxBpm, 112.2f);
    setParamUnnorm(apvts, param_ids::dxVolume, 0.85f);

    juce::AudioBuffer<float> buf(2, 512);
    juce::MidiBuffer midi;
    proc.setStandaloneStartHeldFromUi(true);
    proc.processBlock(buf, midi);
    proc.setStandaloneStartHeldFromUi(false);
    REQUIRE(proc.getTelemetry().lamp.isRunning);
    REQUIRE(proc.getTelemetry().lamp.beatFlash);

    std::unique_ptr<juce::AudioProcessorEditor> rawEditor(proc.createEditorIfNeeded());
    REQUIRE(rawEditor != nullptr);
    auto* dxEditor = dynamic_cast<DXRhythmboxAudioProcessorEditor*>(rawEditor.get());
    REQUIRE(dxEditor != nullptr);
    REQUIRE(dxEditor->getWidth() == 960);
    REQUIRE(dxEditor->getHeight() == 600);
    auto& panel = dxEditor->rhythmboxPanel();
    panel.refreshTelemetryNow();

    // Multi-Pattern checkbox must NOT show "(Max 4)"
    REQUIRE(panel.multiPatternButton().getButtonText() == "Multi-Pattern");

    // GM Drum Map pads expose ISO note names (e.g. BD = C2 / MIDI 36) in tooltip
    REQUIRE(panel.auditionPad(static_cast<std::size_t>(VoiceId::BassDrum)).getTooltip().contains("C2"));

    // Verify inverted VOLUME & TEMPO vertical sliders (MAX = 0.0 bottom, MIN = 1.0 top) vs normal TONE
    REQUIRE(panel.toneSlider().valueToProportionOfLength(1.0) == Catch::Approx(1.0).margin(1e-4));
    REQUIRE(panel.toneSlider().valueToProportionOfLength(0.0) == Catch::Approx(0.0).margin(1e-4));
    REQUIRE(panel.volumeSlider().valueToProportionOfLength(1.0) == Catch::Approx(0.0).margin(1e-4));
    REQUIRE(panel.volumeSlider().valueToProportionOfLength(0.0) == Catch::Approx(1.0).margin(1e-4));
    REQUIRE(panel.bpmSlider().valueToProportionOfLength(300.0) == Catch::Approx(0.0).margin(1e-4));
    REQUIRE(panel.bpmSlider().valueToProportionOfLength(40.0) == Catch::Approx(1.0).margin(1e-4));

    // Direct BPM text input synced and grouped below TEMPO label
    REQUIRE(panel.bpmInput().getText() == "112.2");
    panel.bpmInput().setText("135.5", juce::dontSendNotification);
    panel.bpmInput().onReturnKey();
    REQUIRE(panel.bpmSlider().getValue() == Catch::Approx(135.5).margin(0.15));
    panel.bpmInput().setText("300.0", juce::dontSendNotification);
    panel.bpmInput().onReturnKey();
    REQUIRE(panel.bpmSlider().getValue() == Catch::Approx(300.0).margin(0.15));
    panel.bpmInput().setText("112.2", juce::dontSendNotification);
    panel.bpmInput().onReturnKey();
    REQUIRE(panel.bpmSlider().getValue() == Catch::Approx(112.2).margin(0.15));

    // Standalone-only yellow START / STOP buttons hidden in plugin mode, visible in Standalone mode
    REQUIRE_FALSE(panel.standaloneStartButton().isVisible());
    REQUIRE_FALSE(panel.standaloneStopButton().isVisible());
    proc.setForceStandaloneForTest(true);
    dxEditor->syncUiNow();
    REQUIRE(panel.standaloneStartButton().isVisible());
    REQUIRE(panel.standaloneStopButton().isVisible());
    proc.setForceStandaloneForTest(false);
    dxEditor->syncUiNow();
    REQUIRE_FALSE(panel.standaloneStartButton().isVisible());
    REQUIRE_FALSE(panel.standaloneStopButton().isVisible());

    // Gear Settings overlay open/close check
    REQUIRE_FALSE(dxEditor->isSettingsOpen());
    dxEditor->settingsGearButton().setToggleState(true, juce::sendNotificationSync);
    REQUIRE(dxEditor->isSettingsOpen());
    dxEditor->settingsGearButton().setToggleState(false, juce::sendNotificationSync);
    REQUIRE_FALSE(dxEditor->isSettingsOpen());

    // Render default GUI snapshot (Plugin mode)
    juce::Image heroImg(juce::Image::ARGB, dxEditor->getWidth(), dxEditor->getHeight(), true);
    {
        juce::Graphics g(heroImg);
        dxEditor->paintEntireComponent(g, true);
    }
    REQUIRE(!heroImg.isNull());
    REQUIRE(heroImg.getPixelAt(30, 14) != heroImg.getPixelAt(200, 180));

    if (shouldExportScreenshots())
    {
        const juce::File repoRoot(DX_RHYTHMBOX_SOURCE_DIR);
        REQUIRE(repoRoot.isDirectory());
        saveImageToPng(heroImg, repoRoot.getChildFile("assets/gui_screenshot.png"));
        saveImageToPng(heroImg, repoRoot.getChildFile("research/figures/08_dx_rhythmbox_gui.png"));

        // Second snapshot: VARIATION ON (BOSSA-NOVA II) + Drum Fill In 2 actively self-illuminated!
        proc.setCurrentProgram(static_cast<int>(PatternId::BossaNova2));
        setParamUnnorm(apvts, param_ids::dxEnabled, 1.0f);
        panel.fillInButton(2).onClick(); // Slot 2 = FILL_IN_2 (16th notes)
        proc.processBlock(buf, midi);
        dxEditor->syncUiNow();
        REQUIRE(panel.fillInButton(2).isIlluminated());

        juce::Image litImg(juce::Image::ARGB, dxEditor->getWidth(), dxEditor->getHeight(), true);
        {
            juce::Graphics lg(litImg);
            dxEditor->paintEntireComponent(lg, true);
        }
        saveImageToPng(litImg, repoRoot.getChildFile("research/figures/08b_dx_rhythmbox_gui_fillin_lit.png"));

        // Third snapshot: Gear Settings overlay open (plugin mode background)
        dxEditor->settingsGearButton().setToggleState(true, juce::sendNotificationSync);
        dxEditor->syncUiNow();
        juce::Image settingsImg(juce::Image::ARGB, dxEditor->getWidth(), dxEditor->getHeight(), true);
        {
            juce::Graphics sg(settingsImg);
            dxEditor->paintEntireComponent(sg, true);
        }
        saveImageToPng(settingsImg, repoRoot.getChildFile("research/figures/08c_dx_rhythmbox_gui_settings.png"));
        dxEditor->settingsGearButton().setToggleState(false, juce::sendNotificationSync);

        // Fourth snapshot: Standalone mode showing yellow START / STOP buttons right of RHYTHMER
        proc.setForceStandaloneForTest(true);
        proc.setCurrentProgram(static_cast<int>(PatternId::Rock1));
        dxEditor->syncUiNow();
        juce::Image standaloneImg(juce::Image::ARGB, dxEditor->getWidth(), dxEditor->getHeight(), true);
        {
            juce::Graphics stg(standaloneImg);
            dxEditor->paintEntireComponent(stg, true);
        }
        saveImageToPng(standaloneImg, repoRoot.getChildFile("research/figures/08d_dx_rhythmbox_gui_standalone.png"));
        proc.setForceStandaloneForTest(false);
        dxEditor->syncUiNow();
    }

    // Verify audition pad click enqueues audio
    setParamUnnorm(apvts, param_ids::dxEnabled, 0.0f);
    proc.prepareToPlay(48000.0, 512);
    proc.processBlock(buf, midi);
    REQUIRE(bufferMaxAbs(buf) == 0.0f);
    panel.auditionPad(static_cast<std::size_t>(VoiceId::BassDrum)).onClick();
    proc.processBlock(buf, midi);
    REQUIRE(bufferMaxAbs(buf) > 0.02f);

    // Verify Single mode & Multi-Pattern 4-button cap in UI
    auto countBtns = [&]()
    {
        int cnt = 0;
        for (std::size_t j = 0; j < kNumButtons; ++j)
        {
            if (apvts.getParameter(param_ids::kButtonParamIds[j])->getValue() > 0.5f)
            {
                ++cnt;
            }
        }
        return cnt;
    };
    auto clickBtn = [&](std::size_t idx)
    {
        auto& b = panel.patternButton(idx);
        panel.handlePatternPressState(idx, true);
        b.setToggleState(!b.getToggleState(), juce::sendNotificationSync);
        panel.handlePatternPressState(idx, false);
    };

    panel.multiPatternButton().setToggleState(false, juce::sendNotificationSync);
    clickBtn(0);
    REQUIRE(countBtns() == 1);
    clickBtn(12);
    REQUIRE(countBtns() == 1);
    REQUIRE(apvts.getParameter(param_ids::kButtonParamIds[12])->getValue() > 0.5f);

    // Unlatch same button -> 0 active
    clickBtn(12);
    REQUIRE(countBtns() == 0);

    // Multi-Pattern max 4 enforcement when clicking 5 distinct buttons
    panel.multiPatternButton().setToggleState(true, juce::sendNotificationSync);
    for (std::size_t bIdx : {0u, 2u, 4u, 6u})
    {
        clickBtn(bIdx);
    }
    REQUIRE(countBtns() == 4);
    clickBtn(8u); // 5th button should be rejected cleanly by 4-pattern hardware cap
    REQUIRE(countBtns() == 4);
    REQUIRE(apvts.getParameter(param_ids::kButtonParamIds[8])->getValue() < 0.5f);

    // Dynamic resizing at min and max bounds
    dxEditor->setSize(800, 520);
    REQUIRE(dxEditor->getWidth() == 800);
    REQUIRE(dxEditor->getHeight() == 520);
    juce::Image minImg(juce::Image::ARGB, 800, 520, true);
    {
        juce::Graphics mg(minImg);
        dxEditor->paintEntireComponent(mg, true);
    }
    REQUIRE(!minImg.isNull());

    dxEditor->setSize(1280, 800);
    REQUIRE(dxEditor->getWidth() == 1280);
    REQUIRE(dxEditor->getHeight() == 800);

    rawEditor.reset();
}

TEST_CASE("DX Plugin 9: MIDI Key Trigger Start, Expression Rhythm Stop CC & Standalone playhead bypass",
          "[dx_plugin][midi_start][stop_cc][standalone]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    DXRhythmboxAudioProcessor proc;
    auto& apvts = proc.apvts();

    // Setup Rock I, Internal, Drum voices OFF (index 17) so we isolate rhythm sequencer behavior cleanly
    proc.setCurrentProgram(static_cast<int>(PatternId::Rock1));
    setParamUnnorm(apvts, param_ids::dxSyncMode, 0.0f);
    setParamUnnorm(apvts, param_ids::dxMidiCh, 17.0f); // Off for manual drum key playback
    setParamUnnorm(apvts, param_ids::dxMidiStartEn, 1.0f);
    setParamUnnorm(apvts, param_ids::dxMidiStartCh, 2.0f); // Ch 2
    setParamUnnorm(apvts, param_ids::dxStopCcEn, 1.0f);
    setParamUnnorm(apvts, param_ids::dxStopCcNum, 67.0f);
    setParamUnnorm(apvts, param_ids::dxStopCcCh, 0.0f); // Omni
    setParamUnnorm(apvts, param_ids::dxEnabled, 1.0f);
    setParamUnnorm(apvts, param_ids::dxVolume, 1.0f);

    proc.prepareToPlay(48000.0, 512);
    juce::AudioBuffer<float> buf(2, 512);
    juce::MidiBuffer midi;

    // 1) Armed, waiting for MIDI Note-On on Ch 2 -> silent initially
    proc.processBlock(buf, midi);
    REQUIRE(proc.isMidiArmed());
    REQUIRE_FALSE(proc.isRhythmLatched());
    REQUIRE_FALSE(proc.getTelemetry().lamp.isRunning);
    REQUIRE(bufferMaxAbs(buf) == 0.0f);

    // 2) Note-On on Ch 1 ignored by Ch 2 filter -> remains armed & silent
    midi.addEvent(juce::MidiMessage::noteOn(1, 48, 0.8f), 10);
    proc.processBlock(buf, midi);
    REQUIRE(proc.isMidiArmed());
    REQUIRE_FALSE(proc.isRhythmLatched());
    REQUIRE(bufferMaxAbs(buf) == 0.0f);

    // 3) Note-On on Ch 2 triggers Beat 1 cleanly at sample offset!
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(2, 48, 0.8f), 100);
    proc.processBlock(buf, midi);
    REQUIRE(proc.isRhythmLatched());
    REQUIRE_FALSE(proc.isMidiArmed());
    REQUIRE(proc.getTelemetry().lamp.isRunning);
    REQUIRE(bufferMaxAbs(buf, 0, 100) == 0.0f);
    REQUIRE(bufferMaxAbs(buf, 100, 412) > 0.02f);

    // 4) Rhythm Stop Switch CC 67 = 127 stops immediately and inhibits start while held
    midi.clear();
    midi.addEvent(juce::MidiMessage::controllerEvent(3, 67, 127), 0);
    proc.processBlock(buf, midi);
    REQUIRE(proc.isStopSwitchHeld());
    REQUIRE_FALSE(proc.isRhythmLatched());
    for (int b = 0; b < 80; ++b)
    {
        midi.clear();
        proc.processBlock(buf, midi);
    }
    REQUIRE_FALSE(proc.getTelemetry().lamp.isRunning);
    REQUIRE(bufferMaxAbs(buf) < 1e-3f);

    // While held: MIDI Key Start and Fill-In Intro are BOTH inhibited
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(2, 48, 1.0f), 0);
    proc.triggerFillInFromUi(FillInId::FillIn1);
    proc.processBlock(buf, midi);
    REQUIRE(proc.isStopSwitchHeld());
    REQUIRE_FALSE(proc.isRhythmLatched());
    REQUIRE_FALSE(proc.getTelemetry().lamp.isRunning);
    REQUIRE_FALSE(proc.getTelemetry().lamp.fillInActive);

    // Release Stop Switch CC 67 = 0: inhibition cleared, remains stopped until new trigger
    midi.clear();
    midi.addEvent(juce::MidiMessage::controllerEvent(3, 67, 0), 0);
    proc.processBlock(buf, midi);
    REQUIRE_FALSE(proc.isStopSwitchHeld());
    REQUIRE_FALSE(proc.isRhythmLatched());

    // Now Note-On on Ch 2 restarts cleanly!
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(2, 48, 1.0f), 0);
    proc.processBlock(buf, midi);
    REQUIRE(proc.isRhythmLatched());
    REQUIRE(proc.getTelemetry().lamp.isRunning);
    REQUIRE(bufferMaxAbs(buf) > 0.02f);

    // 5) Standalone mode bypasses stopped DAW playhead & yellow START/STOP remain decoupled from dx_enabled
    MockPlayHead stoppedHost;
    stoppedHost.playing = false;
    stoppedHost.bpm = 120.0;
    proc.setPlayHead(&stoppedHost);
    setParamUnnorm(apvts, param_ids::dxSyncMode, 2.0f); // Host Sync
    setParamUnnorm(apvts, param_ids::dxMidiStartEn, 1.0f);
    setParamUnnorm(apvts, param_ids::dxEnabled, 0.0f);
    proc.setForceStandaloneForTest(true);
    proc.prepareToPlay(48000.0, 512);

    // Pressing Play when RHYTHM (dx_enabled) is OFF does NOT turn dx_enabled ON or start rhythm
    proc.setStandaloneStartHeldFromUi(true);
    midi.clear();
    proc.processBlock(buf, midi);
    proc.setStandaloneStartHeldFromUi(false);
    REQUIRE_FALSE(proc.getTelemetry().lamp.isRunning);
    REQUIRE(apvts.getParameter(param_ids::dxEnabled)->getValue() < 0.5f);

    // Turning red RHYTHM (dx_enabled) ON alone NEVER auto-starts rhythm even when MIDI Key Start is OFF!
    setParamUnnorm(apvts, param_ids::dxMidiStartEn, 0.0f);
    setParamUnnorm(apvts, param_ids::dxEnabled, 1.0f);
    proc.processBlock(buf, midi);
    REQUIRE_FALSE(proc.isRhythmLatched());
    REQUIRE_FALSE(proc.getTelemetry().lamp.isRunning);
    REQUIRE(bufferMaxAbs(buf) == 0.0f);

    // Pressing cream Play triggers Beat 1 cleanly WITHOUT altering dx_enabled
    proc.setStandaloneStartHeldFromUi(true);
    proc.processBlock(buf, midi);
    proc.setStandaloneStartHeldFromUi(false);
    REQUIRE(proc.isRhythmLatched());
    REQUIRE(proc.getTelemetry().lamp.isRunning);
    REQUIRE(bufferMaxAbs(buf) > 0.02f);
    REQUIRE(apvts.getParameter(param_ids::dxEnabled)->getValue() > 0.5f);

    // Turning red RHYTHM OFF stops rhythm & unlatches; turning it back ON stays silent until Start/MIDI/Fill-In
    setParamUnnorm(apvts, param_ids::dxEnabled, 0.0f);
    for (int b = 0; b < 80; ++b)
    {
        proc.processBlock(buf, midi);
    }
    REQUIRE_FALSE(proc.isRhythmLatched());
    REQUIRE_FALSE(proc.getTelemetry().lamp.isRunning);

    setParamUnnorm(apvts, param_ids::dxEnabled, 1.0f);
    proc.processBlock(buf, midi);
    REQUIRE_FALSE(proc.isRhythmLatched());
    REQUIRE_FALSE(proc.getTelemetry().lamp.isRunning);

    // Re-trigger via cream Play, then verify Stop stops immediately WITHOUT turning dx_enabled OFF!
    proc.setStandaloneStartHeldFromUi(true);
    proc.processBlock(buf, midi);
    proc.setStandaloneStartHeldFromUi(false);
    REQUIRE(proc.getTelemetry().lamp.isRunning);

    proc.setStandaloneStopHeldFromUi(true);
    proc.processBlock(buf, midi);
    proc.setStandaloneStopHeldFromUi(false);
    REQUIRE_FALSE(proc.getTelemetry().lamp.isRunning);
    REQUIRE(apvts.getParameter(param_ids::dxEnabled)->getValue() > 0.5f);
}

