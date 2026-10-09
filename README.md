# EQ-11

[Русский](README.ru.md) · [Download VST3](https://github.com/alexeyPetrushin/EQ-11/releases) · [User guide](docs/USER_GUIDE.en.md)

EQ-11 is a dynamic equalizer with 24 bands, resonance control and a graph-first interface. Move a point to shape the sound; double-click it when you need detailed parameters. The interface supports English and Russian.


## Download and install

The [Releases page](https://github.com/alexeyPetrushin/EQ-11/releases) contains VST3 downloads. Choose the archive for your operating system: `win` for Windows x64, `mac` for macOS Intel and Apple Silicon. Each archive contains the complete `EQ-11.vst3` bundle.

**Windows:** extract the archive and copy the entire `EQ-11.vst3` folder to `%CommonProgramFiles%\VST3`. This uses the actual system location, even if Windows is installed on a different drive. Installing there may require administrator access. Hosts that support per-user VST3 locations can use `%LOCALAPPDATA%\Programs\Common\VST3`; otherwise choose a custom VST3 folder in your DAW. Rescan plug-ins and add **Shaper Audio / EQ-11** to an audio track.

**macOS:** extract the Mac archive and copy the whole `EQ-11.vst3` bundle to `~/Library/Audio/Plug-Ins/VST3` for your account or `/Library/Audio/Plug-Ins/VST3` for all users. Rescan plug-ins. The universal build supports macOS 11 or newer. It uses an ad-hoc signature and has no Apple notarization; macOS may require approval in Privacy & Security before a host can load it.

In Ableton Live, enable VST3 in the plug-in settings and rescan. Other hosts need VST3 support and the matching platform architecture. Close the DAW before replacing an installed build. EQ-11 retains its development predecessor's VST3 identity so saved sessions can continue using their parameters; install only one version of that plug-in identity.

## Features

- 24 bands; Bell, shelves, cuts, Notch, Band-pass, All-pass and Tilt. Cuts have 12–96 dB/oct steps.
- Per-band Stereo, Left, Right, Mid and Side processing.
- Dynamic EQ with above/below ranges, knee, ratio, attack/release and relative detection.
- Independent detector frequency/Q, external mono/stereo sidechain and detector audition.
- Resonance control with a causal filter bank or the optional STFT engine.
- Pre/post spectrum, contextual sidechain spectrum, freeze, band audition and difference monitoring.
- Presets, automation, undo/redo, A/B snapshots, band copying, EQ sketching and same-process instance copying.
- Optional saturation with 2× oversampling; English/Russian interface.

## Latency and release status

Version 0.3.3 is a development alpha. **Clean + Live adds zero samples of audio-buffer latency**, including dynamic EQ and causal resonance control. Filters still change phase and detectors have a response time. The Spectral engine adds **2048 samples** (42.67 ms at 48 kHz); conventional EQ filters remain minimum phase. Colour processing adds a small oversampling delay. Change engine or colour while playback is stopped so the host can update delay compensation.

Native checks cover VST3 loading, automation/state, mono/stereo, external sidechain, editor creation, latency and signal processing. Listening comparisons and full DAW session testing remain open. EQ Match, conventional linear-phase EQ, automatic gain compensation, masking analysis and remote instance editing are not implemented. Output has no limiter.

See the [guide](docs/USER_GUIDE.en.md), [validation record](docs/VALIDATION.md) and [build instructions](docs/BUILD.md). Public download archives contain the plug-in; source and development tools are in this repository.

## Support

[DonationAlerts](https://www.donationalerts.com/r/lesha_diabet_)

## License

GPL-3.0-or-later; see [LICENSE.txt](LICENSE.txt) and [dependency notices](ThirdParty/README.md). JUCE 7.0.12 is pinned to commit `4f43011b96eb0636104cb3e433894cda98243626`; compatibility patches are in `Build/`.
