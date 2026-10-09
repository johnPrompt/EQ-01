from pathlib import Path
import sys
version, destination = sys.argv[1:]
Path(destination).write_text(f'''# EQ-01 {version}

Dynamic VST3 equalizer with 24 bands, resonance control, external sidechain and English/Russian interface. Development alpha.

## Downloads / Загрузки

- `EQ-01-{version}-win-vst3.zip` — Windows x64.
- `EQ-01-{version}-mac-vst3.zip` — macOS 11+, universal Intel / Apple Silicon.

Archives contain the complete EQ-01.vst3 plug-in bundle. / В архиве находится полный бандл EQ-01.vst3.

Installation: [English](https://github.com/alexeyPetrushin/EQ-01#download-and-install) · [Русский](https://github.com/alexeyPetrushin/EQ-01/blob/main/README.ru.md#скачать-и-установить).

Clean + Live: zero samples of audio-buffer latency. Spectral: 2048 samples. Colour adds a small oversampling delay. Output has no limiter. / Чистый + Живой: 0 отсчётов буферной задержки; Спектр: 2048; окраска добавляет небольшую задержку. Выходного лимитера нет.

Checks cover DSP, native VST3 loading/state, editor creation and latency. Full DAW session testing remains open. Mac builds have an ad-hoc signature and no Apple notarization.

GPL-3.0-or-later. Corresponding source: [v{version}](https://github.com/alexeyPetrushin/EQ-01/tree/v{version}).
''', encoding='utf-8')
