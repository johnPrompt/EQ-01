"""Create the public Windows VST3-only download from a tested build."""
import argparse
import hashlib
from pathlib import Path
import re
import shutil
import zipfile

parser = argparse.ArgumentParser()
parser.add_argument('--build-dir', required=True, type=Path)
parser.add_argument('--output-dir', required=True, type=Path)
args = parser.parse_args()
source = Path(__file__).resolve().parent.parent
version = re.search(r'project\(EQ1 VERSION ([\d.]+)', (source / 'CMakeLists.txt').read_text()).group(1)
bundle = args.build_dir.resolve() / 'EQ1_artefacts/Release/VST3/EQ-11.vst3'
assert (bundle / 'Contents/x86_64-win/EQ-11.vst3').is_file(), 'Missing built Windows VST3'
output = args.output_dir.resolve()
output.mkdir(parents=True, exist_ok=True)
package = output / 'EQ-11.vst3'
shutil.copytree(bundle, package, dirs_exist_ok=True)
resources = package / 'Contents/Resources'
resources.mkdir(parents=True, exist_ok=True)
shutil.copy2(source / 'LICENSE.txt', resources / 'LICENSE.txt')
(resources / 'SOURCE.txt').write_text(
    f'EQ-11 {version} — Windows x64\nLicense: GPL-3.0-or-later\n'
    f'Corresponding source: https://github.com/alexeyPetrushin/EQ-11/tree/v{version}\n'
    'JUCE 7.0.12 source: https://github.com/juce-framework/JUCE/archive/4f43011b96eb0636104cb3e433894cda98243626.zip\n'
    'JUCE compatibility patch: Build/patch_juce.py in the EQ-11 source.\n', encoding='utf-8')
archive = output / f'EQ-11-{version}-win-vst3.zip'
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as z:
    for path in sorted(package.rglob('*')):
        if path.is_file():
            z.write(path, path.relative_to(output).as_posix())
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None, 'Archive corruption'
    assert all(p.startswith('EQ-11.vst3/') for p in z.namelist()), 'Unexpected public archive contents'
checksum = hashlib.sha256(archive.read_bytes()).hexdigest()
(output / f'EQ-11-{version}-win-SHA256.txt').write_text(f'{checksum}  {archive.name}\n', encoding='utf-8')
print(f'Created {archive.name}: {archive.stat().st_size / 1048576:.2f} MB; SHA-256 {checksum}')
