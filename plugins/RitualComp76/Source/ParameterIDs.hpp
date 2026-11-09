#pragma once

// Parameter IDs: must match .ideas/parameter-spec.md and Source/ui/public/js/index.js
namespace ParameterIDs
{
    static constexpr const char* input     = "input";
    static constexpr const char* output    = "output";
    static constexpr const char* attack    = "attack";
    static constexpr const char* release   = "release";
    static constexpr const char* ratio     = "ratio";
    static constexpr const char* meterMode = "meter_mode";
    static constexpr const char* bypass    = "bypass";
}

namespace ParameterRanges
{
    // Input/Output attenuators: values below muteBelowDb are treated as -inf (mute, shown as "∞").
    static constexpr float attenMinDb   = -49.0f;
    static constexpr float attenMaxDb   =   0.0f;
    static constexpr float muteBelowDb  = -48.5f;
    static constexpr float attenDefault = -24.0f;

    static constexpr float stepMin = 1.0f;
    static constexpr float stepMax = 7.0f;
    static constexpr float attackDefault  = 3.0f;
    static constexpr float releaseDefault = 5.0f;
}

// Ratio choice indices (order matters: shared with the WebView UI)
enum class RatioMode : int { Out = 0, R4, R8, R12, R20, All };

// Meter mode choice indices
enum class MeterMode : int { GR = 0, Plus8, Plus4, Off };
