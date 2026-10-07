#ifndef INDY_INDYTOUCH_H
#define INDY_INDYTOUCH_H
// Indycompiled: touch controls (Android).
//
// Left half: a floating stick appears where the thumb lands; it drives the modern, camera-relative movement
// (ENH-0005), and climbing/swimming through the classic keys. Right half: dragging orbits the camera (ENH-0002), a tap
// attacks, a quick flick down toggles crouching. Buttons: Jump, Action (the game's action key ACT2, held as long as
// touched: use, pick up, hold a block while the stick pushes or pulls it) and Menu; a tap on
// the health indicator opens the menu too. In the HUD menu (the game's inventory/system strip) swipes move through
// it, a tap on an item selects it and a tap on the selected item uses it (JonesHud), the Menu button closes it. The overlay hides while a keyboard or gamepad is used.
//
// The platform layer (wkernelSDL.c) passes finger events in and injects keys (Enter, Escape); the game reads the stick,
// camera drag and control functions; JonesMain draws the overlay after the HUD.
#include <j3dcore/j3d.h>
#include <sith/types.h>
#include <stdbool.h>
#include <stdint.h>

J3D_EXTERN_C_START

typedef enum eIndyTouchEvent
{
    INDY_TOUCH_DOWN,
    INDY_TOUCH_MOTION,
    INDY_TOUCH_UP,
} IndyTouchEvent;

typedef void (*IndyTouchKeyFunc)(uint8_t dik, bool bDown);

// Platform: a finger event (x, y: 0..1 of the window), other input (keyboard, gamepad: hides the overlay), the key
// injector for DIK keys, and the per-frame update (releases key pulses)
void indyTouch_OnFinger(IndyTouchEvent event, uint64_t fingerId, float x, float y, uint32_t msecTime);
void indyTouch_OnOtherInput(void);
void indyTouch_SetKeyFunc(IndyTouchKeyFunc pfKey);
void indyTouch_Update(uint32_t msecTime);

// Game: latches this frame's presses (sithControl_ReadControls); the movement stick (x right, y up, -1..1; false when
// not touched); the camera drag since the last call in degrees (false when no finger drags the camera)
void indyTouch_BeginControlFrame(void);
bool indyTouch_GetStick(float* pX, float* pY);
bool indyTouch_TakeCameraDelta(float* pYaw, float* pPitch);

// JonesHud's menu: a tap since the last call (back buffer pixels)
bool indyTouch_TakeMenuTap(float* pX, float* pY);

// sithControl_GetKey: the touch state of a control function, ORed with its bindings (*pValue pressed now, *pNumPressed
// presses this frame); false when touch doesn't drive it
bool J3DAPI indyTouch_GetKey(SithControlFunction function, int* pValue, int* pNumPressed);

typedef struct sIndyTouchFrame
{
    uint32_t width;  // back buffer size
    uint32_t height;
    bool bMenuOpen;  // the HUD menu (inventory) is open
    bool bCinematic; // a cutscene: no movement controls
    float hudX;      // health indicator (pixels): a tap opens the menu
    float hudY;
    float hudWidth;
    float hudHeight;
} IndyTouchFrame;

// After the HUD (inside the scene): remembers the game state and draws the overlay
void indyTouch_Frame(const IndyTouchFrame* pFrame);

J3D_EXTERN_C_END
#endif // INDY_INDYTOUCH_H
