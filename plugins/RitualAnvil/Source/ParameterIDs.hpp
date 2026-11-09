#pragma once

#include <array>
#include <cstddef>

// Parameter IDs: must match .ideas/parameter-spec.md and Source/ui/public/js/index.js
namespace ParameterIDs
{
    static constexpr const char* mixReady         = "mix_ready";
    static constexpr const char* inputGain        = "input_gain";
    static constexpr const char* hpf              = "hpf";
    static constexpr const char* transientAttack  = "transient_attack";
    static constexpr const char* transientSustain = "transient_sustain";
    static constexpr const char* eqKick           = "eq_kick";
    static constexpr const char* eqBox            = "eq_box";
    static constexpr const char* eqSnare          = "eq_snare";
    static constexpr const char* eqPresence       = "eq_presence";
    static constexpr const char* eqAir            = "eq_air";
    static constexpr const char* punch            = "punch";
    static constexpr const char* heat             = "heat";
    static constexpr const char* width            = "width";
    static constexpr const char* ceiling          = "ceiling";
    static constexpr const char* bypass           = "bypass";
}

namespace anvil
{
    /** Index of every continuous (slider) parameter. Order is shared by
        AnvilParams, the APVTS layout, and the editor's "eff" payload. */
    enum P : int
    {
        MixReady = 0,
        InputGain,
        Hpf,
        TransientAttack,
        TransientSustain,
        EqKick,
        EqBox,
        EqSnare,
        EqPresence,
        EqAir,
        Punch,
        Heat,
        Width,
        Ceiling,
        NumSliderParams
    };

    struct SliderSpec
    {
        const char* id;
        const char* name;
        const char* label;
        float min, max, def;
    };

    inline constexpr std::array<SliderSpec, NumSliderParams> sliderSpecs {{
        { ParameterIDs::mixReady,         "Mix Ready", "%",    0.0f,   100.0f,  0.0f },
        { ParameterIDs::inputGain,        "Input",     "dB",  -12.0f,   12.0f,  0.0f },
        { ParameterIDs::hpf,              "HPF",       "Hz",   20.0f,  100.0f, 40.0f },
        { ParameterIDs::transientAttack,  "Attack",    "dB",  -12.0f,   12.0f,  0.0f },
        { ParameterIDs::transientSustain, "Sustain",   "dB",  -12.0f,    6.0f,  0.0f },
        { ParameterIDs::eqKick,           "Kick",      "dB",  -12.0f,   12.0f,  0.0f },
        { ParameterIDs::eqBox,            "Box",       "dB",  -12.0f,   12.0f,  0.0f },
        { ParameterIDs::eqSnare,          "Snare",     "dB",  -12.0f,   12.0f,  0.0f },
        { ParameterIDs::eqPresence,       "Presence",  "dB",  -12.0f,   12.0f,  0.0f },
        { ParameterIDs::eqAir,            "Air",       "dB",  -12.0f,   12.0f,  0.0f },
        { ParameterIDs::punch,            "Punch",     "%",     0.0f,  100.0f,  0.0f },
        { ParameterIDs::heat,             "Heat",      "dB",    0.0f,   20.0f,  0.0f },
        { ParameterIDs::width,            "Width",     "%",    50.0f,  150.0f, 100.0f },
        { ParameterIDs::ceiling,          "Ceiling",   "dBTP", -1.0f,   -0.1f, -0.3f },
    }};

    /** One value per slider parameter, in P order (parameter units). */
    using AnvilParams = std::array<float, NumSliderParams>;

    inline AnvilParams defaultParams() noexcept
    {
        AnvilParams p {};
        for (std::size_t i = 0; i < p.size(); ++i)
            p[i] = sliderSpecs[i].def;
        return p;
    }
}

// Fixed internal values (parameter-spec.md "Fixed Internal Values")
namespace anvil::fixed
{
    static constexpr float transientSplitLow  = 300.0f;
    static constexpr float transientSplitHigh = 20000.0f;

    static constexpr float heatSplitLow  = 200.0f;
    static constexpr float heatSplitHigh = 5000.0f;

    static constexpr float punchThresholdDb = -20.0f;
    static constexpr float punchRatio       = 8.0f;
    static constexpr float punchAttackMs    = 3.0f;
    static constexpr float punchReleaseMs   = 150.0f;
    static constexpr float punchMaxBlend    = 0.5f;

    static constexpr float widthSideSplitHz = 200.0f;
    static constexpr float limiterReleaseMs = 150.0f;

    static constexpr float eqFreq[5] = { 60.0f, 200.0f, 3000.0f, 8000.0f, 12000.0f };
    static constexpr float eqQ[5]    = { 1.0f, 1.0f, 1.0f, 0.707f, 0.5f };
}
