# Bundled UI fonts

Derived from **Noto Sans CJK SC Regular** and **Noto Sans CJK JP Regular**,
copyright © 2014–2021 Adobe (http://www.adobe.com/), distributed under the
**SIL Open Font License 1.1**. The complete license is [OFL.txt](OFL.txt);
copyright and license metadata are retained inside both fonts.

Official source: [notofonts/noto-cjk](https://github.com/notofonts/noto-cjk),
pinned revision `f8d157532fbfaeda587e826d4cd5b21a49186f7c`:

- `Sans/OTF/SimplifiedChinese/NotoSansCJKsc-Regular.otf`
- `Sans/OTF/Japanese/NotoSansCJKjp-Regular.otf`
- `Sans/LICENSE`

These are actual glyph subsets, renamed **Last Connection UI SC / JP** to
distinguish the modified fonts. SC supplies the Chinese/English UI; JP supplies
Japanese regional glyphs. Each includes all **380** codepoints used by the
central UI table, printable ASCII and arrow controls, including all language
button names. No system CJK fonts or external font service is required.

| File | Bytes |
| --- | ---: |
| LC-UI-SC.otf | 127,308 |
| LC-UI-JP.otf | 127,660 |
| Total | 254,968 (249.0 KiB) |

`manifest.json` records original/subset SHA-256 hashes. Full source fonts total
32.9 MB and are **not** shipped. Web ships the same subsets as CSS font assets
and inside the Emscripten preload data for raylib canvas text (about 498 KiB of
font bytes before HTTP compression). Native loads the bundled OTF files.

To regenerate after adding UI characters, obtain the pinned originals into an
ignored source directory as `SC.otf` and `JP.otf`, install `fonttools==4.61.1`,
and run `python scripts/subset_fonts.py --sources <directory>`.
The script validates source glyph coverage and retains OFL metadata.
