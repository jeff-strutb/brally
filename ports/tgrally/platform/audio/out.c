/* out.c: the audio interface's buffers to the host's output.
 *
 * Each buffer the game queues (osAiSetNextBuffer) is 16-bit stereo,
 * big-endian as the N64's RAM held it, at the rate the game set.  It goes
 * into a ring the host's audio callback drains, resampled to the device's
 * rate.  The game paces itself on the modelled interface (os/io.c), never on
 * this ring, so audio cannot change what the game does. */
#include <stdlib.h>
#include <string.h>
#include "host.h"
#include "plat.h"

#define RING (1 << 16)              /* stereo frames */
static float s_ring[RING][2];
static volatile uint32_t s_w, s_r;
static double s_pos, s_step = 1.0;
static int s_open, s_devrate = 48000;
static host_mutex *s_m;

static void cb(float *lr, int frames, void *user)
{
    int i;
    (void)user;
    host_mutex_lock(s_m);
    for (i = 0; i < frames; i++) {
        uint32_t have = s_w - s_r;
        if (have < 2) {
            lr[2 * i] = lr[2 * i + 1] = 0.0f;
            continue;
        }
        {
            uint32_t k = s_r & (RING - 1);
            lr[2 * i] = s_ring[k][0];
            lr[2 * i + 1] = s_ring[k][1];
        }
        s_pos += s_step;
        while (s_pos >= 1.0 && s_w != s_r) {
            s_pos -= 1.0;
            s_r++;
        }
    }
    host_mutex_unlock(s_m);
}

void tgr_audio_init(void)
{
    s_m = host_mutex_new();
    if (g_tgr.headless || getenv("TGR_NOSOUND"))
        return;
    s_open = host_audio_open(s_devrate, cb, NULL) != 0;
}

void tgr_audio_buffer(const int16_t *lr, int frames, int rate)
{
    const uint8_t *b = (const uint8_t *)lr;
    int i;
    if (!s_open || rate <= 0)
        return;
    host_mutex_lock(s_m);
    s_step = (double)rate / s_devrate;
    for (i = 0; i < frames; i++) {
        uint32_t k;
        if (s_w - s_r >= RING - 1)
            break;
        k = s_w & (RING - 1);
        s_ring[k][0] = (int16_t)(b[4 * i] << 8 | b[4 * i + 1]) / 32768.0f;
        s_ring[k][1] = (int16_t)(b[4 * i + 2] << 8 | b[4 * i + 3]) / 32768.0f;
        s_w++;
    }
    host_mutex_unlock(s_m);
}
