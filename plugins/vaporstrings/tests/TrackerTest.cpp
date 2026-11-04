/*  VaporStrings — offline pitch-tracker harness (phase 2.1.2)

    Built only on request (target is EXCLUDE_FROM_ALL):
        cmake --build build --config Release --target vaporstrings_TrackerTest

    Usage:
        vaporstrings_TrackerTest                 synthetic suite (Karplus-Strong plucks, chord, noise)
        vaporstrings_TrackerTest di.wav out.csv  log time,f0,confidence,voiced,onset for a real DI file

    Exit code 0 = all synthetic checks passed.                                   */

#include "../Source/dsp/Analysis.h"
#include "../Source/dsp/Synth.h"
#include "../Source/dsp/Shifter.h"
#include "../Source/dsp/Envelope.h"
#include "../Source/dsp/FxChain1.h"
#include "../Source/dsp/FxChain2.h"
#include "../Source/dsp/Modulation.h"

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

    /* ---------------------------------------------------------------- synth source [C] */
    std::printf ("\n-- synth source --\n");
    using Wave = vs::SynthSource::Wave;
    auto renderSynth = [] (vs::SynthSource::Params p, float hz, double seconds, std::vector<float>& L, std::vector<float>& R)
    {
        vs::SynthSource s; s.prepare (kSr); s.setParams (p);
        L.assign ((size_t) (seconds * kSr), 0.0f); R = L;
        for (size_t i = 0; i < L.size(); ++i) s.tick (hz, 1.0f, L[i], R[i]);
    };
    auto check = [&failures] (const char* name, bool ok, const char* detail)
    {
        failures += ok ? 0 : 1;
        std::printf ("%-28s %-40s %s\n", name, detail, ok ? "PASS" : "FAIL");
    };
    char buf[128];

    // pitch per waveform: render at 220 Hz, read it back with the tracker
    const struct { Wave w; float pw; const char* name; } waves[] = {
        { Wave::Saw, 0.5f, "saw @220" }, { Wave::Square, 0.5f, "square @220" }, { Wave::Pulse, 0.25f, "pulse 25% @220" },
        { Wave::Triangle, 0.5f, "triangle @220" }, { Wave::Sine, 0.5f, "sine @220" } };
    for (const auto& wv : waves)
    {
        vs::SynthSource::Params p; p.wave1 = wv.w; p.oscMix = 0.0f; p.pulseWidth = wv.pw; p.unison = 1;
        std::vector<float> L, R; renderSynth (p, 220.0f, 1.0, L, R);
        const auto r = run (L, 220.0);
        double mean = 0; for (float v : L) mean += v; mean /= (double) L.size();
        std::snprintf (buf, sizeof buf, "inTune %d/%d, DC %+.4f", r.inTune, r.voiced, mean);
        check (wv.name, r.voiced > 0 && r.inTune == r.voiced && std::abs (mean) < 0.02, buf);
    }

    // octave switch: osc1 +1 oct → 440
    {
        vs::SynthSource::Params p; p.oct1 = 1; p.oscMix = 0.0f;
        std::vector<float> L, R; renderSynth (p, 220.0f, 1.0, L, R);
        const auto r = run (L, 440.0);
        std::snprintf (buf, sizeof buf, "inTune %d/%d", r.inTune, r.voiced);
        check ("osc1 +1 oct → 440 Hz", r.voiced > 0 && r.inTune == r.voiced, buf);
    }

    // headroom: 7-voice saw unison on both oscs + full sub must not exceed 1.0
    {
        vs::SynthSource::Params p; p.wave1 = p.wave2 = Wave::Saw; p.unison = 7; p.spread = 1.0f; p.subLevel = 1.0f; p.oscMix = 0.5f;
        std::vector<float> L, R; renderSynth (p, 82.41f, 2.0, L, R);
        float pk = 0; bool finite = true;
        for (size_t i = 0; i < L.size(); ++i) { pk = std::max ({ pk, std::abs (L[i]), std::abs (R[i]) }); finite &= std::isfinite (L[i]) && std::isfinite (R[i]); }
        std::snprintf (buf, sizeof buf, "peak %.3f", pk);
        check ("unison 7 + sub headroom", finite && pk < 1.0f, buf);
    }

    // stereo: unison 1 is mono, unison 5 + spread is wide
    {
        auto corr = [] (const std::vector<float>& a, const std::vector<float>& b)
        {
            double ab = 0, aa = 0, bb = 0;
            for (size_t i = 0; i < a.size(); ++i) { ab += a[i] * b[i]; aa += a[i] * a[i]; bb += b[i] * b[i]; }
            return ab / std::sqrt (aa * bb + 1e-12);
        };
        vs::SynthSource::Params p; p.unison = 1;
        std::vector<float> L, R; renderSynth (p, 220.0f, 1.0, L, R);
        const double c1 = corr (L, R);
        p.unison = 5; p.spread = 1.0f; renderSynth (p, 220.0f, 1.0, L, R);
        const double c5 = corr (L, R);
        std::snprintf (buf, sizeof buf, "corr mono %.3f, wide %.3f", c1, c5);
        check ("unison stereo image", c1 > 0.999 && c5 < 0.9, buf);
    }

    // glide: 110 → 220 Hz with 100 ms glide: mid-way after 30 ms, arrived by 600 ms
    {
        vs::SynthSource s; s.prepare (kSr);
        vs::SynthSource::Params p; p.glideMs = 100.0f; s.setParams (p);
        float l, r;
        for (int i = 0; i < 4800; ++i) s.tick (110.0f, 1.0f, l, r);
        for (int i = 0; i < (int) (0.03 * kSr); ++i) s.tick (220.0f, 1.0f, l, r);
        const float at30 = s.getCurrentHz();
        for (int i = 0; i < (int) (0.57 * kSr); ++i) s.tick (220.0f, 1.0f, l, r);
        const float at600 = s.getCurrentHz();
        std::snprintf (buf, sizeof buf, "30 ms: %.1f Hz, 600 ms: %.2f Hz", at30, at600);
        check ("glide 100 ms", at30 > 115.0f && at30 < 210.0f && std::abs (1200.0 * std::log2 (at600 / 220.0)) < 5.0, buf);
    }

    /* ---------------------------------------------------------------- octave layer + square-up */
    std::printf ("\n-- octave layer / square-up --\n");
    auto rms = [] (const std::vector<float>& v, size_t from = 0)
    {
        double s = 0; for (size_t i = from; i < v.size(); ++i) s += (double) v[i] * v[i];
        return std::sqrt (s / (double) std::max<size_t> (1, v.size() - from));
    };
    {
        double g3 = 196.0;
        const auto src = pluck (196.0, 1.5, 0.4, 77, &g3);
        const struct { float ratio; const char* name; } shifts[] = {
            { 0.5f, "shift -12 st (G3→G2)" }, { 2.0f, "shift +12 st (G3→G4)" }, { std::exp2 (7.0f / 12.0f), "shift +7 st (G3→D4)" } };
        for (const auto& s : shifts)
        {
            vs::PitchShifter ps; ps.prepare (kSr, 40.0f); ps.setRatio (s.ratio);
            std::vector<float> out (src.size());
            for (size_t i = 0; i < src.size(); ++i) out[i] = ps.process (src[i]);
            const auto r = run (out, g3 * s.ratio);
            const double lvl = 20.0 * std::log10 (rms (out, 4800) / rms (src, 4800));
            std::snprintf (buf, sizeof buf, "inTune %d/%d, octErr %d, level %+.1f dB", r.inTune, r.voiced, r.octaveErr, lvl);
            check (s.name, r.voiced > 0 && r.inTune >= r.voiced * 0.8 && r.octaveErr * 20 < r.voiced && std::abs (lvl) < 4.0, buf);
        }
    }

    // oversampler round trip must be transparent (after its fixed latency)
    {
        vs::Oversampler2x os;
        const int L = vs::Oversampler2x::getLatency();
        double err = 0, ref = 0;
        std::vector<float> in (48000);
        for (size_t i = 0; i < in.size(); ++i) in[i] = 0.5f * (float) std::sin (2.0 * 3.14159265 * 1000.0 * (double) i / kSr);
        for (size_t i = 0; i < in.size(); ++i)
        {
            float a, b; os.up (in[i], a, b);
            const float y = os.down (a, b);
            if (i > 1000) { const float d = y - in[i - (size_t) L]; err += d * d; ref += in[i] * in[i]; }
        }
        const double nullDb = 10.0 * std::log10 (err / ref + 1e-20);
        std::snprintf (buf, sizeof buf, "latency %d smp, null %.1f dB", L, nullDb);
        check ("2x oversampler round trip", nullDb < -60.0, buf);
    }

    // square-up at 0 is linear (only the oversampler latency)
    {
        // KS starts as white noise up to Nyquist; a DI has ~nothing above 16 kHz, and the
        // half-band filter (correctly) removes that band — so band-limit the probe first.
        double g = 196.0; auto src = pluck (196.0, 1.0, 0.4, 78, &g);
        { vs::Biquad a, b; a.setLowPass (kSr, 16000.0, 0.5412); b.setLowPass (kSr, 16000.0, 1.3066);
          for (auto& v : src) v = b.process (a.process (v)); }
        vs::SquareUp sq; sq.reset(); sq.setAmount (0.0f);
        const int L = vs::SquareUp::getLatency();
        double err = 0, ref = 0;
        for (size_t i = 0; i < src.size(); ++i)
        {
            const float y = sq.process (src[i]);
            if (i > (size_t) L + 100) { const float d = y - src[i - (size_t) L]; err += d * d; ref += src[i] * src[i]; }
        }
        const double nullDb = 10.0 * std::log10 (err / ref + 1e-20);
        std::snprintf (buf, sizeof buf, "null vs delayed input %.1f dB", nullDb);
        check ("square-up 0% transparent", nullDb < -40.0, buf);
    }

    // square-up at 100%: pitch kept, peak bounded, crest factor collapses toward a square
    {
        double g = 196.0; const auto src = pluck (196.0, 1.5, 0.4, 79, &g);
        vs::SquareUp sq; sq.reset(); sq.setAmount (1.0f);
        std::vector<float> out (src.size());
        float pk = 0; bool finite = true;
        for (size_t i = 0; i < src.size(); ++i) { out[i] = sq.process (src[i]); pk = std::max (pk, std::abs (out[i])); finite &= std::isfinite (out[i]); }
        float pkIn = 0; for (float v : src) pkIn = std::max (pkIn, std::abs (v));
        const double crestIn = pkIn / rms (src, 4800), crestOut = pk / rms (out, 4800);
        const auto r = run (out, g);
        std::snprintf (buf, sizeof buf, "inTune %d/%d, peak %.2f, crest %.1f→%.1f", r.inTune, r.voiced, pk, crestIn, crestOut);
        check ("square-up 100% on G3 pluck", finite && pk < 1.0f && r.inTune >= r.voiced * 0.95 && r.voiced > 0 && crestOut < crestIn, buf);
    }

    /* ---------------------------------------------------------------- voice envelope */
    std::printf ("\n-- voice envelope --\n");
    using EM = vs::VoiceEnvelope::Mode;
    auto msToN = [] (double ms) { return (int) (ms * 0.001 * kSr); };

    // ADSR: A=10 ms, D=200 ms, S=0.5, R=300 ms; gate closes at 500 ms
    {
        vs::VoiceEnvelope e; e.prepare (kSr); e.setParams (EM::ADSR, 10.0f, 200.0f, 0.5f, 300.0f);
        e.noteOn (1.0f, 0.3f);
        float at5 = 0, at10 = 0, at250 = 0, at499 = 0, at520 = 0, at800 = 0;
        for (int i = 1; i <= msToN (900); ++i)
        {
            const bool gate = i < msToN (500);
            e.tick (0.3f, gate);
            if (i == msToN (5))   at5 = e.level();
            if (i == msToN (10) + 2) at10 = e.level();
            if (i == msToN (250)) at250 = e.level();
            if (i == msToN (499)) at499 = e.level();
            if (i == msToN (520)) at520 = e.level();
            if (i == msToN (800)) at800 = e.level();
        }
        std::snprintf (buf, sizeof buf, "5ms %.2f 10ms %.2f 250ms %.2f 499ms %.2f 520ms %.2f 800ms %.3f", at5, at10, at250, at499, at520, at800);
        check ("ADSR stages + release", std::abs (at5 - 0.5f) < 0.05f && at10 > 0.95f && std::abs (at250 - 0.5f) < 0.03f
                                         && std::abs (at499 - 0.5f) < 0.01f && at520 < 0.5f && at520 > 0.1f && at800 < 0.001f, buf);
    }

    // velocity scales the level, contour (A/B gain) does not see velocity
    {
        vs::VoiceEnvelope e; e.prepare (kSr); e.setParams (EM::ADSR, 1.0f, 100.0f, 1.0f, 100.0f);
        e.noteOn (0.5f, 0.3f);
        for (int i = 0; i < msToN (50); ++i) e.tick (0.3f, true);
        std::snprintf (buf, sizeof buf, "level %.2f, contour %.2f", e.level(), e.shape());
        check ("velocity 0.5", std::abs (e.level() - 0.5f) < 0.02f && std::abs (e.shape() - 1.0f) < 0.02f, buf);
    }

    // Follow: tracks a decaying string relative to its pick peak; A/B contour stays 1
    {
        vs::VoiceEnvelope e; e.prepare (kSr); e.setParams (EM::Follow, 5.0f, 300.0f, 0.8f, 400.0f);
        e.noteOn (1.0f, 0.4f);
        float atStart = 0, atHalf = 0, contourMin = 1;
        for (int i = 0; i < msToN (1000); ++i)
        {
            const float f = 0.4f * std::exp (-(float) i / (float) msToN (500));   // string decays
            e.tick (f, true);
            if (i == 10) atStart = e.level();
            if (i == msToN (346)) atHalf = e.level();                              // e^-0.693 = 0.5
            contourMin = std::min (contourMin, e.shape());
        }
        std::snprintf (buf, sizeof buf, "start %.2f, half-life %.2f, contour min %.2f", atStart, atHalf, contourMin);
        check ("Follow normalised decay", atStart > 0.95f && std::abs (atHalf - 0.5f) < 0.05f && contourMin > 0.999f, buf);
    }

    // Swell: 400 ms bloom on a sustained note — ~50 % at 200 ms, full by 400 ms
    {
        vs::VoiceEnvelope e; e.prepare (kSr); e.setParams (EM::Swell, 400.0f, 300.0f, 0.8f, 400.0f);
        e.noteOn (1.0f, 0.3f);
        float at50 = 0, at200 = 0, at420 = 0;
        for (int i = 1; i <= msToN (420); ++i)
        {
            e.tick (0.3f, true);
            if (i == msToN (50))  at50 = e.level();
            if (i == msToN (200)) at200 = e.level();
            if (i == msToN (420)) at420 = e.level();
        }
        std::snprintf (buf, sizeof buf, "50ms %.2f 200ms %.2f 420ms %.2f, contour %.2f", at50, at200, at420, e.shape());
        check ("Swell bloom", at50 < 0.1f && std::abs (at200 - 0.5f) < 0.05f && at420 > 0.99f && e.shape() > 0.99f, buf);
    }

    // ADSR without a pick: A/B are silent until the first note (documented behaviour)
    {
        vs::VoiceEnvelope e; e.prepare (kSr); e.setParams (EM::ADSR, 5.0f, 100.0f, 0.8f, 100.0f);
        for (int i = 0; i < 1000; ++i) e.tick (0.3f, true);
        std::snprintf (buf, sizeof buf, "level %.2f, contour %.2f", e.level(), e.shape());
        check ("ADSR idle before first pick", e.level() == 0.0f && e.shape() == 0.0f, buf);
    }

    /* ---------------------------------------------------------------- FX chain I */
    std::printf ("\n-- FX chain I --\n");
    auto runFx = [] (auto& fx, bool on, std::vector<float> L, std::vector<float>& R)   // returns L, fills R
    {
        vs::Bypass bp; bp.prepare (kSr);
        R = L;
        vs::runInsert (bp, on, fx, L.data(), R.data(), (int) L.size());
        return L;
    };
    double g3 = 196.0;
    const auto g3src = pluck (196.0, 1.5, 0.4, 91, &g3);
    std::vector<float> sine440 ((size_t) (3.0 * kSr)), sine10k ((size_t) kSr);
    for (size_t i = 0; i < sine440.size(); ++i) sine440[i] = 0.4f * (float) std::sin (2.0 * 3.14159265 * 440.0 * (double) i / kSr);
    for (size_t i = 0; i < sine10k.size(); ++i) sine10k[i] = 0.4f * (float) std::sin (2.0 * 3.14159265 * 10000.0 * (double) i / kSr);

    // bypass: a disabled FX is bit-exact
    {
        vs::Drive d; d.prepare (kSr); d.setParams (vs::Drive::Mode::Fuzz, 1.0f, 0.5f);
        std::vector<float> R; const auto L = runFx (d, false, g3src, R);
        check ("bypass bit-exact", L == g3src && R == g3src, "Fuzz 100% switched off");
    }

    // slowdown
    for (float st : { -12.0f, 5.0f })
    {
        vs::Slowdown s; s.prepare (kSr); s.setParams (st, 0.0f);
        std::vector<float> R; const auto L = runFx (s, true, g3src, R);
        const auto r = run (L, g3 * std::exp2 (st / 12.0));
        std::snprintf (buf, sizeof buf, "inTune %d/%d, octErr %d", r.inTune, r.voiced, r.octaveErr);
        check (st < 0 ? "slowdown -12 st" : "slowdown +5 st", r.voiced > 0 && r.inTune >= r.voiced * 0.8 && r.octaveErr * 20 < r.voiced, buf);
    }

    // drive: every mode keeps pitch and stays bounded
    const char* driveNames[] = { "drive tape 50%", "drive tube 50%", "drive fuzz 50%", "drive bitcrush 50%" };
    for (int m = 0; m < 4; ++m)
    {
        vs::Drive d; d.prepare (kSr); d.setParams ((vs::Drive::Mode) m, 0.5f, 0.5f); d.snap();
        std::vector<float> R; const auto L = runFx (d, true, g3src, R);
        float pk = 0; bool fin = true;
        for (float v : L) { pk = std::max (pk, std::abs (v)); fin &= std::isfinite (v); }
        const auto r = run (L, g3);
        std::snprintf (buf, sizeof buf, "inTune %d/%d, peak %.2f", r.inTune, r.voiced, pk);
        check (driveNames[m], fin && pk < 1.2f && r.voiced > 0 && r.inTune >= r.voiced * 0.9, buf);
    }
    {
        vs::Drive d; d.prepare (kSr); d.setParams (vs::Drive::Mode::Bitcrush, 1.0f, 0.5f); d.snap();
        d.setParams (vs::Drive::Mode::Bitcrush, 1.0f, 0.5f);
        // measure the quantiser before the tilt EQ: tone 0.5 = flat, so tilt is unity except the one-pole split
        std::vector<float> in (4800); for (size_t i = 0; i < in.size(); ++i) in[i] = 0.9f * (float) std::sin (6.2831853 * 100.0 * (double) i / kSr);
        std::vector<float> R; const auto L = runFx (d, true, in, R);
        std::vector<float> q (L.begin() + 2400, L.end()); std::sort (q.begin(), q.end());
        const size_t distinct = (size_t) (std::unique (q.begin(), q.end(), [] (float a, float b) { return std::abs (a - b) < 1e-4f; }) - q.begin());
        std::snprintf (buf, sizeof buf, "%zu distinct levels (3-bit → ≤ 9)", distinct);
        check ("bitcrush 100% quantises", distinct <= 9, buf);
    }
    {
        auto hf = [] (const std::vector<float>& v) { double s = 0; for (size_t i = 1; i < v.size(); ++i) s += (v[i] - v[i - 1]) * (v[i] - v[i - 1]); return s; };
        vs::Drive dark, bright; dark.prepare (kSr); bright.prepare (kSr);
        dark.setParams (vs::Drive::Mode::Tape, 0.3f, 0.0f); dark.snap();
        bright.setParams (vs::Drive::Mode::Tape, 0.3f, 1.0f); bright.snap();
        std::vector<float> R1, R2; const auto a = runFx (dark, true, g3src, R1); const auto b = runFx (bright, true, g3src, R2);
        const double ratioDb = 10.0 * std::log10 (hf (b) / hf (a));
        std::snprintf (buf, sizeof buf, "bright vs dark HF %+.1f dB", ratioDb);
        check ("drive tone tilt", ratioDb > 6.0, buf);
    }

    // ensemble
    {
        vs::Ensemble e; e.prepare (kSr); e.setParams (vs::Ensemble::Mode::Ensemble, 0.6f, 0.5f, 1.0f); e.snap();
        std::vector<float> R; const auto L = runFx (e, true, g3src, R);
        double ab = 0, aa = 0, bb = 0;
        for (size_t i = 4800; i < L.size(); ++i) { ab += L[i] * R[i]; aa += L[i] * L[i]; bb += R[i] * R[i]; }
        const double c = ab / std::sqrt (aa * bb + 1e-12);
        const double lvl = 20.0 * std::log10 (rms (L, 4800) / rms (g3src, 4800));
        const auto r = run (L, g3);
        std::snprintf (buf, sizeof buf, "L/R corr %.2f, level %+.1f dB, inTune %d/%d", c, lvl, r.inTune, r.voiced);
        check ("ensemble width/pitch/level", c < 0.95 && std::abs (lvl) < 4.0 && r.voiced > 0 && r.inTune >= r.voiced * 0.9, buf);
    }

    // wobble: wow pitch modulation within tape-like bounds
    {
        vs::Wobble w; w.prepare (kSr); w.setParams (1.0f, 0.0f, 0.0f, 0.0f); w.snap();
        std::vector<float> R; const auto L = runFx (w, true, sine440, R);
        vs::PitchTracker t; t.prepare (kSr); t.setResponse (1.0f, 0.5f, vs::PitchTracker::Priority::Last);
        double sum = 0, sum2 = 0; int n = 0; double maxDev = 0;
        for (size_t i = 0; i < L.size(); ++i)
        {
            t.push (L[i], true);
            if (i % 256 == 0 && i > (size_t) kSr / 2 && t.isVoiced())
            { const double c = 1200.0 * std::log2 (t.getHz() / 440.0); sum += c; sum2 += c * c; ++n; maxDev = std::max (maxDev, std::abs (c)); }
        }
        const double sd = n ? std::sqrt (std::max (0.0, sum2 / n - (sum / n) * (sum / n))) : 0;
        std::snprintf (buf, sizeof buf, "pitch sd %.1f ct, max %.1f ct", sd, maxDev);
        check ("wobble wow 100%", sd > 3.0 && maxDev < 80.0, buf);
    }
    // wobble: age darkens highs
    {
        vs::Wobble a0, a1; a0.prepare (kSr); a1.prepare (kSr);
        a0.setParams (0, 0, 0.0f, 0); a0.snap(); a1.setParams (0, 0, 1.0f, 0); a1.snap();
        std::vector<float> R0, R1; const auto x0 = runFx (a0, true, sine10k, R0); const auto x1 = runFx (a1, true, sine10k, R1);
        const double att = 20.0 * std::log10 (rms (x1, 4800) / rms (x0, 4800));
        std::snprintf (buf, sizeof buf, "10 kHz %.1f dB at age 100%%", att);
        check ("wobble age LPF", att < -12.0, buf);
    }
    // wobble: dropouts dip the level
    {
        vs::Wobble w; w.prepare (kSr); w.setParams (0, 0, 0, 1.0f); w.snap();
        std::vector<float> longSine ((size_t) (8.0 * kSr));
        for (size_t i = 0; i < longSine.size(); ++i) longSine[i] = 0.4f * (float) std::sin (2.0 * 3.14159265 * 440.0 * (double) i / kSr);
        std::vector<float> R; const auto L = runFx (w, true, longSine, R);
        double minDb = 0; const int win = (int) (0.01 * kSr);
        for (size_t s = 4800; s + (size_t) win < L.size(); s += (size_t) win)
        {
            double e = 0; for (int k = 0; k < win; ++k) e += L[s + (size_t) k] * L[s + (size_t) k];
            minDb = std::min (minDb, 10.0 * std::log10 (e / win / (0.08) + 1e-12));   // 0.08 = sine power
        }
        std::snprintf (buf, sizeof buf, "deepest 10 ms window %.1f dB over 8 s", minDb);
        check ("wobble dropouts", minDb < -8.0, buf);
    }

    /* ---------------------------------------------------------------- FX chain II */
    std::printf ("\n-- FX chain II --\n");
    // run a send FX at mix 1 (wet only) with an on-switch schedule; returns L, fills R
    auto runSendFx = [] (auto& fx, std::vector<float> L, std::vector<float>& R, size_t offAt = (size_t) -1)
    {
        vs::Smooth mix, onS; mix.prepare (kSr, 1.0f); onS.prepare (kSr, 1.0f); mix.snap (1.0f); onS.snap (1.0f);
        bool active = false;
        R = L;
        const int blk = 256;
        for (size_t s = 0; s < L.size(); s += blk)
        {
            const int n = (int) std::min<size_t> (blk, L.size() - s);
            vs::runSend (fx, s < offAt, 1.0f, mix, onS, active, L.data() + s, R.data() + s, n);
        }
        return L;
    };
    auto burst = [] (double seconds, double burstMs, unsigned seed)
    {
        std::vector<float> v ((size_t) (seconds * kSr), 0.0f);
        std::mt19937 rng (seed); std::uniform_real_distribution<float> u (-0.5f, 0.5f);
        for (size_t i = 0; i < (size_t) (burstMs * 0.001 * kSr); ++i) v[i] = u (rng);
        return v;
    };
    auto envDb = [] (const std::vector<float>& L, const std::vector<float>& R, double winMs)
    {
        const size_t w = (size_t) (winMs * 0.001 * kSr);
        std::vector<double> e;
        for (size_t s = 0; s + w <= L.size(); s += w)
        {
            double a = 0; for (size_t k = 0; k < w; ++k) a += L[s + k] * L[s + k] + R[s + k] * R[s + k];
            e.push_back (10.0 * std::log10 (a / (double) w + 1e-30));
        }
        return e;
    };

    // RT60 accuracy: time from -5 to -25 dB (re peak) × 3
    for (float target : { 2.0f, 6.0f })
    {
        vs::Haze h; h.prepare (kSr);
        vs::Haze::Params p; p.decayS = target; p.mod = 0.0f; p.damp = 0.0f; p.predelayMs = 0.0f; p.size = 0.7f;
        h.setParams (p); h.snap();
        std::vector<float> R; const auto L = runSendFx (h, burst (target * 1.6, 10.0, 5), R);
        const auto e = envDb (L, R, 20.0);
        const double pk = *std::max_element (e.begin(), e.end());
        int i5 = -1, i25 = -1;
        const size_t ipk = (size_t) (std::max_element (e.begin(), e.end()) - e.begin());
        for (size_t k = ipk; k < e.size(); ++k) { if (i5 < 0 && e[k] < pk - 5) i5 = (int) k; if (i25 < 0 && e[k] < pk - 25) { i25 = (int) k; break; } }
        const double rt = (i5 >= 0 && i25 > i5) ? 3.0 * (i25 - i5) * 0.020 : -1.0;
        std::snprintf (buf, sizeof buf, "target %.1f s, measured %.2f s", target, rt);
        check (target < 3 ? "haze RT60 2 s" : "haze RT60 6 s", rt > target * 0.7 && rt < target * 1.3, buf);
    }

    // freeze: hold the tail; never blow up
    {
        vs::Haze h; h.prepare (kSr);
        vs::Haze::Params p; p.decayS = 3.0f; p.mod = 0.4f; p.damp = 0.4f; h.setParams (p); h.snap();
        auto in = burst (6.0, 300.0, 6);
        vs::Smooth mix, onS; mix.prepare (kSr, 1.0f); onS.prepare (kSr, 1.0f); mix.snap (1.0f); onS.snap (1.0f);
        bool active = false; std::vector<float> L = in, R = in;
        for (size_t s = 0; s < L.size(); s += 256)
        {
            p.freeze = s >= (size_t) (0.5 * kSr); h.setParams (p);
            vs::runSend (h, true, 1.0f, mix, onS, active, L.data() + s, R.data() + s, (int) std::min<size_t> (256, L.size() - s));
        }
        const auto e = envDb (L, R, 100.0);
        const double at1 = e[10], at5 = e[50];
        float pk = 0; bool fin = true; for (size_t i = 0; i < L.size(); ++i) { pk = std::max ({ pk, std::abs (L[i]), std::abs (R[i]) }); fin &= std::isfinite (L[i]); }
        std::snprintf (buf, sizeof buf, "1 s %.1f dB → 5 s %.1f dB, peak %.2f", at1, at5, pk);
        check ("haze freeze sustains", fin && std::abs (at5 - at1) < 3.0 && pk < 2.0f, buf);
    }

    // worst-case stability: 30 s decay, size 1, mod 1, no damping, repeated loud bursts
    {
        vs::Haze h; h.prepare (kSr);
        vs::Haze::Params p; p.decayS = 30.0f; p.size = 1.0f; p.mod = 1.0f; p.damp = 0.0f; h.setParams (p); h.snap();
        std::vector<float> in ((size_t) (20.0 * kSr), 0.0f);
        std::mt19937 rng (8); std::uniform_real_distribution<float> u (-0.9f, 0.9f);
        for (int b = 0; b < 10; ++b) for (size_t i = 0; i < 4800; ++i) in[(size_t) (b * 2.0 * kSr) + i] = u (rng);
        std::vector<float> R; const auto L = runSendFx (h, in, R);
        float pk = 0; bool fin = true; for (size_t i = 0; i < L.size(); ++i) { pk = std::max ({ pk, std::abs (L[i]), std::abs (R[i]) }); fin &= std::isfinite (L[i]) && std::isfinite (R[i]); }
        std::snprintf (buf, sizeof buf, "20 s, peak %.2f", pk);
        check ("haze worst-case stability", fin && pk < 4.0f, buf);
    }

    // stereo decorrelation of the tail
    {
        vs::Haze h; h.prepare (kSr); h.setParams ({}); h.snap();
        std::vector<float> R; const auto L = runSendFx (h, burst (2.0, 10.0, 9), R);
        double ab = 0, aa = 0, bb = 0;
        for (size_t i = (size_t) (0.2 * kSr); i < L.size(); ++i) { ab += L[i] * R[i]; aa += L[i] * L[i]; bb += R[i] * R[i]; }
        const double c = ab / std::sqrt (aa * bb + 1e-30);
        std::snprintf (buf, sizeof buf, "tail L/R corr %.2f", c);
        check ("haze stereo tail", std::abs (c) < 0.5, buf);
    }

    // reverse swell: wet energy should peak well after the transient
    {
        auto peakAt = [&] (float revAmt)
        {
            vs::Haze h; h.prepare (kSr);
            vs::Haze::Params p; p.reverse = revAmt; p.decayS = 1.0f; p.predelayMs = 0.0f; h.setParams (p); h.snap();
            std::vector<float> R; const auto L = runSendFx (h, burst (1.5, 5.0, 10), R);
            const auto e = envDb (L, R, 10.0);
            return 10.0 * (double) (std::max_element (e.begin(), e.end()) - e.begin());
        };
        const double p0 = peakAt (0.0f), p1 = peakAt (1.0f);
        std::snprintf (buf, sizeof buf, "wet peak at %.0f ms → %.0f ms", p0, p1);
        check ("haze reverse swell", p1 > p0 + 150.0, buf);
    }

    // spillover: switching off keeps the tail ringing
    {
        vs::Haze h; h.prepare (kSr);
        vs::Haze::Params p; p.decayS = 4.0f; h.setParams (p); h.snap();
        std::vector<float> R;
        const auto L = runSendFx (h, burst (3.0, 300.0, 11), R, (size_t) (0.5 * kSr));
        const auto e = envDb (L, R, 50.0);
        const double before = e[9], after = e[16];   // 450 ms vs 800 ms (switched off at 500 ms)
        std::snprintf (buf, sizeof buf, "450 ms %.1f dB, 800 ms (off) %.1f dB", before, after);
        check ("haze tail spillover", after > before - 12.0 && after < before, buf);
    }

    // echo: free time, sync, feedback ratio, stability
    auto echoRun = [&] (vs::Echo::Params ep, std::vector<float> in)
    {
        vs::Echo e; e.prepare (kSr); e.setParams (ep); e.snap();
        std::vector<float> R; auto L = runSendFx (e, std::move (in), R);
        return L;
    };
    auto firstPeaks = [] (const std::vector<float>& L, int count, size_t from)
    {
        std::vector<std::pair<size_t, float>> pk;
        size_t i = from;
        while ((int) pk.size() < count && i < L.size())
        {
            if (std::abs (L[i]) > 0.02f)
            {
                size_t best = i; for (size_t k = i; k < std::min (L.size(), i + 960); ++k) if (std::abs (L[k]) > std::abs (L[best])) best = k;
                pk.push_back ({ best, std::abs (L[best]) });
                i = best + 2400;
            }
            else ++i;
        }
        return pk;
    };
    std::vector<float> click ((size_t) (2.0 * kSr), 0.0f); click[0] = 1.0f; click[1] = -0.5f;
    {
        vs::Echo::Params ep; ep.sync = false; ep.timeMs = 250.0f; ep.feedback = 0.0f; ep.tone = 1.0f;
        const auto L = echoRun (ep, click);
        const auto pk = firstPeaks (L, 1, 10);
        const double ms = pk.empty() ? -1 : 1000.0 * pk[0].first / kSr;
        std::snprintf (buf, sizeof buf, "first repeat at %.1f ms", ms);
        check ("echo free 250 ms", std::abs (ms - 250.0) < 2.0, buf);
    }
    {
        vs::Echo::Params ep; ep.sync = true; ep.division = 8; ep.bpm = 120.0; ep.feedback = 0.0f; ep.tone = 1.0f;
        const auto L = echoRun (ep, click);
        const auto pk = firstPeaks (L, 1, 10);
        const double ms = pk.empty() ? -1 : 1000.0 * pk[0].first / kSr;
        std::snprintf (buf, sizeof buf, "1/4 @ 120 BPM → %.1f ms", ms);
        check ("echo synced 1/4", std::abs (ms - 500.0) < 2.0, buf);
    }
    {
        vs::Echo::Params ep; ep.sync = false; ep.timeMs = 200.0f; ep.feedback = 0.5f; ep.tone = 0.5f;
        std::vector<float> tone ((size_t) (1.0 * kSr), 0.0f);
        for (size_t i = 0; i < 2400; ++i) tone[i] = 0.3f * (float) std::sin (6.2831853 * 1000.0 * (double) i / kSr) * (float) std::sin (3.14159265 * (double) i / 2400.0);
        const auto L = echoRun (ep, tone);
        double e1 = 0, e2 = 0;
        for (size_t i = (size_t) (0.19 * kSr); i < (size_t) (0.27 * kSr); ++i) e1 += L[i] * L[i];
        for (size_t i = (size_t) (0.39 * kSr); i < (size_t) (0.47 * kSr); ++i) e2 += L[i] * L[i];
        const double ratioDb = 10.0 * std::log10 (e2 / (e1 + 1e-30));
        std::snprintf (buf, sizeof buf, "repeat 2 vs 1: %+.1f dB (fb 0.5 ≈ -6)", ratioDb);
        check ("echo feedback ratio", ratioDb < -4.0 && ratioDb > -9.0, buf);
    }
    {
        vs::Echo::Params ep; ep.sync = false; ep.timeMs = 120.0f; ep.feedback = 0.95f; ep.tone = 1.0f;
        std::vector<float> in ((size_t) (10.0 * kSr));
        std::mt19937 rng (12); std::uniform_real_distribution<float> u (-0.9f, 0.9f);
        for (auto& v : in) v = u (rng);
        const auto L = echoRun (ep, in);
        float pk = 0; bool fin = true; for (float v : L) { pk = std::max (pk, std::abs (v)); fin &= std::isfinite (v); }
        std::snprintf (buf, sizeof buf, "10 s full-scale noise, fb 95%%, peak %.2f", pk);
        check ("echo stability", fin && pk < 2.0f, buf);
    }

    /* ---------------------------------------------------------------- modulation + VAPOR */
    std::printf ("\n-- modulation / VAPOR --\n");
    using LS = vs::Lfo::Shape;
    auto lfoRun = [] (LS shape, float rate, bool sync, double bpm, double seconds)
    {
        vs::Lfo l; l.prepare (kSr); l.setShape (shape);
        std::vector<float> v;
        const int chunk = 32;
        for (int s = 0; s < (int) (seconds * kSr); s += 512)
        {
            l.beginBlock (rate, sync, bpm, -1.0);
            for (int c = 0; c < 512; c += chunk) { v.push_back (l.value()); l.advance (chunk); }
        }
        return v;
    };
    auto crossings = [] (const std::vector<float>& v) { int n = 0; for (size_t i = 1; i < v.size(); ++i) n += (v[i - 1] < 0) != (v[i] < 0); return n; };
    {
        const auto s = lfoRun (LS::Sine, 2.0f, false, 120.0, 1.0);
        const float mx = *std::max_element (s.begin(), s.end()), mn = *std::min_element (s.begin(), s.end());
        std::snprintf (buf, sizeof buf, "%d zero crossings in 1 s, range %.2f..%.2f", crossings (s), mn, mx);
        check ("LFO sine 2 Hz", std::abs (crossings (s) - 4) <= 1 && mx > 0.99f && mn < -0.99f, buf);
    }
    {
        const auto q = lfoRun (LS::Square, 3.0f, false, 120.0, 2.0);
        double mean = 0; for (float v : q) mean += v; mean /= (double) q.size();
        const auto sh = lfoRun (LS::SampleHold, 4.0f, false, 120.0, 2.0);
        int steps = 0; for (size_t i = 1; i < sh.size(); ++i) steps += sh[i] != sh[i - 1];
        std::snprintf (buf, sizeof buf, "square mean %+.3f, S&H %d steps in 2 s @4 Hz", mean, steps);
        check ("LFO square / S&H", std::abs (mean) < 0.05 && std::abs (steps - 8) <= 1, buf);
    }
    {
        const bool mapOk = vs::Lfo::syncedCyclesPerBeat (1.0f) == 1.0 && vs::Lfo::syncedCyclesPerBeat (2.0f) == 2.0
                        && vs::Lfo::syncedCyclesPerBeat (0.5f) == 0.5 && vs::Lfo::syncedCyclesPerBeat (0.01f) == 1.0 / 16.0;
        const auto s = lfoRun (LS::Sine, 1.0f, true, 120.0, 2.0);   // 1/4 notes at 120 BPM = 2 Hz
        vs::Lfo l; l.prepare (kSr); l.setShape (LS::Sine); l.beginBlock (1.0f, true, 120.0, 3.25);
        std::snprintf (buf, sizeof buf, "map %s, %d crossings in 2 s, ppq 3.25 → %.2f", mapOk ? "ok" : "BAD", crossings (s), l.value());
        check ("LFO tempo sync", mapOk && std::abs (crossings (s) - 8) <= 1 && l.value() > 0.99f, buf);
    }
    {
        vs::ModValues a; a.add (vs::ModDest::Volume, -1.0f, 1.0f);
        vs::ModValues b; b.add (vs::ModDest::Volume, 1.0f, 1.0f);
        vs::ModValues c; c.add (vs::ModDest::Volume, -1.0f, 0.5f);
        vs::ModValues d; d.add (vs::ModDest::Cutoff, 1.0f, -0.5f); d.add (vs::ModDest::Cutoff, 1.0f, 1.0f);
        std::snprintf (buf, sizeof buf, "vol %.2f / %.2f / %.2f, cutoff %+.1f oct", a.volume, b.volume, c.volume, d.cutoffOct);
        check ("mod destinations", a.volume == 0.0f && b.volume == 1.0f && std::abs (c.volume - 0.5f) < 1e-6f && std::abs (d.cutoffOct - 2.0f) < 1e-6f, buf);
    }
    {
        const auto z = vs::VaporMacro::from (0.0f), f = vs::VaporMacro::from (1.0f);
        const bool neutral = z.hazeMix == 0 && z.wobWow == 0 && z.slowPitchSt == 0 && z.attackMul == 1 && z.synthGain == 1
                          && z.directGain == 1 && z.swellBlend == 0 && ! z.engageHaze && ! z.engageWobble;
        const bool full = std::abs (f.hazeMix - 0.5f) < 1e-6f && std::abs (f.slowPitchSt + 5.0f) < 1e-6f && std::abs (f.attackMul - 40.0f) < 1e-4f
                       && f.engageHaze && f.engageWobble;
        std::snprintf (buf, sizeof buf, "0 → neutral %s, 1 → haze +%.2f, slow %.0f st, attack ×%.0f", neutral ? "yes" : "NO", f.hazeMix, f.slowPitchSt, f.attackMul);
        check ("VAPOR macro mapping", neutral && full, buf);
    }
    {
        vs::VoiceEnvelope e; e.prepare (kSr); e.setParams (vs::VoiceEnvelope::Mode::Follow, 400.0f, 300.0f, 0.8f, 400.0f);
        e.setSwellBlend (1.0f);
        e.noteOn (1.0f, 0.3f);
        float at200 = 0;
        for (int i = 1; i <= (int) (0.2 * kSr); ++i) { e.tick (0.3f, true); at200 = e.level(); }
        vs::VoiceEnvelope n; n.prepare (kSr); n.setParams (vs::VoiceEnvelope::Mode::Follow, 400.0f, 300.0f, 0.8f, 400.0f);
        n.noteOn (1.0f, 0.3f); for (int i = 0; i < 100; ++i) n.tick (0.3f, true);
        std::snprintf (buf, sizeof buf, "VAPOR swell 200 ms %.2f, no VAPOR %.2f", at200, n.level());
        check ("Follow + VAPOR swell", std::abs (at200 - 0.5f) < 0.05f && n.level() > 0.99f, buf);
    }

    std::printf ("\n%s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
