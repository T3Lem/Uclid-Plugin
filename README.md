# Uclid

**Euclidean rhythm volume gate** — a JUCE effect that applies **[E(pulses, steps)](https://en.wikipedia.org/wiki/Euclidean_rhythm)** as a tempo-synced gain pattern on your audio. Ships as **VST3** and **Standalone** for Windows.

## Before & after

| First prototype (FL Studio) | Current Uclid |
|----------------------------|----------------|
| ![Early Uclid in FL Studio](docs/images/uclid-before.png) | ![Current Uclid UI](docs/images/uclid-after.png) |

The first build proved the idea: Euclidean math gating volume in the DAW. It worked, but the UI was rough, smoothing fought the groove, and the plugin was easy to install in the wrong folder so FL Studio would not pick it up.

This repo keeps the same core idea and makes it practical — clear step timing, a readable circular pattern, short edge ramps only where needed, and a reliable VST3 install path.

## Features

- VST3 and Standalone builds
- Circular pattern UI — drag the ring to set pulses
- Link or split Grid / Steps
- De-click smoothing on step edges (0–10 ms)
- Tempo sync from the DAW playhead (FL Studio and others)

## Requirements

- **CMake** 3.22+
- **Visual Studio 2022** or **2026** with **Desktop development with C++**
- **Windows SDK**

JUCE 8.0.4 is fetched on first configure (or place a copy in `external/JUCE`).

## Build

```powershell
cd CRSR
cmake --preset windows-release
cmake --build build --config Release --target Uclid_VST3 Uclid_Standalone
```

| Output | Path |
|--------|------|
| VST3 | `final/VST3/Uclid.vst3` |
| Standalone | `final/Standalone/Uclid.exe` |

## Install in FL Studio

**User folder** (no admin):

```powershell
powershell -ExecutionPolicy Bypass -File install-uclid-vst3.ps1
```

**System folder** (admin PowerShell):

```powershell
powershell -ExecutionPolicy Bypass -File install-to-program-files.ps1
```

Then rescan plugins in FL Studio (`Options → Manage plugins`). Use **64-bit** FL; default system path is `C:\Program Files\Common Files\VST3`.

## Layout

```
Uclid/Source/     Processor & editor
CMakeLists.txt    Build
docs/images/      README screenshots
final/            Build output (gitignored)
```

## Links

- **Repo:** [github.com/T3Lem/Uclid-Plugin](https://github.com/T3Lem/Uclid-Plugin)
- **Author:** [Tal Shimoni](https://github.com/yodem/tal-protfolio) · [Portfolio](https://tal-protfolio.vercel.app)

## License

All rights reserved.
