/* Virtual-analog voice for the remaster: PolyBLEP saw and pulse oscillators with detuned unison
 * spread across the stereo field, a sub-octave sine, a 4-pole zero-delay-feedback ladder low-pass
 * with its own envelope, tanh drive, and an ADSR amp. The pitch is a per-sample frequency curve,
 * so slides and vibrato from the score come through exactly. */
#include <math.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    int   unison;            /* voices per note, 1..16 */
    float detune_cents;      /* total spread across the unison */
    float width;             /* 0 mono .. 1 full stereo spread */
    float saw, pulse, pulse_width, sub, noise;
    float cutoff_hz, env_oct, key_track, reso, drive;
    float fa, fd, fs, fr;    /* filter envelope, seconds / level */
    float aa, ad, as, ar;    /* amp envelope */
    float rate;
    uint32_t seed;
} Voice;

static float polyblep(float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

static float adsr(float t, float gate, float a, float d, float s, float r)
{
    float v;
    if (t < gate) {
        if (t < a) return a > 0 ? t / a : 1.0f;
        if (t < a + d) return 1.0f - (1.0f - s) * (t - a) / (d > 0 ? d : 1e-6f);
        return s;
    }
    /* level reached at the gate, then an exponential release */
    v = adsr(gate - 1e-6f, gate + 1.0f, a, d, s, r);
    return v * expf(-(t - gate) / (r > 1e-4f ? r / 4.6f : 1e-4f));
}

static uint32_t xs(uint32_t *s) { *s ^= *s << 13; *s ^= *s >> 17; *s ^= *s << 5; return *s; }

void render_voice(const Voice *p, int n, const float *freq, float gate_s, float *outL, float *outR)
{
    float phase[16], pphase[16], det[16], panL[16], panR[16];
    float sub_phase = 0.0f, s[4] = {0, 0, 0, 0};
    uint32_t seed = p->seed ? p->seed : 1u;
    int u, i, U = p->unison < 1 ? 1 : (p->unison > 16 ? 16 : p->unison);
    float norm = 1.0f / sqrtf((float)U);
    for (u = 0; u < U; u++) {
        float x = U > 1 ? (float)u / (float)(U - 1) * 2.0f - 1.0f : 0.0f;   /* -1..1 */
        det[u] = powf(2.0f, x * p->detune_cents * 0.5f / 1200.0f);
        phase[u] = (float)(xs(&seed) % 10000) / 10000.0f;
        pphase[u] = phase[u];
        {
            float pan = 0.5f + 0.5f * x * p->width;
            panL[u] = cosf(pan * 1.5707963f); panR[u] = sinf(pan * 1.5707963f);
        }
    }
    for (i = 0; i < n; i++) {
        float t = (float)i / p->rate, f0 = freq[i], L = 0.0f, R = 0.0f, mono, g, fe, fc, G, k, a, v, y4;
        for (u = 0; u < U; u++) {
            float fu = f0 * det[u], dt = fu / p->rate, sw, pl;
            if (dt > 0.49f) dt = 0.49f;
            sw = 2.0f * phase[u] - 1.0f - polyblep(phase[u], dt);
            pl = (pphase[u] < p->pulse_width ? 1.0f : -1.0f) + polyblep(pphase[u], dt)
                 - polyblep(fmodf(pphase[u] + (1.0f - p->pulse_width), 1.0f), dt);
            v = p->saw * sw + p->pulse * pl;
            L += v * panL[u]; R += v * panR[u];
            phase[u] += dt; if (phase[u] >= 1.0f) phase[u] -= 1.0f;
            pphase[u] += dt; if (pphase[u] >= 1.0f) pphase[u] -= 1.0f;
        }
        L *= norm; R *= norm;
        mono = p->sub * sinf(6.2831853f * sub_phase);
        sub_phase += 0.5f * f0 / p->rate; if (sub_phase >= 1.0f) sub_phase -= 1.0f;
        mono += p->noise * ((float)(xs(&seed) % 20001) / 10000.0f - 1.0f);
        /* filter the mid channel and carry the stereo spread as a side signal filtered the same way */
        fe = adsr(t, gate_s, p->fa, p->fd, p->fs, p->fr);
        fc = p->cutoff_hz * powf(2.0f, p->env_oct * fe) * powf(f0 / 261.63f, p->key_track);
        if (fc > 0.45f * p->rate) fc = 0.45f * p->rate;
        G = tanf(3.14159265f * fc / p->rate); k = 4.0f * p->reso;
        {
            float xin = 0.5f * (L + R) + mono, side = 0.5f * (L - R);
            /* ZDF ladder (Zavalishin), one step */
            float g1 = G / (1.0f + G), S = g1 * g1 * g1 * s[0] + g1 * g1 * s[1] + g1 * s[2] + s[3];
            float gg = g1 * g1 * g1 * g1, u0 = (xin - k * S / (1.0f + G)) / (1.0f + k * gg);
            float x1 = u0, y;
            int st;
            u0 = tanhf(u0 * (1.0f + p->drive)) / (1.0f + 0.5f * p->drive);
            x1 = u0;
            for (st = 0; st < 4; st++) {
                float vv = (x1 - s[st]) * g1;
                y = vv + s[st]; s[st] = y + vv; x1 = y;
            }
            y4 = x1;
            a = adsr(t, gate_s, p->aa, p->ad, p->as, p->ar);
            /* the side signal keeps the unison width; low-pass it gently with the same cutoff */
            side *= fminf(1.0f, fc / 4000.0f);
            outL[i] = (y4 + side) * a;
            outR[i] = (y4 - side) * a;
        }
    }
}
