#pragma once

#include "ParameterIDs.hpp"
#include <algorithm>
#include <cmath>

namespace anvil
{
    /** Mix Ready macro (architecture.md, "Mix Ready — Additive Law").

        Pure function: manual parameter values in, effective values out.
        effective = clamp (manual + offset (t), paramMin, paramMax), t = mix_ready / 100.
        Exceptions: HPF uses a rising floor; Punch offsets the wet blend (0..0.5),
        expressed back in Punch % (blend 0.5 == 100 %).
        Mirrored in Source/ui/public/js/mock-backend.js for the browser preview. */
    inline AnvilParams applyMacro (const AnvilParams& manual) noexcept
    {
        const auto clampTo = [] (int idx, float v)
        {
            return std::clamp (v, sliderSpecs[(size_t) idx].min, sliderSpecs[(size_t) idx].max);
        };

        const float t = std::clamp (manual[MixReady] / 100.0f, 0.0f, 1.0f);
        AnvilParams e = manual;

        // HPF floor rises 20 → 50 Hz (no jump at t = 0+)
        e[Hpf] = clampTo (Hpf, std::max (manual[Hpf], 20.0f + 30.0f * t));

        // Transients: attack lift above 50 %. Sustain is not touched by the macro: the POC's
        // sustain dip below 50 % made Mix Ready ~1.5 dB quieter around 25 % (removed in the 4.1.3 review, from the measured loudness curve).
        e[TransientAttack] = clampTo (TransientAttack, manual[TransientAttack] + (t > 0.5f ? (t - 0.5f) * 8.0f : 0.0f));

        // Tone EQ
        e[EqKick]     = clampTo (EqKick,     manual[EqKick]     + 4.0f * t);
        e[EqBox]      = clampTo (EqBox,      manual[EqBox]      - 3.0f * t);
        e[EqSnare]    = clampTo (EqSnare,    manual[EqSnare]    + 3.0f * t);
        e[EqPresence] = clampTo (EqPresence, manual[EqPresence] + 2.0f * t);
        e[EqAir]      = clampTo (EqAir,      manual[EqAir]      + 4.0f * t);

        // Punch: blend offset engages above 40 %, total blend <= 0.5
        const float manualBlend = manual[Punch] / 100.0f * fixed::punchMaxBlend;
        const float macroBlend  = std::clamp ((t - 0.4f) * 0.5f / 0.6f, 0.0f, fixed::punchMaxBlend);
        const float blend       = std::clamp (manualBlend + macroBlend, 0.0f, fixed::punchMaxBlend);
        e[Punch] = blend / fixed::punchMaxBlend * 100.0f;

        // Heat: drive offset above 60 %
        e[Heat] = clampTo (Heat, manual[Heat] + (t > 0.6f ? (t - 0.6f) * 15.0f : 0.0f));

        // Width: +20 % above 50 %
        e[Width] = clampTo (Width, manual[Width] + std::clamp ((t - 0.5f) * 2.0f, 0.0f, 1.0f) * 20.0f);

        return e;
    }
}
