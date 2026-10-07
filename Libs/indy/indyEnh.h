#ifndef INDY_INDYENH_H
#define INDY_INDYENH_H
// Indycompiled enhancement and fix toggles (PROJECT.md §8.3, registry: Docs/Indycompiled/enhancements.md).
//
// Every deviation from the original game's behaviour has an ID and a toggle. Jones.cfg selects a profile and
// may override single toggles:
//   "indycompiled": { "profile": "enhanced", "toggles": { "analogMovement": true } }
// Profiles: "vanilla" (everything off), "fixed" (bug fixes only), "enhanced" (everything on, default).
#include <j3dcore/j3d.h>
#include <stdbool.h>

J3D_EXTERN_C_START

typedef enum eIndyEnh
{
    // Order and IDs are permanent: add new entries at the end, never renumber.
    INDY_ENH_ANALOG_MOVEMENT = 0, // ENH-0001: gamepad stick deflection scales walking/running and turning
    INDY_FIX_TURN_RATE       = 1, // FIX-0001: key/stick turning speed independent of the frame rate
    INDY_FIX_CONTINUOUS_DAMAGE = 2, // FIX-0002: drowning/raft/IMP damage independent of the frame rate
    INDY_FIX_TIME_CYCLES       = 3, // FIX-0003: "every N-th frame" events (idle anims, breathing, ...) on game time
    INDY_FIX_JEWEL_FLY_THRUST  = 4, // FIX-0004: jewel-flight up/down acceleration independent of the frame rate
    INDY_ENH_RIGHT_STICK_CAMERA = 5, // ENH-0002: right stick swings the third-person camera around Indy
    INDY_ENH_QUICK_DIRECTION    = 6, // ENH-0003: reversing doesn't wait for the stop animation (input grace period)
    INDY_ENH_SKIP_INTRO         = 7, // ENH-0004: no intro video at game start
    INDY_ENH_MODERN_CONTROLS    = 8, // ENH-0005: camera-relative left stick (modern controls)
    INDY_ENH_FRAMES_4TO3        = 9, // ENH-0006: 4:3 content (cutscenes, loading map, movies) in a centred 4:3 frame
    INDY_ENH_COUNT
} IndyEnh;

typedef enum eIndyProfile
{
    INDY_PROFILE_VANILLA  = 0,
    INDY_PROFILE_FIXED    = 1,
    INDY_PROFILE_ENHANCED = 2,
} IndyProfile;

// True if the toggle is on. Settings are read from Jones.cfg on first use after the config system started.
// "indycompiled.toggles" only holds explicit overrides; toggles not listed there follow the profile.
bool J3DAPI indyEnh_IsEnabled(IndyEnh id);

IndyProfile indyEnh_GetProfile(void);

// FIX-0001: the frame rate key/stick turning is calibrated to ("indycompiled.turnRateFps", default 60, 15..240).
float indyEnh_GetTurnRateFps(void);

// Re-read Jones.cfg (e.g. after the settings changed).
void indyEnh_Reload(void);

J3D_EXTERN_C_END
#endif // INDY_INDYENH_H
