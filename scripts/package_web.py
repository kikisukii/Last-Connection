"""Package only browser runtime files; shared by local and GitHub Actions builds."""
import argparse
from pathlib import Path
import re
import shutil

parser = argparse.ArgumentParser()
parser.add_argument('--build', type=Path, default=Path('build/web'))
parser.add_argument('--output', type=Path, default=Path('build/pages'))
args = parser.parse_args()
required = ['index.html', 'index.js', 'index.wasm', 'index.data']
for name in required + ['assets/fonts/LC-UI-SC.otf', 'assets/fonts/LC-UI-JP.otf', 'assets/fonts/OFL.txt']:
    if not (args.build / name).is_file():
        raise SystemExit('Missing runtime file: ' + name)
args.output.mkdir(parents=True, exist_ok=True)
for name in required:
    shutil.copy2(args.build / name, args.output / name)
for pattern in ['index.data', 'index.worker.js']:
    for source in args.build.glob(pattern):
        shutil.copy2(source, args.output / source.name)
if (args.build / 'assets').is_dir():
    shutil.copytree(args.build / 'assets', args.output / 'assets', dirs_exist_ok=True)
html = (args.output / 'index.html').read_text(encoding='utf-8')
if re.search(r'(?:src|href)=[\"\']/', html):
    raise SystemExit('Root-relative URL would break the GitHub project site')
js = (args.output / 'index.js').read_text(encoding='utf-8')
if 'index.wasm' not in js or re.search(r'[\"\']/index\.wasm', js):
    raise SystemExit('Expected relative index.wasm loader')
(args.output / '.nojekyll').touch()
print('Packaged static site: ' + ', '.join(p.name for p in args.output.iterdir()))
