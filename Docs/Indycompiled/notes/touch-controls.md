# Touch controls for Android: design (2026-10-07)

Goal: Indy playable on a phone without a controller, without making it a different game. The touch controls reuse the
gamepad work (modern controls ENH-0005, camera ENH-0002) and add context awareness, so the screen needs few buttons.

## 1. One input layer for every device

The game code doesn't care where input comes from. A small layer of *intents* sits between devices and the classic
controls (it exists already for the gamepad, `Libs/indy/indyModern.c`):

| Intent | Gamepad | Touch | Keyboard |
|---|---|---|---|
| Move direction + strength (camera-relative) | left stick | floating joystick, left half | arrows (classic) |
| Camera orbit | right stick | drag on the right half | — |
| Jump / swim up | A | Jump button | X / Alt |
| Context action (grab, climb, use, push, whip, talk) | X | Action button, icon shows what it does | Ctrl |
| Weapon draw / fire | D-pad up / fire | Weapon button, Fire button while drawn | Space / ... |
| Inventory, map, menu | Start, Back | small buttons at the top | I, M, Esc |

On Android, SDL3 (Stage 5's platform layer) delivers touch, gamepads (Bluetooth controllers work like on the desktop)
and the back button; touch events go into the same intents. So everything built for the gamepad carries over, and
a phone with a controller simply uses the gamepad path.

## 2. Screen layout (landscape)

- **Left half: floating joystick** (decided 2026-10-07). It appears where the thumb lands (no fixed spot to hit), and the distance from
  that point sets the speed: a short push walks, beyond the ring runs (ENH-0001's threshold). Walking keeps the
  original's safety: Indy stops at ledge edges instead of falling, which matters even more on glass.
- **Right half: camera.** One-finger drag orbits the camera (ENH-0002); the camera trails behind Indy more than with a
  gamepad (touch players move it less). A tap on empty space does nothing (no accidental actions).
- **Bottom right: two big buttons, Jump and Action**, placed in the natural arc of the right thumb. A third, smaller
  Weapon button above them. While a weapon is drawn, Action becomes **Fire** (with the game's own auto-aim).
- **Top edge: small buttons** for inventory, map and pause, out of the thumbs' way.
- Everything semi-transparent, resizable and movable in an edit screen; left-handed mirror layout.

## 3. Context: let the game tell the button what to do

The engine already knows what Indy can do where he stands; the classic controls just make the player find out by
trying. The touch layer asks each frame and shows it:

| Situation (engine check) | Action button | Jump button |
|---|---|---|
| Ledge in reach (`sithPlayerControls_CanClimbOn1m/2m`) | climb up | jump |
| Pushable block in front (push/pull ready) | grab; then the joystick pushes/pulls | let go |
| Whip point in range (`sithWhip_SearchWhipClimbThing`) | whip swing | jump |
| Lever, item, door (activation search) | use / pick up | jump |
| Hanging from a ledge | pull up | let go |
| In water | — | dive / surface |
| Ledge behind Indy (`sithPlayerActions_CheckClimbDownWall`) | climb down | — |
| Weapon drawn | fire | jump |

The icon changes with the context, and the interactive object gets a subtle highlight, so players see *that*
something is possible without a tutorial. That replaces most of the keyboard's ~20 actions with two buttons.

## 4. Precision helpers (optional, **off by default**: mobile keeps the original difficulty)

Indy's platforming expects exact positions and headings; a thumb on glass is less precise than keys. These helpers
exist as toggles for players who want them, but the default on mobile is the original challenge:
- **Align on approach**: when walking slowly toward a ledge, block or climb surface, turn Indy square to it (the game
  has the alignment code for climbing already).
- **Jump assist**: a running jump toward a ledge in reach is aimed at it (small heading correction, not a teleport).
- **Edge safety**: walking never falls off; only running or an explicit jump does (the original behaviour).
These stay toggles (ENH entries), so purists can switch them off.

## 5. Feedback

- **Haptics**: short vibration on button presses, a stronger one on landing hard or taking damage (upstream already
  has rumble for XInput; Android has a vibrator API through SDL).
- **Visual**: pressed states, the context icon, the object highlight; the HUD's health indicator unchanged.

## 6. Menus and inventory

The 3D inventory carousel maps well to touch: swipe left/right to turn it, tap an item to use it, tap outside to close.
Win32 dialogs (save/load, options) don't exist on Android; Stage 5 replaces them with engine-drawn menus, which get
large touch targets from the start.

## 7. How we get there

1. **Now (desktop):** generalise `indyModern` into the intent layer (move vector, camera delta, context action) and add
   the context checks; usable immediately on the gamepad (a context-aware X button).
2. **Stage 5 (SDL3):** SDL3 input backend for keyboard, mouse, gamepad and touch; touch can be tested on the desktop
   with a touchscreen or synthetic events (like `virtual_pad.py` for the gamepad).
3. **Stage 7 (Android):** draw the overlay with the engine's HUD renderer (`JonesHud` already draws 2D elements),
   the layout editor, haptics, Android back button and lifecycle (pause on focus loss).
4. **Tests:** synthetic touch scripts drive the same checks as the pad tests (`test_modern.sh` with touch input).

## Decisions (2026-10-07)

- **Floating joystick** by default (it appears where the thumb lands).
- **Original difficulty on mobile**: the precision helpers of section 4 are off by default; the context buttons
  (section 3) stay, since they show what's possible without making it easier.

## Open questions

- Tablet layout (bigger screen: more buttons visible, e.g. a dedicated Look button)?
