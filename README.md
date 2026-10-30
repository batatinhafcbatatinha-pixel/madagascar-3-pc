# Madagascar 3: The Video Game — Native PC Port (work in progress) | No Emulated GPU or Audio Hardware

> [!WARNING]
> This repository does **not** include the game, an ISO/XEX, game assets, generated game code or a precompiled executable.
> You need your own Xbox 360 USA disc. See [the full notice](#notice) at the end.

An experimental port of the Xbox 360 game *DreamWorks Madagascar 3: The Video Game*, built through static recompilation with the [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk), with its own Direct3D 11 renderer and its own XMA audio pipeline, like the [FIFA Street 3](https://github.com/batatinhafcbatatinha-pixel/fifa-street-3-pc) and [Battlefield 2: Modern Combat](https://github.com/batatinhafcbatatinha-pixel/bf2-modern-combat-pc) ports. **This public repository is a work-in-progress source snapshot, not a ready-to-play game download or a self-contained build kit yet.** The game's PowerPC code is translated locally from your own disc; no game executable or generated game code is distributed here.

## Status

- Reaches gameplay (Monte Carlo level, characters, HUD, minimap and dialogue) on Windows 11 x64 at 30 fps, as on the console, without the SDK's Xenos GPU plugin.
- Graphics: the game's D3D calls are drawn by a native Direct3D 11 renderer; Xenos shader microcode is translated to HLSL.
- Audio: the game's XMA exports are reimplemented on the host (FFmpeg's XMA decoder, SDL3 output).
- **F1** (or Back + Start) opens the PC settings, drawn in the style of the game's own Options menu: display mode, internal resolution, VSync, FXAA, MSAA (off, 2x, 4x or 8x; applies at once), anisotropic filtering up to 16×, DirectX 11 or Vulkan through DXVK, volume.
- Known issues: a hang if the language is confirmed within about 3 seconds of the screen appearing; a rare fatal call to address 0; the F1 menu still uses the plain theme; MSAA does not apply to the deferred G-buffer. Saves, full progression and stability are unverified.

## What is in this repository

- `project/` — CMake project, ReXGlue manifest (`game_root` points to your local `game_files`), and the native code in `project/src/` (renderer, shader translation, audio, PC settings, scripted input).
- `scripts/` — ISO inspection/extraction, image summary, import-call scan, function-extent and indirect-entry recovery, function matching, PowerPC disassembler, build pipeline and test runner.
- `inputs/` — scripted-input files for automated test runs.
- [NOTES.md](NOTES.md) and [CONTINUACAO.md](CONTINUACAO.md) — development log (in Portuguese).

Build with `build-madagascar.cmd release build` (ReXGlue SDK `0.10.0.24-dev.gbd833a2`, Clang, Ninja). The script currently expects the author's local SDK and build locations.

Linux (x86-64, native, no Wine): configure `project/` with the ReXGlue SDK's Linux package on `CMAKE_PREFIX_PATH`,
Clang 20, `DXVK_NATIVE_DIR` (DXVK native 3.1.1: the D3D11 renderer on Vulkan) and vkd3d-shader 1.19+ on
`PKG_CONFIG_PATH` (HLSL compiler); fontconfig and FreeType are needed for the PC settings text. Put the SDK
runtime, DXVK native and vkd3d-shader next to the executable and add `libSDL3.so.0` as a link to the SDK
runtime (it carries SDL3; DXVK's SDL3 WSI then shares it). The FIFA Street 3 kit's `tools/build_local.py` does
all of this and is the reference until this repository has its own kit.

## Your game files

```text
default.xex  38d0d7e6665c89567b0566dff387d2925a0b7f001367b99fe08bfd48e32fd1a0
```

Do not upload the ISO, extracted files, generated code or the built executable.

## Credits and licenses

- Port integration, native renderer and audio: [batatinhafcbatatinha-pixel](https://github.com/batatinhafcbatatinha-pixel).
- [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk): Tom Clay and contributors, BSD-3-Clause; derived from [Xenia](https://github.com/xenia-project/xenia) (Ben Vanik and contributors). Shader-microcode semantics and XMA field layouts follow the SDK's Xenia-derived headers.
- [PT Sans](https://github.com/google/fonts/tree/main/ofl/ptsans) by ParaType (SIL Open Font License 1.1, `project/fonts/OFL.txt`): the game's own typeface, shipped for the PC settings menu.
- [FFmpeg](https://ffmpeg.org) (LGPL-2.1) for XMA decoding, [SDL3](https://www.libsdl.org) (zlib) for audio output, [DXVK](https://github.com/doitsujin/dxvk) (zlib) as the optional Vulkan path; external dependencies, not bundled.
- Madagascar 3: The Video Game, its code, trademarks and assets belong to DreamWorks Animation, D3Publisher, Monkey Bar Games and the respective rights holders. This project is not affiliated with or endorsed by them or by Microsoft.

---

<a id="notice"></a>

**⚠️ THIS PROJECT DOES NOT INCLUDE THE GAME, AN ISO/XEX, GAME ASSETS, GENERATED GAME CODE OR A PRECOMPILED GAME EXECUTABLE. YOU MUST PROVIDE YOUR OWN LAWFULLY OBTAINED XBOX 360 GAME FILES AND HAVE THE RIGHTS REQUIRED FOR YOUR USE. THIS PROJECT DOES NOT AUTHORIZE COPYRIGHT INFRINGEMENT. ALL GAME RIGHTS BELONG TO THEIR RESPECTIVE OWNERS. ⚠️**
