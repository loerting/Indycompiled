// Native builds: XInput 1.4 (cmake/compat/native/Xinput.h) on SDL3 gamepads. User index n is the n-th connected
// gamepad; SDL's gamepad mapping gives every supported controller the Xbox layout. Y axes point up, as in XInput.
#include <j3dcore/j3d.h>
#include <Xinput.h>

#include <SDL3/SDL.h>
#include <string.h>

static SDL_Gamepad* XInputSDL_apGamepads[XUSER_MAX_COUNT];
static XINPUT_GAMEPAD XInputSDL_aLastState[XUSER_MAX_COUNT];
static DWORD XInputSDL_aPacket[XUSER_MAX_COUNT];

static SDL_Gamepad* XInputSDL_GetGamepad(DWORD userIndex)
{
    if ( userIndex >= XUSER_MAX_COUNT ) return NULL;

    SDL_Gamepad* pPad = XInputSDL_apGamepads[userIndex];
    if ( pPad && SDL_GamepadConnected(pPad) ) return pPad;
    if ( pPad )
    {
        SDL_CloseGamepad(pPad);
        XInputSDL_apGamepads[userIndex] = NULL;
    }

    int count = 0;
    SDL_JoystickID* aIds = SDL_GetGamepads(&count);
    if ( aIds && (int)userIndex < count )
    {
        pPad = SDL_OpenGamepad(aIds[userIndex]);
        XInputSDL_apGamepads[userIndex] = pPad;
    }
    SDL_free(aIds);
    return XInputSDL_apGamepads[userIndex];
}

static SHORT XInputSDL_Axis(SDL_Gamepad* pPad, SDL_GamepadAxis axis, bool bInvert)
{
    int v = SDL_GetGamepadAxis(pPad, axis);
    if ( bInvert ) v = -v - 1; // -32768..32767 maps onto 32767..-32768
    return (SHORT)v;
}

static BYTE XInputSDL_Trigger(SDL_Gamepad* pPad, SDL_GamepadAxis axis)
{
    int v = SDL_GetGamepadAxis(pPad, axis); // 0..32767
    return (BYTE)((v < 0 ? 0 : v) * 255 / 32767);
}

DWORD XInputGetState(DWORD userIndex, XINPUT_STATE* pState)
{
    SDL_Gamepad* pPad = XInputSDL_GetGamepad(userIndex);
    if ( !pPad ) return ERROR_DEVICE_NOT_CONNECTED;

    static const struct { SDL_GamepadButton button; WORD mask; } aButtons[] = {
        { SDL_GAMEPAD_BUTTON_DPAD_UP, XINPUT_GAMEPAD_DPAD_UP }, { SDL_GAMEPAD_BUTTON_DPAD_DOWN, XINPUT_GAMEPAD_DPAD_DOWN },
        { SDL_GAMEPAD_BUTTON_DPAD_LEFT, XINPUT_GAMEPAD_DPAD_LEFT }, { SDL_GAMEPAD_BUTTON_DPAD_RIGHT, XINPUT_GAMEPAD_DPAD_RIGHT },
        { SDL_GAMEPAD_BUTTON_START, XINPUT_GAMEPAD_START }, { SDL_GAMEPAD_BUTTON_BACK, XINPUT_GAMEPAD_BACK },
        { SDL_GAMEPAD_BUTTON_LEFT_STICK, XINPUT_GAMEPAD_LEFT_THUMB }, { SDL_GAMEPAD_BUTTON_RIGHT_STICK, XINPUT_GAMEPAD_RIGHT_THUMB },
        { SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, XINPUT_GAMEPAD_LEFT_SHOULDER }, { SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, XINPUT_GAMEPAD_RIGHT_SHOULDER },
        { SDL_GAMEPAD_BUTTON_SOUTH, XINPUT_GAMEPAD_A }, { SDL_GAMEPAD_BUTTON_EAST, XINPUT_GAMEPAD_B },
        { SDL_GAMEPAD_BUTTON_WEST, XINPUT_GAMEPAD_X }, { SDL_GAMEPAD_BUTTON_NORTH, XINPUT_GAMEPAD_Y },
    };

    XINPUT_GAMEPAD pad = { 0 };
    for ( size_t i = 0; i < SDL_arraysize(aButtons); ++i )
    {
        if ( SDL_GetGamepadButton(pPad, aButtons[i].button) ) pad.wButtons |= aButtons[i].mask;
    }
    pad.bLeftTrigger  = XInputSDL_Trigger(pPad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
    pad.bRightTrigger = XInputSDL_Trigger(pPad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
    pad.sThumbLX      = XInputSDL_Axis(pPad, SDL_GAMEPAD_AXIS_LEFTX, false);
    pad.sThumbLY      = XInputSDL_Axis(pPad, SDL_GAMEPAD_AXIS_LEFTY, true);
    pad.sThumbRX      = XInputSDL_Axis(pPad, SDL_GAMEPAD_AXIS_RIGHTX, false);
    pad.sThumbRY      = XInputSDL_Axis(pPad, SDL_GAMEPAD_AXIS_RIGHTY, true);

    if ( memcmp(&pad, &XInputSDL_aLastState[userIndex], sizeof(pad)) != 0 )
    {
        XInputSDL_aLastState[userIndex] = pad;
        ++XInputSDL_aPacket[userIndex];
    }

    pState->dwPacketNumber = XInputSDL_aPacket[userIndex];
    pState->Gamepad        = pad;
    return ERROR_SUCCESS;
}

DWORD XInputSetState(DWORD userIndex, XINPUT_VIBRATION* pVibration)
{
    SDL_Gamepad* pPad = XInputSDL_GetGamepad(userIndex);
    if ( !pPad ) return ERROR_DEVICE_NOT_CONNECTED;
    // XInput keeps vibrating until the next call; SDL needs a duration
    SDL_RumbleGamepad(pPad, pVibration->wLeftMotorSpeed, pVibration->wRightMotorSpeed, 60000);
    return ERROR_SUCCESS;
}

DWORD XInputGetCapabilities(DWORD userIndex, DWORD flags, XINPUT_CAPABILITIES* pCaps)
{
    J3D_UNUSED(flags);
    if ( !XInputSDL_GetGamepad(userIndex) ) return ERROR_DEVICE_NOT_CONNECTED;
    memset(pCaps, 0, sizeof(XINPUT_CAPABILITIES));
    pCaps->Type    = XINPUT_DEVTYPE_GAMEPAD;
    pCaps->SubType = XINPUT_DEVSUBTYPE_GAMEPAD;
    pCaps->Gamepad.wButtons     = 0xF3FF;
    pCaps->Gamepad.bLeftTrigger = pCaps->Gamepad.bRightTrigger = 0xFF;
    pCaps->Gamepad.sThumbLX = pCaps->Gamepad.sThumbLY = pCaps->Gamepad.sThumbRX = pCaps->Gamepad.sThumbRY = (SHORT)0xFFC0;
    pCaps->Vibration.wLeftMotorSpeed = pCaps->Vibration.wRightMotorSpeed = 0xFFFF;
    return ERROR_SUCCESS;
}
