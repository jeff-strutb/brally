/* gl_ctx.c: the OpenGL renderer's context (gl_api.h), built with the
 * system's own headers (link64.sh, as the OS layer):
 *
 *   Windows  WGL on the host's window: a pixel format with double buffering,
 *            a legacy context to find wglCreateContextAttribsARB, then a 3.3
 *            core one; offscreen, the same on a hidden window of its own
 *   macOS    CGL, a 3.2+ core profile (4.1 in practice), always offscreen: the
 *            Mac draws with Metal, this is for checking the GL renderer there
 *
 * Anything else: no context (the renderer fails to open). */
#include <stdio.h>
#include <string.h>

#include "gl_api.h"

#define GL_DEF(ret, name, args) PFN_##name name##_p;
GL_FUNCS(GL_DEF)
#undef GL_DEF

int gl_api_load(void)
{
    int ok = 1;
#define GL_LOAD(ret, name, args) \
    if (!(name##_p = (PFN_##name)gl_ctx_proc(#name))) { fprintf(stderr, "brr: gl: no %s\n", #name); ok = 0; }
    GL_FUNCS(GL_LOAD)
#undef GL_LOAD
    return ok;
}

#if defined(_WIN32)
#include <windows.h>

#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
typedef HGLRC (WINAPI *PFNCREATEATTRIBS)(HDC, HGLRC, const int *);
typedef BOOL (WINAPI *PFNSWAPINTERVAL)(int);

static HDC s_dc;
static HGLRC s_rc;
static HMODULE s_gl;

void *gl_ctx_proc(const char *name)
{
    void *p = (void *)wglGetProcAddress(name);
    if (!p || p == (void *)1 || p == (void *)2 || p == (void *)3 || p == (void *)-1) {   /* the 1.1 ones: opengl32.dll's */
        if (!s_gl)
            s_gl = LoadLibraryA("opengl32.dll");
        p = s_gl ? (void *)GetProcAddress(s_gl, name) : NULL;
    }
    return p;
}

int gl_ctx_open(void *win)
{
    HWND w = (HWND)win;
    PIXELFORMATDESCRIPTOR pfd;
    HGLRC legacy;
    PFNCREATEATTRIBS create;
    PFNSWAPINTERVAL interval;
    int pf;
    static const int attrs[] = { WGL_CONTEXT_MAJOR_VERSION_ARB, 3, WGL_CONTEXT_MINOR_VERSION_ARB, 3,
                                 WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB, 0 };
    if (!w) {   /* offscreen: a hidden window to hold the context */
        WNDCLASSA wc;
        memset(&wc, 0, sizeof wc);
        wc.style = CS_OWNDC;
        wc.lpfnWndProc = DefWindowProcA;
        wc.hInstance = GetModuleHandleA(NULL);
        wc.lpszClassName = "BrGlHidden";
        RegisterClassA(&wc);
        w = CreateWindowExA(0, "BrGlHidden", "", WS_OVERLAPPEDWINDOW, 0, 0, 16, 16, NULL, NULL, wc.hInstance, NULL);
        if (!w)
            return 0;
    }
    s_dc = GetDC(w);
    memset(&pfd, 0, sizeof pfd);
    pfd.nSize = sizeof pfd;
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    if (!(pf = ChoosePixelFormat(s_dc, &pfd)) || !SetPixelFormat(s_dc, pf, &pfd))
        return 0;
    if (!(legacy = wglCreateContext(s_dc)) || !wglMakeCurrent(s_dc, legacy))
        return 0;
    create = (PFNCREATEATTRIBS)(void *)wglGetProcAddress("wglCreateContextAttribsARB");
    if (!create || !(s_rc = create(s_dc, NULL, attrs))) {
        fprintf(stderr, "brr: gl: no 3.3 core context\n");
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(legacy);
        return 0;
    }
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(legacy);
    if (!wglMakeCurrent(s_dc, s_rc))
        return 0;
    if ((interval = (PFNSWAPINTERVAL)(void *)wglGetProcAddress("wglSwapIntervalEXT")))
        interval(0);   /* the game paces its own frames */
    return 1;
}

void gl_ctx_swap(void)
{
    if (s_dc)
        SwapBuffers(s_dc);
}

#elif defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
#include <OpenGL/OpenGL.h>
#include <dlfcn.h>

static CGLContextObj s_ctx;

void *gl_ctx_proc(const char *name)
{
    return dlsym(RTLD_DEFAULT, name);
}

int gl_ctx_open(void *win)
{
    CGLPixelFormatAttribute a[] = { kCGLPFAOpenGLProfile, (CGLPixelFormatAttribute)kCGLOGLPVersion_3_2_Core,
                                    kCGLPFAAccelerated, kCGLPFAColorSize, (CGLPixelFormatAttribute)24,
                                    kCGLPFAAlphaSize, (CGLPixelFormatAttribute)8, (CGLPixelFormatAttribute)0 };
    CGLPixelFormatObj pf;
    GLint n;
    (void)win;   /* (the Mac's windows draw with Metal) */
    if (CGLChoosePixelFormat(a, &pf, &n) != kCGLNoError || !pf)
        return 0;
    if (CGLCreateContext(pf, NULL, &s_ctx) != kCGLNoError) {
        CGLDestroyPixelFormat(pf);
        return 0;
    }
    CGLDestroyPixelFormat(pf);
    return CGLSetCurrentContext(s_ctx) == kCGLNoError;
}

void gl_ctx_swap(void) {}

#else
void *gl_ctx_proc(const char *name) { (void)name; return NULL; }
int gl_ctx_open(void *win) { (void)win; return 0; }
void gl_ctx_swap(void) {}
#endif
