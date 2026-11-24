// Pure C++20 zero-allocation voice circuits for DX701 Rhythmbox (Phase 2.5 v3 parity).
#pragma once

#include "Biquad.hpp"
#include "DXRhythmboxParams.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace dx701::dsp
{

class Xorshift32PRNG
{
public:
    explicit Xorshift32PRNG(std::uint32_t seed = 0x9E3779B9u) noexcept
        : initSeed_(seed != 0u ? seed : 0x9E3779B9u)
        , state_(initSeed_)
    {
    }

    void reset() noexcept
    {
        state_ = initSeed_;
    }

    void reset(std::uint32_t seed) noexcept
    {
        state_ = (seed != 0u) ? seed : initSeed_;
    }

    double nextScalar() noexcept
    {
        state_ ^= (state_ << 13u);
        state_ ^= (state_ >> 17u);
        state_ ^= (state_ << 5u);
        constexpr double kInvScale = 1.0 / 2147483648.0;
        return static_cast<double>(static_cast<std::int32_t>(state_)) * kInvScale;
    }

private:
    std::uint32_t initSeed_ = 0x9E3779B9u;
    std::uint32_t state_ = 0x9E3779B9u;
};

// FPU-rounding-mode-independent IEEE half-to-even rounding matching Python built-in round().
inline double roundHalfToEven(double x) noexcept
{
    const double fl = std::floor(x);
    const double rem = x - fl;
    if (rem > 0.5)
    {
        return fl + 1.0;
    }
    if (rem < 0.5)
    {
        return fl;
    }
    return (std::fmod(std::abs(fl), 2.0) == 0.0) ? fl : (fl + 1.0);
}

inline int evalMaskBit(std::uint32_t mask, int idx) noexcept
{
    return static_cast<int>((mask >> static_cast<unsigned>(idx)) & 1u);
}

inline double evalGateBit(int a, int b, DigitalGateMode gate) noexcept
{
    switch (gate)
    {
        case DigitalGateMode::Or:
            return static_cast<double>(a | b);
        case DigitalGateMode::And:
            return static_cast<double>(a & b);
        case DigitalGateMode::OrInvA:
            return static_cast<double>((1 - a) | b);
        case DigitalGateMode::OrInvB:
            return static_cast<double>(a | (1 - b));
        case DigitalGateMode::AndInvA:
            return static_cast<double>((1 - a) & b);
        case DigitalGateMode::AndInvB:
            return static_cast<double>(a & (1 - b));
        case DigitalGateMode::Sum:
            return static_cast<double>(a + b);
        case DigitalGateMode::Xor:
            return static_cast<double>(a ^ b);
    }
    return 0.0;
}

class DigitalMetalNoiseClock
{
public:
    struct BlockBuffers
    {
        double* lfsr = nullptr;
        double* div21Chh = nullptr;
        double* div21Ohh = nullptr;
        double* div21Cym = nullptr;
        double* div25 = nullptr;
        double* div3 = nullptr;
        double* sq50 = nullptr;
        double* clkLine = nullptr;
        double* gateChh = nullptr;
        double* gateOhh = nullptr;
        double* gateCym = nullptr;
    };

    void prepare(double fs, const DigitalClockConfig& cfg) noexcept
    {
        fs_ = fs;
        cfg_ = cfg;
        chhDc_ = computeGateDcMean(cfg_.chhBits21Mask, cfg_.chhBits25Mask, cfg_.chhGate);
        ohhDc_ = computeGateDcMean(cfg_.ohhBits21Mask, cfg_.ohhBits25Mask, cfg_.ohhGate);
        cymDc_ = computeGateDcMean(cfg_.cymBits21Mask, cfg_.cymBits25Mask, cfg_.cymGate);
        reset();
    }

    void reset() noexcept
    {
        phase_ = 0.0;
        lfsr_ = 0x1A5A5u;
        cnt21_ = 0;
        cnt25_ = 0;
        cnt3_ = 0;
        cnt50_ = 0;
        heldLfsr_ = -1.0;
        heldDiv21Chh_ = 0.0;
        heldDiv21Ohh_ = 0.0;
        heldDiv21Cym_ = 0.0;
        heldDiv25_ = 0.0;
        heldDiv3_ = 0.0;
        heldSq50_ = 1.0;
        heldGateChh_ = 0.0;
        heldGateOhh_ = 0.0;
        heldGateCym_ = 0.0;
    }

    void renderBlockExt(const BlockBuffers& bufs, int n) noexcept
    {
        const double inc = cfg_.clockHz / fs_;
        double ph = phase_;
        std::uint32_t reg = lfsr_;
        int c21 = cnt21_;
        int c25 = cnt25_;
        int c3 = cnt3_;
        int c50 = cnt50_;

        double hLfsr = heldLfsr_;
        double hChh = heldDiv21Chh_;
        double hOhh = heldDiv21Ohh_;
        double hCym = heldDiv21Cym_;
        double hD25 = heldDiv25_;
        double hD3 = heldDiv3_;
        double hSq50 = heldSq50_;
        double hGChh = heldGateChh_;
        double hGOhh = heldGateOhh_;
        double hGCym = heldGateCym_;

        constexpr double kTwoPi = 2.0 * std::numbers::pi;

        for (int i = 0; i < n; ++i)
        {
            ph += inc;
            while (ph >= 1.0)
            {
                ph -= 1.0;
                const std::uint32_t bit = ((reg >> 16u) ^ (reg >> 13u)) & 1u;
                reg = ((reg << 1u) | bit) & 0x1FFFFu;
                if (reg == 0u)
                {
                    reg = 0x1A5A5u;
                }
                hLfsr = ((reg & 1u) != 0u) ? 1.0 : -1.0;

                c21 = (c21 < 20) ? (c21 + 1) : 0;
                c25 = (c25 < 24) ? (c25 + 1) : 0;
                c3 = (c3 < 2) ? (c3 + 1) : 0;
                c50 = (c50 < 49) ? (c50 + 1) : 0;

                hChh = zeroMeanPulse(c21, cfg_.chhDuty21, 21) * 2.0;
                hOhh = zeroMeanPulse(c21, cfg_.ohhDuty21, 21) * 2.0;
                hCym = zeroMeanPulse(c21, cfg_.cymDuty21, 21) * 2.0;
                hD25 = zeroMeanPulse(c25, cfg_.duty25, 25) * 2.0;
                hD3 = zeroMeanPulse(c3, 1, 3) * 2.0;
                hSq50 = zeroMeanPulse(c50, cfg_.duty50, 50) * 2.0;

                hGChh = (evalGateBit(evalMaskBit(cfg_.chhBits21Mask, c21), evalMaskBit(cfg_.chhBits25Mask, c25), cfg_.chhGate) - chhDc_) * 2.0;
                hGOhh = (evalGateBit(evalMaskBit(cfg_.ohhBits21Mask, c21), evalMaskBit(cfg_.ohhBits25Mask, c25), cfg_.ohhGate) - ohhDc_) * 2.0;
                hGCym = (evalGateBit(evalMaskBit(cfg_.cymBits21Mask, c21), evalMaskBit(cfg_.cymBits25Mask, c25), cfg_.cymGate) - cymDc_) * 2.0;
            }

            bufs.lfsr[i] = hLfsr;
            bufs.div21Chh[i] = hChh;
            bufs.div21Ohh[i] = hOhh;
            bufs.div21Cym[i] = hCym;
            bufs.div25[i] = hD25;
            bufs.div3[i] = hD3;
            bufs.sq50[i] = hSq50;
            bufs.clkLine[i] = std::sin(kTwoPi * ph);
            bufs.gateChh[i] = hGChh;
            bufs.gateOhh[i] = hGOhh;
            bufs.gateCym[i] = hGCym;
        }

        phase_ = ph;
        lfsr_ = reg;
        cnt21_ = c21;
        cnt25_ = c25;
        cnt3_ = c3;
        cnt50_ = c50;
        heldLfsr_ = hLfsr;
        heldDiv21Chh_ = hChh;
        heldDiv21Ohh_ = hOhh;
        heldDiv21Cym_ = hCym;
        heldDiv25_ = hD25;
        heldDiv3_ = hD3;
        heldSq50_ = hSq50;
        heldGateChh_ = hGChh;
        heldGateOhh_ = hGOhh;
        heldGateCym_ = hGCym;
    }

private:
    static double zeroMeanPulse(int cnt, int width, int mod) noexcept
    {
        const double mean = static_cast<double>(width) / static_cast<double>(mod);
        return (cnt < width) ? (1.0 - mean) : -mean;
    }

    static double computeGateDcMean(std::uint32_t b21Mask, std::uint32_t b25Mask, DigitalGateMode gate) noexcept
    {
        double total = 0.0;
        for (int t = 0; t < 525; ++t)
        {
            total += evalGateBit(evalMaskBit(b21Mask, t % 21), evalMaskBit(b25Mask, t % 25), gate);
        }
        return total / 525.0;
    }

    double fs_ = 48000.0;
    DigitalClockConfig cfg_{};
    double chhDc_ = 0.0;
    double ohhDc_ = 0.0;
    double cymDc_ = 0.0;

    double phase_ = 0.0;
    std::uint32_t lfsr_ = 0x1A5A5u;
    int cnt21_ = 0;
    int cnt25_ = 0;
    int cnt3_ = 0;
    int cnt50_ = 0;

    double heldLfsr_ = -1.0;
    double heldDiv21Chh_ = 0.0;
    double heldDiv21Ohh_ = 0.0;
    double heldDiv21Cym_ = 0.0;
    double heldDiv25_ = 0.0;
    double heldDiv3_ = 0.0;
    double heldSq50_ = 1.0;
    double heldGateChh_ = 0.0;
    double heldGateOhh_ = 0.0;
    double heldGateCym_ = 0.0;
};

inline double cytomicSvfPoleRadius(double fHz, double tauDecS, double fs) noexcept
{
    const double fcMax = 0.45 * fs;
    const double fClamped = std::min(fcMax, std::max(5.0, fHz));
    const double q = std::max(0.7, std::numbers::pi * fClamped * std::max(4e-4, tauDecS));
    const double g = std::tan(std::numbers::pi * fClamped / fs);
    const double k = 1.0 / q;
    const double det = (1.0 + g * (g - k)) / (1.0 + g * (g + k));
    return std::sqrt(std::max(0.0, det));
}

class BridgedTDampedVoice
{
public:
    void prepare(double fs, const BridgedTSpec& spec) noexcept
    {
        fs_ = fs;
        spec_ = spec;
        hasPostFilter_ = (spec_.postFilter.numSections > 0u);
        if (hasPostFilter_)
        {
            postFilter_.design(spec_.postFilter.sections, spec_.postFilter.numSections, spec_.postFilter.filterGainDb, fs_);
        }
        reset();
    }

    void reset() noexcept
    {
        ic1_ = 0.0;
        ic2_ = 0.0;
        pF_ = 0.0;
        pR_ = 0.0;
        hpState_ = 0.0;
        ageS_ = 100.0;
        crackAgeS_ = 100.0;
        trigVel_ = 0.0;
        pitchMult_ = 1.0;
        decayMult_ = 1.0;
        if (hasPostFilter_)
        {
            postFilter_.reset();
        }
    }

    void trigger(double velocity = 1.0, double pitchMult = 1.0, double decayMult = 1.0) noexcept
    {
        const double vel = std::max(0.0, velocity);
        ageS_ = 0.0;
        crackAgeS_ = 0.0;
        trigVel_ = vel;
        pitchMult_ = pitchMult;
        decayMult_ = std::max(0.2, decayMult);
        pF_ += vel;
        pR_ += vel;
    }

    void processBlock(double* out, int n) noexcept
    {
        if (n <= 0)
        {
            return;
        }

        const double tauCkS = std::max(4e-5, spec_.tauCrackMs * 1e-3);
        const double crackMaxS = (spec_.crackAmp != 0.0) ? (tauCkS * 14.0) : 0.0;

        if (crackAgeS_ >= crackMaxS &&
            std::abs(ic1_) + std::abs(ic2_) + std::abs(pF_) + std::abs(pR_) + std::abs(hpState_) < 1e-15)
        {
            std::fill_n(out, n, 0.0);
            if (hasPostFilter_)
            {
                postFilter_.processInPlace(out, n);
            }
            return;
        }

        const double dt = 1.0 / fs_;
        const double fBase = spec_.freqHz;
        const double dropRatio = spec_.pitchDropRatio;
        const double tauDropS = std::max(1e-4, spec_.tauDropMs * 1e-3);
        const double tauDecS = std::max(4e-4, spec_.tauDecayMs * 1e-3) * decayMult_;
        const double tauRiseS = std::max(1e-5, spec_.tauRiseMs * 1e-3);
        const double tauFallS = std::max(tauRiseS * 1.15, spec_.tauFallMs * 1e-3);
        const double rTf = tauRiseS / tauFallS;
        const double uScale = 1.0 / std::max((1.0 - rTf) * std::pow(rTf, rTf / (1.0 - rTf)), 1e-6);

        const double feedGain = spec_.feedGain;
        const double hpFeedHz = spec_.hpFeedHz;
        const double h2Ratio = spec_.h2Ratio;
        const double satDrive = spec_.satDrive;
        const bool hasSat = satDrive > 1e-6;
        const double invTanhSat = hasSat ? (1.0 / std::tanh(satDrive)) : 1.0;
        const double crackAmp = spec_.crackAmp;
        const double crackFreqHz = spec_.crackFreqHz;
        const double crackPhase = spec_.crackPhaseRad;
        const double gain = spec_.gain;

        const double cF = std::exp(-dt / tauFallS);
        const double cR = std::exp(-dt / tauRiseS);
        constexpr double kTwoPi = 2.0 * std::numbers::pi;
        const double cHp = (hpFeedHz > 0.0) ? std::exp(-kTwoPi * hpFeedHz * dt) : 0.0;
        const double fcMax = 0.45 * fs_;

        const bool hasDrop = std::abs(dropRatio) > 1e-6;
        double a1Fixed = 0.0;
        double a2Fixed = 0.0;
        double a3Fixed = 0.0;
        double nlScaleFixed = 0.0;

        if (!hasDrop)
        {
            const double fFixed = std::min(fcMax, fBase * pitchMult_);
            const double qFixed = std::max(0.7, std::numbers::pi * fFixed * tauDecS);
            const double gFixed = std::tan(std::numbers::pi * fFixed * dt);
            a1Fixed = 1.0 / (1.0 + gFixed * (gFixed + 1.0 / qFixed));
            a2Fixed = gFixed * a1Fixed;
            a3Fixed = gFixed * a2Fixed;
            nlScaleFixed = 2.0 * h2Ratio / std::max(1.0, qFixed * 0.25);
        }

        double ic1 = ic1_;
        double ic2 = ic2_;
        double pF = pF_;
        double pR = pR_;
        double hpSt = hpState_;
        double t = ageS_;
        double tc = crackAgeS_;
        const double vel = trigVel_;

        for (int i = 0; i < n; ++i)
        {
            pF *= cF;
            pR *= cR;
            const double u = (pF - pR) * uScale;
            double hpOut = u;
            if (hpFeedHz > 0.0)
            {
                hpOut = u - hpSt;
                hpSt = u + cHp * (hpSt - u);
            }

            double a1 = a1Fixed;
            double a2 = a2Fixed;
            double a3 = a3Fixed;
            double nlS = nlScaleFixed;

            if (hasDrop)
            {
                const double fInst = std::min(fcMax, fBase * pitchMult_ * (1.0 + dropRatio * std::exp(-t / tauDropS)));
                const double qCur = std::max(0.7, std::numbers::pi * fInst * tauDecS);
                const double gCur = std::tan(std::numbers::pi * fInst * dt);
                a1 = 1.0 / (1.0 + gCur * (gCur + 1.0 / qCur));
                a2 = gCur * a1;
                a3 = gCur * a2;
                nlS = 2.0 * h2Ratio / std::max(1.0, qCur * 0.25);
            }

            const double v3 = u - ic2;
            const double v1 = a1 * ic1 + a2 * v3;
            const double v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0 * v1 - ic1;
            ic2 = 2.0 * v2 - ic2;

            const double bodySig = hasSat ? (std::tanh(satDrive * v1) * invTanhSat) : v1;
            double val = bodySig + feedGain * hpOut;
            if (h2Ratio != 0.0)
            {
                val += nlS * (v1 * v2);
            }
            if (crackAmp != 0.0 && tc < crackMaxS)
            {
                val += vel * crackAmp * std::exp(-tc / tauCkS) * std::sin(kTwoPi * crackFreqHz * tc + crackPhase);
            }
            out[i] = gain * val;
            t += dt;
            tc += dt;
        }

        ic1_ = ic1;
        ic2_ = ic2;
        pF_ = pF;
        pR_ = pR;
        hpState_ = hpSt;
        ageS_ = t;
        crackAgeS_ = tc;

        if (hasPostFilter_)
        {
            postFilter_.processInPlace(out, n);
        }
    }

    double maxPoleRadius() const noexcept
    {
        const double tauDecS = std::max(4e-4, spec_.tauDecayMs * 1e-3);
        double maxR = cytomicSvfPoleRadius(spec_.freqHz, tauDecS, fs_);
        if (spec_.pitchDropRatio > 1e-6)
        {
            maxR = std::max(maxR, cytomicSvfPoleRadius(spec_.freqHz * (1.0 + spec_.pitchDropRatio), tauDecS, fs_));
        }
        if (hasPostFilter_)
        {
            maxR = std::max(maxR, postFilter_.maxPoleRadius());
        }
        return maxR;
    }

private:
    double fs_ = 48000.0;
    BridgedTSpec spec_{};
    bool hasPostFilter_ = false;
    BiquadCascade<kMaxFilterSections> postFilter_{};

    double ic1_ = 0.0;
    double ic2_ = 0.0;
    double pF_ = 0.0;
    double pR_ = 0.0;
    double hpState_ = 0.0;
    double ageS_ = 100.0;
    double crackAgeS_ = 100.0;
    double trigVel_ = 0.0;
    double pitchMult_ = 1.0;
    double decayMult_ = 1.0;
};

class T6118DigitalDrumVoice
{
public:
    void prepare(double fs, const T6118DrumSpec& spec, double clockHz = kClockHz) noexcept
    {
        fs_ = fs;
        spec_ = spec;
        clockHz_ = clockHz;
        postFilter_.design(spec_.postFilter.sections, spec_.postFilter.numSections, spec_.postFilter.filterGainDb, fs_);
        reset();
    }

    void reset() noexcept
    {
        clkPh_ = 0.999999;
        wtPh_ = 0.0;
        ageS_ = 100.0;
        trigVel_ = 0.0;
        pitchMult_ = 1.0;
        decayMult_ = 1.0;
        heldE_ = 0.0;
        heldW_ = 0.0;
        envSlew_ = 0.0;
        postFilter_.reset();
    }

    void trigger(double velocity = 1.0, double pitchMult = 1.0, double decayMult = 1.0) noexcept
    {
        ageS_ = 0.0;
        trigVel_ = std::max(0.0, velocity);
        pitchMult_ = pitchMult;
        decayMult_ = std::max(0.2, decayMult);
        wtPh_ = 0.0;
    }

    void processBlock(double* out, int n) noexcept
    {
        if (n <= 0)
        {
            return;
        }

        const double gateOffS = (spec_.gateOffMs > 0.0) ? (spec_.gateOffMs * 1e-3) : 0.0;
        const double tauDecS = std::max(1e-3, spec_.tauDecayMs * 1e-3) * decayMult_;
        const double holdS = std::max(0.0, spec_.holdMs * 1e-3);
        const double maxActiveS = (gateOffS > 0.0) ? gateOffS : (holdS + 14.0 * tauDecS);

        if (ageS_ >= maxActiveS + 0.01 && envSlew_ < 1e-15)
        {
            const double inc = clockHz_ / fs_;
            double ph = clkPh_;
            for (int i = 0; i < n; ++i)
            {
                ph += inc;
                while (ph >= 1.0)
                {
                    ph -= 1.0;
                }
            }
            clkPh_ = ph;
            heldE_ = 0.0;
            envSlew_ = 0.0;
            std::fill_n(out, n, 0.0);
            postFilter_.processInPlace(out, n);
            return;
        }

        const double dt = 1.0 / fs_;
        const double inc = clockHz_ / fs_;
        const double fBase = spec_.freqHz;
        const double phase0 = spec_.phase0;
        const double initPitchOff = spec_.initPitchOffset;
        const double tauInitPS = std::max(2e-4, spec_.tauInitPitchMs * 1e-3);
        const double h2Amp = spec_.h2Amp;
        const double h3Amp = spec_.h3Amp;
        const double h5Amp = spec_.h5Amp;
        const double h7Amp = spec_.h7Amp;
        const int dacLevels = spec_.dacLevels;

        const double atkS = std::max(0.0, spec_.attackRampMs * 1e-3);
        const double initEnvGain = spec_.initEnvGain;
        const double attStepDb = std::max(0.0, spec_.attStepDb);
        const double gateOffDb = spec_.gateOffDb;
        const double attSlewS = std::max(4e-5, spec_.attSlewMs * 1e-3);
        const double cSlew = std::exp(-dt / attSlewS);

        const double atkClickAmp = spec_.attackClickAmp;
        const double atkClickS = std::max(5e-5, spec_.attackClickMs * 1e-3);
        const double atkClickMaxS = (atkClickAmp != 0.0) ? (atkClickS * 14.0) : 0.0;

        const double gain = spec_.gain;
        const double vel = trigVel_;
        constexpr double kTwoPi = 2.0 * std::numbers::pi;
        const double invClk = 1.0 / clockHz_;

        double clkPh = clkPh_;
        double wtPh = wtPh_;
        double t = ageS_;
        double hE = heldE_;
        double hW = heldW_;
        double slew = envSlew_;

        for (int i = 0; i < n; ++i)
        {
            clkPh += inc;
            while (clkPh >= 1.0)
            {
                clkPh -= 1.0;
                double fInst = fBase * pitchMult_;
                if (initPitchOff != 0.0)
                {
                    fInst *= 1.0 + initPitchOff * std::exp(-t / tauInitPS);
                }
                wtPh += fInst * invClk;
                if (wtPh >= 1.0)
                {
                    wtPh -= std::floor(wtPh);
                }

                const double phRad = kTwoPi * (wtPh + phase0);
                double w = std::sin(phRad);
                if (h2Amp != 0.0)
                {
                    w += h2Amp * std::sin(2.0 * phRad);
                }
                if (h3Amp != 0.0)
                {
                    w += h3Amp * std::sin(3.0 * phRad);
                }
                if (h5Amp != 0.0)
                {
                    w += h5Amp * std::sin(5.0 * phRad);
                }
                if (h7Amp != 0.0)
                {
                    w += h7Amp * std::sin(7.0 * phRad);
                }
                if (dacLevels > 1)
                {
                    const double half = 0.5 * static_cast<double>(dacLevels);
                    w = roundHalfToEven(w * half) / half;
                }

                double rawE = 0.0;
                if (gateOffS > 0.0 && t >= gateOffS)
                {
                    rawE = 0.0;
                }
                else if (atkS > 0.0 && t < atkS)
                {
                    const double frac = t / atkS;
                    rawE = vel * (initEnvGain + (1.0 - initEnvGain) * 0.5 * (1.0 - std::cos(std::numbers::pi * frac)));
                }
                else if (t < holdS)
                {
                    rawE = vel;
                }
                else
                {
                    const double attenDb = (t - holdS) * 8.685889638065037 / tauDecS;
                    if (gateOffDb < 0.0 && attenDb >= -gateOffDb)
                    {
                        rawE = 0.0;
                    }
                    else if (attStepDb > 0.0)
                    {
                        const double stepIdx = std::floor(attenDb / attStepDb);
                        rawE = vel * std::pow(10.0, -stepIdx * attStepDb / 20.0);
                    }
                    else
                    {
                        rawE = vel * std::exp(-(t - holdS) / tauDecS);
                    }
                }

                hE = rawE;
                hW = w;
            }

            slew = hE + cSlew * (slew - hE);
            if (slew < 1e-16 && hE == 0.0)
            {
                slew = 0.0;
            }
            double sig = gain * slew * hW;
            if (atkClickAmp != 0.0 && t < atkClickMaxS)
            {
                sig += gain * vel * atkClickAmp * std::exp(-t / atkClickS);
            }
            out[i] = sig;
            t += dt;
        }

        clkPh_ = clkPh;
        wtPh_ = wtPh;
        ageS_ = t;
        heldE_ = hE;
        heldW_ = hW;
        envSlew_ = slew;

        postFilter_.processInPlace(out, n);
    }

    double maxPoleRadius() const noexcept
    {
        return postFilter_.maxPoleRadius();
    }

private:
    double fs_ = 48000.0;
    double clockHz_ = kClockHz;
    T6118DrumSpec spec_{};
    BiquadCascade<kMaxFilterSections> postFilter_{};

    double clkPh_ = 0.999999;
    double wtPh_ = 0.0;
    double ageS_ = 100.0;
    double trigVel_ = 0.0;
    double pitchMult_ = 1.0;
    double decayMult_ = 1.0;
    double heldE_ = 0.0;
    double heldW_ = 0.0;
    double envSlew_ = 0.0;
};

class GatedNoiseVCA
{
public:
    void prepare(double fs, const GatedVcaSpec& spec) noexcept
    {
        fs_ = fs;
        spec_ = spec;
        reset();
    }

    void reset() noexcept
    {
        rc_ = 0.0;
        slewState_ = 0.0;
        ageS_ = 100.0;
        pingAgeS_ = 100.0;
        trigVel_ = 0.0;
        decayMult_ = 1.0;
        choked_ = false;
    }

    void trigger(double velocity = 1.0, double decayMult = 1.0) noexcept
    {
        trigVel_ = std::max(0.0, velocity);
        decayMult_ = std::max(0.2, decayMult);
        ageS_ = 0.0;
        pingAgeS_ = 0.0;
        choked_ = false;
    }

    void choke(double fastDecayMult = 0.08) noexcept
    {
        if (rc_ > 1e-6)
        {
            ageS_ = 1.0;
            decayMult_ = std::min(decayMult_, fastDecayMult);
            choked_ = true;
        }
    }

    void renderEnvelopeAndPing(double* envOut, double* pingOut, int n) noexcept
    {
        const double gateS = std::max(1e-4, spec_.gateMs * 1e-3);
        const double pingAmp = spec_.pingAmp;
        const double pingFreqHz = std::min(0.45 * fs_, spec_.pingFreqHz);
        const double tauPingS = std::max(1e-4, spec_.tauPingMs * 1e-3);
        const double pingMaxS = (pingAmp != 0.0) ? (tauPingS * 14.0) : 0.0;

        const double spikeAmp = spec_.spikeAmp;
        const double spikeTauS = std::max(1e-4, spec_.spikeTauMs * 1e-3);
        const double spikeMaxS = (spikeAmp > 0.0) ? (spikeTauS * 14.0) : 0.0;

        if (ageS_ >= gateS && rc_ < 1e-15 && slewState_ < 1e-15 && pingAgeS_ >= pingMaxS)
        {
            std::fill_n(envOut, n, 0.0);
            if (pingOut != nullptr)
            {
                std::fill_n(pingOut, n, 0.0);
            }
            return;
        }

        const double dt = 1.0 / fs_;
        const double droopTauS = std::max(5e-3, spec_.holdDroopMs * 1e-3);
        const double tauAtkS = std::max(5e-5, spec_.tauAttackMs * 1e-3);
        const double tauRelS = std::max(1e-3, spec_.tauReleaseMs * 1e-3) * decayMult_;
        const double satDrive = std::max(0.2, spec_.vcaTanhDrive);
        const double attStepDb = std::max(0.0, spec_.attStepDb);
        const double gateOffDb = spec_.gateOffDb;
        const double attSlewS = std::max(4e-5, spec_.attSlewMs * 1e-3);
        const double gain = spec_.gain;

        const double cAtk = std::exp(-dt / tauAtkS);
        const double cRel = std::exp(-dt / tauRelS);
        const double cSlew = std::exp(-dt / attSlewS);
        const double td = std::tanh(satDrive);
        constexpr double kTwoPi = 2.0 * std::numbers::pi;

        double t = ageS_;
        double tp = pingAgeS_;
        double rc = rc_;
        double slewSt = slewState_;
        const double vel = trigVel_;

        for (int i = 0; i < n; ++i)
        {
            if (t < gateS)
            {
                const double target = vel * std::exp(-t / droopTauS);
                rc = target + cAtk * (rc - target);
            }
            else
            {
                rc *= cRel;
            }

            double shaped = 0.0;
            if (attStepDb > 0.0)
            {
                double qEnv = 0.0;
                if (rc <= 1e-12 || vel <= 1e-12)
                {
                    qEnv = 0.0;
                }
                else
                {
                    const double ratio = rc / vel;
                    if (ratio >= 1.0)
                    {
                        qEnv = vel;
                    }
                    else
                    {
                        const double attenDb = -8.685889638065037 * std::log(std::max(ratio, 1e-12));
                        if (gateOffDb < 0.0 && attenDb >= -gateOffDb)
                        {
                            qEnv = 0.0;
                        }
                        else
                        {
                            const double stepIdx = std::floor(attenDb / attStepDb);
                            qEnv = vel * std::pow(10.0, -stepIdx * attStepDb / 20.0);
                        }
                    }
                }
                slewSt = qEnv + cSlew * (slewSt - qEnv);
                if (slewSt < 1e-16 && qEnv == 0.0)
                {
                    slewSt = 0.0;
                }
                shaped = slewSt;
            }
            else
            {
                shaped = std::tanh(satDrive * std::max(0.0, rc)) / td;
            }

            if (spikeAmp > 0.0 && t < spikeMaxS)
            {
                shaped += vel * spikeAmp * std::exp(-t / spikeTauS);
            }
            envOut[i] = gain * shaped;

            if (pingOut != nullptr)
            {
                pingOut[i] = (tp < pingMaxS)
                                 ? (pingAmp * vel * std::exp(-tp / tauPingS) * std::sin(kTwoPi * pingFreqHz * tp))
                                 : 0.0;
            }

            t += dt;
            tp += dt;
        }

        ageS_ = t;
        pingAgeS_ = tp;
        rc_ = rc;
        slewState_ = slewSt;
    }

private:
    double fs_ = 48000.0;
    GatedVcaSpec spec_{};
    double rc_ = 0.0;
    double slewState_ = 0.0;
    double ageS_ = 100.0;
    double pingAgeS_ = 100.0;
    double trigVel_ = 0.0;
    double decayMult_ = 1.0;
    bool choked_ = false;
};

class ClockedCowbellVoice
{
public:
    void prepare(double fs, const CowbellSpec& spec) noexcept
    {
        fs_ = fs;
        spec_ = spec;
        vca_.prepare(fs_, spec_.vca);
        filter_.design(spec_.filter.sections, spec_.filter.numSections, spec_.filter.filterGainDb, fs_);
        reset();
    }

    void reset() noexcept
    {
        vca_.reset();
        filter_.reset();
        osc2Phase_ = 0.0;
        dcState_ = 0.0;
        pitchMult_ = 1.0;
        clankEnv_ = 0.0;
        clankLpState_ = 0.0;
        clankDecayMult_ = 1.0;
    }

    void trigger(double velocity = 1.0, double pitchMult = 1.0, double decayMult = 1.0) noexcept
    {
        pitchMult_ = pitchMult;
        vca_.trigger(velocity, decayMult);
        clankEnv_ = velocity * spec_.attackClankAmp;
        clankLpState_ = 0.0;
        clankDecayMult_ = decayMult;
    }

    void processBlock(const double* sq50, double* out, int n, int maxChunkSize) noexcept
    {
        std::array<double, 128> env{};
        std::array<double, 128> ping{};
        std::array<double, 128> exc{};
        std::array<double, 128> clankOut{};
        (void)maxChunkSize;

        vca_.renderEnvelopeAndPing(env.data(), ping.data(), n);

        double maxEnv = 0.0;
        double maxPing = 0.0;
        for (int i = 0; i < n; ++i)
        {
            maxEnv = std::max(maxEnv, std::abs(env[static_cast<std::size_t>(i)]));
            maxPing = std::max(maxPing, std::abs(ping[static_cast<std::size_t>(i)]));
        }

        const double inc = std::min(0.45, spec_.osc2FreqHz * pitchMult_ / fs_);
        if (maxEnv < 1e-15 && maxPing < 1e-15 && clankEnv_ < 1e-15 && std::abs(clankLpState_) < 1e-15)
        {
            osc2Phase_ = std::fmod(osc2Phase_ + static_cast<double>(n) * inc, 1.0);
            clankEnv_ = 0.0;
            clankLpState_ = 0.0;
            std::fill_n(out, n, 0.0);
            filter_.processInPlace(out, n);
            return;
        }

        const double sq50Mix = spec_.sq50Mix;
        const double osc2Mix = spec_.osc2Mix;
        const double evenAsym = spec_.evenAsym;

        double ph = osc2Phase_;
        double dc = dcState_;
        constexpr double kTwoPi = 2.0 * std::numbers::pi;
        const double cDc = std::exp(-kTwoPi * 120.0 / fs_);
        const double clankTauS = std::max(0.0005, spec_.attackClankTauMs * 1e-3 * clankDecayMult_);
        const double clankDecay = std::exp(-1.0 / (clankTauS * fs_));
        const double aLp = 1.0 - std::exp(-kTwoPi * spec_.attackClankLpHz / fs_);
        double cEnv = clankEnv_;
        double cLp = clankLpState_;

        for (int i = 0; i < n; ++i)
        {
            ph += inc;
            if (ph >= 1.0)
            {
                ph -= std::floor(ph);
            }
            const double osc2 = (ph < 0.5) ? 1.0 : -1.0;
            double raw = sq50Mix * sq50[i] + osc2Mix * osc2;
            if (evenAsym != 0.0)
            {
                const double sqPart = evenAsym * (raw * raw);
                const double sqAc = sqPart - dc;
                dc = sqPart + cDc * (dc - sqPart);
                raw += sqAc;
            }
            const std::size_t idx = static_cast<std::size_t>(i);
            exc[idx] = raw * env[idx] + ping[idx];

            double clankIn = 0.0;
            if (cEnv >= 1e-15)
            {
                clankIn = cEnv * (osc2 + 0.55 * (sq50[i] * osc2));
                cEnv *= clankDecay;
                if (cEnv < 1e-15)
                {
                    cEnv = 0.0;
                }
            }
            if (clankIn != 0.0 || std::abs(cLp) >= 1e-16)
            {
                cLp += aLp * (clankIn - cLp);
                if (cEnv == 0.0 && std::abs(cLp) < 1e-16)
                {
                    cLp = 0.0;
                }
            }
            clankOut[idx] = cLp;
        }

        osc2Phase_ = ph;
        dcState_ = dc;
        clankEnv_ = cEnv;
        clankLpState_ = cLp;

        filter_.processBlock(exc.data(), out, n);
        for (int i = 0; i < n; ++i)
        {
            out[i] += clankOut[static_cast<std::size_t>(i)];
        }
    }

    double maxPoleRadius() const noexcept
    {
        return filter_.maxPoleRadius();
    }

private:
    double fs_ = 48000.0;
    CowbellSpec spec_{};
    GatedNoiseVCA vca_{};
    BiquadCascade<kMaxFilterSections> filter_{};
    double osc2Phase_ = 0.0;
    double dcState_ = 0.0;
    double pitchMult_ = 1.0;
    double clankEnv_ = 0.0;
    double clankLpState_ = 0.0;
    double clankDecayMult_ = 1.0;
};

class GuiroRatchetVCA
{
public:
    void prepare(double fs, const GuiroSpec& spec) noexcept
    {
        fs_ = fs;
        spec_ = spec;
        vca_.prepare(fs_, spec_.vca);
        reset();
    }

    void reset() noexcept
    {
        vca_.reset();
        ratchetPhase_ = 0.0;
    }

    void trigger(double velocity = 1.0, double decayMult = 1.0) noexcept
    {
        vca_.trigger(velocity, decayMult);
    }

    void renderEnvelope(double* envOut, int n) noexcept
    {
        const double duty = std::clamp(spec_.ratchetDuty, 0.15, 0.85);
        const double inc = spec_.ratchetHz / fs_;
        vca_.renderEnvelopeAndPing(envOut, nullptr, n);

        double maxAbs = 0.0;
        for (int i = 0; i < n; ++i)
        {
            maxAbs = std::max(maxAbs, std::abs(envOut[i]));
        }
        if (maxAbs < 1e-15)
        {
            ratchetPhase_ = std::fmod(ratchetPhase_ + static_cast<double>(n) * inc, 1.0);
            return;
        }

        double ph = ratchetPhase_;
        const double invDuty = 1.0 / duty;
        constexpr double kPi = std::numbers::pi;
        for (int i = 0; i < n; ++i)
        {
            if (ph < duty)
            {
                const double s = std::sin(kPi * ph * invDuty);
                envOut[i] *= s * s;
            }
            else
            {
                envOut[i] = 0.0;
            }
            ph += inc;
            if (ph >= 1.0)
            {
                ph -= std::floor(ph);
            }
        }
        ratchetPhase_ = ph;
    }

private:
    double fs_ = 48000.0;
    GuiroSpec spec_{};
    GatedNoiseVCA vca_{};
    double ratchetPhase_ = 0.0;
};

} // namespace dx701::dsp
