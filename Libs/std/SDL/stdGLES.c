// Native builds: loads the OpenGL ES 3.0 functions of stdGLES.h through SDL.
#include "stdGLES.h"

#include <SDL3/SDL.h>
#include <string.h>

#define STDGLES_DEFINE(ret, name, args) PFN_##name name = NULL;
STDGLES_FUNCTIONS(STDGLES_DEFINE)
#undef STDGLES_DEFINE

bool stdGLES_LoadFunctions(void)
{
    bool bOk = true;
#define STDGLES_LOAD(ret, name, args) \
    name = (PFN_##name)SDL_GL_GetProcAddress(#name); \
    if ( !name ) { SDL_Log("OpenGL ES function %s not found", #name); bOk = false; }
    STDGLES_FUNCTIONS(STDGLES_LOAD)
#undef STDGLES_LOAD
    return bOk;
}

bool stdGLES_HasExtension(const char* pName)
{
    GLint num = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &num);
    for ( GLint i = 0; i < num; ++i )
    {
        const char* pExt = (const char*)glGetStringi(GL_EXTENSIONS, (GLuint)i);
        if ( pExt && strcmp(pExt, pName) == 0 ) return true;
    }
    return false;
}
