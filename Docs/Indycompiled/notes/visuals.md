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
