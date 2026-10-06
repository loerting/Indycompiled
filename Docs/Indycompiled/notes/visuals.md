# Visuals: findings (2026-10-06)

## HUD scaling in widescreen (upstream issue #13)

Upstream `develop` already sizes the health and endurance indicators by `JonesHud_heightAspectRatioScale` (height/480),
which keeps their original share of the screen height at any aspect ratio. Nothing to do on our side. To verify in a
widescreen test: `graphics.width/height` = 1280×720 in `Jones.cfg`, and a matching Xvfb screen.

## Fixed: integer division in the inventory item scale (ultrawide)

`JonesHud_Update` computed `RD_REF_APECTRATIO / (width / height)` with `uint32_t` width and height: integer
division, so 1.333 for every aspect ratio below 2:1, and 0.667 at 21:9 (2560/1080 = 2). Screenshots at 1024×768,
1280×720 and 2560×1080 with the inventory open: same item size at 4:3 and 16:9, half size at 21:9.

Since the camera is Hor+ (`rdCamera_BuildFOV` keeps the vertical field of view of 4:3), 3D objects in front of the
camera keep their size relative to the screen height at any aspect ratio, so the right factor is the constant. Both
uses (item scale and the inventory's Z position) now use `RD_REF_APECTRATIO`: unchanged at 4:3 and 16:9, fixed at
21:9. Verified with the same screenshots.

Widescreen otherwise works: Hor+ field of view (more to the sides, same vertical framing) and the health indicator
scaled by height.

## MSAA

See `enhancements.md`: video frames (and other CPU-drawn content) went through `StretchRect` into the MSAA back buffer,
which Wine refuses. The fallback in `Libs/indy/indyDisplayDX9.c` draws them as a quad instead.

## Idea for later: texture packs (2026-10-07)

The blur comes from the source data, not the renderer. Measured in the v1.2 files:
- Level textures are embedded in the 17 `.cnd` files (materials section): about 3,300 unique names, 5,772 counting
  repeats across levels, 250–480 per level. Largest side: 4 px (21), 8 (95), 16 (558), 32 (1,343), 64 (1,940),
  128 (1,526), 256 (288), 512 (1). All 16-bit colour.
- Items, HUD and effects: 148 `.mat` files in CD1/CD2.GOB (plus 3 in Jones3D.GOB), mostly 256 px, 16-bit.

Plan if we do it:
- **Engine:** in the material loaders (`sithMaterial_ReadMaterialsListBinary`, `rdMaterial_LoadEntry`), look up a
  replacement by material name (`textures/<name>.png` on the desktop, KTX2/ASTC on Android). Keep the original
  width/height for everything the level data relies on, and give only the GPU texture the higher resolution. 32-bit
  colour, mipmaps and anisotropic filtering come with it. A missing replacement falls back to the original.
- **Content:** AI upscaling of all originals as the base (keeps the look), with hand replacements of the most visible
  surfaces from CC0 material libraries, colour-matched to the original. Faces, signs and text need manual work.
- **Android:** ASTC at 512–1024 px, loaded per level, behind a quality setting (uncompressed 1024 px would exceed
  1 GB per level).
- **Legal:** an upscaled pack is derived from the game's textures: private only. To share it, ship a tool that builds
  the pack from the player's own game files; packs made only from CC0 sources could be distributed.
