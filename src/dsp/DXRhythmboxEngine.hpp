// Pure C++20 zero-allocation DSP synthesis & sequencer engine for 1982 KAWAI DX701 Rhythmbox.
#pragma once

#include "Biquad.hpp"
#include "DXRhythmboxClock.hpp"
#include "DXRhythmboxParams.hpp"
#include "DXRhythmboxVoices.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace dx701::dsp
{

constexpr std::uint16_t capButtonMask(std::uint16_t mask, bool multiPatternEnabled) noexcept
{
    if (mask == 0u)
    {
        return 0u;
    }
    if (!multiPatternEnabled)
    {
        return static_cast<std::uint16_t>(mask & static_cast<std::uint16_t>(~mask + 1u));
    }
    std::uint16_t kept = 0u;
    std::size_t count = 0u;
    for (unsigned bit = 0u; bit < 16u; ++bit)
    {
        if (((mask >> bit) & 1u) != 0u)
        {
            kept = static_cast<std::uint16_t>(kept | (1u << bit));
            ++count;
            if (count >= kMaxSimultaneousPatterns)
            {
                break;
            }
        }
    }
    return kept;
}

struct DXRhythmboxState
{
    bool enabled = true;
    std::uint16_t buttonMask = static_cast<std::uint16_t>(1u << 5); // Default ROCK I (row 0, col 5)
    bool multiPatternEnabled = false;
    bool variation = false;
    ClockMode clockMode = ClockMode::Internal;
    double bpm = kDefaultBpm;
    double volume = 1.0;
    double tone = 0.5;
    double analogVariation = kAnalogVariationDefault;
};

struct DXRhythmboxTelemetry
{
    double outputRms = 0.0;
    double outputPeak = 0.0;
    TempoLampState lamp{};
};

class DXRhythmboxEngine
{
public:
    static constexpr int kMaxChunk = 128;

    void prepare(double sampleRate) noexcept
    {
        fs_ = std::max(8000.0, sampleRate);
        digitalClock_.prepare(fs_, kDigitalClockConfig);

        bassDrum_.prepare(fs_, kBassDrumSpec, kClockHz);
        snareBody_.prepare(fs_, kSnareBodySpec);
        lowTom_.prepare(fs_, kLowTomSpec, kClockHz);
        midTom_.prepare(fs_, kMidTomSpec, kClockHz);
        highTom_.prepare(fs_, kHighTomSpec, kClockHz);
        lowBongo_.prepare(fs_, kLowBongoSpec);
        rimShot_.prepare(fs_, kRimShotSpec);
        claves_.prepare(fs_, kClavesSpec);
        cowbell_.prepare(fs_, kCowbellSpec);

        snareNoiseVca_.prepare(fs_, kSnareNoiseVcaSpec);
        snareNoiseFilter_.design(kSnareNoiseFilterSpec.sections, kSnareNoiseFilterSpec.numSections,
                                 kSnareNoiseFilterSpec.filterGainDb, fs_);
        tambVca_.prepare(fs_, kTambourineSpec.vca);
        tambFilter_.design(kTambourineSpec.filter.sections, kTambourineSpec.filter.numSections,
                           kTambourineSpec.filter.filterGainDb, fs_);
        guiroVca_.prepare(fs_, kGuiroSpec);
        guiroFilter_.design(kGuiroSpec.filter.sections, kGuiroSpec.filter.numSections,
                            kGuiroSpec.filter.filterGainDb, fs_);
        scratchHiVca_.prepare(fs_, kScratchHiSpec.vca);
        scratchHiFilter_.design(kScratchHiSpec.filter.sections, kScratchHiSpec.filter.numSections,
                                kScratchHiSpec.filter.filterGainDb, fs_);
        scratchLoVca_.prepare(fs_, kScratchLoSpec.vca);
        scratchLoFilter_.design(kScratchLoSpec.filter.sections, kScratchLoSpec.filter.numSections,
                                kScratchLoSpec.filter.filterGainDb, fs_);
        chhVca_.prepare(fs_, kClosedHiHatSpec.vca);
        chhFilter_.design(kClosedHiHatSpec.filter.sections, kClosedHiHatSpec.filter.numSections,
                          kClosedHiHatSpec.filter.filterGainDb, fs_);
        ohhVca_.prepare(fs_, kOpenHiHatSpec.vca);
        ohhFilter_.design(kOpenHiHatSpec.filter.sections, kOpenHiHatSpec.filter.numSections,
                          kOpenHiHatSpec.filter.filterGainDb, fs_);
        cymVca_.prepare(fs_, kCymbalSpec.vca);
        cymFilter_.design(kCymbalSpec.filter.sections, kCymbalSpec.filter.numSections,
                          kCymbalSpec.filter.filterGainDb, fs_);

        reset();
    }

    void reset() noexcept
    {
        driftGen_.reset(0x9E3779B9u);
        digitalClock_.reset();

        bassDrum_.reset();
        snareBody_.reset();
        lowTom_.reset();
        midTom_.reset();
        highTom_.reset();
        lowBongo_.reset();
        rimShot_.reset();
        claves_.reset();
        cowbell_.reset();

        snareNoiseVca_.reset();
        snareNoiseFilter_.reset();
        tambVca_.reset();
        tambFilter_.reset();
        guiroVca_.reset();
        guiroFilter_.reset();
        scratchHiVca_.reset();
        scratchHiFilter_.reset();
        scratchLoVca_.reset();
        scratchLoFilter_.reset();
        chhVca_.reset();
        chhFilter_.reset();
        ohhVca_.reset();
        ohhFilter_.reset();
        cymVca_.reset();
        cymFilter_.reset();

        railSagEnv_ = 0.0;
        beatPos_ = 0.0;
        idleBeatPos_ = 0.0;
        lastStepIndices_.fill(-1);
        fillinActiveIdx_ = -1;
        fillinBeatPos_ = 0.0;
        fillinLastStep_ = -1;
        fillinContinue_ = true;

        wasRunning_ = false;
        hasExpectedPpq_ = false;
        expectedNextPpq_ = 0.0;
        prevActivePatMask_ = 0u;
        telemetry_ = DXRhythmboxTelemetry{};
    }

    void restartSequencer() noexcept
    {
        wasRunning_ = false;
        hasExpectedPpq_ = false;
        beatPos_ = 0.0;
        lastStepIndices_.fill(-1);
        fillinActiveIdx_ = -1;
        fillinBeatPos_ = 0.0;
        fillinLastStep_ = -1;
    }

    void applyState(const DXRhythmboxState& newState) noexcept
    {
        state_ = newState;
        state_.buttonMask = capButtonMask(state_.buttonMask, state_.multiPatternEnabled);
    }

    void triggerFillIn(FillInId fillin, std::optional<bool> continueIntoRhythm = std::nullopt) noexcept
    {
        const int idx = static_cast<int>(fillin);
        if (idx < 0 || idx >= static_cast<int>(kNumFillIns))
        {
            return;
        }
        fillinActiveIdx_ = idx;
        fillinBeatPos_ = 0.0;
        fillinLastStep_ = -1;
        if (continueIntoRhythm.has_value())
        {
            fillinContinue_ = *continueIntoRhythm;
        }
        else
        {
            std::array<std::size_t, kMaxSimultaneousPatterns> dummy{};
            fillinContinue_ = state_.enabled && (activePatternIndices(dummy) > 0u);
        }
    }

    void triggerVoice(VoiceId voice, double velocity = 1.0) noexcept
    {
        const double vel = std::max(0.0, velocity);
        if (vel <= 0.0)
        {
            return;
        }

        const DriftTriple d = computeHitDrift(voice, vel);

        switch (voice)
        {
            case VoiceId::BassDrum:
                bassDrum_.trigger(d.effVel, d.pitchMult, d.decayMult);
                if (state_.analogVariation > 1e-6)
                {
                    railSagEnv_ = std::min(1.5, railSagEnv_ + 1.0);
                }
                break;
            case VoiceId::SnareDrum:
                snareBody_.trigger(d.effVel, d.pitchMult, d.decayMult);
                snareNoiseVca_.trigger(d.effVel, d.decayMult);
                break;
            case VoiceId::LowTom:
                lowTom_.trigger(d.effVel, d.pitchMult, d.decayMult);
                break;
            case VoiceId::MidTom:
                midTom_.trigger(d.effVel, d.pitchMult, d.decayMult);
                break;
            case VoiceId::HighTom:
                highTom_.trigger(d.effVel, d.pitchMult, d.decayMult);
                break;
            case VoiceId::LowBongo:
                lowBongo_.trigger(d.effVel, d.pitchMult, d.decayMult);
                break;
            case VoiceId::RimShot:
                rimShot_.trigger(d.effVel, d.pitchMult, d.decayMult);
                break;
            case VoiceId::Claves:
                claves_.trigger(d.effVel, d.pitchMult, d.decayMult);
                break;
            case VoiceId::Cowbell:
                cowbell_.trigger(d.effVel, d.pitchMult, d.decayMult);
                break;
            case VoiceId::Tambourine:
                tambVca_.trigger(d.effVel, d.decayMult);
                break;
            case VoiceId::Guiro:
                guiroVca_.trigger(d.effVel, d.decayMult);
                break;
            case VoiceId::ScratchHi:
                scratchHiVca_.trigger(d.effVel, d.decayMult);
                break;
            case VoiceId::ScratchLo:
                scratchLoVca_.trigger(d.effVel, d.decayMult);
                break;
            case VoiceId::ClosedHiHat:
                chhVca_.trigger(d.effVel, d.decayMult);
                if constexpr (kChhChokesOhh)
                {
                    ohhVca_.choke();
                }
                break;
            case VoiceId::OpenHiHat:
                ohhVca_.trigger(d.effVel, d.decayMult);
                break;
            case VoiceId::Cymbal:
                cymVca_.trigger(d.effVel, d.decayMult);
                break;
        }
    }

    bool triggerMidiNote(int midiNote, int velocity = 100) noexcept
    {
        if (velocity <= 0)
        {
            return false;
        }
        const auto voice = mapGmMidiNoteToVoice(midiNote);
        if (!voice.has_value())
        {
            return false;
        }
        triggerVoice(*voice, static_cast<double>(velocity) / 127.0);
        return true;
    }

    void computeToneBusGains(double tone, double& outDrumGain, double& outMetalGain) const noexcept
    {
        const double t = std::clamp(tone, 0.0, 1.0);
        if (std::abs(t - 0.5) <= 1e-6)
        {
            outDrumGain = 1.0;
            outMetalGain = 1.0;
            return;
        }
        const double power = std::max(0.2, kToneCircuit.taperPower);
        double drumDb = 0.0;
        double metalDb = 0.0;
        if (t < 0.5)
        {
            const double x = std::pow((0.5 - t) * 2.0, power);
            drumDb = kToneCircuit.drumDarkDb * x;
            metalDb = kToneCircuit.metalDarkDb * x;
        }
        else
        {
            const double x = std::pow((t - 0.5) * 2.0, power);
            drumDb = kToneCircuit.drumBrightDb * x;
            metalDb = kToneCircuit.metalBrightDb * x;
        }
        outDrumGain = std::pow(10.0, drumDb / 20.0);
        outMetalGain = std::pow(10.0, metalDb / 20.0);
    }

    void processBlock(double* outMono, int numSamples, const HostTransportInfo& host = {}) noexcept
    {
        if (outMono == nullptr || numSamples <= 0)
        {
            return;
        }

        const bool hostHasBpm = host.hasBpm && host.bpm >= 10.0;
        const bool usingHostBpm = (state_.clockMode != ClockMode::Internal) && hostHasBpm;
        const bool useHostClock = (state_.clockMode == ClockMode::HostSync) && host.hasTransport;
        const double effectiveBpm = std::max(10.0, usingHostBpm ? host.bpm : state_.bpm);
        const double beatsPerSample = effectiveBpm / (60.0 * fs_);
        const bool useHostPpq = useHostClock && host.hasPpqPosition;

        double hostPpqAdvance = 0.0;
        bool blockJustFinishedFillIn = false;
        int pos = 0;

        std::array<std::size_t, kMaxSimultaneousPatterns> activePats{};

        while (pos < numSamples)
        {
            const int rem = numSamples - pos;
            const std::size_t activeCount = activePatternIndices(activePats);
            std::uint32_t activePatMask = 0u;
            for (std::size_t k = 0; k < activeCount; ++k)
            {
                activePatMask |= (1u << static_cast<unsigned>(activePats[k]));
            }

            if (fillinActiveIdx_ >= 0)
            {
                const FillInSpec& fpat = kFillIns[static_cast<std::size_t>(fillinActiveIdx_)];
                const int spb = fpat.stepsPerBar;
                const double stepsPerBeat = static_cast<double>(spb) / 4.0;
                const int stepAbs = static_cast<int>(std::floor(fillinBeatPos_ * stepsPerBeat + 1e-9));

                if (stepAbs > fillinLastStep_ && stepAbs < spb)
                {
                    fillinLastStep_ = stepAbs;
                    for (std::size_t v = 0; v < kNumVoices; ++v)
                    {
                        if (fpat.hasHit(v, stepAbs))
                        {
                            triggerVoice(static_cast<VoiceId>(v), 1.0);
                        }
                    }
                }

                const int nextStep = std::min(spb, stepAbs + 1);
                const int sStep = static_cast<int>(
                    std::ceil((static_cast<double>(nextStep) / stepsPerBeat - fillinBeatPos_) / beatsPerSample));
                const int sEnd = static_cast<int>(
                    std::ceil((4.0 - fillinBeatPos_) / beatsPerSample));
                const int chunk = std::min({kMaxChunk, rem, std::max(1, sStep), std::max(1, sEnd)});

                processVoicesChunk(outMono + pos, chunk);
                fillinBeatPos_ += static_cast<double>(chunk) * beatsPerSample;
                hostPpqAdvance += static_cast<double>(chunk) * beatsPerSample;
                pos += chunk;

                if (fillinBeatPos_ >= 4.0 - 1e-9)
                {
                    const double over = std::max(0.0, fillinBeatPos_ - 4.0);
                    fillinActiveIdx_ = -1;
                    fillinLastStep_ = -1;
                    fillinBeatPos_ = 0.0;
                    const bool resumeRhythm =
                        fillinContinue_ && (activeCount > 0u) && (!useHostClock || host.isPlaying);
                    if (resumeRhythm)
                    {
                        state_.enabled = true;
                        beatPos_ = over;
                        lastStepIndices_.fill(-1);
                        wasRunning_ = true;
                        prevActivePatMask_ = activePatMask;
                        hasExpectedPpq_ = false;
                        blockJustFinishedFillIn = true;
                    }
                    else
                    {
                        if (!useHostClock)
                        {
                            state_.enabled = false;
                        }
                        wasRunning_ = false;
                        prevActivePatMask_ = 0u;
                        hasExpectedPpq_ = false;
                    }
                }
                continue;
            }

            const bool patRunning =
                useHostClock ? (state_.enabled && host.isPlaying && (activeCount > 0u))
                             : (state_.enabled && (activeCount > 0u));

            if (!patRunning)
            {
                const int chunk = std::min(kMaxChunk, rem);
                processVoicesChunk(outMono + pos, chunk);
                idleBeatPos_ += static_cast<double>(chunk) * beatsPerSample;
                pos += chunk;
                wasRunning_ = false;
                hasExpectedPpq_ = false;
                prevActivePatMask_ = 0u;
                continue;
            }

            idleBeatPos_ = 0.0;

            if (useHostPpq && !blockJustFinishedFillIn)
            {
                const double targetPpq = host.ppqPosition + hostPpqAdvance;
                const bool needResync = !wasRunning_ || !hasExpectedPpq_ ||
                                        std::abs(targetPpq - expectedNextPpq_) > 0.05;
                if (needResync)
                {
                    beatPos_ = targetPpq;
                    for (std::size_t p = 0; p < kNumPatterns; ++p)
                    {
                        if ((activePatMask & (1u << static_cast<unsigned>(p))) != 0u)
                        {
                            lastStepIndices_[p] = computeInitialLastStepForPpq(beatPos_, kPatterns[p]);
                        }
                        else
                        {
                            lastStepIndices_[p] = -1;
                        }
                    }
                }
                else if (activePatMask != prevActivePatMask_)
                {
                    for (std::size_t p = 0; p < kNumPatterns; ++p)
                    {
                        const std::uint32_t bit = (1u << static_cast<unsigned>(p));
                        if ((activePatMask & bit) != 0u && (prevActivePatMask_ & bit) == 0u)
                        {
                            lastStepIndices_[p] = computeInitialLastStepForPpq(beatPos_, kPatterns[p]);
                        }
                    }
                }
            }
            else if (!wasRunning_)
            {
                beatPos_ = 0.0;
                lastStepIndices_.fill(-1);
            }

            // Wired-OR deduplication in canonical VoiceId order (0..15)
            std::uint16_t voicesToFireMask = 0u;
            int nextCrossSamples = rem;

            for (std::size_t k = 0; k < activeCount; ++k)
            {
                const std::size_t pIdx = activePats[k];
                const PatternSpec& pat = kPatterns[pIdx];
                const double stepsPerBeat = static_cast<double>(pat.stepsPerBar) / pat.beatsPerBar;

                if (beatPos_ < -1e-9)
                {
                    const int sZero = static_cast<int>(std::ceil((-beatPos_) / beatsPerSample));
                    nextCrossSamples = std::min(nextCrossSamples, std::max(1, sZero));
                    continue;
                }

                const std::int64_t stepIdxAbs = static_cast<std::int64_t>(std::floor(beatPos_ * stepsPerBeat + 1e-9));
                if (stepIdxAbs > lastStepIndices_[pIdx])
                {
                    lastStepIndices_[pIdx] = stepIdxAbs;
                    const int stepMod = positiveMod(stepIdxAbs, pat.totalSteps);
                    for (std::size_t v = 0; v < kNumVoices; ++v)
                    {
                        if (pat.hasHit(v, stepMod))
                        {
                            voicesToFireMask = static_cast<std::uint16_t>(voicesToFireMask | (1u << v));
                        }
                    }
                }

                const std::int64_t nextStepInt = stepIdxAbs + 1;
                const int samplesToNext = static_cast<int>(
                    std::ceil((static_cast<double>(nextStepInt) / stepsPerBeat - beatPos_) / beatsPerSample));
                nextCrossSamples = std::min(nextCrossSamples, std::max(1, samplesToNext));
            }

            if (voicesToFireMask != 0u)
            {
                for (std::size_t v = 0; v < kNumVoices; ++v)
                {
                    if ((voicesToFireMask & (1u << v)) != 0u)
                    {
                        triggerVoice(static_cast<VoiceId>(v), 1.0);
                    }
                }
            }

            const int chunk = std::min({kMaxChunk, rem, std::max(1, nextCrossSamples)});
            processVoicesChunk(outMono + pos, chunk);
            beatPos_ += static_cast<double>(chunk) * beatsPerSample;
            hostPpqAdvance += static_cast<double>(chunk) * beatsPerSample;
            pos += chunk;

            wasRunning_ = true;
            prevActivePatMask_ = activePatMask;
            expectedNextPpq_ = beatPos_;
            hasExpectedPpq_ = true;
        }

        if (useHostPpq && wasRunning_ && fillinActiveIdx_ < 0)
        {
            expectedNextPpq_ = beatPos_;
            hasExpectedPpq_ = true;
        }
        else if (!wasRunning_)
        {
            hasExpectedPpq_ = false;
        }

        updateTelemetry(outMono, numSamples, effectiveBpm, usingHostBpm, activePats, activePatternIndices(activePats));
    }

    void processBlockMono(float* outMono,
                          int numSamples,
                          const HostTransportInfo& host = {},
                          double* monoScratch = nullptr) noexcept
    {
        if (outMono == nullptr || numSamples <= 0)
        {
            return;
        }
        if (monoScratch != nullptr)
        {
            processBlock(monoScratch, numSamples, host);
            for (int i = 0; i < numSamples; ++i)
            {
                outMono[i] = static_cast<float>(monoScratch[i]);
            }
            return;
        }

        std::array<double, kMaxChunk> localMono{};
        HostTransportInfo subHost = host;
        const bool hostHasBpm = host.hasBpm && host.bpm >= 10.0;
        const double effBpm = ((state_.clockMode != ClockMode::Internal) && hostHasBpm) ? host.bpm : state_.bpm;
        const double beatsPerSample = std::max(10.0, effBpm) / (60.0 * fs_);

        int pos = 0;
        while (pos < numSamples)
        {
            const int blk = std::min(kMaxChunk, numSamples - pos);
            processBlock(localMono.data(), blk, subHost);
            for (int i = 0; i < blk; ++i)
            {
                outMono[pos + i] = static_cast<float>(localMono[static_cast<std::size_t>(i)]);
            }
            if (subHost.hasPpqPosition)
            {
                subHost.ppqPosition += static_cast<double>(blk) * beatsPerSample;
            }
            pos += blk;
        }
    }

    void processBlockStereo(float* outL,
                            float* outR,
                            int numSamples,
                            const HostTransportInfo& host = {},
                            double* monoScratch = nullptr) noexcept
    {
        if (numSamples <= 0)
        {
            return;
        }
        if (monoScratch != nullptr)
        {
            processBlock(monoScratch, numSamples, host);
            for (int i = 0; i < numSamples; ++i)
            {
                const float s = static_cast<float>(monoScratch[i]);
                if (outL != nullptr)
                {
                    outL[i] = s;
                }
                if (outR != nullptr)
                {
                    outR[i] = s;
                }
            }
            return;
        }

        std::array<double, kMaxChunk> localMono{};
        HostTransportInfo subHost = host;
        const bool hostHasBpm = host.hasBpm && host.bpm >= 10.0;
        const double effBpm = ((state_.clockMode != ClockMode::Internal) && hostHasBpm) ? host.bpm : state_.bpm;
        const double beatsPerSample = std::max(10.0, effBpm) / (60.0 * fs_);

        int pos = 0;
        while (pos < numSamples)
        {
            const int blk = std::min(kMaxChunk, numSamples - pos);
            processBlock(localMono.data(), blk, subHost);
            for (int i = 0; i < blk; ++i)
            {
                const float s = static_cast<float>(localMono[static_cast<std::size_t>(i)]);
                if (outL != nullptr)
                {
                    outL[pos + i] = s;
                }
                if (outR != nullptr)
                {
                    outR[pos + i] = s;
                }
            }
            if (subHost.hasPpqPosition)
            {
                subHost.ppqPosition += static_cast<double>(blk) * beatsPerSample;
            }
            pos += blk;
        }
    }

    double maxPoleRadius() const noexcept
    {
        double maxR = 0.0;
        maxR = std::max(maxR, bassDrum_.maxPoleRadius());
        maxR = std::max(maxR, snareBody_.maxPoleRadius());
        maxR = std::max(maxR, lowTom_.maxPoleRadius());
        maxR = std::max(maxR, midTom_.maxPoleRadius());
        maxR = std::max(maxR, highTom_.maxPoleRadius());
        maxR = std::max(maxR, lowBongo_.maxPoleRadius());
        maxR = std::max(maxR, rimShot_.maxPoleRadius());
        maxR = std::max(maxR, claves_.maxPoleRadius());
        maxR = std::max(maxR, cowbell_.maxPoleRadius());
        maxR = std::max(maxR, snareNoiseFilter_.maxPoleRadius());
        maxR = std::max(maxR, tambFilter_.maxPoleRadius());
        maxR = std::max(maxR, guiroFilter_.maxPoleRadius());
        maxR = std::max(maxR, scratchHiFilter_.maxPoleRadius());
        maxR = std::max(maxR, scratchLoFilter_.maxPoleRadius());
        maxR = std::max(maxR, chhFilter_.maxPoleRadius());
        maxR = std::max(maxR, ohhFilter_.maxPoleRadius());
        maxR = std::max(maxR, cymFilter_.maxPoleRadius());
        return maxR;
    }

    const DXRhythmboxState& state() const noexcept { return state_; }
    const DXRhythmboxTelemetry& telemetry() const noexcept { return telemetry_; }
    double sampleRate() const noexcept { return fs_; }

private:
    struct DriftTriple
    {
        double effVel;
        double pitchMult;
        double decayMult;
    };

    static int positiveMod(std::int64_t val, int mod) noexcept
    {
        const std::int64_t r = val % static_cast<std::int64_t>(mod);
        return static_cast<int>((r < 0) ? (r + mod) : r);
    }

    static std::int64_t computeInitialLastStepForPpq(double ppq, const PatternSpec& pat) noexcept
    {
        if (ppq < -1e-9)
        {
            return -1;
        }
        const double stepsPerBeat = static_cast<double>(pat.stepsPerBar) / pat.beatsPerBar;
        const double stepFloat = ppq * stepsPerBeat + 1e-9;
        const double stepFloor = std::floor(stepFloat);
        const double frac = stepFloat - stepFloor;
        const std::int64_t floorIdx = static_cast<std::int64_t>(stepFloor);
        return (frac < 0.02) ? (floorIdx - 1) : floorIdx;
    }

    std::size_t activePatternIndices(std::array<std::size_t, kMaxSimultaneousPatterns>& outIndices) const noexcept
    {
        const std::uint16_t mask = capButtonMask(state_.buttonMask, state_.multiPatternEnabled);
        if (mask == 0u)
        {
            return 0u;
        }
        const std::size_t varOffset = state_.variation ? 1u : 0u;
        std::size_t count = 0u;
        for (std::size_t b = 0u; b < kNumButtons; ++b)
        {
            if (((mask >> static_cast<unsigned>(b)) & 1u) != 0u)
            {
                outIndices[count++] = 2u * b + varOffset;
                if (count >= kMaxSimultaneousPatterns)
                {
                    break;
                }
            }
        }
        return count;
    }

    DriftTriple computeHitDrift(VoiceId voice, double vel) noexcept
    {
        const double v = std::clamp(state_.analogVariation, 0.0, 2.0);
        if (v <= 1e-6)
        {
            return DriftTriple{vel, 1.0, 1.0};
        }

        const double uP = driftGen_.nextScalar();
        const double uA = driftGen_.nextScalar();
        const double uD = driftGen_.nextScalar();

        double pJit = 0.0018;
        double aJit = 0.016;
        double dJit = 0.018;

        if (voice == VoiceId::RimShot)
        {
            pJit = 0.0035;
            aJit = 0.040;
            dJit = 0.020;
        }
        else if (voice == VoiceId::LowBongo || voice == VoiceId::Claves)
        {
            pJit = 0.0030;
            aJit = 0.020;
            dJit = 0.018;
        }
        else if (voice == VoiceId::LowTom || voice == VoiceId::MidTom || voice == VoiceId::HighTom)
        {
            pJit = 0.0020;
            aJit = 0.020;
            dJit = 0.018;
        }
        else if (voice == VoiceId::SnareDrum)
        {
            pJit = 0.0020;
            aJit = 0.022;
            dJit = 0.020;
        }
        else if (voice == VoiceId::Tambourine || voice == VoiceId::Guiro ||
                 voice == VoiceId::ScratchHi || voice == VoiceId::ScratchLo ||
                 voice == VoiceId::ClosedHiHat || voice == VoiceId::OpenHiHat ||
                 voice == VoiceId::Cymbal || voice == VoiceId::Cowbell)
        {
            pJit = 0.0000;
            aJit = 0.016;
            dJit = 0.018;
        }

        const double sagAtten = (voice == VoiceId::BassDrum)
                                    ? 1.0
                                    : std::max(0.90, 1.0 - 0.025 * v * railSagEnv_);
        const double effVel = vel * (1.0 + v * aJit * uA) * sagAtten;
        const double pitchMult = 1.0 + v * pJit * uP;
        const double decayMult = 1.0 + v * dJit * uD;
        return DriftTriple{effVel, pitchMult, decayMult};
    }

    static void mixMetallicSource(const MetallicVoiceSpec& sp,
                                  const double* lfsr,
                                  const double* d3,
                                  const double* clkLine,
                                  const double* gateSig,
                                  double* outSrc,
                                  int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            double s = sp.wGate * gateSig[i] + sp.wLfsr * lfsr[i];
            if (sp.wDiv3 != 0.0)
            {
                s += sp.wDiv3 * d3[i];
            }
            if (sp.clockLeakAmp != 0.0)
            {
                s += sp.clockLeakAmp * clkLine[i];
            }
            outSrc[i] = s;
        }
    }

    void processVoicesChunk(double* outMono, int n) noexcept
    {
        DigitalMetalNoiseClock::BlockBuffers clockBufs{
            lfsr_.data(),
            div21Chh_.data(),
            div21Ohh_.data(),
            div21Cym_.data(),
            div25_.data(),
            div3_.data(),
            sq50_.data(),
            clkLine_.data(),
            gateChh_.data(),
            gateOhh_.data(),
            gateCym_.data(),
        };
        digitalClock_.renderBlockExt(clockBufs, n);

        // DRUM_BUS accumulation
        bassDrum_.processBlock(tmpA_.data(), n);
        for (int i = 0; i < n; ++i)
        {
            drumBus_[static_cast<std::size_t>(i)] = tmpA_[static_cast<std::size_t>(i)];
        }

        auto accumulateDrumVoice = [&](auto& voice) noexcept
        {
            voice.processBlock(tmpA_.data(), n);
            for (int i = 0; i < n; ++i)
            {
                drumBus_[static_cast<std::size_t>(i)] += tmpA_[static_cast<std::size_t>(i)];
            }
        };

        accumulateDrumVoice(snareBody_);
        accumulateDrumVoice(lowTom_);
        accumulateDrumVoice(midTom_);
        accumulateDrumVoice(highTom_);
        accumulateDrumVoice(lowBongo_);
        accumulateDrumVoice(rimShot_);
        accumulateDrumVoice(claves_);

        cowbell_.processBlock(sq50_.data(), tmpA_.data(), n, kMaxChunk);
        for (int i = 0; i < n; ++i)
        {
            drumBus_[static_cast<std::size_t>(i)] += tmpA_[static_cast<std::size_t>(i)];
        }

        // METAL_BUS: 8 clocked digital/LFSR gated VCAs -> continuous analog filters
        // 1. Snare noise
        snareNoiseVca_.renderEnvelopeAndPing(envBuf_.data(), pingBuf_.data(), n);
        for (int i = 0; i < n; ++i)
        {
            const std::size_t idx = static_cast<std::size_t>(i);
            tmpA_[idx] = lfsr_[idx] * envBuf_[idx] + pingBuf_[idx];
        }
        snareNoiseFilter_.processBlock(tmpA_.data(), tmpB_.data(), n);
        if (kSnareNoiseVcaSpec.clockLeakAmp != 0.0)
        {
            for (int i = 0; i < n; ++i)
            {
                const std::size_t idx = static_cast<std::size_t>(i);
                tmpB_[idx] += kSnareNoiseVcaSpec.clockLeakAmp * envBuf_[idx] * clkLine_[idx];
            }
        }
        for (int i = 0; i < n; ++i)
        {
            metalBus_[static_cast<std::size_t>(i)] = tmpB_[static_cast<std::size_t>(i)];
        }

        // 2. Tambourine
        tambVca_.renderEnvelopeAndPing(envBuf_.data(), pingBuf_.data(), n);
        for (int i = 0; i < n; ++i)
        {
            const std::size_t idx = static_cast<std::size_t>(i);
            const double tbSrc = kTambourineSpec.lfsrMix * lfsr_[idx] + kTambourineSpec.div21Mix * div21Chh_[idx];
            tmpA_[idx] = tbSrc * envBuf_[idx] + pingBuf_[idx];
        }
        tambFilter_.processBlock(tmpA_.data(), tmpB_.data(), n);
        for (int i = 0; i < n; ++i)
        {
            metalBus_[static_cast<std::size_t>(i)] += tmpB_[static_cast<std::size_t>(i)];
        }

        // 3. Guiro
        guiroVca_.renderEnvelope(envBuf_.data(), n);
        for (int i = 0; i < n; ++i)
        {
            const std::size_t idx = static_cast<std::size_t>(i);
            tmpA_[idx] = lfsr_[idx] * envBuf_[idx];
        }
        guiroFilter_.processBlock(tmpA_.data(), tmpB_.data(), n);
        for (int i = 0; i < n; ++i)
        {
            metalBus_[static_cast<std::size_t>(i)] += tmpB_[static_cast<std::size_t>(i)];
        }

        // 4. Scratch Hi
        scratchHiVca_.renderEnvelopeAndPing(envBuf_.data(), pingBuf_.data(), n);
        for (int i = 0; i < n; ++i)
        {
            const std::size_t idx = static_cast<std::size_t>(i);
            tmpA_[idx] = lfsr_[idx] * envBuf_[idx] + pingBuf_[idx];
        }
        scratchHiFilter_.processBlock(tmpA_.data(), tmpB_.data(), n);
        for (int i = 0; i < n; ++i)
        {
            metalBus_[static_cast<std::size_t>(i)] += tmpB_[static_cast<std::size_t>(i)];
        }

        // 5. Scratch Lo
        scratchLoVca_.renderEnvelopeAndPing(envBuf_.data(), pingBuf_.data(), n);
        for (int i = 0; i < n; ++i)
        {
            const std::size_t idx = static_cast<std::size_t>(i);
            tmpA_[idx] = lfsr_[idx] * envBuf_[idx] + pingBuf_[idx];
        }
        scratchLoFilter_.processBlock(tmpA_.data(), tmpB_.data(), n);
        for (int i = 0; i < n; ++i)
        {
            metalBus_[static_cast<std::size_t>(i)] += tmpB_[static_cast<std::size_t>(i)];
        }

        // 6. Closed Hi-Hat
        chhVca_.renderEnvelopeAndPing(envBuf_.data(), pingBuf_.data(), n);
        mixMetallicSource(kClosedHiHatSpec, lfsr_.data(), div3_.data(), clkLine_.data(), gateChh_.data(), srcBuf_.data(), n);
        for (int i = 0; i < n; ++i)
        {
            const std::size_t idx = static_cast<std::size_t>(i);
            tmpA_[idx] = srcBuf_[idx] * envBuf_[idx] + pingBuf_[idx];
        }
        chhFilter_.processBlock(tmpA_.data(), tmpB_.data(), n);
        for (int i = 0; i < n; ++i)
        {
            metalBus_[static_cast<std::size_t>(i)] += tmpB_[static_cast<std::size_t>(i)];
        }

        // 7. Open Hi-Hat
        ohhVca_.renderEnvelopeAndPing(envBuf_.data(), pingBuf_.data(), n);
        mixMetallicSource(kOpenHiHatSpec, lfsr_.data(), div3_.data(), clkLine_.data(), gateOhh_.data(), srcBuf_.data(), n);
        for (int i = 0; i < n; ++i)
        {
            const std::size_t idx = static_cast<std::size_t>(i);
            tmpA_[idx] = srcBuf_[idx] * envBuf_[idx] + pingBuf_[idx];
        }
        ohhFilter_.processBlock(tmpA_.data(), tmpB_.data(), n);
        for (int i = 0; i < n; ++i)
        {
            metalBus_[static_cast<std::size_t>(i)] += tmpB_[static_cast<std::size_t>(i)];
        }

        // 8. Cymbal
        cymVca_.renderEnvelopeAndPing(envBuf_.data(), pingBuf_.data(), n);
        mixMetallicSource(kCymbalSpec, lfsr_.data(), div3_.data(), clkLine_.data(), gateCym_.data(), srcBuf_.data(), n);
        for (int i = 0; i < n; ++i)
        {
            const std::size_t idx = static_cast<std::size_t>(i);
            tmpA_[idx] = srcBuf_[idx] * envBuf_[idx] + pingBuf_[idx];
        }
        cymFilter_.processBlock(tmpA_.data(), tmpB_.data(), n);
        for (int i = 0; i < n; ++i)
        {
            metalBus_[static_cast<std::size_t>(i)] += tmpB_[static_cast<std::size_t>(i)];
        }

        double gDrum = 1.0;
        double gMetal = 1.0;
        computeToneBusGains(state_.tone, gDrum, gMetal);

        if (railSagEnv_ > 1e-15)
        {
            railSagEnv_ *= std::exp(-static_cast<double>(n) / (0.018 * fs_));
        }
        else
        {
            railSagEnv_ = 0.0;
        }

        const double outGain = state_.volume * kMasterTrim;
        const bool doClip = kOutputSoftClipDrive > 1e-6;
        const double tanhDrive = doClip ? std::tanh(kOutputSoftClipDrive) : 1.0;

        for (int i = 0; i < n; ++i)
        {
            const std::size_t idx = static_cast<std::size_t>(i);
            double s = (drumBus_[idx] * gDrum + metalBus_[idx] * gMetal) * outGain;
            if (doClip)
            {
                s = std::tanh(kOutputSoftClipDrive * s) / tanhDrive;
            }
            outMono[i] = s;
        }
    }

    void updateTelemetry(const double* outMono,
                         int numSamples,
                         double effectiveBpm,
                         bool usingHostBpm,
                         const std::array<std::size_t, kMaxSimultaneousPatterns>& activePats,
                         std::size_t activeCount) noexcept
    {
        double sumSq = 0.0;
        double peak = 0.0;
        for (int i = 0; i < numSamples; ++i)
        {
            const double s = outMono[i];
            sumSq += s * s;
            peak = std::max(peak, std::abs(s));
        }
        telemetry_.outputRms = std::sqrt(sumSq / static_cast<double>(std::max(1, numSamples)));
        telemetry_.outputPeak = peak;

        TempoLampState lamp{};
        lamp.usingHostBpm = usingHostBpm;
        lamp.effectiveBpm = effectiveBpm;
        lamp.activeFillInIndex = fillinActiveIdx_;
        lamp.fillInActive = (fillinActiveIdx_ >= 0);

        if (fillinActiveIdx_ >= 0)
        {
            const FillInSpec& fpat = kFillIns[static_cast<std::size_t>(fillinActiveIdx_)];
            lamp.isRunning = true;
            lamp.totalSteps = fpat.stepsPerBar;
            const double beatMod = std::fmod(std::max(0.0, fillinBeatPos_), 1.0);
            const double barMod = std::fmod(std::max(0.0, fillinBeatPos_), 4.0);
            lamp.beatPhase = beatMod;
            lamp.isDownbeat = (barMod < 0.25);
            lamp.beatFlash = (barMod < 0.25);
            const double spb = static_cast<double>(fpat.stepsPerBar) / 4.0;
            lamp.currentStep = std::clamp(static_cast<int>(std::floor(fillinBeatPos_ * spb)), 0, fpat.stepsPerBar - 1);
        }
        else if (wasRunning_ && activeCount > 0u)
        {
            const PatternSpec& leadPat = kPatterns[activePats[0]];
            const double leadBeats = leadPat.beatsPerBar;
            const double spb = static_cast<double>(leadPat.stepsPerBar) / leadBeats;
            const double beatInBar = std::fmod(std::max(0.0, beatPos_), leadBeats);
            lamp.isRunning = true;
            lamp.totalSteps = leadPat.totalSteps;
            lamp.beatPhase = std::fmod(std::max(0.0, beatPos_), 1.0);
            lamp.isDownbeat = (beatInBar < 0.25);
            lamp.beatFlash = (beatInBar < 0.25);
            lamp.currentStep = positiveMod(static_cast<std::int64_t>(std::floor(std::max(0.0, beatPos_) * spb)),
                                           leadPat.totalSteps);
        }
        else
        {
            lamp.isRunning = false;
            const std::size_t leadIdx = (activeCount > 0u) ? activePats[0] : 0u;
            lamp.totalSteps = (activeCount > 0u) ? kPatterns[leadIdx].totalSteps : 64;
            lamp.beatPhase = std::fmod(std::max(0.0, idleBeatPos_), 1.0);
            lamp.isDownbeat = false;
            lamp.beatFlash = (lamp.beatPhase < 0.25);
            lamp.currentStep = 0;
        }
        telemetry_.lamp = lamp;
    }

    double fs_ = 48000.0;
    DXRhythmboxState state_{};
    DXRhythmboxTelemetry telemetry_{};

    Xorshift32PRNG driftGen_{0x9E3779B9u};
    DigitalMetalNoiseClock digitalClock_{};

    T6118DigitalDrumVoice bassDrum_{};
    BridgedTDampedVoice snareBody_{};
    T6118DigitalDrumVoice lowTom_{};
    T6118DigitalDrumVoice midTom_{};
    T6118DigitalDrumVoice highTom_{};
    BridgedTDampedVoice lowBongo_{};
    BridgedTDampedVoice rimShot_{};
    BridgedTDampedVoice claves_{};
    ClockedCowbellVoice cowbell_{};

    GatedNoiseVCA snareNoiseVca_{};
    BiquadCascade<kMaxFilterSections> snareNoiseFilter_{};
    GatedNoiseVCA tambVca_{};
    BiquadCascade<kMaxFilterSections> tambFilter_{};
    GuiroRatchetVCA guiroVca_{};
    BiquadCascade<kMaxFilterSections> guiroFilter_{};
    GatedNoiseVCA scratchHiVca_{};
    BiquadCascade<kMaxFilterSections> scratchHiFilter_{};
    GatedNoiseVCA scratchLoVca_{};
    BiquadCascade<kMaxFilterSections> scratchLoFilter_{};
    GatedNoiseVCA chhVca_{};
    BiquadCascade<kMaxFilterSections> chhFilter_{};
    GatedNoiseVCA ohhVca_{};
    BiquadCascade<kMaxFilterSections> ohhFilter_{};
    GatedNoiseVCA cymVca_{};
    BiquadCascade<kMaxFilterSections> cymFilter_{};

    double railSagEnv_ = 0.0;
    double beatPos_ = 0.0;
    double idleBeatPos_ = 0.0;
    std::array<std::int64_t, kNumPatterns> lastStepIndices_{};
    int fillinActiveIdx_ = -1;
    double fillinBeatPos_ = 0.0;
    int fillinLastStep_ = -1;
    bool fillinContinue_ = true;

    bool wasRunning_ = false;
    bool hasExpectedPpq_ = false;
    double expectedNextPpq_ = 0.0;
    std::uint32_t prevActivePatMask_ = 0u;

    // Pre-allocated scratch buffers (never dynamically allocated during audio processing)
    std::array<double, kMaxChunk> lfsr_{};
    std::array<double, kMaxChunk> div21Chh_{};
    std::array<double, kMaxChunk> div21Ohh_{};
    std::array<double, kMaxChunk> div21Cym_{};
    std::array<double, kMaxChunk> div25_{};
    std::array<double, kMaxChunk> div3_{};
    std::array<double, kMaxChunk> sq50_{};
    std::array<double, kMaxChunk> clkLine_{};
    std::array<double, kMaxChunk> gateChh_{};
    std::array<double, kMaxChunk> gateOhh_{};
    std::array<double, kMaxChunk> gateCym_{};
    std::array<double, kMaxChunk> drumBus_{};
    std::array<double, kMaxChunk> metalBus_{};
    std::array<double, kMaxChunk> tmpA_{};
    std::array<double, kMaxChunk> tmpB_{};
    std::array<double, kMaxChunk> envBuf_{};
    std::array<double, kMaxChunk> pingBuf_{};
    std::array<double, kMaxChunk> srcBuf_{};
};

} // namespace dx701::dsp
