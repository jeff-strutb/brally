/* 0x00401340 WinMain — CPlay && inits; drain/fullscreen goto fail; no nReturn=hPrev */
#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: the intro program itself -- start COM, open brally.avi, play
 * it full screen, then launch the real game launcher. Skipping the video is
 * what the key and mouse handlers do. */
/* @implements 0x00401340 bossrally.exe WinMain */

#include <windows.h>

int InitCOM(void);
int RegisterWindowClass(HINSTANCE, HINSTANCE);
int CreatePlayerWindow(int);
int InitMedia(void);
int OpenClip(HWND, const char *);
int HasGraph(void);
int CanRun(void);
int SetVideoDrain(HWND);
int SetFullScreen(void);
void OnMediaPlay(void);
int DoMainLoop(void);
int IsPlayingOrPaused(void);
void OnMediaPauseStop(void);
void DeleteContents(void);
void CoUninitialize_thunk(void);
int SpawnWait(const char *cmd, ...);
extern HWND gHwnd;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdParam, int nCmdShow)
{
    UINT nReturn;

    if (InitCOM() == FALSE)
        return 0;

    if (RegisterWindowClass(hInstance, hPrevInstance) &&
        CreatePlayerWindow(nCmdShow) &&
        InitMedia())
    {
        OpenClip(gHwnd, "brally.avi");
        if (HasGraph()) {
            if (CanRun()) {
                if (SetVideoDrain(gHwnd) == 0)
                    goto fail;
                if (SetFullScreen() == 0)
                    goto fail;
                OnMediaPlay();
            }
            nReturn = DoMainLoop();
            if (IsPlayingOrPaused())
                OnMediaPauseStop();
            DeleteContents();
        }
    }
fail:
    DeleteContents();
    CoUninitialize_thunk();
    SpawnWait("brally.exe", "brally.exe", 0);
    return nReturn;
}

#endif
