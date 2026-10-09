# Third-party source and notices

EQ-01 uses JUCE 7.0.12 under GPL-3.0-or-later. The exact framework source is available at https://github.com/juce-framework/JUCE/tree/4f43011b96eb0636104cb3e433894cda98243626. Build compatibility patches are in `Build/patch_juce.py` and `Build/patch_juce_macos.py`.

JUCE includes VST3 SDK components and other dependencies with their own notices in the framework source. Preserve those notices when distributing rebuilt source or binaries.

The editor's `Skin.h` and `Skin.cpp.inc` derive from the project's NoteShaper skin, also distributed under GPL-3.0-or-later. Release bundles include the project license and the corresponding source location in `Contents/Resources`.
