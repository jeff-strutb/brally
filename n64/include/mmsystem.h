/* Multimedia (winmm) type shim for the IDO/N64 cross-compile.  See windows.h. */
#ifndef _MMSYSTEM_H_SHIM
#define _MMSYSTEM_H_SHIM
#include <windows.h>
typedef UINT MMRESULT;  typedef UINT MMVERSION;
typedef void *HWAVEOUT; typedef void *HWAVEIN; typedef void *HMIDIOUT;
typedef void *HMMIO;    typedef void *HWAVE;
#define MMSYSERR_NOERROR 0
#define WAVE_FORMAT_PCM  1
typedef struct tWAVEFORMATEX {
    WORD wFormatTag, nChannels; DWORD nSamplesPerSec, nAvgBytesPerSec;
    WORD nBlockAlign, wBitsPerSample, cbSize;
} WAVEFORMATEX, *LPWAVEFORMATEX;
typedef struct wavehdr_tag {
    LPSTR lpData; DWORD dwBufferLength, dwBytesRecorded, dwUser, dwFlags, dwLoops;
    struct wavehdr_tag *lpNext; DWORD reserved;
} WAVEHDR, *LPWAVEHDR;
typedef struct { WORD wMid, wPid; MMVERSION vDriverVersion; CHAR szPname[32];
    DWORD dwFormats; WORD wChannels, wReserved1; DWORD dwSupport; } WAVEOUTCAPS;
#define WHDR_DONE 1
#define WHDR_PREPARED 2
#endif
