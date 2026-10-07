#ifndef JONES3D_JONESMENUSDL_H
#define JONES3D_JONESMENUSDL_H
// Native builds: engine-drawn menus for the dialogs Windows shows as dialog boxes (jonesConfigSDL.c). A menu is a modal
// loop over the dimmed game frame, drawn with the game's font; it takes touch (tap, drag to scroll), keyboard (arrows,
// Enter, Escape) and gamepads (stick or D-pad, A, B). Widgets are immediate mode: each frame the dialog calls them in
// drawing order; buttons are focusable in that order (Up/Down).
#include <j3dcore/j3d.h>
#include <stdbool.h>

J3D_EXTERN_C_START

typedef struct sJonesMenuRect
{
    float x;
    float y;
    float width;
    float height;
} JonesMenuRect; // back buffer pixels

typedef enum eJonesMenuTextStyle
{
    JONESMENU_TEXT_NORMAL,
    JONESMENU_TEXT_TITLE,
    JONESMENU_TEXT_DIM,
} JonesMenuTextStyle;

// A menu session (over the dimmed game frame, or black before a level is loaded); false when there is no display yet
// (callers fall back). End waits until the confirm and back keys are released, so the game doesn't take them as its own
// input.
bool JonesMenu_Begin(void);
void JonesMenu_End(void);

// One frame; BeginFrame is false when the window closes
bool JonesMenu_BeginFrame(void);
void JonesMenu_EndFrame(void);

float JonesMenu_GetWidth(void);
float JonesMenu_GetHeight(void);
float JonesMenu_GetUnit(void); // one pixel of the game's 640x480 reference, scaled to the screen height

void JonesMenu_Panel(const JonesMenuRect* pRect);
void JonesMenu_Text(const char* pText, float x, float y, float sizePt, int alignFlags, JonesMenuTextStyle style); // y: top
bool JonesMenu_Button(const JonesMenuRect* pRect, const char* pText, const char* pSubText); // true when chosen

bool JonesMenu_IsBack(void);       // Escape, Android Back, gamepad B this frame
int JonesMenu_GetFocus(void);      // index of the focused button
void JonesMenu_SetFocus(int index);
float JonesMenu_TakeScroll(void);  // touch drag since the last call (pixels, positive: content follows the finger down)

J3D_EXTERN_C_END
#endif // JONES3D_JONESMENUSDL_H
