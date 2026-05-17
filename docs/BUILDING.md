# Building Uclid

For contributors or anyone compiling locally. If you only need the plugin in FL Studio, use the install scripts from the [README](../README.md) — skip this page.

## Requirements

- **CMake** 3.22+
- **Visual Studio 2022** or **2026** with **Desktop development with C++**
- **Windows SDK**

JUCE **8.0.4** is fetched automatically on first `cmake` configure. For offline builds, run `scripts/bootstrap-juce.ps1` or place JUCE in `external/JUCE`.

## Build

```powershell
cd CRSR
cmake --preset windows-release
cmake --build build --config Release --target Uclid_VST3 Uclid_Standalone
```

| Artifact | Path |
|----------|------|
| VST3 | `final/VST3/Uclid.vst3` |
| Standalone | `final/Standalone/Uclid.exe` |

## Install VST3

**FL user folder** (no admin):

```powershell
powershell -ExecutionPolicy Bypass -File install-uclid-vst3.ps1
```

→ `%USERPROFILE%\Documents\Image-Line\FL Studio\Plugins\VST3`

**System folder** (admin PowerShell):

```powershell
powershell -ExecutionPolicy Bypass -File install-to-program-files.ps1
```

→ `C:\Program Files\Common Files\VST3`

Then rescan in FL Studio (**Options → Manage plugins**). Use **64-bit** FL.
