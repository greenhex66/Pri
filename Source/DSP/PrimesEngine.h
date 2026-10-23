#pragma once
// HEX-Q PRIMES - saturation engines. Pure C++ (no JUCE). No heap allocation while processing.
//
// OPTIMUS  (smooth saturation): drive -> tanh with a small asymmetry (even + odd harmonics) evaluated with
//          first-order antiderivative anti-aliasing (ADAA, no oversampling) -> DC blocker -> tone (tilt EQ).
// MEGATRON (destructive):       drive -> asymmetric wave-FOLDING -> DC blocker -> true bit-depth quantiser
//          -> sample-and-hold sample-rate reducer (no anti-alias filter: the aliasing is the sound).
// The two engines share nothing except the Mix / Output / bypass stage around them.
#include <algorithm>
#include <array>
#include <cmath>

namespace hexq::primes
{
enum class Mode : int { Optimus = 0, Megatron = 1 };

struct Settings
{
    Mode mode = Mode::Optimus;
    bool bypass = false;
    float mix = 1.0f;          // 0..1
    float outputDb = 0.0f;     // -18 .. +6
    float optDrive = 0.0f;     // 0..1
    float optTone = 0.5f;      // 0..1 (0.5 = flat)
    float megDrive = 0.25f;    // 0..1
    float megBits = 8.0f;      // 4..16
    float megRate = 0.5f;      // 0.01..1 (1 = no reduction)
};

namespace shape
{
    constexpr float kOptBeta = 0.25f;                 // asymmetry of the OPTIMUS curve (adds 2nd harmonic)
    constexpr float kMegBias = 0.15f;                 // asymmetry of the MEGATRON fold (scaled with drive)
    constexpr float kRefAmp = 0.25f;                  // -12 dBFS sine used for loudness matching

    inline double logCosh (double x) noexcept
    {
        const double a = std::abs (x);
        return a + std::log1p (std::exp (-2.0 * a)) - 0.6931471805599453;
    }

    inline float optGain (float d) noexcept  { return 0.1f * std::pow (500.0f, d); }          // 0.1 .. 50
    inline float megGain (float d) noexcept  { return 1.0f + 99.0f * std::pow (d, 1.5f); }    // 1 .. 100

    // triangle fold with period 4: identity on [-1, 1], reflected outside
    inline float fold (float u) noexcept
    {
        const float w = (u + 1.0f) - 4.0f * std::floor ((u + 1.0f) * 0.25f);
        return 1.0f - std::abs (w - 2.0f);
    }

    // sample-rate setting 0.01..1 -> hold ratio 1/64 .. 1 (exponential)
    inline float megRatio (float p) noexcept { return std::exp (-4.158883083f * (1.0f - std::clamp (p, 0.0f, 1.0f))); }

    inline float quantize (float x, float scale) noexcept
    {
        return std::clamp (std::floor (x * scale + 0.5f) / scale, -1.0f, 1.0f);   // mid-tread, no dither
    }
}

namespace detail
{
    inline void slew (float& x, float target, float coef) noexcept
    {
        x += (target - x) * coef;
        if (std::abs (target - x) < 1.0e-6f)
            x = target;
    }

    // Static compensation tables: the wet level of a -12 dBFS sine is matched to the input level, so
    // DRIVE changes the harmonic content, not how loud the effect is at the reference level.
    struct CompTables
    {
        static constexpr int kSize = 257;
        std::array<float, kSize> opt {}, meg {};

        CompTables()
        {
            constexpr double twoPi = 6.283185307179586;
            const double tb = std::tanh ((double) shape::kOptBeta);
            for (int i = 0; i < kSize; ++i)
            {
                const float d = (float) i / (float) (kSize - 1);
                const double g = shape::optGain (d), gm = shape::megGain (d), bm = shape::kMegBias * d;
                double so = 0, soo = 0, sm = 0, smm = 0;
                const int n = 512;
                for (int k = 0; k < n; ++k)
                {
                    const double x = shape::kRefAmp * std::sin (twoPi * k / n);
                    const double fo = (std::tanh (g * x + shape::kOptBeta) - tb) / g;
                    const double fm = shape::fold ((float) (gm * x + bm));
                    so += fo; soo += fo * fo; sm += fm; smm += fm * fm;
                }
                const double rmsIn = shape::kRefAmp / std::sqrt (2.0);
                const double rmsO = std::sqrt (std::max (1.0e-12, soo / n - (so / n) * (so / n)));
                const double rmsM = std::sqrt (std::max (1.0e-12, smm / n - (sm / n) * (sm / n)));
                opt[(size_t) i] = (float) std::clamp (rmsIn / rmsO, 0.05, 20.0);
                meg[(size_t) i] = (float) std::clamp (rmsIn / rmsM, 0.05, 4.0);
            }
        }

        static float lookup (const std::array<float, kSize>& t, float d) noexcept
        {
            const float pos = std::clamp (d, 0.0f, 1.0f) * (float) (kSize - 1);
            const int i = std::min ((int) pos, kSize - 2);
            const float f = pos - (float) i;
            return t[(size_t) i] + (t[(size_t) i + 1] - t[(size_t) i]) * f;
        }
    };

    struct OptimusChannel
    {
        double u1 = 0.0;
        bool primed = false;
        float dcX = 0.0f, dcY = 0.0f, lp = 0.0f;

        void reset() noexcept { u1 = 0.0; primed = false; dcX = dcY = lp = 0.0f; }

        // g: pre-gain, comp: loudness compensation, gL/gH: tone gains below/above the pivot
        float process (float x, bool shaped, float g, float comp, float gL, float gH, float lpCoef, float dcR) noexcept
        {
            float s;
            if (shaped)
            {
                const double u = (double) g * (double) x + (double) shape::kOptBeta;
                if (! primed) { u1 = u; primed = true; }
                const double du = u - u1;
                double y;
                if (std::abs (du) < 1.0e-5) y = std::tanh (0.5 * (u + u1));
                else                        y = (shape::logCosh (u) - shape::logCosh (u1)) / du;     // ADAA: mean of tanh over [u1, u]
                u1 = u;

                s = (float) ((y - std::tanh ((double) shape::kOptBeta)) / (double) g) * comp;
                const float d = s - dcX + dcR * dcY;                                                // DC blocker
                dcX = s; dcY = d; s = d;
            }
            else
            {
                s = x;                                                                              // drive 0 %: exactly the input
                u1 = 0.0; primed = false; dcX = dcY = 0.0f;
            }

            lp += lpCoef * (s - lp);                                                                // tone: tilt around the pivot
            if (gH == 1.0f)
                return s;                                                                           // tone 50 %: flat, exactly the input of this stage
            return lp * gL + (s - lp) * gH;
        }
    };

    struct MegatronChannel
    {
        float dcX = 0.0f, dcY = 0.0f, held = 0.0f;

        void reset() noexcept { dcX = dcY = held = 0.0f; }

        float distort (float x, bool shaped, float g, float bias, float offset, float dcR) noexcept
        {
            if (! shaped)
            {
                dcX = dcY = 0.0f;
                return x;
            }
            const float s = shape::fold (g * x + bias) - offset;          // offset = fold(bias): zero input stays exactly zero
            const float d = s - dcX + dcR * dcY;
            dcX = s; dcY = d;
            return d;
        }
    };
}

class PrimesEngine
{
public:
    void prepare (double sr) noexcept
    {
        sampleRate = (float) sr;
        coefP    = 1.0f - std::exp (-1.0f / (0.010f * sampleRate));     // parameter smoothing 10 ms
        coefBits = 1.0f - std::exp (-1.0f / (0.005f * sampleRate));
        rampMode = 1.0f / (0.020f * sampleRate);                        // mode crossfade 20 ms
        dcR      = std::exp (-6.283185307f * 4.0f / sampleRate);        // 4 Hz DC blocker
        lpCoef   = 1.0f - std::exp (-6.283185307f * 800.0f / sampleRate);   // tone pivot 800 Hz
        reset();
        initialised = false;
    }

    void reset() noexcept
    {
        resetOptimus();
        resetMegatron();
    }

    int getLatencySamples() const noexcept { return 0; }

    // R may be nullptr (mono). In place. Any block size.
    void process (float* L, float* R, int n, const Settings& s) noexcept
    {
        using detail::slew;
        if (n <= 0)
            return;
        const int nCh = (R != nullptr) ? 2 : 1;
        float* ch[2] = { L, R };

        const float tOptD = std::clamp (s.optDrive, 0.0f, 1.0f), tOptT = std::clamp (s.optTone, 0.0f, 1.0f);
        const float tMegD = std::clamp (s.megDrive, 0.0f, 1.0f), tBits = std::clamp (s.megBits, 4.0f, 16.0f);
        const float tRate = std::clamp (s.megRate, 0.01f, 1.0f);
        const float tMix = std::clamp (s.mix, 0.0f, 1.0f);
        const float tOut = std::pow (10.0f, std::clamp (s.outputDb, -18.0f, 6.0f) / 20.0f);
        const float tBy = s.bypass ? 1.0f : 0.0f;

        if (! initialised)
        {
            optD = tOptD; optT = tOptT; megD = tMegD; bits = tBits; rate = tRate; mixS = tMix; outS = tOut; byS = tBy;
            curMode = prevMode = s.mode;
            xf = 1.0f;
            lastOptD = lastOptT = lastMegD = lastBits = lastRate = -1.0f;
            initialised = true;
        }

        if (byS >= 1.0f && tBy >= 1.0f)             // fully bypassed: audio untouched, zero CPU
        {
            asleep = true;
            return;
        }
        if (asleep)
        {
            reset();
            asleep = false;
        }

        if (s.mode != curMode)                      // mode change: crossfade, start the new engine from a clean state
        {
            prevMode = curMode;
            curMode = s.mode;
            xf = 0.0f;
            if (curMode == Mode::Optimus) resetOptimus(); else resetMegatron();
        }

        for (int i = 0; i < n; ++i)
        {
            slew (optD, tOptD, coefP); slew (optT, tOptT, coefP); slew (megD, tMegD, coefP);
            slew (bits, tBits, coefBits); slew (rate, tRate, coefP);
            slew (mixS, tMix, coefP); slew (outS, tOut, coefP); slew (byS, tBy, coefP);

            const bool fading = xf < 1.0f;
            const bool needOpt = (curMode == Mode::Optimus) || (fading && prevMode == Mode::Optimus);
            const bool needMeg = (curMode == Mode::Megatron) || (fading && prevMode == Mode::Megatron);

            if (needOpt)
            {
                if (optD != lastOptD)
                {
                    lastOptD = optD;
                    optG = shape::optGain (optD);
                    optComp = detail::CompTables::lookup (tables.opt, optD);
                }
                if (optT != lastOptT)
                {
                    lastOptT = optT;
                    const float tiltDb = (optT - 0.5f) * 2.0f * 12.0f;           // +-12 dB tilt (+-6 dB each side)
                    gH = std::pow (10.0f, tiltDb / 40.0f);
                    gL = 1.0f / gH;
                }
            }
            bool tick = false;
            if (needMeg)
            {
                if (megD != lastMegD)
                {
                    lastMegD = megD;
                    megG = shape::megGain (megD);
                    megBias = shape::kMegBias * megD;
                    megOffset = shape::fold (megBias);
                    megComp = detail::CompTables::lookup (tables.meg, megD);
                }
                if (bits != lastBits) { lastBits = bits; qScale = std::exp2 (bits - 1.0f); }
                if (rate != lastRate) { lastRate = rate; ratio = shape::megRatio (rate); }

                if (ratio >= 0.9999f) { tick = true; megAcc = 0.0f; }
                else
                {
                    megAcc += ratio;
                    if (megAcc >= 1.0f) { megAcc -= 1.0f; tick = true; }
                }
            }

            if (xf < 1.0f)
                xf = std::min (1.0f, xf + rampMode);

            for (int c = 0; c < nCh; ++c)
            {
                const float dry = ch[c][i];
                float wetO = 0.0f, wetM = 0.0f;

                if (needOpt)
                    wetO = opt[c].process (dry, optD > 0.0f, optG, optComp, gL, gH, lpCoef, dcR);
                if (needMeg)
                {
                    const float dsx = meg[c].distort (dry, megD > 0.0f, megG, megBias, megOffset, dcR);
                    const float q = shape::quantize (dsx, qScale);
                    if (tick) meg[c].held = q;
                    wetM = meg[c].held * megComp;
                }

                float wet;
                if (xf < 1.0f)
                {
                    const float a = (prevMode == Mode::Optimus) ? wetO : wetM;
                    const float b = (curMode == Mode::Optimus) ? wetO : wetM;
                    wet = a + (b - a) * xf;
                }
                else
                    wet = (curMode == Mode::Optimus) ? wetO : wetM;

                float out = (dry * (1.0f - mixS) + wet * mixS) * outS;          // mix 0 % -> dry, mix 100 % -> wet, output 0 dB -> x1
                if (byS > 0.0f)
                    out += (dry - out) * byS;                                   // global bypass crossfade
                if (! (std::abs (out) < 100.0f))
                    out = 0.0f;                                                 // NaN / runaway guard
                ch[c][i] = out;
            }
        }
    }

private:
    void resetOptimus() noexcept { opt[0].reset(); opt[1].reset(); lastOptD = lastOptT = -1.0f; }
    void resetMegatron() noexcept { meg[0].reset(); meg[1].reset(); megAcc = 1.0f; lastMegD = lastBits = lastRate = -1.0f; }

    detail::CompTables tables;
    std::array<detail::OptimusChannel, 2> opt;
    std::array<detail::MegatronChannel, 2> meg;

    float sampleRate = 44100.0f, coefP = 0.002f, coefBits = 0.004f, rampMode = 0.001f, dcR = 0.9995f, lpCoef = 0.1f;

    float optD = 0, optT = 0.5f, megD = 0, bits = 8, rate = 0.5f, mixS = 1, outS = 1, byS = 0;
    float lastOptD = -1, lastOptT = -1, lastMegD = -1, lastBits = -1, lastRate = -1;
    float optG = 0.1f, optComp = 1, gL = 1, gH = 1;
    float megG = 1, megBias = 0, megOffset = 0, megComp = 1, qScale = 128, ratio = 1, megAcc = 1;

    Mode curMode = Mode::Optimus, prevMode = Mode::Optimus;
    float xf = 1.0f;
    bool initialised = false, asleep = false;
};
} // namespace hexq::primes
