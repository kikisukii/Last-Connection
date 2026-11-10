"""Rebuild bundled UI-only OTF subsets from pinned Noto CJK sources.

python -m pip install fonttools==4.61.1
python scripts/subset_fonts.py --sources .downloads/fonts
SC.otf / JP.otf are the original Regular OTF files (see assets/fonts/README.md).
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
from fontTools import subset
from fontTools.ttLib import TTFont

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--sources', type=Path, required=True)
args = parser.parse_args()
strings = [json.loads(s) for s in re.findall(r'"(?:[^"\\]|\\.)*"',
    (root/'include/localization_data.inc').read_text(encoding='utf-8'))]
# Both regional fonts cover the full UI, including the three language buttons.
codepoints = set(range(32, 127)) | set(map(ord, ''.join(strings) + '←→'))
report = {}
for region in ('SC', 'JP'):
    source = args.sources/f'{region}.otf'
    font = TTFont(source)
    missing = codepoints - set(font.getBestCmap())
    assert not missing, sorted(missing)
    options = subset.Options()
    options.name_IDs = ['*']
    options.name_languages = ['*']
    sub = subset.Subsetter(options=options)
    sub.populate(unicodes=codepoints)
    sub.subset(font)
    family = f'Last Connection UI {region}'
    for name in font['name'].names:
        if name.nameID in (1, 3, 4, 6, 16):
            name.string = (family.replace(' ', '') if name.nameID == 6 else family).encode(name.getEncoding())
    cff = font['CFF '].cff
    cff.fontNames = [family.replace(' ', '')]
    cff.topDictIndex[0].FamilyName = family
    cff.topDictIndex[0].FullName = family
    output = root/f'assets/fonts/LC-UI-{region}.otf'
    font.save(output)
    report[region] = dict(bytes=output.stat().st_size, codepoints=len(codepoints),
        sha256=hashlib.sha256(output.read_bytes()).hexdigest(),
        source_sha256=hashlib.sha256(source.read_bytes()).hexdigest())
    print(region, report[region])
(root/'assets/fonts/manifest.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
