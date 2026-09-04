/* 0x00401A30 SpawnWait: _spawnv(P_WAIT, cmd, &rest). User wrapper around CRT. */
#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: spawn another program with a variable argument list and wait
 * for it to finish. This is what runs the game launcher once the intro is
 * over. */
/* @implements 0x00401A30 bossrally.exe SpawnWait */

#include <windows.h>

int Spawnve3(const char *cmd, const char *const *argv, const char *const *env);

int SpawnWait(const char *cmd, ...)
{
    return Spawnve3(cmd, (const char *const *)(&cmd + 1), 0);
}

#endif
