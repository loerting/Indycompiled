# Renderer: upstream OpenGL branch vs our DX9 build (2026-10-07)

Analysis of `upstream/OpenGL` (last commit 2026-07-24) against `main`, for Stage 5 (platform layer) and Stage 7
(Android, OpenGL ES 3.0). `GL:` = path on `upstream/OpenGL`.

## Summary

- **Merge, don't copy.** `git merge-tree main upstream/OpenGL` gives 2 conflicts, both trivial: `CMakeLists.txt`
  (develop's `JONES3D_RUNTIME_GUARDS` vs GL's `JONES3D_RENDERER`) and `JonesMain.c` (our ENH-0004 block). Our XInput
  fixes survive the rename `stdControlDX9.c` → `stdControl.c`. The branch vendors SDL3 and glad: ~900k lines.
- **Still a Win32 hook DLL.** The branch keeps the following on Win32:
  - Window: the SDL window's HWND is subclassed, and SDL events are turned into `WM_*` messages.
  - GDI: DIB sections and fonts.
  - Input: DirectInput/XInput.
  - Sound: DirectSound.
  - Dialogs: native Win32 (`DialogBoxParam`, `GetOpenFileName`).
  - The exe: SMUSH decoding and the `Indy3D_WinMain` hook still come from it.

  It's a renderer, not a native build.
- **Context:** SDL3, OpenGL 3.3 core, 15 shaders with `#version 330 core`, glad generated at configure time (python +
  jinja2, pip fallback over the network).
- **New render path (GL only):**
  - a welded static world VBO built at level load;
  - instancing for models, sprites, particles, polylines and shadows;
  - per-pixel lights from a UBO (cap 64);
  - sky shaders.

  `graphics.legacyRendering` switches back to the CPU path.
- **The look changes** (per-pixel smoothstep lighting, a magic fog factor 0.03459, new sky). To keep the game's
  character, the vanilla/fixed profiles need the legacy path or an equivalent toggle.

## Merge follow-ups

1. **Default renderer:** it becomes OpenGL (`JONES3D_RENDERER`). Set DirectX9 in our presets and add `mingw-gl-*` presets.
2. **Case-sensitive paths:** `Win95/Gl/Shaders` vs `GL` breaks on case-sensitive filesystems (works on exFAT).
3. **glad:** use pacman's `python-jinja` or check in the generated loader (no pip).
4. **Vendored SDL3:** it inherits our global flags (`-include indy_prelude.h`, `-fms-extensions`, `-fasm-blocks`); scope those away.
5. **Unguarded `D3DTLVERTEX` change:** it gains `nx, ny, nz` (32 → 44 bytes) for DX9 too. Re-verify the DX9 path
   (`SetFVF(D3DTLVERTEX_FVF)`, stride `sizeof(D3DTLVERTEX)`).

## Bugs found in the GL branch (fix when adopting; candidates for upstream reports)

- **Legacy setting inverted:** `std3GL.c:246-247` writes back the inverted value of `graphics.legacyRendering` on
  every init and reset, so first run enables the legacy renderer.
- **Light buffer overflow:** `rdShader.c:147-160` writes up to 128 lights (`RDCAMERA_MAX_LIGHTS`) into a block sized
  for 64.
- **Out-of-bounds read:** `stdDisplayGL.c:447` indexes the device array with an `SDL_DisplayID`.
- **Wrong null check:** `std3GL.c:1440` checks the polyline shader instead of the legacy shader.
- **Lock refcount:** `stdDisplayGL.c:1375-1419` never decrements `stdDisplay_backLockRef`, so later
  `LockBackBuffer` calls skip the readback.
- **Full-buffer uploads:** `std3GL.c:1723-1728` uploads the whole vertex buffer (2.9 MB) and index buffer (256 KB)
  on every flush.
- **Ignored arguments:** `stdDisplayGL.c:1275` `BackBufferFill` ignores colour and rectangle (callers pass 0/NULL).
- **Syntax:** `stdControl.h:233` has a bare `#elif`.
- **arm64:** `stdShaderGL.c:249` passes `(const GLint*)&size_t` to `glShaderSource` (wrong on 64-bit).

## GLES 3.0 port (estimated 6–10 days, plus 3–5 for device bring-up)

**Small edits:**

| Item | Where | Change |
|---|---|---|
| `#version 330 core` | all 15 shaders | `#version 300 es` |
| No default float precision | all 6 fragment shaders | add `precision highp float; precision highp int;` |
| Uniform initialisers | 10 | remove (all defaults are 0) |
| Implicit int→float | `fbo.vert`, `ceilingSky.frag` | make explicit |
| Variable `dot` shadows `dot()` | `common.incl:139` | rename |
| `GL_BGRA`/`GL_BGR`, `8_8_8_8_REV` | `std3GL.c:58-70` | RGBA8 + `UNSIGNED_BYTE`, or texture swizzle |
| `glReadPixels(GL_BGRA)` | `stdDisplayGL.c:1519` | RGBA + R↔B swap |
| `glPolygonMode` | debug wireframe only | stub it |
| Single-sample → MSAA blit | | not allowed in ES: draw a quad (same idea as our `indyDisplayDX9.c`) |
| Optional extensions | anisotropic filtering, NVX/ATI memory queries, KHR_debug | guard each by extension |

**Real work:**

| Item | Change |
|---|---|
| `glMultiDrawElements` (not in ES) | loop over `glDrawElements` or build a compacted index list |
| 16-bit textures | upload `GL_RGB565` directly; rotate ARGB1555 to `RGB5_A1`; ARGB4444 via swizzle (optional, halves VRAM) |
| Mobile performance | per-pixel light loops (up to 64 lights), full-buffer uploads every flush, synchronous readback per locked frame (videos) |
| EGL context loss on Android | not handled (`stdDisplay_CheckDeviceState` always returns 0) |

**Already fine in ES 3.0:**
- Core features: UBOs (std140, largest 3 KB), instancing, VAOs, samplers, 32-bit indices, `textureSize`, `gl_VertexID`.
- Attribute locations: 12 attributes, within ES's 16.
- Extensions: none required.

## Our DX9-specific work and its GL equivalent

| Our change | GL equivalent |
|---|---|
| MSAA video/dialog quad (`indyDisplayDX9.c`) | not needed on desktop GL (`glBlitFramebuffer` handles it); needed on ES |
| MSAA settings | carry over |
| HUD ultrawide fix, Hor+ FOV | renderer-independent (check 21:9 once) |
| `Libs/indy/*` | no renderer dependency |
| Test and deploy scripts | need GL presets |

## Effort (person-days)

| Package | Days |
|---|---|
| Merge, 2 conflicts, presets | 0.5 |
| GL build with clang/MinGW (glad, case fix, SDL3 flags) | 1.5–3 |
| Bring-up under Wine (Xvfb/llvmpipe and GPU, fullscreen, MSAA, video, thumbnails, dialogs) | 3–5 |
| Fix the bugs above | 1–2 |
| Visual parity across 17 levels; toggle for the new lighting/fog | 2–4 |
| **Desktop GL total** | **8–15** |
| GLES 3.0 renderer | 6–10 |
| Android device bring-up (renderer only) | 3–5 |
