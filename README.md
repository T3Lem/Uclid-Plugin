# Uclid

Euclidean rhythm volume gate — a JUCE audio effect plugin (VST3 + Standalone).

Applies an **[E(pulses, steps)](https://en.wikipedia.org/wiki/Euclidean_rhythm)** pattern as a tempo-synced gain sequencer on your audio.

## Features

- VST3 and Standalone builds
- Interactive circular pattern UI (drag the ring to set pulses)
- Link or split Grid / Steps
- Short de-click smoothing on step edges (0–10 ms)
- FL Studio / DAW tempo sync via playhead

## Requirements

- **CMake** 3.22+
- **Visual Studio 2022** or **2026** with **Desktop development with C++**
- **Windows SDK**

JUCE 8.0.4 is fetched automatically on first configure (or place it in `external/JUCE`).

## Build

```powershell
cd CRSR
cmake --preset windows-release
cmake --build build --config Release --target Uclid_VST3 Uclid_Standalone
```

Outputs:

| Artifact | Path |
|----------|------|
| VST3 | `final/VST3/Uclid.vst3` |
| Standalone | `final/Standalone/Uclid.exe` |

## Install VST3 (FL Studio)

Admin PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File install-to-program-files.ps1
```

Default scan path: `C:\Program Files\Common Files\VST3`

## Project layout

```
Uclid/Source/     Plugin processor & editor
CMakeLists.txt    Build configuration
final/            Build outputs (gitignored)
```

## Author

[Tal Shimoni](https://github.com/yodem/tal-protfolio) · [Portfolio](https://tal-protfolio.vercel.app)

## License

All rights reserved.
