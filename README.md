# HEX-Q PRIMES

Saturation / distortion plugin (VST3, Windows 64-bit) built with C++ / JUCE. Fourth product of the HEX-Q family.
Two **genuinely different DSP engines**: **OPTIMUS** (smooth saturation, red accent `#FF4D5A`) and **MEGATRON**
(distortion + bit crusher + sample-rate reduction, purple accent `#A78BFA`). The selected mode sets the accent of
the whole interface; the layout is the shared HEX-Q layout.

## Status - read this first

| Item | Status |
|---|---|
| DSP (`Source/DSP/PrimesEngine.h`) | **Compiled and tested offline** (g++, 36 / 36 checks pass, see "Test report") |
| JUCE plugin, editor, parameters | **Written but NOT compiled or tested yet** - the first build may show errors |
| Running inside FL Studio | **Not tested** |
| Listening tests (drums, synth notes, full mix) | **Not done** - I cannot listen. The numbers below are measurements, not opinions about sound. |
| Windows VST3 build | **Not built here** (no Windows compiler in this environment): it is built by the GitHub workflow included in the project |

## Build (no compiler on your PC needed)
1. Create a free GitHub account and a **new** repository (e.g. `HexQ-Primes`).
2. Upload **all files of this folder to the repository root** (`CMakeLists.txt` must be visible on the front page; keep the `.github` folder).
3. Actions tab -> **Build HEX-Q PRIMES (Windows)** (~10 min; also runs on every upload).
4. Download the artifact **HEX-Q-PRIMES-Windows**: `HEX-Q-PRIMES-Setup.exe` + a manual-install zip.

Local build: Windows 10/11, Visual Studio 2022 (Desktop C++), CMake 3.22+, Git:
```
cmake -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```
JUCE 8.0.4 is downloaded by CMake. Output: `build/HexQPrimes_artefacts/Release/VST3/HEX-Q PRIMES.vst3`

## Install in FL Studio
1. Run `HEX-Q-PRIMES-Setup.exe` (SmartScreen may warn because it is unsigned: More info -> Run anyway). Manual: copy the `HEX-Q PRIMES.vst3` folder to `C:\Program Files\Common Files\VST3\`.
2. FL Studio: Options -> Manage plugins -> Start scan.
3. Put **HEX-Q PRIMES** on a mixer track or bus (Effects slot).

Diagnostic trace: this build writes `%TEMP%\HEX-Q-PRIMES-log.txt` (Win+R, `%TEMP%`). If FL reports a crash, the last lines show where. It will be removed once the plugin is confirmed stable.

## Controls
- **Mode buttons**: OPTIMUS / MEGATRON. The selected mode defines the accent colour and which controls are shown.
- **OPTIMUS**: DRIVE 0-100 % (default 0), TONE 0-100 % (default 50).
- **MEGATRON**: DRIVE 0-100 % (default 25), BIT DEPTH 4-16 bit (default 8), SAMPLE RATE 1-100 % (default 50).
- **Output section (both modes)**: MIX 0-100 % (default 100), OUTPUT -18 .. +6 dB (default 0). These two are *global*: one value shared by both modes (the spec lists them in each mode and also calls them global; I implemented one shared pair so Mix/Output behave identically in both modes).
- Header: bypass, version. Every control: drag, wheel, click the number to type, double-click = default.
- **Presets**: the shared HEX-Q framework has no in-plugin preset system, so none was added. Use FL Studio's own plugin preset menu; project / preset state is saved by the host.
- The settings of the inactive mode are always kept: all 9 parameters are stored at all times (switching OPTIMUS <-> MEGATRON erases nothing).
- Parameter IDs (stable): `mode, bypass, mix, output, optDrive, optTone, megDrive, megBits, megRate`. All are host-automatable.

## DSP approach

### OPTIMUS - smooth saturation
1. **Drive** sets a pre-gain `g = 0.1 * 500^drive` (0.1 .. 50) in front of a `tanh` curve with a small asymmetry (`tanh(g*x + 0.25) - tanh(0.25)`): odd *and* even harmonics (warmth), subtle at low drive, strong at the top. At exactly 0 % the stage is skipped: the output is the input.
2. **Anti-aliasing without oversampling**: first-order antiderivative anti-aliasing (ADAA): the output is the *average of tanh over the step between two samples*, computed with `ln cosh`. No extra latency, one `log` + one `exp` per sample.
3. **DC blocker** (4 Hz high-pass) after the curve, so the asymmetry cannot leave a DC offset.
4. **Loudness matching**: the wet signal is scaled so that a -12 dBFS sine keeps the same RMS at every drive (a table computed in `prepare`, interpolated per sample). DRIVE therefore changes the *harmonic content*, not how loud the effect is at that reference level. Nothing is boosted to make it sound better. OUTPUT is yours to use.
5. **Tone**: a tilt EQ on the saturated signal around 800 Hz (+-6 dB on each side at the extremes, flat at 50 %). With Tone at 50 % this stage is bypassed exactly.

### MEGATRON - destructive
1. **Drive**: pre-gain `1 .. 100` (0 .. +40 dB) into an asymmetric **wave-folder** (triangle fold, period 4; the signal is reflected back instead of clipped) - a different non-linearity from OPTIMUS, with rich, inharmonic-sounding high-order content. At 0 % the stage is skipped.
2. **DC blocker** (4 Hz); the static offset of the fold is subtracted so silence stays exactly silent.
3. **Bit depth**: real quantisation, `round(x * 2^(bits-1)) / 2^(bits-1)`, mid-tread, **no dither**; fractional bit depths are allowed so it can be swept smoothly.
4. **Sample rate**: sample-and-hold with a fractional phase accumulator. The setting maps exponentially to a hold ratio 1/64 .. 1 (50 % = 1/8, i.e. 6 kHz at 48 kHz; 100 % = off). **There is no anti-alias filter** - the aliasing / images are the intended sound. Both channels sample at the same instants.
5. The same loudness matching as OPTIMUS is applied after the crusher stages (so the quantiser always sees a full-scale signal and BIT DEPTH means the same at every drive). The fold output is bounded to +-1 before this.

### Shared stage (both engines)
`out = (dry*(1-mix) + wet*mix) * outputGain`, bypass crossfade (10 ms smoothing on the parameters, 20 ms crossfade when the mode changes; the engine that is switched on starts from a clean state). Mix 0 % returns the dry signal bit-for-bit; Output 0 dB multiplies by exactly 1. Mono and stereo: the channel layout is preserved (mono -> mono, stereo -> stereo). A NaN / runaway guard silences any non-finite or absurd sample.

## Latency
Reported latency: **0 samples** (no look-ahead, no buffer). Strictly, OPTIMUS' ADAA averages two consecutive samples, i.e. its saturated path has a group delay of half a sample (about 10 us at 48 kHz); it is not compensated, and the dry/wet blend at Mix 50 % therefore carries a half-sample offset.

## Test report

### Verified offline - `tests/primes_dsp_test.cpp` (g++ 13, -O2): 36 / 36 passed
OPTIMUS
- Drive 0 %, tone 50 %: output identical to input (max diff 0).
- 1 kHz sine at -12 dBFS: THD 0.57 % at 10 % drive, 1.4 % at 25 %, 6.4 % at 50 %, 23 % at 75 %, 40 % at 100 % (grows with every step); the 2nd harmonic is present from the lowest settings (about -45 dB at 10 %). Level change of that sine at any drive: at most 0.03 dB. Noise at about -18 dBFS RMS: at most +3 dB (the matching is done at -12 dBFS, so quieter material is lifted a little at high drive).
- Tone: dark = +5.9 dB at 100 Hz / -4.5 dB at 10 kHz; bright = -5.2 dB / +5.6 dB; 50 % = 0.0000 dB.
- DC offset with a 100 Hz sine at max drive: below 0.002. Full-scale square and noise at 100 % drive: finite, bounded.
- Mix 0 % = dry bit-for-bit; Mix 50 % = half dry + half wet (error < 1e-6); Output +6 dB gives +6.00 dB.
- Aliasing (energy that is not a harmonic of the input, after the DC blocker settled; ADAA vs plain tanh): at 25 / 50 % drive about -90 to -72 dB for both at 1.5 / 3.5 / 5 kHz tones; at 75 % drive -29 dB vs -23 dB (5 kHz); at 100 % -19 dB vs -12 dB (5 kHz). So ADAA lowers aliasing by roughly 6 dB, but **at high drive on bright, loud material aliasing is still audible**.

MEGATRON
- Drive 0, 16 bit, 100 % rate: practically clean (max diff < 1e-4).
- Bit depth is real quantisation: 4 bit -> 17 distinct output levels, 8 bit -> 245; going from 8 to 12 bit improves the signal-to-quantisation-noise ratio by 23.98 dB (theory: 24 dB).
- Sample rate: 100 % = a new value every sample, 50 % = 6000 new values per second (1/8), 1 % = 780 per second (1/64 = 750).
- Distinct signatures: images of a 1.1 kHz sine around the 6 kHz hold rate: -12.8 dB (sample-rate reduction only) vs -51.9 dB (bit crush only).
- Different from OPTIMUS: at max drive, energy above 10 kHz is 19 dB higher in MEGATRON; RMS of the difference between the two outputs is 1.4 x the OPTIMUS output.
- Mix 0 % = dry bit-for-bit, Mix 50 % exact; max drive / 4 bit / 1 % rate on full-scale noise: finite and bounded (peak 1.0 before output gain).

General
- Silence in -> exactly silence out (both engines). Mono processing identical to stereo channels.
- All parameters swept during playback (both modes): finite and bounded; OPTIMUS drive sweep has no larger steps than a fixed 100 % drive.
- Switching mode every 5 ms and bypass on/off every 0.1 s: no burst, no step beyond the steady-state maximum; bypassed output is identical to the input after the fade.
- Random automation of every parameter at 22.05 - 192 kHz: finite and bounded. Identical output for host block sizes 1 ... 4096.
- **30 minutes of audio** (changing settings every 100 ms-blocks, random levels): finite and bounded.
- **0 heap allocations** while processing; CPU for stereo at 48 kHz: OPTIMUS about 0.6 %, MEGATRON about 0.15 % of one core (this machine, rough indication).

### Still to be tested on your machine (not tested)
- [ ] The CI build succeeds (JUCE compile)
- [ ] Loads in FL Studio, window opens, no crash on open / close / re-open; accent changes with the mode
- [ ] Mono and stereo tracks, buses, several instances
- [ ] Host automation of every control, including Mode
- [ ] Save the project, reopen: mode and all 9 values restored (including the inactive mode)
- [ ] Bypass and mode switching in FL: no clicks
- [ ] Listening: silence, low and high sine, drum transients, sustained synth notes, a full mix; check that OPTIMUS stays controlled at low drive and that MEGATRON is clearly a different, harsher character
- [ ] Long playback session in FL

## Decisions that are yours
- **Oversampling for OPTIMUS** was *not* added (the spec says: only if measurements show it is necessary). The measurements show ADAA is enough up to about 50 % drive, and that at 75-100 % drive aliasing of bright, loud material is audible (numbers above). If you hear it, 2x oversampling is the option - it costs CPU and adds latency, so I will not add it without asking.
- **Global Mix / Output** (one shared pair) as described above.

## Architecture
```
Source/
  DSP/         PrimesEngine.h               both engines + shared stage, pure C++, no JUCE, no allocation (unit-tested)
  Parameters/  PrimesParams.*               IDs, ranges, defaults, text formatting
  HexQ/        shared design system         theme, look-and-feel, header, footer, mode button
  PluginProcessor.*  PluginEditor.*
tests/primes_dsp_test.cpp
```
**Shared module change**: `Source/HexQ/` is the VANTA / ORBIT / ENTROPY folder plus two *additive* extensions needed for a mode-dependent accent: `HexQLookAndFeel::setPalette()` / `HexQHeader::setPalette()` / `HexQFooter::setPalette()` (change the palette at run time) and a new `HexQModeButton.h`. Nothing existing was changed, so the other three plugins keep working with their older copy; copy this folder over theirs if you want them to have the new pieces too.
