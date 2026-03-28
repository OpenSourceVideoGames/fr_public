# Windows 11 + VS 2026 Build Report

Date: 2026-03-28 (UTC)

Command used:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\windows\vs2026-check-and-build.ps1 -Build -Configuration Release -Platform Win32 -ForcePlatformToolset v143
```

## Result summary

All `vcxproj`-based solutions selected by the checker built successfully (`Release|Win32`, toolset forced to `v143`):

- PASS: `altona_wz4\altona\tools\makeproject\bootstrap\bootstrapVC10.sln`
- PASS: `genthree\genthree.sln`
- PASS: `kkrunchy\kkrunchy.sln`
- PASS: `kkrunchy_k7\kkrunchy.sln`
- PASS: `ktg\demo.sln`
- PASS: `RG2\TG2.sln`
- PASS: `v2\v2.sln`
- PASS: `werkkzeug3\wz3projects.sln`

## Key compatibility fixes applied

1. `genthree` modern MSVC compile fix:
- `genthree/ronan.cpp`: switched legacy `abs(float)` usage to `sFAbs(...)` where needed.

2. `kkrunchy_k7` assembler toolchain fix:
- `kkrunchy_k7/kkrunchy.vcxproj`: switched RDF-related custom build steps to `yasm`; kept `model_asm.asm` on `nasm`.

3. `altona_wz4` bootstrap compatibility:
- prepared missing config/header and modernized parts of Win32 props/dependencies used by bootstrap build.
- removed hard old DX helper dependency (`dxerr`) for this build path.

4. `RG2` entry-point/link cleanup:
- `RG2/Viewer/Viewer.cpp`, `RG2/Viewer/Viewer.vcxproj`: normalized to regular `WinMain`, removed forced custom entrypoint symbol.

5. `v2` CRT/libm compatibility:
- `v2/tinyplayer/tinyplayer.vcxproj`, `v2/tinyplayer_cpp/tinyplayer_cpp.vcxproj`, `v2/libv2/libv2.vcxproj` updated for modern CRT linking.
- added `v2/libv2/libm_compat.cpp` with missing legacy libm shim symbols.
- updated `v2/libv2/libm_compat.cpp` to use x87 instructions (`fpatan`/`fcos`) on Win32, avoiding recursive calls through legacy libm alias symbols at runtime.

6. `werkkzeug3` full link modernization for Win32 Release:
- generated missing shader headers in `werkkzeug3/werkkzeug3/` (`effect_*vs.hpp`, `effect_*ps.hpp`).
- `werkkzeug3/w3texlib/genbitmap.cpp`: fixed `Fade64` visibility/linkage.
- `werkkzeug3/example_cube/example_cube.vcxproj`, `werkkzeug3/player_demo/player_demo.vcxproj`: disabled SAFESEH for Release and added legacy stdio compatibility lib.
- `werkkzeug3/pngloader/legacy_stdio_compat.cpp` + `werkkzeug3/pngloader/pngloader.vcxproj`: added `__iob_func` compatibility shim.
- `werkkzeug3/player_intro/player_intro.vcxproj`: adjusted legacy `/NODEFAULTLIB` style CRT settings and explicit entrypoint behavior.
- `werkkzeug3/player_intro/intro_config.hpp`: set explicit `sNOCRT=0` and disabled startup config dialog (`sCONFIGDIALOG=0`) for stable startup in this environment.
- `werkkzeug3/_start.cpp`: exported `WinMainCRTStartup` with C linkage.
- `werkkzeug3/werkkzeug.cpp`, `werkkzeug3/werkops.cpp`: fixed macro-string concatenation parsing issues on modern C++ compiler.
- `werkkzeug3/werkkzeug3/werkkzeug3.vcxproj`: added `legacy_stdio_definitions.lib` to resolve `_sprintf` from old png objects.

## Notes from checker (non-blocking)

- External placeholder SDK folders remain in `v2` (`Winamp/Buzz/VST2/WTL`) and are still reported as warnings.
- External `WTL\include` path for RG2 is still reported missing by the checker environment probe.
- `DXSDK_DIR` is not set; this can affect some legacy DX9-oriented targets not part of the `vcxproj` build set.

## Runtime smoke test (2026-03-28 UTC)

- Rebuilt all `vcxproj` solutions successfully with `Release|Win32` and `PlatformToolset=v143`.
- Startup check results:
  - GUI apps (`genthree`, `RG2`, `Viewer`, `tinyplayer`, `tinyplayer_cpp`, `example_cube`, `player_demo`, `player_intro`, `werkkzeug3`) start and stay alive for the smoke-test window.
  - CLI tools (`kkrunchy`, `kkrunchy_k7`, `conv2m`, `texlibtest`, `shadercompile`) return their expected usage exit code `1` when launched without arguments.
