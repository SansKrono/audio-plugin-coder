#pragma once

#include <cmath>

namespace anvil
{
    /** RBJ biquad coefficients, normalised (a0 == 1). Computed in double, no allocation. */
    struct BiquadCoeffs
    {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;

        static BiquadCoeffs peak (double fs, double f0, double q, double gainDb) noexcept
        {
            const double A  = std::pow (10.0, gainDb / 40.0);
            const double w  = 2.0 * pi * clampFreq (fs, f0) / fs;
            const double al = std::sin (w) / (2.0 * q);
            const double c  = std::cos (w);
            const double a0 = 1.0 + al / A;
            return { (1.0 + al * A) / a0, -2.0 * c / a0, (1.0 - al * A) / a0, -2.0 * c / a0, (1.0 - al / A) / a0 };
        }

        static BiquadCoeffs highPass (double fs, double f0, double q) noexcept
        {
            const double w  = 2.0 * pi * clampFreq (fs, f0) / fs;
            const double al = std::sin (w) / (2.0 * q);
            const double c  = std::cos (w);
            const double a0 = 1.0 + al;
            return { (1.0 + c) * 0.5 / a0, -(1.0 + c) / a0, (1.0 + c) * 0.5 / a0, -2.0 * c / a0, (1.0 - al) / a0 };
        }

        static double clampFreq (double fs, double f) noexcept { return std::fmin (std::fmax (f, 1.0), fs * 0.49); }
        static constexpr double pi = 3.14159265358979323846;
    };

    /** Transposed direct form II biquad, up to 2 channels, double state. */
    class Biquad
    {
    public:
        void setCoeffs (const BiquadCoeffs& c) noexcept { k = c; }
        void reset() noexcept { for (int ch = 0; ch < 2; ++ch) z1[ch] = z2[ch] = 0.0; }

        float process (int ch, float xf) noexcept
        {
            const double x = xf;
            const double y = k.b0 * x + z1[ch];
            z1[ch] = k.b1 * x - k.a1 * y + z2[ch];
            z2[ch] = k.b2 * x - k.a2 * y;
            return (float) y;
        }

    private:
        BiquadCoeffs k;
        double z1[2] {}, z2[2] {};
    };

    /** One-pole high-pass (TPT), up to 2 channels. lowOut = x - highOut sums back exactly. */
    class OnePoleHP
    {
    public:
        void setCutoff (double fs, double f) noexcept
        {
            const double g = std::tan (BiquadCoeffs::pi * BiquadCoeffs::clampFreq (fs, f) / fs);
            G = g / (1.0 + g);
        }
        void reset() noexcept { s[0] = s[1] = 0.0; }

        float process (int ch, float x) noexcept
        {
            const double v  = (x - s[ch]) * G;
            const double lp = v + s[ch];
            s[ch] = lp + v;
            return (float) (x - lp);
        }

    private:
        double G = 0.0, s[2] {};
    };
}
