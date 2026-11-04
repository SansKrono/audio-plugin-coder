/*  VaporStrings — offline pitch-tracker harness (phase 2.1.2)

    Built only on request (target is EXCLUDE_FROM_ALL):
        cmake --build build --config Release --target vaporstrings_TrackerTest

    Usage:
        vaporstrings_TrackerTest                 synthetic suite (Karplus-Strong plucks, chord, noise)
        vaporstrings_TrackerTest di.wav out.csv  log time,f0,confidence,voiced,onset for a real DI file

    Exit code 0 = all synthetic checks passed.                                   */

#include "../Source/dsp/Analysis.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <random>
#include <string>
#include <vector>

namespace
{
constexpr double kSr = 48000.0;

/** Karplus-Strong pluck: bright attack, decaying harmonics — close to a DI string.
    The feedback averages the sample written N steps ago with the one after it,
    so y[i+N] = ½(y[i] + y[i+1]) — an effective loop of N − 0.5 samples. The
    string therefore sounds at sr / (N − 0.5); actualHz receives that value. */
std::vector<float> pluck (double hz, double seconds, double amp, unsigned seed, double* actualHz = nullptr)
{
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> uni (-1.0f, 1.0f);
    const int period = std::max (2, (int) std::lround (kSr / hz + 0.5));
    if (actualHz != nullptr) *actualHz = kSr / (period - 0.5);
    std::vector<float> line ((size_t) period);
    for (auto& s : line) s = uni (rng);
    std::vector<float> out ((size_t) (seconds * kSr));
    size_t p = 0;
    for (size_t i = 0; i < out.size(); ++i)
    {
        const size_t n = (p + 1) % line.size();
        out[i] = (float) amp * line[p];
        line[p] = 0.996f * 0.5f * (line[p] + line[n]);
        p = n;
    }
    return out;
}

struct Result { int frames = 0, voiced = 0, inTune = 0, octaveErr = 0; double firstLockMs = -1; std::vector<float> hz; };

Result run (const std::vector<float>& sig, double expectHz, float speed = 0.6f, float sens = 0.5f,
            vs::PitchTracker::Priority prio = vs::PitchTracker::Priority::Last)
{
    vs::PitchTracker t;
    t.prepare (kSr);
    t.setResponse (speed, sens, prio);
    Result r;
    const int hop = 64 * 4;   // one analysis hop at 48 kHz (÷4)
    for (size_t i = 0; i < sig.size(); ++i)
    {
        t.push (sig[i], true);
        if (i % hop == 0 && i > (size_t) (0.06 * kSr))   // judge after a 60 ms settle
        {
            ++r.frames;
            if (! t.isVoiced()) continue;
            ++r.voiced;
            r.hz.push_back (t.getHz());
            const double cents = 1200.0 * std::log2 (t.getHz() / expectHz);
            if (std::abs (cents) < 20.0) ++r.inTune;
            if (std::abs (std::abs (cents) - 1200.0) < 50.0) ++r.octaveErr;
        }
        if (r.firstLockMs < 0 && t.isVoiced() && t.getHz() > 0
            && std::abs (1200.0 * std::log2 (t.getHz() / expectHz)) < 20.0)
            r.firstLockMs = 1000.0 * (double) i / kSr;
    }
    return r;
}

bool readWav (const char* path, std::vector<float>& mono, double& sr)
{
    std::ifstream f (path, std::ios::binary);
    if (! f) return false;
    std::vector<char> d ((std::istreambuf_iterator<char> (f)), {});
    if (d.size() < 44 || std::memcmp (d.data(), "RIFF", 4) || std::memcmp (d.data() + 8, "WAVE", 4)) return false;
    size_t pos = 12; int fmt = 0, ch = 0, bits = 0; uint32_t rate = 0;
    while (pos + 8 <= d.size())
    {
        uint32_t len; std::memcpy (&len, d.data() + pos + 4, 4);
        const char* body = d.data() + pos + 8;
        if (! std::memcmp (d.data() + pos, "fmt ", 4))
        {
            uint16_t f16, c16, b16; std::memcpy (&f16, body, 2); std::memcpy (&c16, body + 2, 2);
            std::memcpy (&rate, body + 4, 4); std::memcpy (&b16, body + 14, 2);
            fmt = f16; ch = c16; bits = b16;
            if (fmt == 0xFFFE && len >= 26) { uint16_t sub; std::memcpy (&sub, body + 24, 2); fmt = sub; }
        }
        else if (! std::memcmp (d.data() + pos, "data", 4) && ch > 0)
        {
            const int bps = bits / 8;
            const size_t frames = std::min<size_t> (len, d.size() - pos - 8) / (size_t) (bps * ch);
            mono.resize (frames);
            for (size_t i = 0; i < frames; ++i)
            {
                double acc = 0;
                for (int c = 0; c < ch; ++c)
                {
                    const char* s = body + (i * ch + c) * bps;
                    if (fmt == 3 && bits == 32) { float v; std::memcpy (&v, s, 4); acc += v; }
                    else if (bits == 16) { int16_t v; std::memcpy (&v, s, 2); acc += v / 32768.0; }
                    else if (bits == 24) { int32_t v = (uint8_t) s[0] | ((uint8_t) s[1] << 8) | ((int8_t) s[2] << 16); acc += v / 8388608.0; }
                    else if (bits == 32) { int32_t v; std::memcpy (&v, s, 4); acc += v / 2147483648.0; }
                }
                mono[i] = (float) (acc / ch);
            }
            sr = rate;
            return true;
        }
        pos += 8 + len + (len & 1);
    }
    return false;
}
} // namespace

int main (int argc, char** argv)
{
    if (argc >= 3)
    {
        std::vector<float> x; double sr = 0;
        if (! readWav (argv[1], x, sr)) { std::fprintf (stderr, "cannot read %s (PCM 16/24/32 or float32 WAV)\n", argv[1]); return 2; }
        vs::PitchTracker t; vs::OnsetDetector o;
        t.prepare (sr); o.prepare (sr);
        t.setResponse (0.6f, 0.5f, vs::PitchTracker::Priority::Last);
        o.setSensitivity (0.5f, 0.5f);
        std::ofstream csv (argv[2]);
        csv << "time_s,f0_hz,confidence,voiced,onset,velocity\n";
        const int every = (int) (sr / 200.0);
        bool onsetFlag = false;
        for (size_t i = 0; i < x.size(); ++i)
        {
            t.push (x[i], true);
            if (o.push (x[i], 0.003f)) onsetFlag = true;
            if (i % (size_t) every == 0)
            {
                csv << (double) i / sr << ',' << t.getHz() << ',' << t.getConfidence() << ','
                    << (t.isVoiced() ? 1 : 0) << ',' << (onsetFlag ? 1 : 0) << ',' << o.getVelocity() << '\n';
                onsetFlag = false;
            }
        }
        std::printf ("wrote %s (%zu samples @ %.0f Hz, %u onsets)\n", argv[2], x.size(), sr, o.getOnsetCount());
        return 0;
    }

    struct Case { const char* name; double hz; };
    const Case notes[] = {
        { "E2 (open low E)", 82.41 }, { "A2", 110.0 }, { "D3", 146.83 }, { "G3", 196.0 },
        { "B3", 246.94 }, { "E4 (open high E)", 329.63 }, { "A4", 440.0 }, { "E5 (12th fret)", 659.26 },
        { "C6", 1046.5 }, { "Drop D (D2)", 73.42 } };

    int failures = 0;
    std::printf ("%-18s %8s %8s %8s %8s %10s\n", "case", "voiced", "inTune", "octErr", "lock_ms", "verdict");
    unsigned seed = 1;
    for (const auto& c : notes)
    {
        double actual = c.hz;
        const auto sig = pluck (c.hz, 1.5, 0.5, seed++, &actual);
        const auto r = run (sig, actual);
        const double v = r.frames ? 100.0 * r.voiced / r.frames : 0;
        const double t = r.voiced ? 100.0 * r.inTune / r.voiced : 0;
        const double o = r.voiced ? 100.0 * r.octaveErr / r.voiced : 0;
        const bool ok = v > 80 && t > 95 && o < 2 && r.firstLockMs >= 0 && r.firstLockMs < 80;
        failures += ok ? 0 : 1;
        auto hzs = r.hz;
        float med = 0.0f;
        if (! hzs.empty()) { std::nth_element (hzs.begin(), hzs.begin() + hzs.size() / 2, hzs.end()); med = hzs[hzs.size() / 2]; }
        std::printf ("%-18s %7.1f%% %7.1f%% %7.1f%% %8.1f %10s   (expect %.1f Hz, median %.1f Hz)\n",
                     c.name, v, t, o, r.firstLockMs, ok ? "PASS" : "FAIL", actual, med);
    }

    // chord (E major open) — the mono tracker should report LOW confidence most of the time
    {
        std::vector<float> chord ((size_t) (1.5 * kSr), 0.0f);
        const double ch[] = { 82.41, 123.47, 164.81, 207.65, 246.94, 329.63 };
        for (int k = 0; k < 6; ++k) { auto s = pluck (ch[k], 1.5, 0.18, 100 + k); for (size_t i = 0; i < chord.size(); ++i) chord[i] += s[i]; }
        vs::PitchTracker t; t.prepare (kSr); t.setResponse (0.6f, 0.5f, vs::PitchTracker::Priority::Last);
        int frames = 0, lowConf = 0;
        for (size_t i = 0; i < chord.size(); ++i)
        {
            t.push (chord[i], true);
            if (i % 256 == 0 && i > 4800) { ++frames; if (t.getConfidence() < 0.85f || ! t.isVoiced()) ++lowConf; }
        }
        std::printf ("%-18s lowConf %.1f%% (informational: chord warning relies on this)\n", "E major chord", 100.0 * lowConf / std::max (1, frames));
    }

    // noise / silence must stay unvoiced
    {
        std::mt19937 rng (7); std::normal_distribution<float> n (0.0f, 0.002f);
        vs::PitchTracker t; t.prepare (kSr); t.setResponse (0.6f, 0.5f, vs::PitchTracker::Priority::Last);
        int voicedFrames = 0, frames = 0;
        for (int i = 0; i < (int) kSr; ++i) { t.push (n (rng), true); if (i % 256 == 0) { ++frames; voicedFrames += t.isVoiced(); } }
        const bool ok = voicedFrames < frames / 20;
        failures += ok ? 0 : 1;
        std::printf ("%-18s voiced %d/%d frames %10s\n", "noise floor", voicedFrames, frames, ok ? "PASS" : "FAIL");
    }

    // onset detector: 4 picks 400 ms apart (each ends with a 60 ms fret-hand mute),
    // decreasing level → exactly 4 onsets with falling velocity. Run on low and high strings.
    for (double hz : { 82.41, 196.0, 659.26 })
    {
        vs::OnsetDetector od; od.prepare (kSr); od.setSensitivity (0.5f, 1.0f);
        std::vector<float> sig ((size_t) (2.0 * kSr), 0.0f);
        const double amps[] = { 0.8, 0.4, 0.2, 0.1 };
        for (int k = 0; k < 4; ++k) { auto s = pluck (hz, 0.34, amps[k], 50 + k); std::copy (s.begin(), s.end(), sig.begin() + (long) (k * 0.4 * kSr)); }
        std::vector<float> vel;
        for (float s : sig) if (od.push (s, 0.003f)) vel.push_back (od.getVelocity());
        bool ok = vel.size() == 4;
        for (size_t k = 1; ok && k < vel.size(); ++k) ok = vel[k] < vel[k - 1];
        failures += ok ? 0 : 1;
        std::printf ("onset @ %-8.1f %zu onsets, vel:", hz, vel.size());
        for (float v : vel) std::printf (" %.2f", v);
        std::printf (" %10s\n", ok ? "PASS" : "FAIL");
    }

    std::printf ("\n%s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
