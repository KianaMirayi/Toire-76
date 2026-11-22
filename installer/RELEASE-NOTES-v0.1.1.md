# TOiRE-76 v0.1.1

Maintenance release: bypass fix, latency reporting, preset switching, and a large
improvement to transient behaviour.

## Fixed

- **Host bypass now passes dry audio.** Previously bypassing the plugin (or using a
  host-wide "bypass all effects") produced silence. Bypass now outputs the dry signal
  with the same latency as normal operation.
- **Latency reporting** — 86 samples @44.1/48 kHz, 118 @96 kHz, 151 @192 kHz
  (previously reported 87 at every rate). Host delay compensation now matches reality.
- **Preset switching** no longer carries leftover gain reduction into the new patch;
  the sidechain resets only when several parameters change at once, so normal knob
  moves never cause a reset.
- **NaN/Inf guard** in the sidechain and output stage — a single invalid sample can no
  longer poison the control voltage and silence the plugin.
- **User preset dropdown** — stable selection, the first entry applies correctly after
  the list refreshes.
- **Installer no longer requires the VC++ redistributable** — the plugin is now built
  with a statically linked CRT.

## Improved

- **Transient over-compression removed**: on hard onset steps the gain reduction used
  to overshoot the steady-state value by up to 12.6 dB (audible "pump-then-release").
  It is now within 0.2 dB of steady state.
- Measured similarity against the reference on 16 fresh music excerpts
  (1536 renders) went from **77.8% → 84.0%**; snare 52.6→78.0, kick 52.3→69.9,
  drum kit 66.5→75.3.
- Overall output gain trimmed by 1.2 dB so the level now matches the reference
  (+0.01 dB bias at the test point).

## Known limitations

- The attack-knob "overshoot" metric is a deliberate trade-off: matching it exactly
  would make transients compress harder and reduce the overall musical similarity.
- Release at the fast end (0.85–1.0) is ~12% slower than the reference.
- The ALL-buttons ratio tops out about 2.6 dB below the reference.
- Static 4:1 knee is ~2 dB lower and its effective ratio ~0.9 lower than nominal.

## Install

1. Run `TOIRE-76-0.1.1-win64.exe` (administrator; installs into the shared VST3 folder).
2. If WebView2 is missing, the installer adds it automatically.
3. Rescan plugins in your DAW and look for **TOiRE-76**.

## Checksums

```
SHA256  D3C8687097A5BDB7245D4D2C82402771AE348CD4FA692AACC59C23BCB2224CC5  TOIRE-76-0.1.1-win64.exe
```
