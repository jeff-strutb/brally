/* 0x00401230 DoMainLoop — CPlay while(TRUE) + GetGraphEvent each lap */
#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: the intro player's message loop: pump Windows messages until
 * the video finishes or the player quits. */
/* @implements 0x00401230 bossrally.exe DoMainLoop */

#include <windows.h>

extern HWND gHwnd;
HANDLE GetGraphEvent(void);
int IsPlayingOrPaused(void);
int IsStopped(void);
void OnMediaPauseStop(void);
void OnGraphNotify(void);

UINT DoMainLoop(void)
{
    MSG msg;
    HANDLE ahObjects[1];
    const int cObjects = 1;

    while (TRUE) {
        if ((ahObjects[0] = GetGraphEvent()) == NULL) {
            WaitMessage();
        } else {
            DWORD Result = MsgWaitForMultipleObjects(cObjects,
                                                     ahObjects,
                                                     FALSE,
                                                     INFINITE,
                                                     QS_ALLINPUT);
            if (Result != (WAIT_OBJECT_0 + cObjects)) {
                if (Result == WAIT_OBJECT_0) {
                    OnGraphNotify();
                    if (IsStopped())
                        PostMessageA(gHwnd, WM_QUIT, 0, 0);
                }
                continue;
            }
        }

        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT)
                return msg.wParam;
            if (msg.message == WM_CHAR) {
                if ((BYTE)msg.wParam == 0x1B) {
                    if (IsPlayingOrPaused()) {
                        OnMediaPauseStop();
                        PostMessageA(gHwnd, WM_QUIT, 0, 0);
                    }
                }
            }
            if (msg.message != WM_SYSKEYDOWN) {
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
            }
        }
    }
}

#endif
