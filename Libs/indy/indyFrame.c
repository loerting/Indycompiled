#include "indyFrame.h"
#include "indyEnh.h"

#include <sith/Main/sithMain.h>

#include <math.h>

static size_t indyFrame_curIntended;  // intended frames up to and including this frame
static size_t indyFrame_lastIntended; // ... up to the previous frame
static float indyFrame_secAccumulator;
static unsigned int indyFrame_testCount;

void indyFrame_Reset(void)
{
    indyFrame_curIntended    = 0;
    indyFrame_lastIntended   = 0;
    indyFrame_secAccumulator = 0.0f;
    indyFrame_testCount      = 0;
}

void indyFrame_Advance(float secFrameTime)
{
    const float secIntended = 1.0f / INDY_FRAME_INTENDED_FPS;
    indyFrame_lastIntended  = indyFrame_curIntended;
    indyFrame_secAccumulator += secFrameTime > 0.0f ? secFrameTime : 0.0f;

    size_t steps = (size_t)floorf(indyFrame_secAccumulator / secIntended);
    indyFrame_secAccumulator -= (float)steps * secIntended;
    indyFrame_curIntended += steps;

    indyFrame_testCount += indyFrame_IsTimeCycle(0, 16) ? 1 : 0;
}

int J3DAPI indyFrame_IsTimeCycle(size_t offset, size_t n)
{
    if ( !indyEnh_IsEnabled(INDY_FIX_TIME_CYCLES) )
    {
        return (((uint8_t)sithMain_g_frameNumber + (uint8_t)offset) & (n - 1)) == 0;
    }

    // did an intended frame k with (k + offset) % n == 0 pass since the previous frame, i.e. in (last, cur]?
    if ( indyFrame_curIntended <= indyFrame_lastIntended )
    {
        return 0;
    }

    size_t first = indyFrame_lastIntended + 1;
    size_t count = indyFrame_curIntended - indyFrame_lastIntended;
    if ( count >= n )
    {
        return 1;
    }

    size_t toNext = (n - (first + offset) % n) % n; // steps from first to the next hit
    return toNext < count;
}

unsigned int indyFrame_GetTestCount(void)
{
    return indyFrame_testCount;
}
