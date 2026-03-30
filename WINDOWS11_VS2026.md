# Build/Run on Windows 11 with Visual Studio 2026

This repository is old (mostly VS2008/VS2010 era), but can still be built on a modern machine if prerequisites are installed.

## 1) Install prerequisites

1. Visual Studio 2026 with workload: `Desktop development with C++`
2. MSVC toolsets + Windows 10/11 SDK
3. `YASM` in `PATH`
4. `NASM` in `PATH`
5. Optional but often needed for legacy DX9 tools: DirectX SDK June 2010 (`DXSDK_DIR`)

## 2) Run automated check

From PowerShell in repo root:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\windows\vs2026-check-and-build.ps1
```

The script validates:
- VS/MSBuild availability
- `yasm` and `nasm`
- known placeholder SDK markers in `v2`
- external WTL location expected by RG2
- `DXSDK_DIR`

## 3) Build all modern solutions (`.vcxproj`-based)

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\windows\vs2026-check-and-build.ps1 -Build -Configuration Release -Platform Win32
```

Notes:
- `Win32` is recommended (most projects are 32-bit).
- By default, the script skips legacy solutions that still reference `.vcproj` directly.
- The script forces `PlatformToolset=v143` by default (works with modern VS installs). You can override: `-ForcePlatformToolset v142` or disable override with `-ForcePlatformToolset ''`.

## 4) Include legacy `.vcproj` solutions too (optional)

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\windows\vs2026-check-and-build.ps1 -Build -IncludeLegacySln -Configuration Release -Platform Win32
```

These usually require manual conversion/retargeting in Visual Studio.

## 5) Known blockers

- `v2` plugin projects require extra SDK headers (Winamp/VST2/Buzz/WTL placeholders are present).
- `RG2` expects external WTL include path (`..\\..\\WTL\\include` relative to `RG2`).
- Some Altona/legacy DX projects require old `d3dx9` from DirectX SDK.

## 6) First target to test quickly

If you want one easy smoke test first, start with:

- `kkrunchy\kkrunchy.sln` (needs `yasm`)

Quick wrapper: `tools\windows\build-all-vs2026.cmd`.

Last automated run report: `WINDOWS11_VS2026_BUILD_REPORT.md`.

Current known-good builds from automated run:
- `altona_wz4\altona\tools\makeproject\bootstrap\bootstrapVC10.sln`
- `kkrunchy\kkrunchy.sln`
- `kkrunchy_k7\kkrunchy.sln`
- `genthree\genthree.sln`
- `ktg\demo.sln`
