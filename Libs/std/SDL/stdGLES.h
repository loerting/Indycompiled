// Native builds: the OpenGL ES 3.0 functions and constants the renderer uses (stdDisplaySDL.c, std3DGLES.c), loaded
// through SDL_GL_GetProcAddress, so no GL headers or import libraries are needed (Linux, Android).
#ifndef STD_SDL_STDGLES_H
#define STD_SDL_STDGLES_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef APIENTRY_GL
#  if defined(_WIN32)
#    define APIENTRY_GL __stdcall
#  else
#    define APIENTRY_GL
#  endif
#endif

typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef float GLfloat;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef char GLchar;
typedef unsigned char GLubyte;
typedef intptr_t GLintptr;
typedef intptr_t GLsizeiptr;

#define GL_FALSE                        0
#define GL_TRUE                         1
#define GL_NO_ERROR                     0
#define GL_DEPTH_BUFFER_BIT             0x00000100
#define GL_STENCIL_BUFFER_BIT           0x00000400
#define GL_COLOR_BUFFER_BIT             0x00004000
#define GL_POINTS                       0x0000
#define GL_LINE_STRIP                   0x0003
#define GL_TRIANGLES                    0x0004
#define GL_LEQUAL                       0x0203
#define GL_SRC_ALPHA                    0x0302
#define GL_ONE_MINUS_SRC_ALPHA          0x0303
#define GL_CULL_FACE                    0x0B44
#define GL_DEPTH_TEST                   0x0B71
#define GL_BLEND                        0x0BE2
#define GL_DITHER                       0x0BD0
#define GL_SCISSOR_TEST                 0x0C11
#define GL_UNPACK_ALIGNMENT             0x0CF5
#define GL_PACK_ALIGNMENT               0x0D05
#define GL_MAX_TEXTURE_SIZE             0x0D33
#define GL_TEXTURE_2D                   0x0DE1
#define GL_UNSIGNED_BYTE                0x1401
#define GL_UNSIGNED_SHORT               0x1403
#define GL_FLOAT                        0x1406
#define GL_RGBA                         0x1908
#define GL_VENDOR                       0x1F00
#define GL_RENDERER                     0x1F01
#define GL_VERSION                      0x1F02
#define GL_EXTENSIONS                   0x1F03
#define GL_NEAREST                      0x2600
#define GL_LINEAR                       0x2601
#define GL_NEAREST_MIPMAP_NEAREST       0x2700
#define GL_LINEAR_MIPMAP_NEAREST        0x2701
#define GL_NEAREST_MIPMAP_LINEAR        0x2702
#define GL_LINEAR_MIPMAP_LINEAR         0x2703
#define GL_TEXTURE_MAG_FILTER           0x2800
#define GL_TEXTURE_MIN_FILTER           0x2801
#define GL_TEXTURE_WRAP_S               0x2802
#define GL_TEXTURE_WRAP_T               0x2803
#define GL_REPEAT                       0x2901
#define GL_CLAMP_TO_EDGE                0x812F
#define GL_TEXTURE_MAX_LEVEL            0x813D
#define GL_RGBA8                        0x8058
#define GL_TEXTURE0                     0x84C0
#define GL_TEXTURE_MAX_ANISOTROPY_EXT   0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#define GL_ARRAY_BUFFER                 0x8892
#define GL_ELEMENT_ARRAY_BUFFER         0x8893
#define GL_STREAM_DRAW                  0x88E0
#define GL_STATIC_DRAW                  0x88E4
#define GL_MAP_WRITE_BIT                0x0002
#define GL_MAP_INVALIDATE_RANGE_BIT     0x0004
#define GL_MAP_UNSYNCHRONIZED_BIT       0x0020
#define GL_FRAGMENT_SHADER              0x8B30
#define GL_VERTEX_SHADER                0x8B31
#define GL_COMPILE_STATUS               0x8B81
#define GL_LINK_STATUS                  0x8B82
#define GL_INFO_LOG_LENGTH              0x8B84
#define GL_TEXTURE_SWIZZLE_A            0x8E45
#define GL_ONE                          1
#define GL_NUM_EXTENSIONS               0x821D
#define GL_FRAMEBUFFER                  0x8D40

#define STDGLES_FUNCTIONS(X) \
    X(void, glActiveTexture, (GLenum texture)) \
    X(void, glAttachShader, (GLuint program, GLuint shader)) \
    X(void, glBindAttribLocation, (GLuint program, GLuint index, const GLchar* name)) \
    X(void, glBindBuffer, (GLenum target, GLuint buffer)) \
    X(void, glBindSampler, (GLuint unit, GLuint sampler)) \
    X(void, glBindTexture, (GLenum target, GLuint texture)) \
    X(void, glBindVertexArray, (GLuint array)) \
    X(void, glBlendFunc, (GLenum sfactor, GLenum dfactor)) \
    X(void, glBufferData, (GLenum target, GLsizeiptr size, const void* data, GLenum usage)) \
    X(void, glBufferSubData, (GLenum target, GLintptr offset, GLsizeiptr size, const void* data)) \
    X(void, glClear, (GLbitfield mask)) \
    X(void, glClearColor, (GLfloat r, GLfloat g, GLfloat b, GLfloat a)) \
    X(void, glClearDepthf, (GLfloat d)) \
    X(void, glClearStencil, (GLint s)) \
    X(void, glCompileShader, (GLuint shader)) \
    X(GLuint, glCreateProgram, (void)) \
    X(GLuint, glCreateShader, (GLenum type)) \
    X(void, glDeleteBuffers, (GLsizei n, const GLuint* buffers)) \
    X(void, glDeleteProgram, (GLuint program)) \
    X(void, glDeleteSamplers, (GLsizei n, const GLuint* samplers)) \
    X(void, glDeleteShader, (GLuint shader)) \
    X(void, glDeleteTextures, (GLsizei n, const GLuint* textures)) \
    X(void, glDeleteVertexArrays, (GLsizei n, const GLuint* arrays)) \
    X(void, glDepthFunc, (GLenum func)) \
    X(void, glDepthMask, (GLboolean flag)) \
    X(void, glDisable, (GLenum cap)) \
    X(void, glDrawArrays, (GLenum mode, GLint first, GLsizei count)) \
    X(void, glDrawElements, (GLenum mode, GLsizei count, GLenum type, const void* indices)) \
    X(void, glEnable, (GLenum cap)) \
    X(void, glEnableVertexAttribArray, (GLuint index)) \
    X(void, glFinish, (void)) \
    X(void, glGenBuffers, (GLsizei n, GLuint* buffers)) \
    X(void, glGenSamplers, (GLsizei n, GLuint* samplers)) \
    X(void, glGenTextures, (GLsizei n, GLuint* textures)) \
    X(void, glGenVertexArrays, (GLsizei n, GLuint* arrays)) \
    X(void, glGenerateMipmap, (GLenum target)) \
    X(GLenum, glGetError, (void)) \
    X(void, glGetFloatv, (GLenum pname, GLfloat* data)) \
    X(void, glGetIntegerv, (GLenum pname, GLint* data)) \
    X(void, glGetProgramInfoLog, (GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog)) \
    X(void, glGetProgramiv, (GLuint program, GLenum pname, GLint* params)) \
    X(void, glGetShaderInfoLog, (GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog)) \
    X(void, glGetShaderiv, (GLuint shader, GLenum pname, GLint* params)) \
    X(const GLubyte*, glGetString, (GLenum name)) \
    X(const GLubyte*, glGetStringi, (GLenum name, GLuint index)) \
    X(GLint, glGetUniformLocation, (GLuint program, const GLchar* name)) \
    X(void, glLinkProgram, (GLuint program)) \
    X(void*, glMapBufferRange, (GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access)) \
    X(void, glPixelStorei, (GLenum pname, GLint param)) \
    X(GLboolean, glUnmapBuffer, (GLenum target)) \
    X(void, glReadPixels, (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels)) \
    X(void, glSamplerParameterf, (GLuint sampler, GLenum pname, GLfloat param)) \
    X(void, glSamplerParameteri, (GLuint sampler, GLenum pname, GLint param)) \
    X(void, glScissor, (GLint x, GLint y, GLsizei width, GLsizei height)) \
    X(void, glShaderSource, (GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length)) \
    X(void, glTexImage2D, (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels)) \
    X(void, glTexParameteri, (GLenum target, GLenum pname, GLint param)) \
    X(void, glTexSubImage2D, (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels)) \
    X(void, glUniform1f, (GLint location, GLfloat v0)) \
    X(void, glUniform1i, (GLint location, GLint v0)) \
    X(void, glUniform4f, (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3)) \
    X(void, glUseProgram, (GLuint program)) \
    X(void, glVertexAttribPointer, (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer)) \
    X(void, glViewport, (GLint x, GLint y, GLsizei width, GLsizei height))

#define STDGLES_DECLARE(ret, name, args) typedef ret (APIENTRY_GL* PFN_##name) args; extern PFN_##name name;
STDGLES_FUNCTIONS(STDGLES_DECLARE)
#undef STDGLES_DECLARE

bool stdGLES_LoadFunctions(void);            // after the context was created; false if a function is missing
bool stdGLES_HasExtension(const char* pName);

// std3DGLES.c: draws the queued 3D draws; the display module calls it before it uses GL or ends the frame
void std3D_FlushDraws(void);

#endif // STD_SDL_STDGLES_H
