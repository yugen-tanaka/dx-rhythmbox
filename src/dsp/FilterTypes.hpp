// Shared filter specification types for DX701 Rhythmbox C++20 DSP.
#pragma once

#include <cstddef>

namespace dx701::dsp
{

inline constexpr std::size_t kMaxFilterSections = 6;

enum class FilterType : int
{
    LP1 = 0,
    HP1,
    LP2,
    HP2,
    BP2,
    Peaking,
    LowShelf,
    HighShelf,
    PassiveHP2,
    PassiveLP2,
    PassiveBP2
};

struct FilterSectionSpec
{
    FilterType type = FilterType::LP1;
    // RBJ/1st-order: p0=freqHz, p1=q, p2=gainDb
    // PassiveHP2:    p0=r0, p1=c1, p2=r1, p3=c2, p4=r2, unityNorm=unityHf
    // PassiveLP2:    p0=r1, p1=c1, p2=r2, p3=c2, p4=rload, unityNorm=unityDc
    // PassiveBP2:    p0=r0, p1=c1, p2=c2, p3=rOut, unityNorm=unityPeak
    double p0 = 1000.0;
    double p1 = 0.70710678;
    double p2 = 0.0;
    double p3 = 0.0;
    double p4 = 0.0;
    bool unityNorm = true;

    static constexpr FilterSectionSpec makeRbj(FilterType t,
                                               double freqHz,
                                               double q = 0.70710678,
                                               double gainDb = 0.0) noexcept
    {
        return FilterSectionSpec{t, freqHz, q, gainDb, 0.0, 0.0, true};
    }

    static constexpr FilterSectionSpec makePassiveHp2(double r0,
                                                      double c1,
                                                      double r1,
                                                      double c2,
                                                      double r2,
                                                      bool unityHf = true) noexcept
    {
        return FilterSectionSpec{FilterType::PassiveHP2, r0, c1, r1, c2, r2, unityHf};
    }

    static constexpr FilterSectionSpec makePassiveLp2(double r1,
                                                      double c1,
                                                      double r2,
                                                      double c2,
                                                      double rload,
                                                      bool unityDc = true) noexcept
    {
        return FilterSectionSpec{FilterType::PassiveLP2, r1, c1, r2, c2, rload, unityDc};
    }

    static constexpr FilterSectionSpec makePassiveBp2(double r0,
                                                      double c1,
                                                      double c2,
                                                      double rOut,
                                                      bool unityPeak = true) noexcept
    {
        return FilterSectionSpec{FilterType::PassiveBP2, r0, c1, c2, rOut, 0.0, unityPeak};
    }
};

} // namespace dx701::dsp
