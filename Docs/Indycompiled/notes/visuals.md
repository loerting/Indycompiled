# Visuals: findings (2026-10-06)

## HUD scaling in widescreen (upstream issue #13)

Upstream `develop` already sizes the health and endurance indicators by `JonesHud_heightAspectRatioScale` (height/480),
which keeps their original share of the screen height at any aspect ratio. Nothing to do on our side. To verify in a
widescreen test: `graphics.width/height` = 1280×720 in `Jones.cfg`, and a matching Xvfb screen.

## Likely upstream bug: integer division in the menu-item aspect scale

`JonesHud_Update` and `JonesHud_UpdateHUDLayout` compute `RD_REF_APECTRATIO / (width / height)` with `width` and `height`
as `uint32_t`. `width / height` is integer division: 1 for 4:3 *and* for 16:9. So `JonesHud_itemAspectScaleSize` doesn't
follow the aspect ratio at all (it is 1.333 × `JonesHud_menuItemScale` everywhere). Upstream may have tuned
`JonesHud_menuItemScale` around that, so changing it alters the look. Decide when we do the widescreen pass, with
screenshots at 4:3 and 16:9.

## MSAA

See `enhancements.md`: video frames (and other CPU-drawn content) went through `StretchRect` into the MSAA back buffer,
which Wine refuses. The fallback in `Libs/indy/indyDisplayDX9.c` draws them as a quad instead.
