// HEX-Q PRIMES - offline DSP self-test (no JUCE needed).
#include "DSP/PrimesEngine.h"

#include <chrono>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <random>
#include <set>
#include <vector>

static std::size_t g_allocs = 0;
static bool g_count = false;
void* operator new (std::size_t n)
{
    if (g_count) ++g_allocs;
    if (void* p = std::malloc (n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }

using namespace hexq::primes;
using Vec = std::vector<float>;
static const double PI = 3.14159265358979323846;
static int g_pass = 0, g_fail = 0;

static void check (bool ok, const char* name, double v = 0.0)
{
    ok ? ++g_pass : ++g_fail;
    std::printf ("[%s] %-80s %.5f\n", ok ? "PASS" : "FAIL", name, v);
}

static Vec sine (size_t n, double f, double sr, double amp = 0.25)
{
    Vec v (n); for (size_t i = 0; i < n; ++i) v[i] = (float) (amp * std::sin (2.0 * PI * f * (double) i / sr)); return v;
}
static Vec noise (size_t n, unsigned seed, float amp = 0.3f)
{
    std::mt19937 rng (seed); std::uniform_real_distribution<float> d (-amp, amp);
    Vec v (n); for (auto& x : v) x = d (rng); return v;
}

static Settings optS (float drive, float tone = 0.5f) { Settings s; s.mode = Mode::Optimus; s.optDrive = drive; s.optTone = tone; return s; }
static Settings megS (float drive, float bits, float rate) { Settings s; s.mode = Mode::Megatron; s.megDrive = drive; s.megBits = bits; s.megRate = rate; return s; }

static Vec run (const Vec& in, const Settings& s, double sr = 48000.0, int block = 512)
{
    PrimesEngine e; e.prepare (sr); Vec y = in;
    for (size_t pos = 0; pos < y.size(); pos += (size_t) block)
        e.process (y.data() + pos, nullptr, (int) std::min ((size_t) block, y.size() - pos), s);
    return y;
}

// Hann-windowed amplitude of one frequency
static double amp (const Vec& y, double f, double sr, size_t a, size_t b)
{
    std::complex<double> acc = 0; double wsum = 0;
    for (size_t i = a; i < b; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2.0 * PI * (double) (i - a) / (double) (b - a));
        acc += w * (double) y[i] * std::polar (1.0, -2.0 * PI * f * (double) i / sr); wsum += w;
    }
    return 2.0 * std::abs (acc) / wsum;
}
static double rms (const Vec& v, size_t a = 0, size_t b = 0) { if (! b) b = v.size(); double s = 0; for (size_t i = a; i < b; ++i) s += (double) v[i] * v[i]; return std::sqrt (s / (double) (b - a)); }
static double db (double x) { return 20.0 * std::log10 (std::max (x, 1e-12)); }
static double maxDiff (const Vec& a, const Vec& b, size_t from = 0) { double m = 0; for (size_t i = from; i < a.size(); ++i) m = std::max (m, (double) std::abs (a[i] - b[i])); return m; }
static double maxStep (const Vec& v, size_t from = 1) { double m = 0; for (size_t i = std::max<size_t> (from, 1); i < v.size(); ++i) m = std::max (m, (double) std::abs (v[i] - v[i - 1])); return m; }
static double peak (const Vec& v) { double m = 0; for (float x : v) m = std::max (m, (double) std::abs (x)); return m; }
static bool finite (const Vec& v) { for (float x : v) if (! std::isfinite (x)) return false; return true; }

static double thd (const Vec& y, double f, double sr, size_t a, size_t b, double* evenDb = nullptr, double* oddDb = nullptr)
{
    const double a1 = amp (y, f, sr, a, b); double e = 0, o = 0;
    for (int k = 2; k <= 10; ++k) { const double ak = amp (y, f * k, sr, a, b); (k % 2 ? o : e) += ak * ak; }
    if (evenDb) *evenDb = db (std::sqrt (e) / a1);
    if (oddDb) *oddDb = db (std::sqrt (o) / a1);
    return std::sqrt (e + o) / a1;
}

int main()
{
    const double SR = 48000.0; const size_t N = 96000;

    std::printf ("=== HEX-Q PRIMES DSP self-test ===\n\n-- OPTIMUS (smooth saturation)\n");
    {
        Vec in = noise (N, 1), y = run (in, optS (0.0f));
        check (maxDiff (in, y) == 0.0, "drive 0 %, tone 50 %: output identical to input (max diff)", maxDiff (in, y));

        std::printf ("      1 kHz sine at -12 dBFS:   drive   THD     even    odd     level change\n");
        double prev = 0; bool mono = true; double thd10 = 0, lvlWorst = 0;
        for (float d : { 0.1f, 0.25f, 0.5f, 0.75f, 1.0f })
        {
            Vec x = sine (N, 1000.0, SR), o = run (x, optS (d)); double ev, od;
            const double t = thd (o, 1000.0, SR, 48000, N, &ev, &od);
            const double lvl = db (rms (o, 48000, N) / rms (x, 48000, N));
            std::printf ("                                %4.0f %%  %6.2f %%  %6.1f  %6.1f  %+5.2f dB\n", d * 100, t * 100, ev, od, lvl);
            if (t <= prev) mono = false; prev = t; if (d == 0.1f) thd10 = t; lvlWorst = std::max (lvlWorst, std::abs (lvl));
        }
        check (mono, "THD grows with every Drive step (10 / 25 / 50 / 75 / 100 %)");
        check (thd10 < 0.01, "low drive (10 %) is subtle: THD below 1 % (THD, fraction)", thd10);
        check (lvlWorst < 1.5, "level of a -12 dBFS sine stays within 1.5 dB at all drives (worst, dB)", lvlWorst);

        double pn = 0;
        for (float d : { 0.25f, 0.5f, 1.0f })
        {
            Vec x = noise (N, 4, 0.18f), o = run (x, optS (d));
            pn = std::max (pn, std::abs (db (rms (o, 48000, N) / rms (x, 48000, N))));
        }
        check (pn < 4.0, "noise at about -18 dBFS RMS: level change stays within 4 dB (worst, dB)", pn);

        // tone: tilt measured with sines at 100 Hz and 10 kHz, drive 0
        auto gainAt = [&] (double f, float tone) { Vec x = sine (N, f, SR, 0.2), o = run (x, optS (0.0f, tone)); return db (amp (o, f, SR, 48000, N) / amp (x, f, SR, 48000, N)); };
        const double lo0 = gainAt (100, 0.0f), hi0 = gainAt (10000, 0.0f), lo1 = gainAt (100, 1.0f), hi1 = gainAt (10000, 1.0f), mid = gainAt (3000, 0.5f);
        std::printf ("      tone 0 %%: 100 Hz %+.2f dB, 10 kHz %+.2f dB   tone 100 %%: 100 Hz %+.2f dB, 10 kHz %+.2f dB   tone 50 %%: %+.4f dB\n", lo0, hi0, lo1, hi1, mid);
        check (hi1 > 4.5 && lo1 < -4.5 && lo0 > 4.5 && hi0 < -4.5, "TONE: dark boosts lows / cuts highs, bright does the opposite (about +-6 dB)");
        check (std::abs (mid) < 0.01, "TONE 50 % is flat (dB)", mid);

        // DC
        Vec x = sine (N, 100.0, SR, 0.5), o = run (x, optS (1.0f)); double dc = 0; for (size_t i = 48000; i < N; ++i) dc += o[i]; dc /= (double) (N - 48000);
        check (std::abs (dc) < 0.002, "max drive on a 100 Hz sine: DC offset after the DC blocker (mean)", dc);

        // extreme input
        Vec sq (N); for (size_t i = 0; i < N; ++i) sq[i] = ((i / 40) % 2) ? 1.0f : -1.0f;
        Vec nz = noise (N, 8, 1.0f);
        const bool ok = finite (run (sq, optS (1.0f, 0.0f))) && finite (run (nz, optS (1.0f, 1.0f))) && peak (run (nz, optS (1.0f, 1.0f))) < 4.0;
        check (ok, "full-scale square and noise at 100 % drive: finite and bounded");

        // mix
        Vec w = run (in, optS (1.0f, 0.2f)); Settings m0 = optS (1.0f, 0.2f); m0.mix = 0.0f; Vec d0 = run (in, m0);
        Settings m5 = m0; m5.mix = 0.5f; Vec d5 = run (in, m5); double e5 = 0;
        for (size_t i = 48000; i < N; ++i) e5 = std::max (e5, (double) std::abs (d5[i] - 0.5f * (in[i] + w[i])));
        check (maxDiff (d0, in) == 0.0, "mix 0 %: output is the dry signal bit-for-bit (max diff)", maxDiff (d0, in));
        check (e5 < 1e-6, "mix 50 %: output = half dry + half wet (max error)", e5);
        Settings o6 = m0; o6.outputDb = 6.0f; Vec g6 = run (in, o6);
        check (std::abs (db (rms (g6, 48000, N) / rms (in, 48000, N)) - 6.0) < 0.01, "output +6 dB gives +6 dB (error, dB)", db (rms (g6, 48000, N) / rms (in, 48000, N)) - 6.0);

        // aliasing: 5 kHz sine, residual after removing 5/10/15/20 kHz (and DC); ADAA vs plain tanh
        auto residualDb = [&] (const Vec& y)
        {
            const size_t a = 4800, b = 4800 + 9600; double E = 0, mean = 0; for (size_t i = a; i < b; ++i) { E += (double) y[i] * y[i]; mean += y[i]; } mean /= 9600.0;
            double H = 9600.0 * mean * mean;
            for (double f : { 5000.0, 10000.0, 15000.0, 20000.0 }) { std::complex<double> acc = 0; for (size_t i = a; i < b; ++i) acc += (double) y[i] * std::polar (1.0, -2 * PI * f * (double) i / SR); const double ak = 2.0 * std::abs (acc) / 9600.0; H += 9600.0 * ak * ak / 2.0; }
            return 10.0 * std::log10 (std::max (E - H, 1e-18) / E);
        };
        Vec x5 = sine (24000, 5000.0, SR, 0.5), adaa = run (x5, optS (1.0f));
        const double g = shape::optGain (1.0f); Vec naive (x5.size());
        for (size_t i = 0; i < x5.size(); ++i) naive[i] = (float) ((std::tanh (g * x5[i] + shape::kOptBeta) - std::tanh (shape::kOptBeta)) / g);
        const double rA = residualDb (adaa), rN = residualDb (naive);
        std::printf ("      5 kHz sine, drive 100 %%: non-harmonic (aliasing) energy: ADAA %.1f dB, plain tanh %.1f dB\n", rA, rN);
        check (rA < rN - 3.0, "ADAA reduces aliasing by at least 3 dB versus plain tanh (dB)", rN - rA);

        // mono == stereo
        PrimesEngine em, es; em.prepare (SR); es.prepare (SR); Vec a = noise (20000, 12), b = a, c = a;
        em.process (a.data(), nullptr, 20000, optS (0.6f, 0.3f)); es.process (b.data(), c.data(), 20000, optS (0.6f, 0.3f));
        check (maxDiff (a, b) == 0.0 && maxDiff (b, c) == 0.0, "mono processing equals each stereo channel (max diff)", maxDiff (a, b));
        Vec z (30000, 0.0f), zy = run (z, optS (1.0f, 0.8f));
        check (rms (zy) == 0.0, "silence in, silence out (rms)", rms (zy));
    }

    std::printf ("\n-- MEGATRON (distortion + bit crusher + sample-rate reducer)\n");
    {
        Vec in = noise (N, 2, 0.4f), y = run (in, megS (0.0f, 16.0f, 1.0f));
        check (maxDiff (in, y) < 1e-4, "drive 0, 16 bit, sample rate 100 %: practically clean (max diff)", maxDiff (in, y));

        auto levels = [&] (float bits) { std::set<float> s; Vec x = sine (N, 97.0, SR, 0.95), o = run (x, megS (0.0f, bits, 1.0f)); for (float v : o) s.insert (v); return (int) s.size(); };
        const int l4 = levels (4.0f), l8 = levels (8.0f);
        std::printf ("      distinct output levels: 4 bit = %d, 8 bit = %d\n", l4, l8);
        check (l4 <= 17 && l4 >= 12 && l8 <= 257 && l8 > 100, "bit depth is real quantisation: 4 bit -> <= 17 levels, 8 bit -> <= 257 levels", l4);

        auto snr = [&] (float bits) { Vec x = sine (N, 97.0, SR, 0.9), o = run (x, megS (0.0f, bits, 1.0f)); double e = 0, s = 0; for (size_t i = 0; i < N; ++i) { e += (double) (o[i] - x[i]) * (o[i] - x[i]); s += (double) x[i] * x[i]; } return 10.0 * std::log10 (s / e); };
        const double s8 = snr (8.0f), s12 = snr (12.0f);
        check (std::abs ((s12 - s8) - 24.0) < 3.0, "8 -> 12 bit improves the signal-to-quantisation-noise ratio by about 24 dB (dB)", s12 - s8);

        auto changes = [&] (float rate) { Vec x = noise (48000, 6, 0.4f), o = run (x, megS (0.0f, 16.0f, rate)); int c = 0; for (size_t i = 1; i < o.size(); ++i) if (o[i] != o[i - 1]) ++c; return c; };
        const int c100 = changes (1.0f), c50 = changes (0.5f), c1 = changes (0.01f);
        std::printf ("      new sample values per second (48 kHz): rate 100 %% = %d, 50 %% = %d, 1 %% = %d\n", c100, c50, c1);
        check (std::abs (c50 - 6000) < 150 && std::abs (c1 - 750) < 40 && c100 > 47000, "sample rate: 100 % = every sample, 50 % = 1/8 (6 kHz), 1 % = 1/64 (750 Hz)", c50);

        // distinct audible effects
        Vec x = sine (N, 1100.0, SR, 0.5);
        Vec rateOnly = run (x, megS (0.0f, 16.0f, 0.5f)), bitsOnly = run (x, megS (0.0f, 4.0f, 1.0f));
        auto img = [&] (const Vec& o) { const double f1 = amp (o, 1100.0, SR, 48000, N); return std::max (db (amp (o, 4900.0, SR, 48000, N) / f1), db (amp (o, 7100.0, SR, 48000, N) / f1)); };
        const double imgRate = img (rateOnly), imgBits = img (bitsOnly);
        std::printf ("      images of a 1.1 kHz sine around the 6 kHz hold rate: sample-rate only %.1f dB, bit depth only %.1f dB\n", imgRate, imgBits);
        check (imgRate > -30.0 && imgRate > imgBits + 10.0, "sample rate and bit depth have distinct signatures (image level difference, dB)", imgRate - imgBits);

        // mix / output
        Settings m0 = megS (1.0f, 6.0f, 0.2f); m0.mix = 0.0f; Vec d0 = run (in, m0);
        Settings m1 = m0; m1.mix = 1.0f; Vec w = run (in, m1); Settings m5 = m0; m5.mix = 0.5f; Vec d5 = run (in, m5); double e5 = 0;
        for (size_t i = 48000; i < N; ++i) e5 = std::max (e5, (double) std::abs (d5[i] - 0.5f * (in[i] + w[i])));
        check (maxDiff (d0, in) == 0.0, "mix 0 %: output is the dry signal bit-for-bit (max diff)", maxDiff (d0, in));
        check (e5 < 1e-6, "mix 50 %: output = half dry + half wet (max error)", e5);

        // identity vs OPTIMUS
        Vec xs = sine (N, 1000.0, SR, 0.25), oo = run (xs, optS (1.0f)), mm = run (xs, megS (1.0f, 8.0f, 1.0f));
        double hfO = 0, hfM = 0; for (double f : { 11000.0, 13000.0, 17000.0, 19000.0, 21000.0 }) { hfO += std::pow (amp (oo, f, SR, 48000, N), 2); hfM += std::pow (amp (mm, f, SR, 48000, N), 2); }
        const double hfDiff = db (std::sqrt (hfM) / std::sqrt (hfO + 1e-18));
        Vec df (N); for (size_t i = 0; i < N; ++i) df[i] = oo[i] - mm[i];
        std::printf ("      1 kHz sine at max drive: energy above 10 kHz, MEGATRON vs OPTIMUS: %+.1f dB; RMS of the difference vs OPTIMUS: %.2f\n", hfDiff, rms (df, 48000, N) / rms (oo, 48000, N));
        check (hfDiff > 15.0 && rms (df, 48000, N) > 0.5 * rms (oo, 48000, N), "the engines are different: MEGATRON is much harsher and not a variant of OPTIMUS (dB)", hfDiff);

        // silence, extreme input
        Vec z (30000, 0.0f);
        check (rms (run (z, megS (1.0f, 4.0f, 0.01f))) == 0.0, "silence in, silence out (rms)", rms (run (z, megS (1.0f, 4.0f, 0.01f))));
        Vec nz = noise (N, 8, 1.0f); Vec ny = run (nz, megS (1.0f, 4.0f, 0.01f));
        check (finite (ny) && peak (ny) <= 4.0, "max drive, 4 bit, 1 % rate on full-scale noise: finite and bounded (peak)", peak (ny));
    }

    std::printf ("\n-- general: automation, mode switching, bypass, stability, performance\n");
    {
        // parameter changes during playback
        for (int mode = 0; mode < 2; ++mode)
        {
            PrimesEngine e; e.prepare (SR); Vec x = noise (4 * 48000, 5, 0.3f); bool ok = true; double pk = 0;
            for (size_t pos = 0; pos < x.size(); pos += 480)
            {
                const float t = (float) pos / (float) x.size();
                Settings s = mode ? megS (t, 16.0f - 12.0f * t, 1.0f - 0.99f * t) : optS (t, 1.0f - t);
                e.process (x.data() + pos, nullptr, 480, s);
            }
            for (float v : x) { if (! std::isfinite (v)) ok = false; pk = std::max (pk, (double) std::abs (v)); }
            char label[100]; std::snprintf (label, sizeof label, "%s: all parameters swept during playback: finite and bounded (peak)", mode ? "MEGATRON" : "OPTIMUS");
            check (ok && pk < 4.0, label, pk);
        }
        {   // OPTIMUS sweep is click free
            PrimesEngine e; e.prepare (SR); Vec x = sine (4 * 48000, 300.0, SR, 0.5);
            for (size_t pos = 0; pos < x.size(); pos += 480) e.process (x.data() + pos, nullptr, 480, optS ((float) pos / (float) x.size(), 0.5f));
            Vec ref = run (sine (4 * 48000, 300.0, SR, 0.5), optS (1.0f));
            check (maxStep (x, 48000) < 1.3 * maxStep (ref, 48000), "OPTIMUS drive sweep: no steps larger than at fixed 100 % drive (max step)", maxStep (x, 48000));
        }

        // rapid mode switching
        {
            PrimesEngine e; e.prepare (SR); Vec x = sine (4 * 48000, 300.0, SR, 0.5); Settings s = optS (0.8f, 0.5f); s.megDrive = 0.8f; s.megBits = 8.0f; s.megRate = 0.5f;
            for (size_t pos = 0, k = 0; pos < x.size(); pos += 240, ++k) { s.mode = (k % 2) ? Mode::Megatron : Mode::Optimus; e.process (x.data() + pos, nullptr, 240, s); }
            Settings so = s; so.mode = Mode::Optimus; Settings sm = s; sm.mode = Mode::Megatron;
            const double ref = std::max (maxStep (run (sine (4 * 48000, 300.0, SR, 0.5), so), 48000), maxStep (run (sine (4 * 48000, 300.0, SR, 0.5), sm), 48000));
            const double pkRef = std::max (peak (run (sine (4 * 48000, 300.0, SR, 0.5), so)), peak (run (sine (4 * 48000, 300.0, SR, 0.5), sm)));
            check (finite (x) && peak (x) < 1.1 * pkRef + 0.01 && maxStep (x, 48000) < 1.3 * ref, "switching mode every 5 ms: no burst, no step beyond steady-state (max step)", maxStep (x, 48000));
        }
        // bypass
        {
            PrimesEngine e; e.prepare (SR); Vec x = sine (3 * 48000, 300.0, SR, 0.5), ix = x; Settings s = optS (0.7f, 0.5f);
            for (size_t pos = 0, k = 0; pos < x.size(); pos += 4800, ++k) { s.bypass = (k % 4) >= 2; e.process (x.data() + pos, nullptr, 4800, s); }
            const double ref = maxStep (run (sine (3 * 48000, 300.0, SR, 0.5), optS (0.7f, 0.5f)), 48000);
            check (maxStep (x, 48000) < 1.3 * ref + 0.01, "bypass on/off every 0.1 s: no click (max step)", maxStep (x, 48000));
            Vec y = ix; Settings sb = optS (1.0f); sb.bypass = true; y = run (y, sb);
            check (maxDiff (y, ix, 24000) == 0.0, "bypass: output identical to input after the fade (max diff)", maxDiff (y, ix, 24000));
        }
        // sample rates + random automation
        {
            bool ok = true; double pk = 0;
            for (double sr : { 22050.0, 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
            {
                PrimesEngine e; e.prepare (sr); std::mt19937 rng (3); std::uniform_real_distribution<float> u (0, 1), nz (-1, 1);
                for (int blk = 0; blk < 300; ++blk)
                {
                    Settings s; s.mode = (u (rng) > 0.5f) ? Mode::Megatron : Mode::Optimus; s.optDrive = u (rng); s.optTone = u (rng); s.megDrive = u (rng); s.megBits = 4 + 12 * u (rng);
                    s.megRate = 0.01f + 0.99f * u (rng); s.mix = u (rng); s.outputDb = -18 + 24 * u (rng); s.bypass = u (rng) > 0.9f;
                    Vec a (333), b (333); for (auto& v : a) v = nz (rng); for (auto& v : b) v = nz (rng);
                    e.process (a.data(), b.data(), 333, s);
                    for (size_t i = 0; i < 333; ++i) { if (! std::isfinite (a[i]) || ! std::isfinite (b[i])) ok = false; pk = std::max ({ pk, (double) std::abs (a[i]), (double) std::abs (b[i]) }); }
                }
            }
            check (ok && pk < 10.0, "random automation of every parameter at 6 sample rates: finite and bounded (peak)", pk);
        }
        // block size independence
        {
            double worst = 0; Vec in = noise (30000, 11);
            for (int m = 0; m < 2; ++m)
            {
                Settings s = m ? megS (0.6f, 7.0f, 0.3f) : optS (0.6f, 0.3f); Vec ref = run (in, s, SR, 512);
                for (int b : { 1, 7, 64, 333, 1024, 4096 }) worst = std::max (worst, maxDiff (run (in, s, SR, b), ref));
            }
            check (worst == 0.0, "identical output for host block sizes 1 ... 4096, both modes (max diff)", worst);
        }
        // long session
        {
            PrimesEngine e; e.prepare (SR); std::mt19937 rng (21); std::uniform_real_distribution<float> u (0, 1), nz (-1, 1);
            Vec a (4800), b (4800); bool ok = true; double pk = 0, dc = 0; const long long total = 30LL * 60 * 48000;   // 30 minutes of stereo audio
            Settings s;
            for (long long done = 0; done < total; done += 4800)
            {
                if ((done / 4800) % 100 == 0)
                { s.mode = (u (rng) > 0.5f) ? Mode::Megatron : Mode::Optimus; s.optDrive = u (rng); s.optTone = u (rng); s.megDrive = u (rng); s.megBits = 4 + 12 * u (rng); s.megRate = 0.01f + 0.99f * u (rng); s.mix = u (rng); }
                const float level = 0.05f + 0.9f * u (rng);
                for (size_t i = 0; i < 4800; ++i) { a[i] = level * nz (rng); b[i] = level * std::sin (0.07f * (float) (done + (long long) i)); }
                e.process (a.data(), b.data(), 4800, s);
                for (size_t i = 0; i < 4800; ++i) { if (! std::isfinite (a[i]) || ! std::isfinite (b[i])) ok = false; pk = std::max ({ pk, (double) std::abs (a[i]), (double) std::abs (b[i]) }); }
            }
            for (size_t i = 0; i < 4800; ++i) dc += b[i]; dc /= 4800.0; (void) dc;
            check (ok && pk < 10.0, "30 minutes of audio with changing settings: finite and bounded (peak)", pk);
        }
        // allocations + CPU
        for (int m = 0; m < 2; ++m)
        {
            PrimesEngine e; e.prepare (SR); Vec a = noise (512, 41), b = noise (512, 42); Settings s = m ? megS (0.7f, 6.0f, 0.4f) : optS (0.7f, 0.4f);
            e.process (a.data(), b.data(), 512, s);
            const int blocks = (int) (SR * 20 / 512); g_allocs = 0; g_count = true;
            const auto t0 = std::chrono::steady_clock::now();
            for (int i = 0; i < blocks; ++i) e.process (a.data(), b.data(), 512, s);
            const auto t1 = std::chrono::steady_clock::now(); g_count = false;
            const double pct = 100.0 * std::chrono::duration<double> (t1 - t0).count() / 20.0;
            char label[100]; std::snprintf (label, sizeof label, "%s stereo 48 kHz: heap allocations = 0, CPU %% of one core (this PC)", m ? "MEGATRON" : "OPTIMUS");
            check (g_allocs == 0 && pct < 5.0, label, pct);
        }
    }

    std::printf ("\nRESULT: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
