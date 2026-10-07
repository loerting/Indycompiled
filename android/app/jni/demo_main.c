// Indycompiled Android demo: plays the intro movie jonesopn.snm from the APK assets with
// Libs/smush/smushDecoder.c. Video: RGB565 frames into an SDL texture, letterboxed 4:3.
// Audio: PCM16 into an SDL audio stream. A tap (or Back/Escape) exits; so does the end of the movie.
// The game data is packed into the APK locally by Scripts/android/pack_data.sh (never committed).
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "smushDecoder.h"

#define DEMO_MOVIE "jonesopn.snm"

static size_t ReadIO(void* pUser, void* pBuffer, size_t size)
{
    return SDL_ReadIO((SDL_IOStream*)pUser, pBuffer, size);
}

static void Fail(const char* pWhat)
{
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s: %s", pWhat, SDL_GetError());
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Indycompiled", pWhat, NULL);
}

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
    if ( !SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) )
    {
        // Audio may be missing (headless emulator): carry on without it
        SDL_Log("SDL_Init with audio failed (%s), retrying without audio", SDL_GetError());
        if ( !SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) )
        {
            Fail("SDL_Init");
            return 1;
        }
    }

    SDL_Window* pWindow = NULL;
    SDL_Renderer* pRenderer = NULL;
    if ( !SDL_CreateWindowAndRenderer("Indycompiled", 0, 0, SDL_WINDOW_FULLSCREEN, &pWindow, &pRenderer) )
    {
        Fail("SDL_CreateWindowAndRenderer");
        return 1;
    }
    SDL_SetRenderVSync(pRenderer, 1);

    SDL_IOStream* pFile = SDL_IOFromFile(DEMO_MOVIE, "rb"); // relative: internal storage first, then APK assets
    if ( !pFile )
    {
        Fail(DEMO_MOVIE " not found: pack the game data (Scripts/android/pack_data.sh) and rebuild");
        return 1;
    }
    SmushDecoder* pDecoder = SmushDecoder_Open(ReadIO, pFile);
    if ( !pDecoder )
    {
        Fail(DEMO_MOVIE " is not a SANM movie");
        return 1;
    }
    const SmushInfo* pInfo = SmushDecoder_GetInfo(pDecoder);
    const Uint64 frameNs = (Uint64)(pInfo->frameDuration > 0 ? pInfo->frameDuration : 66667) * 1000;
    SDL_Log("%s: %lld bytes, %dx%d, %d frames, %d us/frame, audio %d Hz", DEMO_MOVIE, (long long)SDL_GetIOSize(pFile),
            pInfo->width, pInfo->height, pInfo->numFrames, pInfo->frameDuration, pInfo->audioRate);

    SDL_Texture* pTexture = SDL_CreateTexture(pRenderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, pInfo->width, pInfo->height);
    if ( !pTexture )
    {
        Fail("SDL_CreateTexture");
        return 1;
    }
    SDL_SetRenderLogicalPresentation(pRenderer, 640, 480, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetRenderDrawColor(pRenderer, 0, 0, 0, 255);

    SDL_AudioStream* pAudio = NULL;
    bool bAudioTried = false;
    bool bRunning = true;
    int numShown = 0;
    const Uint64 startNs = SDL_GetTicksNS();
    for ( int frameIndex = 0; bRunning; frameIndex++ )
    {
        SDL_Event event;
        while ( SDL_PollEvent(&event) )
        {
            switch ( event.type )
            {
                case SDL_EVENT_QUIT:
                case SDL_EVENT_TERMINATING:
                case SDL_EVENT_FINGER_UP:
                case SDL_EVENT_MOUSE_BUTTON_UP:
                    bRunning = false;
                    break;
                case SDL_EVENT_KEY_UP:
                    if ( event.key.scancode == SDL_SCANCODE_AC_BACK || event.key.scancode == SDL_SCANCODE_ESCAPE )
                    {
                        bRunning = false;
                    }
                    break;
                default:
                    break;
            }
        }
        if ( !bRunning )
        {
            SDL_Log("tap/back: exiting at frame %d", frameIndex);
            break;
        }

        // Skip pictures nobody depends on when more than one frame late
        const Uint64 dueNs = startNs + (Uint64)frameIndex * frameNs;
        const bool bLate = SDL_GetTicksNS() > dueNs + frameNs;
        SmushFrame frame;
        const int result = SmushDecoder_DecodeFrame(pDecoder, bLate, &frame);
        if ( result <= 0 )
        {
            SDL_Log(result < 0 ? "decode error at frame %d" : "end of movie after %d frames", frameIndex);
            break;
        }

        if ( frame.pAudio && frame.numAudioSamples )
        {
            if ( !bAudioTried )
            {
                bAudioTried = true;
                const SDL_AudioSpec spec = { SDL_AUDIO_S16, frame.numAudioChannels, pInfo->audioRate };
                pAudio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
                if ( pAudio )
                {
                    SDL_ResumeAudioStreamDevice(pAudio);
                    SDL_Log("audio: %d Hz, %d channel(s)", spec.freq, spec.channels);
                }
                else
                {
                    SDL_Log("audio: no output (%s), playing silently", SDL_GetError());
                }
            }
            if ( pAudio )
            {
                SDL_PutAudioStreamData(pAudio, frame.pAudio, (int)(frame.numAudioSamples * frame.numAudioChannels * sizeof(int16_t)));
            }
        }

        if ( frame.pPixels )
        {
            SDL_UpdateTexture(pTexture, NULL, frame.pPixels, pInfo->width * (int)sizeof(uint16_t));
            numShown++;
        }

        const Uint64 nowNs = SDL_GetTicksNS();
        if ( nowNs < dueNs )
        {
            SDL_DelayNS(dueNs - nowNs);
        }
        SDL_RenderClear(pRenderer);
        SDL_RenderTexture(pRenderer, pTexture, NULL, NULL);
        SDL_RenderPresent(pRenderer);

        if ( frameIndex % 75 == 0 )
        {
            SDL_Log("frame %d/%d, %d pictures shown, %.1f s", frameIndex, pInfo->numFrames, numShown, (double)(SDL_GetTicksNS() - startNs) / 1e9);
        }
    }

    SDL_DestroyAudioStream(pAudio);
    SmushDecoder_Close(pDecoder);
    SDL_CloseIO(pFile);
    SDL_DestroyTexture(pTexture);
    SDL_DestroyRenderer(pRenderer);
    SDL_DestroyWindow(pWindow);
    SDL_Quit();
    return 0;
}
