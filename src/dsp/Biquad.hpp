// Sample-rate-independent RBJ, 1st-order IIR, and continuous-time passive RC/BP biquad cascades (DF2T).
// Formulas match research/dsp_proto/biquad.py 1-to-1 in float64 for exact Python <-> C++20 parity.
#pragma once

#include "FilterTypes.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>

namespace dx701::dsp
{

struct BiquadCoeffs
{
    double b0 = 1.0;
    double b1 = 0.0;
    double b2 = 0.0;
    double a1 = 0.0;
    double a2 = 0.0;
};

inline BiquadCoeffs bilinear2ndOrder(double B2,
                                     double B1,
                                     double B0,
                                     double A2,
                                     double A1,
                                     double A0,
                                     double fs) noexcept
{
    const double K = 2.0 * fs;
    const double K2 = K * K;
    const double b0 = B2 * K2 + B1 * K + B0;
    const double b1 = 2.0 * (B0 - B2 * K2);
    const double b2 = B2 * K2 - B1 * K + B0;
    const double a0 = A2 * K2 + A1 * K + A0;
    const double a1 = 2.0 * (A0 - A2 * K2);
    const double a2 = A2 * K2 - A1 * K + A0;
    const double invA0 = 1.0 / a0;
    return BiquadCoeffs{b0 * invA0, b1 * invA0, b2 * invA0, a1 * invA0, a2 * invA0};
}

inline BiquadCoeffs designSection(const FilterSectionSpec& spec, double fs) noexcept
{
    if (spec.type == FilterType::PassiveHP2)
    {
        const double r0 = spec.p0;
        const double c1 = spec.p1;
        const double r1 = spec.p2;
        const double c2 = spec.p3;
        const double r2 = spec.p4;
        double b2 = c1 * c2 * r1 * r2;
        const double a2 = c1 * c2 * (r0 * r1 + r0 * r2 + r1 * r2);
        const double a1 = c1 * (r0 + r1) + c2 * (r1 + r2);
        if (spec.unityNorm && a2 > 0.0)
        {
            b2 = a2;
        }
        return bilinear2ndOrder(b2, 0.0, 0.0, a2, a1, 1.0, fs);
    }

    if (spec.type == FilterType::PassiveLP2)
    {
        const double r1 = spec.p0;
        const double c1 = spec.p1;
        const double r2 = spec.p2;
        const double c2 = spec.p3;
        const double rload = spec.p4;
        const double a2 = c1 * c2 * r1 * r2 * rload;
        const double a1 = c1 * r1 * (r2 + rload) + c2 * rload * (r1 + r2);
        const double a0 = r1 + r2 + rload;
        const double b0 = spec.unityNorm ? a0 : rload;
        return bilinear2ndOrder(0.0, 0.0, b0, a2, a1, a0, fs);
    }

    if (spec.type == FilterType::PassiveBP2)
    {
        const double r0 = spec.p0;
        const double c1 = spec.p1;
        const double c2 = spec.p2;
        const double rOut = spec.p3;
        const double a2 = c1 * c2 * r0 * rOut;
        const double a1 = r0 * (c1 + c2) + c2 * rOut;
        const double b1 = spec.unityNorm ? a1 : (c2 * rOut);
        return bilinear2ndOrder(0.0, b1, 0.0, a2, a1, 1.0, fs);
    }

    const double fcSafe = std::clamp(spec.p0, 5.0, 0.475 * fs);
    const double q = std::max(0.05, spec.p1);
    const double gainDb = spec.p2;

    if (spec.type == FilterType::LP1)
    {
        const double k = std::tan(std::numbers::pi * fcSafe / fs);
        const double norm = 1.0 / (1.0 + k);
        return BiquadCoeffs{k * norm, k * norm, 0.0, (k - 1.0) * norm, 0.0};
    }
    if (spec.type == FilterType::HP1)
    {
        const double k = std::tan(std::numbers::pi * fcSafe / fs);
        const double norm = 1.0 / (1.0 + k);
        return BiquadCoeffs{norm, -norm, 0.0, (k - 1.0) * norm, 0.0};
    }

    const double w0 = 2.0 * std::numbers::pi * fcSafe / fs;
    const double cosW0 = std::cos(w0);
    const double sinW0 = std::sin(w0);
    const double alpha = sinW0 / (2.0 * q);

    double b0 = 1.0, b1 = 0.0, b2 = 0.0;
    double a0 = 1.0, a1 = 0.0, a2 = 0.0;

    switch (spec.type)
    {
        case FilterType::LP2:
            b0 = 0.5 * (1.0 - cosW0);
            b1 = 1.0 - cosW0;
            b2 = 0.5 * (1.0 - cosW0);
            a0 = 1.0 + alpha;
            a1 = -2.0 * cosW0;
            a2 = 1.0 - alpha;
            break;

        case FilterType::HP2:
            b0 = 0.5 * (1.0 + cosW0);
            b1 = -(1.0 + cosW0);
            b2 = 0.5 * (1.0 + cosW0);
            a0 = 1.0 + alpha;
            a1 = -2.0 * cosW0;
            a2 = 1.0 - alpha;
            break;

        case FilterType::BP2:
            b0 = alpha;
            b1 = 0.0;
            b2 = -alpha;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cosW0;
            a2 = 1.0 - alpha;
            break;

        case FilterType::Peaking:
        {
            const double aLin = std::pow(10.0, gainDb / 40.0);
            b0 = 1.0 + alpha * aLin;
            b1 = -2.0 * cosW0;
            b2 = 1.0 - alpha * aLin;
            a0 = 1.0 + alpha / aLin;
            a1 = -2.0 * cosW0;
            a2 = 1.0 - alpha / aLin;
            break;
        }

        case FilterType::LowShelf:
        {
            const double aLin = std::pow(10.0, gainDb / 40.0);
            const double twoSqrtAAlpha = 2.0 * std::sqrt(aLin) * alpha;
            b0 = aLin * ((aLin + 1.0) - (aLin - 1.0) * cosW0 + twoSqrtAAlpha);
            b1 = 2.0 * aLin * ((aLin - 1.0) - (aLin + 1.0) * cosW0);
            b2 = aLin * ((aLin + 1.0) - (aLin - 1.0) * cosW0 - twoSqrtAAlpha);
            a0 = (aLin + 1.0) + (aLin - 1.0) * cosW0 + twoSqrtAAlpha;
            a1 = -2.0 * ((aLin - 1.0) + (aLin + 1.0) * cosW0);
            a2 = (aLin + 1.0) + (aLin - 1.0) * cosW0 - twoSqrtAAlpha;
            break;
        }

        case FilterType::HighShelf:
        {
            const double aLin = std::pow(10.0, gainDb / 40.0);
            const double twoSqrtAAlpha = 2.0 * std::sqrt(aLin) * alpha;
            b0 = aLin * ((aLin + 1.0) + (aLin - 1.0) * cosW0 + twoSqrtAAlpha);
            b1 = -2.0 * aLin * ((aLin - 1.0) + (aLin + 1.0) * cosW0);
            b2 = aLin * ((aLin + 1.0) + (aLin - 1.0) * cosW0 - twoSqrtAAlpha);
            a0 = (aLin + 1.0) - (aLin - 1.0) * cosW0 + twoSqrtAAlpha;
            a1 = 2.0 * ((aLin - 1.0) - (aLin + 1.0) * cosW0);
            a2 = (aLin + 1.0) - (aLin - 1.0) * cosW0 - twoSqrtAAlpha;
            break;
        }

        case FilterType::LP1:
        case FilterType::HP1:
        case FilterType::PassiveHP2:
        case FilterType::PassiveLP2:
        case FilterType::PassiveBP2:
            break;
    }

    const double invA0 = 1.0 / a0;
    return BiquadCoeffs{b0 * invA0, b1 * invA0, b2 * invA0, a1 * invA0, a2 * invA0};
}

template <std::size_t MaxSections>
class BiquadCascade
{
public:
    void design(const FilterSectionSpec* specs, std::size_t count, double gainDb, double fs) noexcept
    {
        const double g = std::pow(10.0, gainDb / 20.0);
        if (specs == nullptr || count == 0)
        {
            numSections_ = 1;
            coeffs_[0] = BiquadCoeffs{g, 0.0, 0.0, 0.0, 0.0};
            return;
        }
        numSections_ = std::min(count, MaxSections);
        for (std::size_t s = 0; s < numSections_; ++s)
        {
            coeffs_[s] = designSection(specs[s], fs);
        }
        coeffs_[0].b0 *= g;
        coeffs_[0].b1 *= g;
        coeffs_[0].b2 *= g;
    }

    template <std::size_t N>
    void design(const std::array<FilterSectionSpec, N>& specs, std::size_t count, double gainDb, double fs) noexcept
    {
        design(specs.data(), std::min(count, N), gainDb, fs);
    }

    void reset() noexcept
    {
        z1_.fill(0.0);
        z2_.fill(0.0);
    }

    double processSample(double x) noexcept
    {
        double sig = x;
        for (std::size_t s = 0; s < numSections_; ++s)
        {
            const auto& c = coeffs_[s];
            const double y = c.b0 * sig + z1_[s];
            z1_[s] = c.b1 * sig - c.a1 * y + z2_[s];
            z2_[s] = c.b2 * sig - c.a2 * y;
            sig = y;
        }
        return sig;
    }

    void processBlock(const double* in, double* out, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            out[i] = processSample(in[i]);
        }
    }

    void processInPlace(double* buf, int n) noexcept
    {
        processBlock(buf, buf, n);
    }

    double maxPoleRadius() const noexcept
    {
        double maxR = 0.0;
        for (std::size_t s = 0; s < numSections_; ++s)
        {
            const double a1 = coeffs_[s].a1;
            const double a2 = coeffs_[s].a2;
            const double disc = a1 * a1 - 4.0 * a2;
            if (disc >= 0.0)
            {
                const double sq = std::sqrt(disc);
                const double r1 = 0.5 * (-a1 + sq);
                const double r2 = 0.5 * (-a1 - sq);
                maxR = std::max({maxR, std::abs(r1), std::abs(r2)});
            }
            else
            {
                maxR = std::max(maxR, std::sqrt(std::max(0.0, a2)));
            }
        }
        return maxR;
    }

    std::size_t numSections() const noexcept { return numSections_; }

private:
    std::array<BiquadCoeffs, MaxSections> coeffs_{};
    std::array<double, MaxSections> z1_{};
    std::array<double, MaxSections> z2_{};
    std::size_t numSections_ = 0;
};

} // namespace dx701::dsp
