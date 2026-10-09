# Validation scope — 0.3.3

The logs in `checks/` refer to the packaged source and binaries. These are measurements and native interaction checks, not an Ableton certification or a blind comparison with commercial products.

## DSP

Flat identity, +6 dB bell, 48 dB/oct cut attenuation, all-pass magnitude, M/S isolation, above/below/knee dynamics, external trigger and relative gain invariance. STFT identity maximum error: 8.94e-8, exact 2048-sample delay, stereo preservation and selective two-tone suppression.

Causal resonance bank: approximately -8.05 dB at the target 1.5 kHz tone, -0.00023 dB at the distant 9 kHz tone in the test configuration. Complex projections include minimum-phase rotation. It emits no pre-impulse audio, passes the impulse without audio buffering, preserves linked stereo and side-only centre isolation, accepts missing sidechain without above-threshold attenuation, retains relative gain invariance and static EQ gain. These numbers describe one configured test, not every music signal or resolution.

24 conventional bands are stress-tested under frequency/gain automation at 44.1/48/96/192 kHz. A topology change checks the old/new 8 ms transition path and first-sample continuity. Rapid repeated transitions and adaptive resonance parameter stress on music remain future release gates.

The DSP harness reports elapsed calculation time for 1/8/24 causal resonance bands at 48 kHz stereo with 128-sample updates. Read actual timings in DSP.txt. Timing uses a steady wall clock, includes scheduling effects, and excludes the complete DAW, GUI and driver. It does not guarantee an audio deadline or represent the DAW CPU meter.

## Actual VST3

The separate host scans and loads the real binary, checks mono/stereo, parameter/state recall, native editor creation, empty blocks and bypass. It measures +6 dB at 1 kHz and attenuation driven by an enabled external SC bus.

Clean Live reports **0 samples** and its flat impulse is exact at 44.1/48/96/192 kHz with declared blocks of 32/128/512 samples. A resonance band is measured active in Live while latency remains zero. Clean Spectral reports and measures **2048 samples**.

Colour reports **4 samples** at 48 kHz; the test impulse peak occurs at **5 samples**. IIR oversampling has frequency-dependent phase/group delay, so the nominal latency-compensation number is not a pure linear-phase impulse shift. The measured numbers are printed explicitly. Clean has no oversampling stage.

The host must respect its declared VST3 maximum block size. JUCE 7's wrapper has fixed buffers outside our processor; deliberately exceeding that host contract is unsupported. Native processing separately validates bounded segmentation of a 4096-sample buffer prepared for 128.

## Native editor

Actual C++ snapshots at 1080×680 and 820×560, RU/EN, visible language toggle, preset dropdown, actual shape/settings/source popup windows, curved undo/redo icons, no rotary controls or empty slots. The graph exceeds 80% at default size and 75% at minimum. Single click selects without opening parameters or recording an undo/host gesture. New-band creation and direct dragging stay free of an automatic inspector. Double-click an existing point opens parameters. Its position stays fixed through frequency/gain changes, graph drags and refresh; dragging its title moves it. X/Escape close it without losing selection; another band closes it. Reopening the same selected band retains its manual position. Window resizing and expanded controls remain within the tested bounds. This is a native offscreen interaction harness, not a DAW usability study.

Region-based Bell/Shelf/Cut creation, no duplicate double-click, unchanged gain when adding on the existing curve, one-step creation undo, cut-wheel slope and relevant cut fields are exercised. Timing/detector disclosures retain primary range/threshold positions. Numeric label events exercise horizontal range and vertical threshold scrubbing with external SC connected/disconnected, Shift fine control, exactly one host begin/end and grouped values, one-step undo and pending comma/kHz text entry across refresh. The bottom bar and advanced settings are exercised. A routed stereo SC with opposite polarity verifies the displayed 1.2 kHz peak, distant-bin rejection and unchanged zero Live latency; the overlay appears only in an explicitly opened detector context or audition. Temporary audition is tested.

Numeric entry, M/S, clipboard, same-process peer copying, project restore, schema-1 missing-density migration, a real Sketch gesture and keyboard undo are exercised. Audition measurements confirm the selected band is isolated and missing external detector input is silent.

## Still unvalidated

Broad music corpus, level-matched blind comparison, long DAW sessions, in-play engine/colour latency reconfiguration, complete alias/phase specification, screen readers and Windows HiDPI matrix, surround, AAX/CLAP binaries, remote instance editing and intelligent balancing. These remain documented competitive gaps.

## EQ-11 release checks

Windows x64 build 0.3.3 passed DSP, native VST3 and native editor interaction checks on 2026-10-10. The VST3 scan explicitly checks the product name EQ-11. Clean Live remains 0 samples, Clean Spectral 2048 samples and colour 4 reported samples at 48 kHz. Source, parameter IDs, VST3 codes and state schema preserve predecessor compatibility. New preset files use `.eq11preset`; legacy `.cspreset` files remain readable.

macOS release checks are performed by `.github/workflows/macos.yml`: universal architecture/signature verification and native DSP/VST3 tests on Intel and Apple Silicon. Refer to the release's workflow result for completion and logs. Full Ableton/other DAW session testing remains outstanding.
