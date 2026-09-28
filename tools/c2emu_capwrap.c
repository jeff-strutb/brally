#include <windows.h>
#include <stdio.h>
#include <string.h>
/* Copy the IL file set named by -il, then run the real backend unchanged. */
int main(int argc, char **argv)
{
    char base[MAX_PATH] = "", pat[MAX_PATH], src[MAX_PATH], dst[MAX_PATH], cmd[8192];
    WIN32_FIND_DATA fd; HANDLE h; int i; char *p;
    STARTUPINFO si; PROCESS_INFORMATION pi; DWORD rc = 1;
    static int n;
    { FILE *lf = fopen("Z:\\Users\\jeffreywilbur\\projects\\strutb\\brally\\build\\match\\t3d\\ilcap\\cmdline.txt", "a");
      if (lf) { fprintf(lf, "%s\n", GetCommandLine()); for (i = 0; i < argc; i++) fprintf(lf, "[%d]=%s\n", i, argv[i]);
      p = getenv("MSC_CMD_FLAGS"); fprintf(lf, "MSC_CMD_FLAGS=%s\n", p ? p : "(null)"); fclose(lf); } }
    for (i = 1; i < argc - 1; i++) if (!strcmp(argv[i], "-il")) strcpy(base, argv[i + 1]);
    { char *e = getenv("MSC_CMD_FLAGS"), *q;
      if (!base[0] && e && (q = strstr(e, "-il ")) != NULL) { q += 4; i = 0; while (q[i] && q[i] != ' ') { base[i] = q[i]; i++; } base[i] = 0; } }
    if (!base[0]) goto run;
    sprintf(pat, "%s*", base);
    h = FindFirstFile(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        p = strrchr(base, '\\');
        do {
            sprintf(src, "%.*s%s", (int)(p - base + 1), base, fd.cFileName);
            sprintf(dst, "Z:\\Users\\jeffreywilbur\\projects\\strutb\\brally\\build\\match\\t3d\\ilcap\\%s", fd.cFileName);
            CopyFile(src, dst, FALSE);
        } while (FindNextFile(h, &fd));
        FindClose(h);
    }
run:
    GetModuleFileName(NULL, cmd, sizeof cmd);
    p = strrchr(cmd, '\\'); strcpy(p + 1, "C2REAL.EXE");
    strcpy(src, cmd);
    p = GetCommandLine();
    if (*p == '"') { p = strchr(p + 1, '"') + 1; } else { while (*p && *p != ' ') p++; }
    sprintf(cmd, "\"%s\"%s", src, p);
    memset(&si, 0, sizeof si); si.cb = sizeof si;
    if (!CreateProcess(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) return 1;
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &rc);
    return (int)rc;
}
