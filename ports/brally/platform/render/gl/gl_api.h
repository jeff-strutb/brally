/* gl_api.h: the OpenGL 3.3 core the GL renderers (brr_gl.c, Top Gear Rally's rdr_gl.c) use, declared
 * here (types, the enums it names, one function pointer each) and loaded at
 * start-up through the context's own lookup (gl_ctx.c): no system GL header
 * is needed, so Windows' OpenGL 1.1 headers and the Mac's deprecated ones
 * never come into it. */
#ifndef BR_GL_API_H
#define BR_GL_API_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#define GLAPIENTRY __stdcall
#else
#define GLAPIENTRY
#endif

typedef unsigned int GLenum, GLuint, GLbitfield;
typedef int GLint, GLsizei;
typedef unsigned char GLboolean, GLubyte;
typedef float GLfloat;
typedef double GLdouble;
typedef char GLchar;
typedef ptrdiff_t GLsizeiptr, GLintptr;

#define GL_FALSE 0
#define GL_TRUE 1
#define GL_ZERO 0
#define GL_ONE 1
#define GL_TRIANGLES 0x0004
#define GL_TRIANGLE_STRIP 0x0005
#define GL_NEVER 0x0200
#define GL_LESS 0x0201
#define GL_EQUAL 0x0202
#define GL_LEQUAL 0x0203
#define GL_GREATER 0x0204
#define GL_NOTEQUAL 0x0205
#define GL_GEQUAL 0x0206
#define GL_ALWAYS 0x0207
#define GL_SRC_COLOR 0x0300
#define GL_ONE_MINUS_SRC_COLOR 0x0301
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_DST_COLOR 0x0306
#define GL_ONE_MINUS_DST_COLOR 0x0307
#define GL_CULL_FACE 0x0B44
#define GL_DEPTH_TEST 0x0B71
#define GL_BLEND 0x0BE2
#define GL_SCISSOR_TEST 0x0C11
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_PACK_ALIGNMENT 0x0D05
#define GL_UNPACK_ROW_LENGTH 0x0CF2
#define GL_TEXTURE_2D 0x0DE1
#define GL_UNSIGNED_BYTE 0x1401
#define GL_FLOAT 0x1406
#define GL_RED 0x1903
#define GL_RGBA 0x1908
#define GL_DEPTH_COMPONENT 0x1902
#define GL_VENDOR 0x1F00
#define GL_RENDERER 0x1F01
#define GL_VERSION 0x1F02
#define GL_EXTENSIONS 0x1F03
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_NEAREST_MIPMAP_NEAREST 0x2700
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_REPEAT 0x2901
#define GL_COLOR_BUFFER_BIT 0x4000
#define GL_DEPTH_BUFFER_BIT 0x0100
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_TEXTURE_MAX_LEVEL 0x813D
#define GL_TEXTURE_MAX_LOD 0x813B
#define GL_RG 0x8227
#define GL_RG8 0x822B
#define GL_R8 0x8229
#define GL_RGBA8 0x8058
#define GL_RGBA32F 0x8814
#define GL_DEPTH_COMPONENT32F 0x8CAC
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE_MAX_ANISOTROPY 0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY 0x84FF
#define GL_TEXTURE_2D_ARRAY 0x8C1A
#define GL_TEXTURE_BUFFER 0x8C2A
#define GL_ARRAY_BUFFER 0x8892
#define GL_UNIFORM_BUFFER 0x8A11
#define GL_STREAM_DRAW 0x88E0
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_STATIC_DRAW 0x88E4
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_FRAMEBUFFER 0x8D40
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_FUNC_ADD 0x8006
#define GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT 0x8A34
#define GL_NUM_EXTENSIONS 0x821D
#define GL_MAP_WRITE_BIT 0x0002
#define GL_MAP_INVALIDATE_RANGE_BIT 0x0004
#define GL_MAP_UNSYNCHRONIZED_BIT 0x0020
#define GL_BGRA 0x80E1
#define GL_DEPTH_CLAMP 0x864F
#define GL_SAMPLE_ALPHA_TO_COVERAGE 0x809E
#define GL_TEXTURE_2D_MULTISAMPLE 0x9100

/* the functions, one pointer each (gl_api_load fills them) */
#define GL_FUNCS(X) \
    X(const GLubyte *, glGetString, (GLenum)) \
    X(const GLubyte *, glGetStringi, (GLenum, GLuint)) \
    X(void, glGetIntegerv, (GLenum, GLint *)) \
    X(void, glGetFloatv, (GLenum, GLfloat *)) \
    X(GLenum, glGetError, (void)) \
    X(void, glEnable, (GLenum)) \
    X(void, glDisable, (GLenum)) \
    X(void, glViewport, (GLint, GLint, GLsizei, GLsizei)) \
    X(void, glScissor, (GLint, GLint, GLsizei, GLsizei)) \
    X(void, glClearColor, (GLfloat, GLfloat, GLfloat, GLfloat)) \
    X(void, glClearDepth, (GLdouble)) \
    X(void, glClear, (GLbitfield)) \
    X(void, glBlendFuncSeparate, (GLenum, GLenum, GLenum, GLenum)) \
    X(void, glBlendEquation, (GLenum)) \
    X(void, glDepthFunc, (GLenum)) \
    X(void, glDepthMask, (GLboolean)) \
    X(void, glColorMask, (GLboolean, GLboolean, GLboolean, GLboolean)) \
    X(void, glGenTextures, (GLsizei, GLuint *)) \
    X(void, glDeleteTextures, (GLsizei, const GLuint *)) \
    X(void, glBindTexture, (GLenum, GLuint)) \
    X(void, glActiveTexture, (GLenum)) \
    X(void, glTexImage2D, (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *)) \
    X(void, glTexSubImage2D, (GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void *)) \
    X(void, glTexImage3D, (GLenum, GLint, GLint, GLsizei, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *)) \
    X(void, glTexSubImage3D, (GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLenum, const void *)) \
    X(void, glTexParameteri, (GLenum, GLenum, GLint)) \
    X(void, glTexBuffer, (GLenum, GLenum, GLuint)) \
    X(void, glPixelStorei, (GLenum, GLint)) \
    X(void, glReadPixels, (GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *)) \
    X(void, glDrawArrays, (GLenum, GLint, GLsizei)) \
    X(void, glFinish, (void)) \
    X(void, glGenBuffers, (GLsizei, GLuint *)) \
    X(void, glDeleteBuffers, (GLsizei, const GLuint *)) \
    X(void, glBindBuffer, (GLenum, GLuint)) \
    X(void, glBufferData, (GLenum, GLsizeiptr, const void *, GLenum)) \
    X(void, glBufferSubData, (GLenum, GLintptr, GLsizeiptr, const void *)) \
    X(void *, glMapBufferRange, (GLenum, GLintptr, GLsizeiptr, GLbitfield)) \
    X(GLboolean, glUnmapBuffer, (GLenum)) \
    X(void, glBindBufferBase, (GLenum, GLuint, GLuint)) \
    X(void, glBindBufferRange, (GLenum, GLuint, GLuint, GLintptr, GLsizeiptr)) \
    X(void, glGenVertexArrays, (GLsizei, GLuint *)) \
    X(void, glBindVertexArray, (GLuint)) \
    X(void, glVertexAttribPointer, (GLuint, GLint, GLenum, GLboolean, GLsizei, const void *)) \
    X(void, glEnableVertexAttribArray, (GLuint)) \
    X(GLuint, glCreateShader, (GLenum)) \
    X(void, glShaderSource, (GLuint, GLsizei, const GLchar *const *, const GLint *)) \
    X(void, glCompileShader, (GLuint)) \
    X(void, glGetShaderiv, (GLuint, GLenum, GLint *)) \
    X(void, glGetShaderInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *)) \
    X(GLuint, glCreateProgram, (void)) \
    X(void, glAttachShader, (GLuint, GLuint)) \
    X(void, glBindAttribLocation, (GLuint, GLuint, const GLchar *)) \
    X(void, glBindFragDataLocation, (GLuint, GLuint, const GLchar *)) \
    X(void, glLinkProgram, (GLuint)) \
    X(void, glGetProgramiv, (GLuint, GLenum, GLint *)) \
    X(void, glGetProgramInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *)) \
    X(void, glUseProgram, (GLuint)) \
    X(GLint, glGetUniformLocation, (GLuint, const GLchar *)) \
    X(void, glUniform1i, (GLint, GLint)) \
    X(void, glUniform4f, (GLint, GLfloat, GLfloat, GLfloat, GLfloat)) \
    X(GLuint, glGetUniformBlockIndex, (GLuint, const GLchar *)) \
    X(void, glUniformBlockBinding, (GLuint, GLuint, GLuint)) \
    X(void, glGenFramebuffers, (GLsizei, GLuint *)) \
    X(void, glDeleteFramebuffers, (GLsizei, const GLuint *)) \
    X(void, glBindFramebuffer, (GLenum, GLuint)) \
    X(void, glFramebufferTexture2D, (GLenum, GLenum, GLenum, GLuint, GLint)) \
    X(GLenum, glCheckFramebufferStatus, (GLenum)) \
    X(void, glTexImage2DMultisample, (GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLboolean)) \
    X(void, glBlitFramebuffer, (GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum)) \
    X(void, glGenSamplers, (GLsizei, GLuint *)) \
    X(void, glSamplerParameteri, (GLuint, GLenum, GLint)) \
    X(void, glSamplerParameterf, (GLuint, GLenum, GLfloat)) \
    X(void, glBindSampler, (GLuint, GLuint))

#define GL_DECL(ret, name, args) typedef ret (GLAPIENTRY *PFN_##name) args; extern PFN_##name name##_p;
GL_FUNCS(GL_DECL)
#undef GL_DECL

/* the renderer's code calls them by their own names */
#define glGetString glGetString_p
#define glGetStringi glGetStringi_p
#define glGetIntegerv glGetIntegerv_p
#define glGetFloatv glGetFloatv_p
#define glGetError glGetError_p
#define glEnable glEnable_p
#define glDisable glDisable_p
#define glViewport glViewport_p
#define glScissor glScissor_p
#define glClearColor glClearColor_p
#define glClearDepth glClearDepth_p
#define glClear glClear_p
#define glBlendFuncSeparate glBlendFuncSeparate_p
#define glBlendEquation glBlendEquation_p
#define glDepthFunc glDepthFunc_p
#define glDepthMask glDepthMask_p
#define glColorMask glColorMask_p
#define glGenTextures glGenTextures_p
#define glDeleteTextures glDeleteTextures_p
#define glBindTexture glBindTexture_p
#define glActiveTexture glActiveTexture_p
#define glTexImage2D glTexImage2D_p
#define glTexSubImage2D glTexSubImage2D_p
#define glTexImage3D glTexImage3D_p
#define glTexSubImage3D glTexSubImage3D_p
#define glTexParameteri glTexParameteri_p
#define glTexBuffer glTexBuffer_p
#define glPixelStorei glPixelStorei_p
#define glReadPixels glReadPixels_p
#define glDrawArrays glDrawArrays_p
#define glFinish glFinish_p
#define glGenBuffers glGenBuffers_p
#define glDeleteBuffers glDeleteBuffers_p
#define glBindBuffer glBindBuffer_p
#define glBufferData glBufferData_p
#define glBufferSubData glBufferSubData_p
#define glMapBufferRange glMapBufferRange_p
#define glUnmapBuffer glUnmapBuffer_p
#define glBindBufferBase glBindBufferBase_p
#define glBindBufferRange glBindBufferRange_p
#define glGenVertexArrays glGenVertexArrays_p
#define glBindVertexArray glBindVertexArray_p
#define glVertexAttribPointer glVertexAttribPointer_p
#define glEnableVertexAttribArray glEnableVertexAttribArray_p
#define glCreateShader glCreateShader_p
#define glShaderSource glShaderSource_p
#define glCompileShader glCompileShader_p
#define glGetShaderiv glGetShaderiv_p
#define glGetShaderInfoLog glGetShaderInfoLog_p
#define glCreateProgram glCreateProgram_p
#define glAttachShader glAttachShader_p
#define glBindAttribLocation glBindAttribLocation_p
#define glBindFragDataLocation glBindFragDataLocation_p
#define glLinkProgram glLinkProgram_p
#define glGetProgramiv glGetProgramiv_p
#define glGetProgramInfoLog glGetProgramInfoLog_p
#define glUseProgram glUseProgram_p
#define glGetUniformLocation glGetUniformLocation_p
#define glUniform1i glUniform1i_p
#define glUniform4f glUniform4f_p
#define glGetUniformBlockIndex glGetUniformBlockIndex_p
#define glUniformBlockBinding glUniformBlockBinding_p
#define glGenFramebuffers glGenFramebuffers_p
#define glDeleteFramebuffers glDeleteFramebuffers_p
#define glBindFramebuffer glBindFramebuffer_p
#define glFramebufferTexture2D glFramebufferTexture2D_p
#define glCheckFramebufferStatus glCheckFramebufferStatus_p
#define glTexImage2DMultisample glTexImage2DMultisample_p
#define glBlitFramebuffer glBlitFramebuffer_p
#define glGenSamplers glGenSamplers_p
#define glSamplerParameteri glSamplerParameteri_p
#define glSamplerParameterf glSamplerParameterf_p
#define glBindSampler glBindSampler_p

/* ---- the context (gl_ctx.c, built with the system's own headers) ---------------------- */
/* a 3.3 core context current on this thread: on the window (an HWND on Windows) when win is
 * not NULL, else offscreen (headless runs, shots); 1 on success */
int   gl_ctx_open(void *win);
/* a GL function's address (wglGetProcAddress, else opengl32.dll's; the Mac's from its framework) */
void *gl_ctx_proc(const char *name);
/* the window's back buffer shown (nothing offscreen) */
void  gl_ctx_swap(void);
/* every pointer above looked up: 1 when all were found */
int   gl_api_load(void);

#endif
