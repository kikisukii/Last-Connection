"""CI-only black-box browser smoke: pixels + keyboard + isolated browser storage.

No game state injection, time acceleration or Gameplay parameter overrides.
The same test runs against the packaged local site and the deployed Pages URL.
"""
import argparse
import io
import json
import time
from pathlib import Path
from urllib.parse import urljoin

from PIL import Image
from playwright.sync_api import sync_playwright

parser = argparse.ArgumentParser()
parser.add_argument('url')
parser.add_argument('--output', type=Path, default=Path('build/browser-evidence'))
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)


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
    for asset in ['index.js', 'index.wasm']:
        response = context.request.get(urljoin(args.url, asset))
        assert response.status == 200, (asset, response.status)
        if asset.endswith('.wasm'):
            assert response.body()[:4] == b'\0asm', 'Not a WASM module'
            assert 'application/wasm' in response.headers.get('content-type', ''), response.headers
    assert page.goto(args.url).status == 200
    page.locator('#status').filter(has_text='Prototype running').wait_for(timeout=60000)
    canvas = page.locator('#canvas')
    assert canvas.get_attribute('width') == '1360'
    assert page.evaluate("localStorage.getItem('last-connection-best-v1')") is None
    canvas.screenshot(path=str(args.output/'start.png'))
    canvas.press('ArrowLeft')  # start only
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
        final = Image.open(io.BytesIO(canvas.screenshot(path=str(args.output/'finish.png')))).convert('RGB')
        orange = sum(near_color(pixel,(255,161,0)) for pixel in final.crop((395,135,590,190)).getdata())
        assert orange > 50, 'Expected late F result panel'
        page.reload()
        page.locator('#status').filter(has_text='Prototype running').wait_for(timeout=60000)
        assert page.evaluate("localStorage.getItem('last-connection-best-v1')") == record, 'Best time did not persist'
        canvas.screenshot(path=str(args.output/'reloaded.png'))
        assert not errors, errors
        result = {'url':args.url,'fresh_context':True,'wasm_http':200,'keyboard_both_directions':True,
                  'bike_boarding_and_following':True,'finish_seconds':finish_time,'expected_rank':'F',
                  'result_panel_verified':True,'best_persisted_after_reload':True,'page_errors':errors}
        (args.output/'result.json').write_text(json.dumps(result,indent=2)+'\n')
        print(json.dumps(result,indent=2),flush=True)
    except Exception:
        canvas.screenshot(path=str(args.output/'failure.png'))
        (args.output/'errors.json').write_text(json.dumps(errors,indent=2))
        raise
    finally:
        browser.close()
