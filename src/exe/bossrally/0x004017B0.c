/* 0x004017B0 OpenFile: OpenMediaFile + GetFullPathName + title + state=1 */
#ifdef BR_MATCHING_BUILD
/* @implements 0x004017B0 bossrally.exe OpenClip */

#include <windows.h>

int OpenMediaFile(const char *path);
void SetPlayerTitle(HWND hwnd, const char *file);
void SetMediaState(int s);
extern char gFullPath[];
extern char gFileTitle[];
extern char gFlag_40ac77;

char *strncpy(char *d, const char *s, size_t n);

int OpenClip(HWND hwnd, const char *path)
{
    char *filePart;

    if (path != 0) {
        if (OpenMediaFile(path) != 0) {
            GetFullPathNameA(path, 0x104, gFullPath, &filePart);
            strncpy(gFileTitle, filePart, 0x200);
            gFlag_40ac77 = 0;
            SetPlayerTitle(hwnd, gFileTitle);
            SetMediaState(1);
        }
    }
}

#endif
