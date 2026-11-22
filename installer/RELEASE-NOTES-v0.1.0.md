# TOiRE-76 v0.1.0

A 1176-flavoured FET compressor for Windows (VST3, 64-bit), with a Blender-rendered
panel and a WebView2 UI.

## Highlights

- **FET-style compression** with the classic four ratio steps plus ALL-buttons mode
- **Input / Output** gain staging, **Attack / Release** with program-dependent behaviour
- **VU meter** with GR / +8 / +4 / OFF modes; the needle follows real gain reduction
- **Preset bar**: six factory presets, plus save / recall of your own presets
  (`%APPDATA%\TOiRE-76\Presets\*.t76preset`)
- Fixed **86 samples** reporting latency at 48 kHz (internal 2–4× oversampling)

## Install

1. Run `TOIRE-76-0.1.0-win64.exe` (administrator required — it installs into the
   shared VST3 folder).
2. The installer places the plugin at
   `C:\Program Files\Common Files\VST3\TOiRE-76.vst3\`.
3. If the WebView2 runtime is missing it is installed automatically (Windows 11 and
   up-to-date Windows 10 already have it).
4. Rescan plugins in your DAW and look for **TOiRE-76**.

## Requirements

- Windows 10 1809+ / Windows 11, 64-bit
- A VST3 host (REAPER, Cubase, Studio One, Studio Pro, …)

## Notes

- User presets live in `%APPDATA%\TOiRE-76\Presets` and are **not** removed on uninstall.
- Uninstall via *Settings → Apps → TOiRE-76 (VST3)*, or the uninstaller inside the
  plugin folder.

## Checksums

```
SHA256  2AE5F8474F618A13F29C5F8F54A384B7818F7882C48F61D33A6D664C86007BAA  TOIRE-76-0.1.0-win64.exe
```
