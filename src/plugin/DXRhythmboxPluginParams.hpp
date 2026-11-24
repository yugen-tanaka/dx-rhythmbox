// Shared APVTS parameter IDs, 32 factory presets, and lock-free atomic parameter
// reader for 1982 KAWAI DX701 Rhythmbox (Retro Home Organ Collection #3).
#pragma once

#include "dsp/DXRhythmboxEngine.hpp"

#include <juce_audio_processors/juce_audio_processors.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace dx701::plugin
{

namespace param_ids
{
inline constexpr const char* dxEnabled      = "dx_enabled";
inline constexpr const char* dxBreak        = "dx_break";
inline constexpr const char* dxMultiPattern = "dx_multi_pattern";
inline constexpr const char* dxVariation    = "dx_variation";
inline constexpr const char* dxSyncMode     = "dx_sync_mode";
inline constexpr const char* dxBpm          = "dx_bpm";
inline constexpr const char* dxVolume       = "dx_volume";
inline constexpr const char* dxTone         = "dx_tone";
inline constexpr const char* dxAnalogDrift  = "dx_analog_drift";
inline constexpr const char* dxMidiCh       = "dx_midi_ch";
inline constexpr const char* dxMidiStartEn  = "dx_midi_start_enabled";
inline constexpr const char* dxMidiStartCh  = "dx_midi_start_ch";
inline constexpr const char* dxStopCcEn     = "dx_stop_cc_enabled";
inline constexpr const char* dxStopCcNum    = "dx_stop_cc_num";
inline constexpr const char* dxStopCcCh     = "dx_stop_cc_ch";

// 16 dual-function rhythm buttons matching the 2x8 DX701 hardware panel (row-major):
// Row 0 (0..7): WALTZ, BALLAD, SWING I/II, SWING III/IV, SHUFFLE/BOLERO, ROCK I/II, ROCK III/IV, DISCO
// Row 1 (8..15): MARCH I/II, MARCH III/IV, TANGO/HABANERA, CHA-CHA/BAION, SAMBA/MAMBO, BEGUINE/RHUMBA, BOSSA-NOVA, SOUL
inline constexpr std::array<const char*, dx701::dsp::kNumButtons> kButtonParamIds = {{
    "dx_btn_waltz",
    "dx_btn_ballad",
    "dx_btn_swing1",
    "dx_btn_swing3",
    "dx_btn_shuffle",
    "dx_btn_rock1",
    "dx_btn_rock3",
    "dx_btn_disco",
    "dx_btn_march1",
    "dx_btn_march3",
    "dx_btn_tango",
    "dx_btn_chacha",
    "dx_btn_samba",
    "dx_btn_beguine",
    "dx_btn_bossanova",
    "dx_btn_soul"
}};

inline constexpr std::array<const char*, dx701::dsp::kNumButtons> kButtonParamNames = {{
    "Button 1: Waltz / Jazz Waltz",
    "Button 2: Ballad I / II",
    "Button 3: Swing I / II",
    "Button 4: Swing III / IV",
    "Button 5: Shuffle / Bolero",
    "Button 6: Rock I / II",
    "Button 7: Rock III / IV",
    "Button 8: Disco I / II",
    "Button 9: March I / II",
    "Button 10: March III / IV",
    "Button 11: Tango / Habanera",
    "Button 12: Cha-Cha / Baion",
    "Button 13: Samba / Mambo",
    "Button 14: Beguine / Rhumba",
    "Button 15: Bossa-Nova I / II",
    "Button 16: Soul I / II"
}};

inline constexpr std::size_t kDefaultButtonIndex = 5u; // ROCK I
inline constexpr std::size_t kDefaultProgramIndex = static_cast<std::size_t>(dx701::dsp::PatternId::Rock1); // 10 -> "11 Rock I"
} // namespace param_ids

struct FactoryRhythmProgram
{
    const char* name;
    int buttonIndex;
    bool variation;
    float defaultBpm;
};

inline constexpr std::array<FactoryRhythmProgram, dx701::dsp::kNumPatterns> kFactoryPrograms = {{
    {"01 Waltz (3/4)",      0,  false, 111.8f},
    {"02 Jazz Waltz (3/4)", 0,  true,  111.7f},
    {"03 Ballad I",         1,  false, 84.0f},
    {"04 Ballad II",        1,  true,  84.0f},
    {"05 Swing I",          2,  false, 118.0f},
    {"06 Swing II",         2,  true,  122.0f},
    {"07 Swing III",        3,  false, 126.0f},
    {"08 Swing IV",         3,  true,  128.0f},
    {"09 Shuffle",          4,  false, 116.0f},
    {"10 Bolero",           4,  true,  92.0f},
    {"11 Rock I",           5,  false, 112.2f},
    {"12 Rock II",          5,  true,  112.2f},
    {"13 Rock III",         6,  false, 118.0f},
    {"14 Rock IV",          6,  true,  118.0f},
    {"15 Disco I",          7,  false, 120.0f},
    {"16 Disco II",         7,  true,  120.0f},
    {"17 March I",          8,  false, 116.0f},
    {"18 March II",         8,  true,  116.0f},
    {"19 March III",        9,  false, 114.0f},
    {"20 March IV",         9,  true,  114.0f},
    {"21 Tango",            10, false, 110.0f},
    {"22 Habanera",         10, true,  104.0f},
    {"23 Cha-Cha",          11, false, 112.2f},
    {"24 Baion",            11, true,  112.2f},
    {"25 Samba",            12, false, 126.0f},
    {"26 Mambo",            12, true,  124.0f},
    {"27 Beguine",          13, false, 108.0f},
    {"28 Rhumba",           13, true,  108.0f},
    {"29 Bossa-Nova I",     14, false, 112.3f},
    {"30 Bossa-Nova II",    14, true,  112.3f},
    {"31 Soul I",           15, false, 109.3f},
    {"32 Soul II",          15, true,  109.3f}
}};

inline juce::StringArray makeSyncModeChoices()
{
    return {"Internal", "Host Tempo", "Host Sync"};
}

inline juce::StringArray makeMidiChannelChoices()
{
    juce::StringArray arr;
    arr.add("Omni");
    for (int ch = 1; ch <= 16; ++ch)
    {
        arr.add("Ch " + juce::String(ch));
    }
    arr.add("Off (Disabled)");
    return arr;
}

inline juce::StringArray makeMidiTriggerChannelChoices()
{
    juce::StringArray arr;
    arr.add("Omni");
    for (int ch = 1; ch <= 16; ++ch)
    {
        arr.add("Ch " + juce::String(ch));
    }
    return arr;
}

inline void addDXRhythmboxParameters(std::vector<std::unique_ptr<juce::RangedAudioParameter>>& params,
                                     int defaultMidiChannelIndex = 0)
{
    auto addBool = [&params](const char* id, const char* name, bool def)
    {
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(id, 1), name, def));
    };

    // Polite default: RHYTHM & BREAK switches OFF on initial plugin load
    addBool(param_ids::dxEnabled, "Rhythm Enable", false);
    addBool(param_ids::dxBreak, "Break", false);
    addBool(param_ids::dxMultiPattern, "Multi-Pattern", false);
    addBool(param_ids::dxVariation, "Variation", false);

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(param_ids::dxSyncMode, 1), "Sync Mode", makeSyncModeChoices(), 2));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(param_ids::dxBpm, 1), "Tempo (BPM)",
        juce::NormalisableRange<float>(40.0f, 300.0f, 0.1f, 1.0f), 112.2f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(param_ids::dxVolume, 1), "Volume",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 1.0f), 0.85f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(param_ids::dxTone, 1), "Tone",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 1.0f), 0.50f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(param_ids::dxAnalogDrift, 1), "Analog Drift",
        juce::NormalisableRange<float>(0.0f, 1.5f, 0.01f, 1.0f), 1.0f));

    const int defMidiChIdx = std::clamp(defaultMidiChannelIndex, 0, 17);
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(param_ids::dxMidiCh, 1), "MIDI Channel", makeMidiChannelChoices(), defMidiChIdx));

    addBool(param_ids::dxMidiStartEn, "MIDI Key Start", false);
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(param_ids::dxMidiStartCh, 1), "MIDI Start Channel", makeMidiTriggerChannelChoices(), 0));

    addBool(param_ids::dxStopCcEn, "Rhythm Stop CC Enable", true);
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID(param_ids::dxStopCcNum, 1), "Rhythm Stop CC", 0, 127, 67));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(param_ids::dxStopCcCh, 1), "Rhythm Stop CC Channel", makeMidiTriggerChannelChoices(), 0));

    for (std::size_t i = 0; i < dx701::dsp::kNumButtons; ++i)
    {
        const bool isDefault = (i == param_ids::kDefaultButtonIndex);
        addBool(param_ids::kButtonParamIds[i], param_ids::kButtonParamNames[i], isDefault);
    }
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createDXRhythmboxParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    addDXRhythmboxParameters(params);
    return {params.begin(), params.end()};
}

struct CachedDXRhythmboxParams
{
    std::atomic<float>* enabled = nullptr;
    std::atomic<float>* breakMode = nullptr;
    std::atomic<float>* multiPattern = nullptr;
    std::atomic<float>* variation = nullptr;
    std::atomic<float>* syncMode = nullptr;
    std::atomic<float>* bpm = nullptr;
    std::atomic<float>* volume = nullptr;
    std::atomic<float>* tone = nullptr;
    std::atomic<float>* analogDrift = nullptr;
    std::atomic<float>* midiCh = nullptr;
    std::atomic<float>* midiStartEn = nullptr;
    std::atomic<float>* midiStartCh = nullptr;
    std::atomic<float>* stopCcEn = nullptr;
    std::atomic<float>* stopCcNum = nullptr;
    std::atomic<float>* stopCcCh = nullptr;
    std::array<std::atomic<float>*, dx701::dsp::kNumButtons> buttons{};

    void bind(const juce::AudioProcessorValueTreeState& apvts) noexcept
    {
        auto p = [&apvts](const char* id) noexcept { return apvts.getRawParameterValue(id); };
        enabled = p(param_ids::dxEnabled);
        breakMode = p(param_ids::dxBreak);
        multiPattern = p(param_ids::dxMultiPattern);
        variation = p(param_ids::dxVariation);
        syncMode = p(param_ids::dxSyncMode);
        bpm = p(param_ids::dxBpm);
        volume = p(param_ids::dxVolume);
        tone = p(param_ids::dxTone);
        analogDrift = p(param_ids::dxAnalogDrift);
        midiCh = p(param_ids::dxMidiCh);
        midiStartEn = p(param_ids::dxMidiStartEn);
        midiStartCh = p(param_ids::dxMidiStartCh);
        stopCcEn = p(param_ids::dxStopCcEn);
        stopCcNum = p(param_ids::dxStopCcNum);
        stopCcCh = p(param_ids::dxStopCcCh);
        for (std::size_t i = 0; i < dx701::dsp::kNumButtons; ++i)
        {
            buttons[i] = p(param_ids::kButtonParamIds[i]);
        }
    }

    dx701::dsp::DXRhythmboxState readState() const noexcept
    {
        auto b = [](const std::atomic<float>* ptr) noexcept
        {
            return (ptr != nullptr) && (ptr->load(std::memory_order_relaxed) >= 0.5f);
        };
        auto d = [](const std::atomic<float>* ptr, double fallback) noexcept
        {
            return (ptr != nullptr) ? static_cast<double>(ptr->load(std::memory_order_relaxed)) : fallback;
        };
        auto i = [](const std::atomic<float>* ptr, int fallback) noexcept
        {
            return (ptr != nullptr) ? static_cast<int>(std::lround(ptr->load(std::memory_order_relaxed))) : fallback;
        };

        dx701::dsp::DXRhythmboxState st;
        st.enabled = b(enabled);
        st.multiPatternEnabled = b(multiPattern);
        st.variation = b(variation);
        const int modeIdx = std::clamp(i(syncMode, 2), 0, 2);
        st.clockMode = static_cast<dx701::dsp::ClockMode>(modeIdx);
        st.bpm = std::clamp(d(bpm, 112.2), 40.0, 300.0);
        st.volume = std::clamp(d(volume, 0.85), 0.0, 1.0);
        st.tone = std::clamp(d(tone, 0.50), 0.0, 1.0);
        st.analogVariation = std::clamp(d(analogDrift, 1.0), 0.0, 1.5);

        std::uint16_t rawMask = 0u;
        for (std::size_t k = 0; k < dx701::dsp::kNumButtons; ++k)
        {
            if (b(buttons[k]))
            {
                rawMask = static_cast<std::uint16_t>(rawMask | (1u << static_cast<unsigned>(k)));
            }
        }
        st.buttonMask = dx701::dsp::capButtonMask(rawMask, st.multiPatternEnabled);
        return st;
    }

    int readMidiChannel() const noexcept
    {
        if (midiCh == nullptr)
        {
            return 0;
        }
        const int idx = static_cast<int>(std::lround(midiCh->load(std::memory_order_relaxed)));
        return (idx >= 17) ? -1 : std::clamp(idx, 0, 16);
    }

    bool readMidiStartEnabled() const noexcept
    {
        return (midiStartEn != nullptr) && (midiStartEn->load(std::memory_order_relaxed) >= 0.5f);
    }

    int readMidiStartChannel() const noexcept
    {
        if (midiStartCh == nullptr)
        {
            return 0;
        }
        return std::clamp(static_cast<int>(std::lround(midiStartCh->load(std::memory_order_relaxed))), 0, 16);
    }

    bool readStopCcEnabled() const noexcept
    {
        return (stopCcEn == nullptr) || (stopCcEn->load(std::memory_order_relaxed) >= 0.5f);
    }

    int readStopCcNumber() const noexcept
    {
        if (stopCcNum == nullptr)
        {
            return 67;
        }
        return std::clamp(static_cast<int>(std::lround(stopCcNum->load(std::memory_order_relaxed))), 0, 127);
    }

    bool readBreakEnabled() const noexcept
    {
        return (breakMode != nullptr) && (breakMode->load(std::memory_order_relaxed) >= 0.5f);
    }

    int readStopCcChannel() const noexcept
    {
        if (stopCcCh == nullptr)
        {
            return 0;
        }
        return std::clamp(static_cast<int>(std::lround(stopCcCh->load(std::memory_order_relaxed))), 0, 16);
    }
};

} // namespace dx701::plugin
