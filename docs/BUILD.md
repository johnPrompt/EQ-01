# Building EQ-01

Requires CMake 3.22+, C++17 and JUCE 7.0.12 pinned to commit `4f43011b96eb0636104cb3e433894cda98243626`. Apply the platform compatibility patch in `Build/` to a separate framework checkout.

```sh
git clone --depth 1 --branch 7.0.12 https://github.com/juce-framework/JUCE.git JUCE
python Build/patch_juce.py JUCE
cmake -S . -B build -DJUCE_PATH=<absolute-JUCE-path> -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target EQ1_VST3 EQ1DSPTest EQ1Preview EQ1HostCheck
```

For LLVM-MinGW select the MinGW Makefiles generator and its compiler/make tools. Non-ASCII Windows paths may require short paths with that toolchain. The local Windows binary uses LLVM-MinGW 20261006, Clang 23.1.3 and static C++ runtime linkage. Visual Studio is an alternative, untested build route for this artifact.

Build targets keep the internal identifier `EQ1`; the product and installable bundle are named `EQ-01`. Outputs are under `build/EQ1_artefacts/Release`. `EQ1_Standalone` is a development target excluded from public downloads.

## macOS

Apply `python3 Build/patch_juce_macos.py <JUCE-path>`, then configure with `-DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0`. The workflow builds both architectures, verifies their bundle and ad-hoc signature, and runs native DSP/host checks on Intel and Apple Silicon before publishing.

## Checks and packaging

Run `EQ1DSPTest`, `EQ1HostCheck <absolute-EQ-01.vst3-path>` and, on Windows, `EQ1Preview <absolute-screenshot-output-directory>`. Use `.exe` and ASCII-compatible arguments with LLVM-MinGW. Checks do not install plug-ins or change DAW settings.

DSP checks need no JUCE:

```sh
cmake -S . -B build-dsp -DEQ1_BUILD_PLUGIN=OFF
cmake --build build-dsp
ctest --test-dir build-dsp --output-on-failure
```

Package only after checks pass:

```sh
python Build/package_windows.py --build-dir build --output-dir dist
python3 Build/package_macos.py --build-dir build-macos --juce-dir <JUCE-path> --output-dir dist
```

Both packagers create a VST3-only archive with license/source notices and a SHA-256 file. macOS packaging requires macOS tools. VST3 manufacturer/plugin codes, parameter IDs and the legacy `CurveShaperState` root are preserved for saved sessions.
