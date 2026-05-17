# Uclid

Euclidean rhythm volume gate — a JUCE audio effect plugin (VST3 + Standalone).

Applies an **[E(pulses, steps)](https://en.wikipedia.org/wiki/Euclidean_rhythm)** pattern as a tempo-synced gain sequencer on your audio.

## How this started

The first version was a rough JUCE experiment: Euclidean math driving volume on a channel, built in Visual Studio without much UI polish — functional, but honestly **ugly**. The smoothing fought the rhythm, parameters didn’t line up, and FL Studio didn’t always see the plugin where it lived on disk.

From that demo, the goal became clear: keep the **Euclidean gate** idea, but make it **usable** — tight step timing, a readable pattern, short de-click ramps only where needed, and a build path that actually installs as VST3. That’s this repo.

| Early demo | Current Uclid |
|------------|-----------------|
| ![First prototype UI](docs/images/uclid-before.png) | *Build Standalone from this repo for the latest UI* |

> **Before screenshot:** drop `uclid-before.png` into [`docs/images/`](docs/images/) (export from your old Standalone, or a screen capture of the legacy VST). The table above will light up automatically on GitHub.

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

## Repository

[github.com/T3Lem/Uclid-Plugin](https://github.com/T3Lem/Uclid-Plugin)

## Author

[Tal Shimoni](https://github.com/yodem/tal-protfolio) · [Portfolio](https://tal-protfolio.vercel.app)

## License

All rights reserved.
