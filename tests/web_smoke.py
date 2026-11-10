"""CI-only black-box browser smoke: pixels + keyboard + isolated browser storage.

No game state injection, time acceleration or Gameplay parameter overrides.
The same test runs against the packaged local site and the deployed Pages URL.
"""
import argparse
import io
import json
import time
import re
from pathlib import Path
from urllib.parse import urljoin

from PIL import Image
from playwright.sync_api import sync_playwright

parser = argparse.ArgumentParser()
parser.add_argument('url')
parser.add_argument('--output', type=Path, default=Path('build/browser-evidence'))
parser.add_argument('--language', choices=['zh','en','ja'], default='en')
parser.add_argument('--ui-only', action='store_true', help='Only onboarding/layout/input checks')
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
index={'zh':0,'en':1,'ja':2}[args.language]
tag=['zh-CN','en','ja'][index]
copy={}
for line in (Path(__file__).resolve().parents[1]/'include/localization_data.inc').read_text(encoding='utf-8').splitlines():
    if line.startswith('LC_TEXT('):
        values=[json.loads(v) for v in re.findall(r'"(?:[^"\\]|\\.)*"',line)]
        copy[values[0]]=values[index+1]



def near_color(pixel, rgb):
    return all(abs(pixel[i]-rgb[i]) <= 5 for i in range(3))


def bike_center(im, lane):
    x = 138 if lane == 'left' else 448
    rows = [y for y in range(113, 469) if near_color(im.getpixel((x, y)), (90, 203, 221))]
    if len(rows) < 30 or not 35 <= max(rows)-min(rows) <= 42:
        return None
    return (min(rows)+max(rows))/2


with sync_playwright() as p:
    browser = p.chromium.launch(headless=True)
    # Fresh non-persistent context: no cached app or personal browser state.
    context = browser.new_context(viewport={'width': 1400, 'height': 820}, device_scale_factor=1)
    page = context.new_page()
    errors = []
    page.on('pageerror', lambda error: errors.append(str(error)))
    for asset in ['index.js', 'index.wasm', 'index.data', 'assets/fonts/LC-UI-SC.otf', 'assets/fonts/LC-UI-JP.otf']:
        response = context.request.get(urljoin(args.url, asset))
        assert response.status == 200, (asset, response.status)
        if asset.endswith('.wasm'):
            assert response.body()[:4] == b'\0asm', 'Not a WASM module'
            assert 'application/wasm' in response.headers.get('content-type', ''), response.headers
    assert page.goto(args.url).status == 200
    page.locator('body[data-ready="true"]').wait_for(timeout=60000)
    canvas = page.locator('#canvas')
    assert canvas.get_attribute('width') == '1360'
    assert page.evaluate("localStorage.getItem('last-connection-best-v1')") is None
    page.locator('body[data-ready="true"]').wait_for()
    def fit_check(menu):
        page.wait_for_function("""selector => {const r=document.querySelector(selector+' .sheet').getBoundingClientRect();
            return r.top>=0 && r.bottom<=innerHeight+1 && r.left>=0 && r.right<=innerWidth+1;}""",arg=menu)
        for button in page.locator(menu+' button:visible').all():
            box=button.bounding_box()
            viewport=page.viewport_size
            assert box['x']>=0 and box['y']>=0 and box['x']+box['width']<=viewport['width']+1 and box['y']+box['height']<=viewport['height']+1
    for width,height in [(320,568),(640,360),(1400,820)]:
        page.set_viewport_size({'width':width,'height':height})
        fit_check('#language')
    def select_language():
        page.locator('body[data-screen="language"][data-ready="true"]').wait_for(timeout=60000)
        assert not page.locator('#onboarding').is_visible()
        canvas.press('ArrowLeft'); page.wait_for_timeout(50)
        assert page.locator('body').get_attribute('data-screen')=='language'
        page.locator(f'[data-language="{index}"]').click()
        page.locator('body[data-screen="start"]').wait_for()
        assert page.locator('html').get_attribute('lang')==tag
        page.evaluate('document.fonts.ready')
        family='LC-JP' if args.language=='ja' else 'LC-SC'
        assert page.evaluate('(family)=>document.fonts.check("16px "+family)',family)
    page.screenshot(path=str(args.output/'language-selection.png'))
    select_language()
    assert page.locator('#onboarding').is_visible() and not page.locator('#results').is_visible()
    assert page.locator('#start').inner_text() == copy['start']
    expected=[copy[key] for key in ['how','goal','controls','run','vehicles','predict','early','aim','automatic','recover','temporary','once','next','deadline','rank','rankHint']]
    intro = page.locator('#onboarding').inner_text()
    assert all(' '.join(line.split()) in ' '.join(intro.split()) for line in expected), intro
    assert not any(word in intro for word in ['DEBUG', 'Vehicle ID', 'Service Distance', '90'])
    tested_sizes = [(1400,820),(1024,600),(800,600),(640,480),(640,360),(390,844),(360,640),(320,568)]
    for width,height in tested_sizes:
        page.set_viewport_size({'width':width,'height':height})
        page.wait_for_function("""() => { const r=document.querySelector('#onboarding .sheet').getBoundingClientRect();
            return r.top>=0 && r.bottom<=innerHeight+1 && r.left>=0 && r.right<=innerWidth+1; }""",timeout=5000)
        layout = page.evaluate("""() => {
            const sheet=document.querySelector('#onboarding .sheet').getBoundingClientRect();
            const nodes=[...document.querySelectorAll('#onboarding [data-copy], #start')];
            return {sheet:{top:sheet.top,bottom:sheet.bottom},
                scroll:document.documentElement.scrollHeight>innerHeight || document.documentElement.scrollWidth>innerWidth,
                overflow:nodes.filter(e=>{const r=e.getBoundingClientRect();return r.top<0||r.bottom>innerHeight+1||r.left<0||r.right>innerWidth+1||e.scrollWidth>e.clientWidth+1;}).map(e=>e.textContent)};
        }""")
        page.screenshot(path=str(args.output/f'start-{width}x{height}.png'))
        assert not layout['scroll'] and not layout['overflow'] and layout['sheet']['top']>=0 and layout['sheet']['bottom']<=height+1, (width,height,layout)
    page.set_viewport_size({'width':1400,'height':820})
    page.screenshot(path=str(args.output/'start.png'))
    # Each menu start route consumes the input; no initial side-lane move.
    for route in ['ArrowLeft','ArrowRight','click']:
        if route=='click': page.get_by_role('button',name=copy['start'],exact=True).click()
        else: canvas.press(route)
        page.locator('body[data-screen="playing"]').wait_for()
        page.wait_for_timeout(500)
        im=Image.open(io.BytesIO(canvas.screenshot())).convert('RGB')
        assert near_color(im.getpixel((340,370)),(104,226,180)), ('Start also moved player',route)
        assert not page.locator('#onboarding').is_visible()
        page.reload(); select_language()
    canvas.press('ArrowLeft')  # final start only
    page.locator('body[data-screen="playing"]').wait_for()
    def debug_pixels():
        im=Image.open(io.BytesIO(canvas.screenshot())).convert('RGB')
        return sum(near_color(pixel,(104,226,180)) for pixel in im.crop((600,90,850,110)).get_flattened_data())
    assert debug_pixels()==0, 'Debug visible by default'
    page.evaluate("""() => { window.addEventListener('keydown', event => {
        window.testF1DefaultPrevented = event.key==='F1' && event.defaultPrevented;
    }, {once:true}); }""")
    canvas.press('F1'); page.wait_for_timeout(60)
    assert page.evaluate('window.testF1DefaultPrevented'), 'F1 may open browser help'
    assert debug_pixels()>20, 'F1 did not enable debug'
    canvas.press('F1'); page.wait_for_timeout(60)
    assert debug_pixels()==0, 'F1 did not hide debug'
    if args.ui_only:
        assert not errors, errors
        print(json.dumps({'onboarding':True,'responsive_sizes':tested_sizes,'start_routes':3,'debug_opt_in':True,'page_errors':errors}),flush=True)
        browser.close()
        raise SystemExit(0)
    started = time.monotonic()
    lane = None
    try:
        while time.monotonic()-started < 60:
            im = Image.open(io.BytesIO(canvas.screenshot())).convert('RGB')
            for candidate in ['left', 'right']:
                center = bike_center(im, candidate)
                if center is not None and 390 <= center <= 415:
                    lane = candidate
                    canvas.press('ArrowLeft' if lane == 'left' else 'ArrowRight')
                    break
            if lane:
                break
            page.wait_for_timeout(50)
        assert lane, 'No visible Bike boarding opportunity within timeout'
        page.wait_for_timeout(900)
        first = Image.open(io.BytesIO(canvas.screenshot(path=str(args.output/'boarding.png')))).convert('RGB')
        x = 185 if lane == 'left' else 495
        assert near_color(first.getpixel((x, 370)), (104, 226, 180)), 'Lateral input did not reach lane'
        center1 = bike_center(first, lane)
        page.wait_for_timeout(700)
        second = Image.open(io.BytesIO(canvas.screenshot())).convert('RGB')
        center2 = bike_center(second, lane)
        assert center1 is not None and center2 is not None and abs(center2-center1) <= 2, 'Rider did not follow Bike velocity'
        assert 380 <= center2 <= 386, ('Unexpected catch offset', center2)
        canvas.press('ArrowRight' if lane == 'left' else 'ArrowLeft')
        page.wait_for_timeout(800)
        center = Image.open(io.BytesIO(canvas.screenshot(path=str(args.output/'released.png')))).convert('RGB')
        assert near_color(center.getpixel((340,370)),(104,226,180)), 'Return input did not reach CENTER'
        # Completion writes a best record; no source hooks or artificial clocks.
        page.wait_for_function("localStorage.getItem('last-connection-best-v1') !== null", timeout=300000)
        record = page.evaluate("localStorage.getItem('last-connection-best-v1')")
        finish_time = float(record.splitlines()[1])
        assert 90 < finish_time < 110, finish_time  # one short Bike ride, then run
        page.locator('body[data-screen="results"]').wait_for()
        assert page.locator('#result-rank').inner_text()==copy['rankLabel']+' F'
        assert page.locator('#result-headline').inner_text()==copy['missed']
        assert page.locator('html').get_attribute('lang')==tag
        for width,height in [(320,568),(640,360),(1400,820)]:
            page.set_viewport_size({'width':width,'height':height})
            fit_check('#results')
        page.screenshot(path=str(args.output/'finish.png'))
        shown_time=page.locator('#result-time').inner_text()
        page.get_by_role('button',name=copy['how'],exact=True).click()
        page.locator('body[data-screen="help"]').wait_for()
        assert page.locator('#onboarding').is_visible()
        for width,height in tested_sizes:
            page.set_viewport_size({'width':width,'height':height})
            page.wait_for_function("""() => { const r=document.querySelector('#onboarding .sheet').getBoundingClientRect();
                return r.top>=0 && r.bottom<=innerHeight+1 && r.left>=0 && r.right<=innerWidth+1; }""",timeout=5000)
            for selector in ['#onboarding .sheet' ,'#start','#back']:
                rect=page.locator(selector).bounding_box()
                assert rect['x']>=0 and rect['y']>=0 and rect['x']+rect['width']<=width+1 and rect['y']+rect['height']<=height+1, ('help',width,height,selector,rect)
        page.set_viewport_size({'width':1400,'height':820})
        page.get_by_role('button',name=copy['back'] ,exact=True).click()
        page.locator('body[data-screen="results"]').wait_for()
        assert page.locator('#result-time').inner_text()==shown_time
        page.get_by_role('button',name=copy['again'],exact=True).click()
        page.locator('body[data-screen="playing"]').wait_for()
        page.wait_for_timeout(500)
        restart=Image.open(io.BytesIO(canvas.screenshot(path=str(args.output/'restart.png')))).convert('RGB')
        assert page.locator('html').get_attribute('lang')==tag
        assert not page.locator('#onboarding').is_visible(), 'Restart forced tutorial'
        assert near_color(restart.getpixel((340,370)),(104,226,180)), 'Restart triggered transfer'
        page.reload()
        page.locator('body[data-ready="true"]').wait_for(timeout=60000)
        assert page.locator('body').get_attribute('data-screen')=='language'
        assert page.locator('#language').is_visible() and not page.locator('#onboarding').is_visible()
        assert page.evaluate("localStorage.getItem('last-connection-best-v1')") == record, 'Best time did not persist'
        page.locator('body[data-ready="true"]').wait_for()
        page.screenshot(path=str(args.output/'reloaded.png'))
        assert not errors, errors
        result = {'url':args.url,'language':args.language,'explicit_selection':True,'reload_resets_language':True,'bundled_font_loaded':True,'fresh_context':True,'wasm_http':200,'keyboard_both_directions':True,
                  'bike_boarding_and_following':True,'finish_seconds':finish_time,'expected_rank':'F',
                  'result_panel_verified':True,'best_persisted_after_reload':True,'page_errors':errors,
                  'onboarding':True,'responsive_sizes':tested_sizes,'start_routes':3,'debug_opt_in':True,
                  'optional_help_and_back':True,'restart_without_tutorial_or_transfer':True}
        (args.output/'result.json').write_text(json.dumps(result,indent=2)+'\n')
        print(json.dumps(result,indent=2),flush=True)
    except Exception:
        page.screenshot(path=str(args.output/'failure.png'))
        (args.output/'errors.json').write_text(json.dumps(errors,indent=2))
        raise
    finally:
        browser.close()
