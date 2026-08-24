# UT99 Android Cyrillic localization

## v2.0.2 correction

v2.0.1 contained the CP1251 loader and the Android glyph atlas, but it did not
ship any `.rut` localization text. On an International/English data install this
meant the renderer never received Cyrillic characters. v2.0.2 fixes that
bootstrap gap while preserving real user-supplied Russian files.

The Android bootstrap now:

1. normalizes `.rut` filename case to the matching v400 package/map spelling;
2. detects an imported Russian data set before installing any fallback text;
3. restores `Language=rut` when an older Android build had forced such an
   imported Russian installation back to `int`;
4. installs eight small UTF-8 v400-compatible Russian compatibility seeds only
   when the corresponding file is missing (`Core`, `Engine`, `UWindow`, `UMenu`,
   `UTMenu`, `Botpack`, `UBrowser`, `UTBrowser`);
5. tracks only its own seed bytes with a versioned SHA-256 manifest, refreshes
   owned seeds on a seed-version update, and permanently yields ownership when
   a user replaces a seed with a real localization;
6. canonicalizes explicit `ru`/`rus`/`rut` language values to `rut` only while
   the required localization files and glyph atlas are available;
7. logs the active language, resource sizes, UTF-8/UTF-16 conversion and first
   fallback-glyph use for deterministic device diagnosis.

An ordinary English installation stays `Language=int`. The compatibility seeds
are not intended to replace a complete Russian translation; they guarantee a
safe v400-compatible central UI fallback and make the Cyrillic rendering path
testable without distributing a modern 469 localization wholesale.

## Encoding design

UT99 v400 is an 8-bit `TCHAR` engine. This implementation keeps that ABI.

- Legacy Windows-1251 `.rut` files remain byte-for-byte unchanged.
- Valid UTF-8 and UTF-16LE/BE `.rut` files are decoded to Windows-1251 only
  while being read.
- Other language/config files keep the original v400 loading behavior.
- When `Language=rut`, bytes `0x80..0xFF` are rendered through a compact CP1251
  glyph atlas; normal ASCII continues to use the original UT font.
- No global `TCHAR`/package/network serialization conversion is performed.

## Font atlas

`app/src/main/assets/ut99_patches/System/CyrillicFontAtlas.dat`

Binary layout (little-endian):

1. 8-byte magic `UTRUCP1\0`
2. `uint16 width, height, cellHeight, glyphCount`
3. 256 records of `uint16 U, V, W, H, Advance`
4. 512x512 8-bit grayscale pixels

Records `0..127` are regular CP1251 byte slots `0x80..0xFF`; records
`128..255` are bold. The atlas is scaled to the active stock font line height.
The original `UWindowFonts.utx` and `LadderFonts.utx` remain untouched.

The atlas is reproducible with `tools/generate_cyrillic_font_atlas.py`. The
source font family is under SIL OFL 1.1; the license notice is shipped as
`CyrillicFontAtlas.OFL.txt`. No font binary is redistributed.

## Compatibility seeds

The bundled seed files live under:

`app/src/main/assets/ut99_patches/RussianSeed/System/`

They are intentionally small and v400-oriented. For a deterministic visual
check under `Language=rut`, the main menu contains strings such as:

- `Игра`
- `Сетевая игра`
- `Настройки`
- `Статистика`
- `Справка`

A complete Russian UT99 installation can provide many more `.rut` files. Those
files always take precedence over the compatibility seeds.

## Device diagnostics

Useful log lines in `UT99Android.log`/logcat:

- `UT99_ANDROID_V202_CYRILLIC_STATUS language=... rutFiles=... atlas=...`
- `Android Cyrillic: active engine language=rut`
- `Android Cyrillic: decoded UTF-8 localization ... to CP1251`
- `Android Cyrillic: probe UMenu.GameName len=5 highBytes=4 firstBytes=26 C8 E3 F0 E0 `
- `Android Cyrillic: loaded CP1251 fallback atlas 512x512`
- `Android Cyrillic: first fallback glyph byte=0x.. language=rut font=...`

If `language=int`, no Cyrillic fallback glyph should be drawn. Under the bundled
compatibility seed, `Language=rut` must produce the exact probe bytes
`26 C8 E3 F0 E0` for `&Игра`. If that probe succeeds and the atlas/first-glyph
lines appear, the full localization-to-renderer path is active.

## Regression boundary

The Cyrillic fix does not modify the event/mover implementation. In particular,
`UnScript.cpp`, `UnActor.cpp`, `UnLevTic.cpp`, and `UnMover.cpp` remain
byte-identical to the previously tested trigger/event build.

## Release

UT99 Android 2.0.2 (`versionCode 10`).
