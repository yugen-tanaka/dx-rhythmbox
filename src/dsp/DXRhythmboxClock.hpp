// Pure C++20 DAW-syncable clock & transport telemetry types for DX701 Rhythmbox.
#pragma once

#include "DXRhythmboxParams.hpp"

namespace dx701::dsp
{

enum class ClockMode : int
{
    Internal = 0,  // Local TEMPO slider + local RHYTHM / BREAK
    TempoSync = 1, // Follows DAW BPM (when available), local RHYTHM / BREAK
    HostSync = 2   // Follows DAW Play/Stop + BPM + PPQ (falls back to Internal if no host transport)
};

struct HostTransportInfo
{
    bool hasTransport = false;
    bool isPlaying = false;
    bool hasBpm = false;
    double bpm = kDefaultBpm;
    bool hasPpqPosition = false;
    double ppqPosition = 0.0;
};

struct TempoLampState
{
    bool isRunning = false;
    bool beatFlash = false;
    bool isDownbeat = false;
    bool usingHostBpm = false;
    double beatPhase = 0.0;
    double effectiveBpm = kDefaultBpm;
    int currentStep = 0;
    int totalSteps = 64;
    bool fillInActive = false;
    int activeFillInIndex = -1;
};

} // namespace dx701::dsp
