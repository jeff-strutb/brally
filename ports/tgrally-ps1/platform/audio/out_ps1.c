/* out_ps1.c: the audio interface's output on the PlayStation (the SPU, to come) */
#include <stdint.h>
#include "plat.h"

void tgr_audio_init(void) {}
void tgr_audio_lowpass(int hz) { (void)hz; }
void tgr_audio_buffer(const int16_t *lr, int frames, int rate) { (void)lr; (void)frames; (void)rate; }
int  tgr_audio_buffered_ms(void) { return 0; }
