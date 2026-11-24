// Catch2 unit tests for pure C++20 zero-allocation dx_rhythmbox_dsp.
#include <catch2/catch_test_macros.hpp>

#include "dsp/DXRhythmboxEngine.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <set>
#include <vector>

namespace
{

using namespace dx701::dsp;

double computeRms(const double* data, std::size_t n) noexcept
{
    double sumSq = 0.0;
    for (std::size_t i = 0; i < n; ++i)
    {
        sumSq += data[i] * data[i];
    }
    return std::sqrt(sumSq / static_cast<double>(std::max<std::size_t>(1, n)) + 1e-24);
}

double computeMaxAbs(const double* data, std::size_t n) noexcept
{
    double m = 0.0;
    for (std::size_t i = 0; i < n; ++i)
    {
        m = std::max(m, std::abs(data[i]));
    }
    return m;
}

bool allFinite(const double* data, std::size_t n) noexcept
{
    for (std::size_t i = 0; i < n; ++i)
    {
        if (!std::isfinite(data[i]))
        {
            return false;
        }
    }
    return true;
}

double goertzelPower(const double* x, std::size_t n, double freqHz, double fs) noexcept
{
    const double w = 2.0 * std::numbers::pi * freqHz / fs;
    double re = 0.0;
    double im = 0.0;
    for (std::size_t i = 0; i < n; ++i)
    {
        const double win = 0.5 * (1.0 - std::cos(2.0 * std::numbers::pi * static_cast<double>(i) /
                                                static_cast<double>(std::max<std::size_t>(1, n - 1))));
        const double s = x[i] * win;
        const double ph = w * static_cast<double>(i);
        re += s * std::cos(ph);
        im -= s * std::sin(ph);
    }
    return re * re + im * im;
}

std::vector<double> renderPatternMono(double fs,
                                      std::uint16_t buttonMask,
                                      bool multiPattern,
                                      bool variation,
                                      double durSec,
                                      double bpm,
                                      double tone = 0.5,
                                      double analogVariation = 1.0,
                                      int blockSize = 128)
{
    DXRhythmboxEngine eng;
    eng.prepare(fs);
    DXRhythmboxState st;
    st.enabled = true;
    st.buttonMask = buttonMask;
    st.multiPatternEnabled = multiPattern;
    st.variation = variation;
    st.clockMode = ClockMode::Internal;
    st.bpm = bpm;
    st.volume = 1.0;
    st.tone = tone;
    st.analogVariation = analogVariation;
    eng.applyState(st);

    const std::size_t total = static_cast<std::size_t>(std::llround(durSec * fs));
    std::vector<double> out(total, 0.0);
    std::size_t pos = 0;
    while (pos < total)
    {
        const int blk = static_cast<int>(std::min<std::size_t>(static_cast<std::size_t>(blockSize), total - pos));
        eng.processBlock(out.data() + pos, blk);
        pos += static_cast<std::size_t>(blk);
    }
    return out;
}

std::vector<double> renderOneShotMono(double fs,
                                      VoiceId voice,
                                      double durSec,
                                      double analogVariation = 0.0,
                                      double velocity = 1.0)
{
    DXRhythmboxEngine eng;
    eng.prepare(fs);
    DXRhythmboxState st;
    st.enabled = false;
    st.clockMode = ClockMode::Internal;
    st.volume = 1.0;
    st.tone = 0.5;
    st.analogVariation = analogVariation;
    eng.applyState(st);
    eng.triggerVoice(voice, velocity);

    const std::size_t total = static_cast<std::size_t>(std::llround(durSec * fs));
    std::vector<double> out(total, 0.0);
    std::size_t pos = 0;
    while (pos < total)
    {
        const int blk = static_cast<int>(std::min<std::size_t>(128u, total - pos));
        eng.processBlock(out.data() + pos, blk);
        pos += static_cast<std::size_t>(blk);
    }
    return out;
}

} // namespace

TEST_CASE("DXRhythmbox DSP 1: Canonical constants, 16 voices, 32 patterns, 4 fill-ins & GM MIDI map",
          "[dx_rhythmbox_dsp]")
{
    REQUIRE(kNumVoices == 16u);
    REQUIRE(kNumButtons == 16u);
    REQUIRE(kNumPatterns == 32u);
    REQUIRE(kNumFillIns == 4u);
    REQUIRE(kMaxSimultaneousPatterns == 4u);
    REQUIRE(kClockHz == 16672.5);
    REQUIRE(kPatterns[0].name == std::string("WALTZ"));
    REQUIRE(kPatterns[10].name == std::string("ROCK_1"));
    REQUIRE(kPatterns[19].name == std::string("MARCH_4"));
    REQUIRE(kPatterns[31].name == std::string("SOUL_2"));

    // All 16 primary GM MIDI notes map to distinct canonical VoiceIds
    std::set<std::size_t> covered;
    for (std::size_t v = 0; v < kNumVoices; ++v)
    {
        const auto mapped = mapGmMidiNoteToVoice(kPrimaryGmMidiNotes[v]);
        REQUIRE(mapped.has_value());
        REQUIRE(static_cast<std::size_t>(*mapped) == v);
        covered.insert(static_cast<std::size_t>(*mapped));
    }
    REQUIRE(covered.size() == kNumVoices);

    // Invalid notes and velocity == 0 guards
    DXRhythmboxEngine eng;
    eng.prepare(48000.0);
    DXRhythmboxState st;
    st.enabled = false;
    eng.applyState(st);
    REQUIRE_FALSE(eng.triggerMidiNote(10, 127));
    REQUIRE_FALSE(eng.triggerMidiNote(36, 0));
    std::vector<double> buf(512, 0.0);
    eng.processBlock(buf.data(), 512);
    REQUIRE(computeMaxAbs(buf.data(), buf.size()) == 0.0);
}

TEST_CASE("DXRhythmbox DSP 2: Multirate stability across 44.1 / 48 / 88.2 / 96 kHz and all 16 voices",
          "[dx_rhythmbox_dsp]")
{
    constexpr std::array<double, 4> kRates = {44100.0, 48000.0, 88200.0, 96000.0};
    double worstR = 0.0;

    for (double sr : kRates)
    {
        DXRhythmboxEngine eng;
        eng.prepare(sr);
        worstR = std::max(worstR, eng.maxPoleRadius());
        REQUIRE(eng.maxPoleRadius() < 0.9999);

        for (std::size_t v = 0; v < kNumVoices; ++v)
        {
            const auto y = renderOneShotMono(sr, static_cast<VoiceId>(v), 0.35, 0.0, 1.0);
            REQUIRE(allFinite(y.data(), y.size()));
            REQUIRE(computeRms(y.data(), y.size()) > 1e-4);
        }
    }
    REQUIRE(worstR < 0.9999);
}

TEST_CASE("DXRhythmbox DSP 3: Phase 2.5 v3 circuit invariants & 96-step BALLAD_2 / MARCH_4 patterns",
          "[dx_rhythmbox_dsp]")
{
    constexpr double fs = 48000.0;

    // 1. roundHalfToEven matches IEEE half-to-even Python round()
    REQUIRE(roundHalfToEven(1.5) == 2.0);
    REQUIRE(roundHalfToEven(2.5) == 2.0);
    REQUIRE(roundHalfToEven(-1.5) == -2.0);
    REQUIRE(roundHalfToEven(-2.5) == -2.0);

    // 2. BASS_DRUM T6118 Cycle-2 peak > initial & clean gate-off tail
    {
        const auto bd = renderOneShotMono(fs, VoiceId::BassDrum, 0.14, 0.0);
        const std::size_t nInit = static_cast<std::size_t>(0.0055 * fs);
        const std::size_t c2Start = static_cast<std::size_t>(0.007 * fs);
        const std::size_t c2End = static_cast<std::size_t>(0.016 * fs);
        const std::size_t tStart = static_cast<std::size_t>(0.075 * fs);
        const std::size_t tEnd = static_cast<std::size_t>(0.095 * fs);
        const double rInit = computeRms(bd.data(), nInit);
        const double rCyc2 = computeRms(bd.data() + c2Start, c2End - c2Start);
        const double rTail = computeRms(bd.data() + tStart, tEnd - tStart);
        REQUIRE(rCyc2 > rInit);
        REQUIRE(20.0 * std::log10(rTail / rCyc2) < -35.0);
    }

    // 3. CYMBAL sustains via T6118A stepped hold then decays cleanly
    {
        const auto cym = renderOneShotMono(fs, VoiceId::Cymbal, 0.56, 0.0);
        const double r0 = computeRms(cym.data(), static_cast<std::size_t>(0.040 * fs));
        const double r40 = computeRms(cym.data() + static_cast<std::size_t>(0.040 * fs),
                                      static_cast<std::size_t>(0.060 * fs));
        const double r450 = computeRms(cym.data() + static_cast<std::size_t>(0.450 * fs),
                                       static_cast<std::size_t>(0.100 * fs));
        REQUIRE(20.0 * std::log10(r40 / r0) >= -4.0);
        REQUIRE(20.0 * std::log10(r450 / r0) <= -20.0);
    }

    // 4. COWBELL maintains both 334 Hz & 3001 Hz lines (ELC-18 Q11 passive_bp2)
    {
        const auto cb = renderOneShotMono(fs, VoiceId::Cowbell, 0.18, 0.0);
        const double p334 = goertzelPower(cb.data(), cb.size(), 333.45, fs);
        const double p3001 = goertzelPower(cb.data(), cb.size(), 3001.05, fs);
        const double relDb = 10.0 * std::log10((p334 + 1e-24) / (p3001 + 1e-24));
        REQUIRE(relDb >= -6.0);
        REQUIRE(relDb <= 3.0);
    }

    // 5. MARCH_4 (12 steps/bar, 48 steps) & BALLAD_2 (24 steps/bar, 96 steps upper word 64..95)
    const auto& march4 = kPatterns[static_cast<std::size_t>(PatternId::March4)];
    REQUIRE(march4.stepsPerBar == 12);
    REQUIRE(march4.totalSteps == 48);
    int m4CymCount = 0;
    int m4ChhCount = 0;
    for (int s = 0; s < march4.totalSteps; ++s)
    {
        if (march4.hasHit(static_cast<std::size_t>(VoiceId::Cymbal), s))
        {
            ++m4CymCount;
        }
        if (march4.hasHit(static_cast<std::size_t>(VoiceId::ClosedHiHat), s))
        {
            ++m4ChhCount;
        }
    }
    REQUIRE(m4CymCount == 16);
    REQUIRE(m4ChhCount == 0);

    const auto& ballad2 = kPatterns[static_cast<std::size_t>(PatternId::Ballad2)];
    REQUIRE(ballad2.totalSteps == 96);
    REQUIRE(ballad2.hasHit(static_cast<std::size_t>(VoiceId::BassDrum), 72));
    REQUIRE(ballad2.hasHit(static_cast<std::size_t>(VoiceId::SnareDrum), 78));
    REQUIRE(ballad2.hasHit(static_cast<std::size_t>(VoiceId::ClosedHiHat), 94));
}

TEST_CASE("DXRhythmbox DSP 4: TONE exact center bypass & dual-bus Dark/Bright split",
          "[dx_rhythmbox_dsp]")
{
    constexpr double fs = 48000.0;
    DXRhythmboxEngine eng;
    eng.prepare(fs);

    double dGain = 0.0;
    double mGain = 0.0;
    eng.computeToneBusGains(0.5, dGain, mGain);
    REQUIRE(dGain == 1.0);
    REQUIRE(mGain == 1.0);

    // Zero difference at TONE=0.5 after rendering at extremes and returning to 0.5
    const auto yRef = renderPatternMono(fs, static_cast<std::uint16_t>(1u << 7), false, false, 1.2, 112.265, 0.5, 0.0);
    eng.prepare(fs);
    DXRhythmboxState st;
    st.enabled = true;
    st.buttonMask = static_cast<std::uint16_t>(1u << 7);
    st.tone = 0.0;
    st.bpm = 112.265;
    st.analogVariation = 0.0;
    eng.applyState(st);
    std::vector<double> scratch(4096, 0.0);
    eng.processBlock(scratch.data(), 4096);

    eng.reset();
    st.tone = 0.5;
    eng.applyState(st);
    std::vector<double> yBack(yRef.size(), 0.0);
    std::size_t posBack = 0;
    while (posBack < yBack.size())
    {
        const int blk = static_cast<int>(std::min<std::size_t>(128u, yBack.size() - posBack));
        eng.processBlock(yBack.data() + posBack, blk);
        posBack += static_cast<std::size_t>(blk);
    }
    double maxDiff = 0.0;
    for (std::size_t i = 0; i < yRef.size(); ++i)
    {
        maxDiff = std::max(maxDiff, std::abs(yRef[i] - yBack[i]));
    }
    REQUIRE(maxDiff == 0.0);

    // SCRATCH_HI / SCRATCH_LO attenuate by -9.04 dB when TONE=0.0 vs TONE=0.5
    for (VoiceId v : {VoiceId::ScratchHi, VoiceId::ScratchLo})
    {
        DXRhythmboxEngine eDark;
        eDark.prepare(fs);
        DXRhythmboxState sDark;
        sDark.enabled = false;
        sDark.tone = 0.0;
        sDark.analogVariation = 0.0;
        eDark.applyState(sDark);
        eDark.triggerVoice(v, 1.0);

        DXRhythmboxEngine eMid;
        eMid.prepare(fs);
        DXRhythmboxState sMid;
        sMid.enabled = false;
        sMid.tone = 0.5;
        sMid.analogVariation = 0.0;
        eMid.applyState(sMid);
        eMid.triggerVoice(v, 1.0);

        const std::size_t n = static_cast<std::size_t>(0.10 * fs);
        std::vector<double> yDark(n, 0.0);
        std::vector<double> yMid(n, 0.0);
        eDark.processBlock(yDark.data(), static_cast<int>(n));
        eMid.processBlock(yMid.data(), static_cast<int>(n));
        const double attenDb = 20.0 * std::log10(computeRms(yDark.data(), n) / computeRms(yMid.data(), n));
        REQUIRE(std::abs(attenDb - (-9.04)) < 0.25);
    }
}

TEST_CASE("DXRhythmbox DSP 5: Single mutual exclusion vs Multi-4 Wired-OR cap & VARIATION toggle",
          "[dx_rhythmbox_dsp]")
{
    constexpr double fs = 48000.0;
    const std::uint16_t fiveMask = static_cast<std::uint16_t>((1u << 5) | (1u << 7) | (1u << 11) | (1u << 12) | (1u << 14));
    const std::uint16_t fourMask = capButtonMask(fiveMask, true);
    REQUIRE(fourMask == static_cast<std::uint16_t>((1u << 5) | (1u << 7) | (1u << 11) | (1u << 12)));
    REQUIRE(capButtonMask(fiveMask, false) == static_cast<std::uint16_t>(1u << 5));

    // Beat 1 Wired-OR dedup: ROCK_1 + DISCO_1 + CHA_CHA + MAMBO <= single ROCK_1 + ~0.5 dB on BD Beat 1
    const auto y4 = renderPatternMono(fs, fiveMask, true, false, 0.25, 112.0, 0.5, 0.0);
    const auto y4Capped = renderPatternMono(fs, fourMask, true, false, 0.25, 112.0, 0.5, 0.0);
    double capDiff = 0.0;
    for (std::size_t i = 0; i < y4.size(); ++i)
    {
        capDiff = std::max(capDiff, std::abs(y4[i] - y4Capped[i]));
    }
    REQUIRE(capDiff == 0.0);

    // Single mode ROCK_1 vs ROCK_2 (variation toggle changes output)
    const auto yVarOff = renderPatternMono(fs, static_cast<std::uint16_t>(1u << 5), false, false, 1.5, 112.2, 0.5, 0.0);
    const auto yVarOn = renderPatternMono(fs, static_cast<std::uint16_t>(1u << 5), false, true, 1.5, 112.2, 0.5, 0.0);
    double varDiff = 0.0;
    for (std::size_t i = 0; i < yVarOff.size(); ++i)
    {
        varDiff = std::max(varDiff, std::abs(yVarOff[i] - yVarOn[i]));
    }
    REQUIRE(varDiff > 0.01);
}

TEST_CASE("DXRhythmbox DSP 6: All 4 Fill-Ins standalone, 210 seamless carryover, and Tempo Lamp",
          "[dx_rhythmbox_dsp]")
{
    constexpr double fs = 48000.0;

    for (std::size_t f = 0; f < kNumFillIns; ++f)
    {
        DXRhythmboxEngine eng;
        eng.prepare(fs);
        DXRhythmboxState st;
        st.enabled = false;
        st.bpm = kFillIns[f].defaultBpm;
        st.analogVariation = 0.0;
        eng.applyState(st);
        eng.triggerFillIn(static_cast<FillInId>(f), false);

        std::vector<double> buf(static_cast<std::size_t>(2.4 * fs), 0.0);
        eng.processBlock(buf.data(), static_cast<int>(buf.size()));
        REQUIRE(allFinite(buf.data(), buf.size()));
        REQUIRE(computeRms(buf.data(), buf.size()) > 0.01);
        REQUIRE_FALSE(eng.state().enabled);
    }

    // 210-like transition: FILL_IN_2 -> SOUL_1 seamlessly with active LOW_TOM carryover across bar boundary
    DXRhythmboxEngine eng;
    eng.prepare(fs);
    DXRhythmboxState st;
    st.enabled = true;
    st.buttonMask = static_cast<std::uint16_t>(1u << 15);
    st.variation = false; // SOUL_1
    st.bpm = 109.297;
    st.analogVariation = 0.0;
    eng.applyState(st);
    eng.triggerFillIn(FillInId::FillIn2, true);

    const std::size_t nTot = static_cast<std::size_t>(4.5 * fs);
    std::vector<double> yTrans(nTot, 0.0);
    eng.processBlock(yTrans.data(), static_cast<int>(nTot));
    REQUIRE(eng.state().enabled);
    REQUIRE(eng.telemetry().lamp.isRunning);
    REQUIRE_FALSE(eng.telemetry().lamp.fillInActive);

    const std::size_t nBar = static_cast<std::size_t>(std::llround(4.0 * 60.0 / 109.297 * fs));
    REQUIRE(computeRms(yTrans.data() + nBar, static_cast<std::size_t>(1.5 * fs)) > 0.02);
}

TEST_CASE("DXRhythmbox DSP 7: Block-size invariance across 16..512 samples including Fill-In & Wired-OR",
          "[dx_rhythmbox_dsp]")
{
    constexpr double fs = 48000.0;
    constexpr std::array<int, 6> kBlocks = {16, 37, 64, 128, 256, 512};
    const std::uint16_t comboMask = static_cast<std::uint16_t>((1u << 5) | (1u << 12) | (1u << 14));
    const std::size_t nTot = static_cast<std::size_t>(3.2 * fs);

    std::vector<double> ref;
    for (int blk : kBlocks)
    {
        DXRhythmboxEngine eng;
        eng.prepare(fs);
        DXRhythmboxState st;
        st.enabled = true;
        st.buttonMask = comboMask;
        st.multiPatternEnabled = true;
        st.variation = true;
        st.bpm = 114.5;
        st.tone = 0.3;
        st.analogVariation = 1.0;
        eng.applyState(st);
        eng.triggerFillIn(FillInId::FillIn2, true);

        std::vector<double> cur(nTot, 0.0);
        std::size_t pos = 0;
        while (pos < nTot)
        {
            const int n = static_cast<int>(std::min<std::size_t>(static_cast<std::size_t>(blk), nTot - pos));
            eng.processBlock(cur.data() + pos, n);
            pos += static_cast<std::size_t>(n);
        }

        if (ref.empty())
        {
            ref = std::move(cur);
        }
        else
        {
            double maxDiff = 0.0;
            for (std::size_t i = 0; i < nTot; ++i)
            {
                maxDiff = std::max(maxDiff, std::abs(ref[i] - cur[i]));
            }
            REQUIRE(maxDiff < 1e-12);
        }
    }
}

TEST_CASE("DXRhythmbox DSP 8: HostSync PPQ locking, seek/loop-back, TempoSync & Standalone fallback",
          "[dx_rhythmbox_dsp]")
{
    constexpr double fs = 48000.0;
    constexpr double hostBpm = 120.0;
    const double beatsPerSample = hostBpm / (60.0 * fs);
    const std::uint16_t discoMask = static_cast<std::uint16_t>(1u << 7);

    // A) HostSync playing forward from ppq=0 matches Internal bit-for-bit across 256-sample blocks
    const auto yInt = renderPatternMono(fs, discoMask, false, false, 2.0, hostBpm, 0.5, 1.0, 256);

    DXRhythmboxEngine engHost;
    engHost.prepare(fs);
    DXRhythmboxState stH;
    stH.enabled = true;
    stH.buttonMask = discoMask;
    stH.clockMode = ClockMode::HostSync;
    stH.bpm = 90.0; // overridden by host.bpm
    stH.tone = 0.5;
    stH.analogVariation = 1.0;
    engHost.applyState(stH);

    std::vector<double> yHost(yInt.size(), 0.0);
    std::size_t pos = 0;
    while (pos < yHost.size())
    {
        const int blk = static_cast<int>(std::min<std::size_t>(256u, yHost.size() - pos));
        HostTransportInfo info{true, true, true, hostBpm, true, static_cast<double>(pos) * beatsPerSample};
        engHost.processBlock(yHost.data() + pos, blk, info);
        pos += static_cast<std::size_t>(blk);
    }

    double hostDiff = 0.0;
    for (std::size_t i = 0; i < yInt.size(); ++i)
    {
        hostDiff = std::max(hostDiff, std::abs(yInt[i] - yHost[i]));
    }
    REQUIRE(hostDiff < 1e-12);

    // B) Loop-back to ppq=0.0 re-triggers Beat 1
    {
        std::vector<double> loopBuf(1024, 0.0);
        HostTransportInfo infoLoop{true, true, true, hostBpm, true, 0.0};
        engHost.processBlock(loopBuf.data(), 1024, infoLoop);
        REQUIRE(computeMaxAbs(loopBuf.data(), 1024) > 0.05);
    }

    // C) Host stopped -> silence after tail decays
    {
        std::vector<double> stopBuf(static_cast<std::size_t>(0.6 * fs), 0.0);
        HostTransportInfo infoStop{true, false, true, hostBpm, true, 0.0};
        engHost.processBlock(stopBuf.data(), static_cast<int>(stopBuf.size()), infoStop);
        REQUIRE(computeMaxAbs(stopBuf.data() + static_cast<std::size_t>(0.5 * fs),
                              static_cast<std::size_t>(0.1 * fs)) < 1e-4);
    }

    // D) Standalone fallback (!hasTransport) & processBlockStereo float32 parity
    {
        DXRhythmboxEngine engSt;
        engSt.prepare(fs);
        stH.bpm = 132.0;
        engSt.applyState(stH);
        std::vector<float> outL(1024, 0.0f);
        std::vector<float> outR(1024, 0.0f);
        HostTransportInfo noHost{};
        engSt.processBlockStereo(outL.data(), outR.data(), 1024, noHost);
        REQUIRE(engSt.telemetry().lamp.isRunning);
        REQUIRE(std::abs(engSt.telemetry().lamp.effectiveBpm - 132.0) < 1e-6);
        for (std::size_t i = 0; i < 1024; ++i)
        {
            REQUIRE(outL[i] == outR[i]);
        }
    }

    // E) TempoSync uses host BPM while obeying local START/STOP
    {
        DXRhythmboxEngine engTs;
        engTs.prepare(fs);
        DXRhythmboxState stTs;
        stTs.enabled = true;
        stTs.buttonMask = discoMask;
        stTs.clockMode = ClockMode::TempoSync;
        stTs.bpm = 85.0;
        engTs.applyState(stTs);

        HostTransportInfo hPaused{true, false, true, 145.0, true, 0.0};
        std::vector<double> buf(1024, 0.0);
        engTs.processBlock(buf.data(), 1024, hPaused);
        REQUIRE(engTs.telemetry().lamp.isRunning);
        REQUIRE(std::abs(engTs.telemetry().lamp.effectiveBpm - 145.0) < 1e-6);
    }
}
