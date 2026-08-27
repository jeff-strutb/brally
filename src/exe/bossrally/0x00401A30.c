/* 0x00401A30 SpawnWait: _spawnv(P_WAIT, cmd, &rest). User wrapper around CRT. */
#ifdef BR_MATCHING_BUILD
/* @implements 0x00401A30 bossrally.exe SpawnWait */

#include <windows.h>

int Spawnve3(const char *cmd, const char *const *argv, const char *const *env);

int SpawnWait(const char *cmd, ...)
{
    return Spawnve3(cmd, (const char *const *)(&cmd + 1), 0);
}

#endif
