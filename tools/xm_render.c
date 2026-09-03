/* xm_render.c -- render a FastTracker II .xm module to raw PCM. Build tooling.
 *
 * The N64 build of the game (Top Gear Rally) plays its soundtrack as XM tracker
 * modules. The port plays lossless audio instead, so the modules have to be
 * rendered once, at build time, on the builder's own machine. ffmpeg here has no
 * libopenmpt, so this does the rendering.
 *
 *     cc -std=c99 -O2 -o xm_render xm_render.c -lm
 *     xm_render [options] <in.xm> <out.raw>      # s16le stereo, JSON on stdout
 *
 * This is NOT a general-purpose XM player. It implements the feature set the six
 * modules in the ROM actually use, which was enumerated by parsing every pattern:
 *
 *     effects      0 arpeggio, 1 porta up, 2 porta down, 3 tone porta,
 *                  4 vibrato, A volume slide, C set volume, E9 retrigger,
 *                  F set speed/tempo
 *     volume col   set-volume only (0x10..0x5F)
 *     samples      8-bit, forward loops and no-loop
 *     envelopes    volume envelope with sustain + loop, fadeout; no panning
 *                  envelopes and no autovibrato appear
 *     frequency    both linear (4 modules) and Amiga (2 modules) tables
 *
 * 16-bit samples and ping-pong loops are also handled, because they cost a dozen
 * lines and make the tool safe to point at another module. Effects outside the
 * list above are counted and reported in the JSON as "unhandled_effects" rather
 * than being silently ignored -- if that field is non-empty the output is not
 * trustworthy and the tool says so.
 *
 * Pattern jump (Bxx) and pattern break (Dxx) do NOT occur in these modules, so
 * the order list plays straight through; they are still implemented so that the
 * song-position bookkeeping stays honest if they ever do appear.
 *
 * Integers are decoded byte-wise from the file image, so the tool is
 * endian-agnostic and does not care about struct padding or alignment.
 *
 * Accuracy, and how it is measured
 * --------------------------------
 * The effect census above says which effects the modules USE. It says nothing
 * about whether they are implemented correctly, and that is the failure mode a
 * hand-written replayer really has: for a long time the Amiga branch of
 * period_of_note() had the wrong octave divisor and two of the six modules
 * played two octaves sharp, with a clean census the whole time.
 *
 * tools/xm_oracle.py is the check. It scores this file against libopenmpt --
 * OpenMPT's replayer, validated against FastTracker II itself, and what
 * MilkyTracker and VLC use -- as windowed correlation, so a wrong effect shows
 * up as a few bad windows with a timestamp to read the rows at.
 *
 *     brew install libopenmpt
 *     python3 tools/xm_oracle.py --per-channel <module.xm>
 *
 * Scores on the six ROM modules as of 2026-09-03 (median correlation over 0.25s
 * windows), which is the baseline any change here has to beat:
 *
 *     xm_0EBC00 0.9903   xm_113660 0.9742   xm_12EAB0 0.9674
 *     xm_149C80 0.9515   xm_164B60 0.9921   xm_17FD10 0.9525
 *
 * Known open, in the order worth attacking:
 *
 *   - Vibrato depth scaling is a guess. vibrato_offset is normalised as
 *     2.0f*sine*depth/15 and then multiplied by the SAME 16.0f in both the
 *     linear and the Amiga period domains, and one constant cannot be right in
 *     two different unit systems. The two lowest-scoring modules are the two
 *     Amiga ones, which is consistent with this.
 *   - Amiga finetune is folded into the continuous note index and interpolated
 *     between adjacent SEMITONE periods; FT2 interpolates within a finetune
 *     table instead. Same suspects.
 *   - Key-off on an instrument with no volume envelope: FT2 cuts the note, this
 *     keeps it alive under the instrument fadeout. No module here uses note-off
 *     (verified: changing the fadeout rate moves no score at all), so this is
 *     latent rather than active.
 *   - tick_envelope() tests the envelope loop before the sustain point; FT2
 *     tests sustain first, so a sustain point at or past the loop end behaves
 *     differently. No instrument in these six modules enables a volume envelope
 *     at all, so this too is latent.
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CHANNELS   32
#define MAX_PATTERNS   256
#define MAX_INSTR      128
#define MAX_SAMPLES    16
#define MAX_ORDER      256
#define ENV_POINTS     12

/* ------------------------------------------------------------------ helpers */

static void die(const char *fmt, ...);

static uint16_t rd_u16le(const unsigned char *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t rd_u32le(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

#include <stdarg.h>
static void die(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fputs("xm_render: ", stderr);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
    exit(2);
}

static void *xmalloc(size_t n)
{
    void *p = calloc(1, n ? n : 1);
    if (!p)
        die("out of memory (%zu bytes)", n);
    return p;
}

/* -------------------------------------------------------------- module data */

typedef struct {
    int32_t  length;        /* in frames */
    int32_t  loop_start;
    int32_t  loop_length;
    int      loop_type;     /* 0 none, 1 forward, 2 ping-pong */
    int      volume;        /* 0..64 */
    int      finetune;      /* -128..127 */
    int      relative_note; /* -128..127 */
    int      panning;       /* 0..255 */
    float   *data;          /* normalised to -1..1 */
} XmSample;

typedef struct {
    int      num_samples;
    uint8_t  sample_of_note[96];
    XmSample sample[MAX_SAMPLES];

    int      vol_env_on, vol_sustain_on, vol_loop_on;
    int      vol_points;
    int      vol_x[ENV_POINTS], vol_y[ENV_POINTS];
    int      vol_sustain, vol_loop_start, vol_loop_end;
    int      fadeout;       /* 0..0xFFF */
} XmInstr;

typedef struct {
    uint8_t note, instr, vol, eff, par;
} XmCell;

typedef struct {
    int     rows;
    XmCell *cell;           /* rows * channels */
} XmPattern;

typedef struct {
    char       name[21];
    int        channels, num_patterns, num_instr;
    int        song_length, restart_position;
    int        linear_freq;
    int        default_speed, default_bpm;
    uint8_t    order[MAX_ORDER];
    XmPattern  pattern[MAX_PATTERNS];
    XmInstr    instr[MAX_INSTR];
} XmModule;

/* ------------------------------------------------------------ channel state */

typedef struct {
    const XmInstr  *instr;
    const XmSample *sample;
    int    instr_index;

    int    note;             /* 1..96, 0 = none */
    float  period;
    float  target_period;    /* tone portamento destination */
    float  step;             /* sample frames advanced per output frame */
    double pos;
    int    direction;        /* +1 / -1 for ping-pong */
    int    active;

    int    volume;           /* 0..64 */
    int    panning;          /* 0..255 */

    int    key_off;
    float  fadeout_volume;   /* 1.0 down to 0.0 */
    int    env_vol_frame;
    float  env_vol_value;    /* 0..1 */

    int    vibrato_ticks;
    float  vibrato_offset;   /* period offset */
    float  arp_offset;       /* semitone offset */

    /* effect memory */
    int    m_porta_up, m_porta_down, m_tone_porta, m_volume_slide;
    int    m_vibrato, m_retrig;
    int    retrig_counter;
} XmChannel;

/* ------------------------------------------------------------- frequency */

/* Amiga period table: one octave, 12 semitones plus the wrap entry.
 *
 * Entry 0 (1712) is the period of note index 48 -- the note whose frequency is
 * the 8363 Hz reference, since 8363*1712/1712 == 8363. That fixes the octave
 * divisor below: floor(48/12) == 4, so the table is unshifted at note 48 only
 * when 4 is subtracted. Getting this constant wrong transposes the whole
 * module, which is silent in the effect census -- the two Amiga-table modules
 * played two octaves sharp until 2026-09-03. The invariant to test against is
 * that period_of_note agrees with the linear table on frequency for every note:
 *
 *     8363 * 1712 / amiga(n)  ==  8363 * 2^((4608 - (7680 - n*64)) / 768)
 */
static const uint16_t amiga_period[13] = {
    1712, 1616, 1524, 1440, 1356, 1280, 1208, 1140, 1076, 1016, 960, 907, 856
};

static float lerp(float a, float b, float t) { return a + t * (b - a); }

/* `note` is a continuous note index: (note-1) + relative_note + finetune/128. */
static float period_of_note(const XmModule *m, float note)
{
    if (m->linear_freq)
        return 7680.0f - note * 64.0f;
    {
        int    inote  = (int)floorf(note);
        int    a      = ((inote % 12) + 12) % 12;
        int    octave = (int)floorf(note / 12.0f) - 4;
        float  p1 = amiga_period[a], p2 = amiga_period[a + 1];
        if (octave > 0)      { p1 /= (float)(1u << octave);  p2 /= (float)(1u << octave); }
        else if (octave < 0) { p1 *= (float)(1u << -octave); p2 *= (float)(1u << -octave); }
        return lerp(p1, p2, note - (float)inote);
    }
}

/* Convert a period (plus arpeggio/vibrato offsets) to Hz.
 *
 * Arpeggio is a semitone offset and vibrato a period offset; in the linear
 * domain a semitone is exactly 64 period units, so both fold into the period.
 * In the Amiga domain the mapping is not linear, so arpeggio is applied by
 * re-deriving the period from the shifted note. */
static float frequency_of_period(const XmModule *m, float period,
                                 float arp_semitones, float vib_period)
{
    if (m->linear_freq) {
        float p = period - 64.0f * arp_semitones - 16.0f * vib_period;
        return 8363.0f * powf(2.0f, (4608.0f - p) / 768.0f);
    } else {
        float p = period;
        if (arp_semitones != 0.0f) {
            /* Recover the note this period represents, shift it, re-derive. */
            float note = 0.0f;
            if (p > 0.0f)
                note = 12.0f * (log2f(1712.0f / p)) + 24.0f;
            p = period_of_note(m, note + arp_semitones);
        }
        p += 16.0f * vib_period;
        if (p < 1.0f)
            p = 1.0f;
        return 8363.0f * 1712.0f / p;
    }
}

/* XM sine waveform, 64 steps, returned as -1..1. */
static float waveform_sine(int step)
{
    return -sinf(2.0f * 3.14159265358979f * (float)(step & 0x3F) / 64.0f);
}

/* ------------------------------------------------------------------ parsing */

typedef struct {
    const unsigned char *p;
    size_t               size;
} Buf;

static const unsigned char *need(const Buf *b, size_t off, size_t n, const char *what)
{
    if (off > b->size || n > b->size - off)
        die("truncated module: need %zu bytes at offset %zu for %s (file is %zu)",
            n, off, what, b->size);
    return b->p + off;
}

static void parse_envelope(const unsigned char *q, XmInstr *ins)
{
    int i;
    for (i = 0; i < ENV_POINTS; i++) {
        ins->vol_x[i] = rd_u16le(q + 0x81 + i * 4);
        ins->vol_y[i] = rd_u16le(q + 0x83 + i * 4);
    }
    ins->vol_points     = q[0x8B];
    ins->vol_sustain    = q[0x8D];
    ins->vol_loop_start = q[0x8E];
    ins->vol_loop_end   = q[0x8F];
    ins->vol_env_on     = (q[0x91] & 1) != 0;
    ins->vol_sustain_on = (q[0x91] & 2) != 0;
    ins->vol_loop_on    = (q[0x91] & 4) != 0;
    ins->fadeout        = rd_u16le(q + 0x9A);
    if (ins->vol_points > ENV_POINTS)
        ins->vol_points = ENV_POINTS;
}

static void parse_module(Buf *b, XmModule *m)
{
    const unsigned char *h = need(b, 0, 80, "header");
    size_t off;
    int i, j;

    if (memcmp(h, "Extended Module: ", 17) != 0)
        die("not an XM module (missing \"Extended Module: \" signature)");
    memcpy(m->name, h + 17, 20);
    m->name[20] = '\0';
    for (i = 19; i >= 0 && (m->name[i] == ' ' || m->name[i] == '\0'); i--)
        m->name[i] = '\0';

    if (h[37] != 0x1A)
        die("bad XM header: byte 37 is 0x%02X, expected 0x1A", h[37]);

    {
        uint32_t header_size = rd_u32le(h + 60);
        m->song_length      = rd_u16le(h + 64);
        m->restart_position = rd_u16le(h + 66);
        m->channels         = rd_u16le(h + 68);
        m->num_patterns     = rd_u16le(h + 70);
        m->num_instr        = rd_u16le(h + 72);
        m->linear_freq      = (rd_u16le(h + 74) & 1) != 0;
        m->default_speed    = rd_u16le(h + 76);
        m->default_bpm      = rd_u16le(h + 78);

        if (m->channels < 1 || m->channels > MAX_CHANNELS)
            die("unsupported channel count %d (max %d)", m->channels, MAX_CHANNELS);
        if (m->num_patterns > MAX_PATTERNS)
            die("unsupported pattern count %d", m->num_patterns);
        if (m->num_instr > MAX_INSTR)
            die("unsupported instrument count %d", m->num_instr);
        if (m->song_length < 1 || m->song_length > MAX_ORDER)
            die("bad song length %d", m->song_length);
        if (m->default_speed < 1) m->default_speed = 6;
        if (m->default_bpm < 32)  m->default_bpm = 125;
        if (m->restart_position >= m->song_length)
            m->restart_position = 0;

        memcpy(m->order, need(b, 80, (size_t)m->song_length, "order table"),
               (size_t)m->song_length);
        off = 60 + header_size;
    }

    for (i = 0; i < m->num_patterns; i++) {
        const unsigned char *q = need(b, off, 9, "pattern header");
        uint32_t phsize = rd_u32le(q);
        int      rows   = rd_u16le(q + 5);
        uint16_t packed = rd_u16le(q + 7);
        const unsigned char *d;
        XmPattern *pat = &m->pattern[i];
        int cell_index = 0, total;

        if (q[4] != 0)
            die("pattern %d uses packing type %d, only 0 is defined", i, q[4]);
        if (rows < 1 || rows > 256)
            die("pattern %d has %d rows", i, rows);
        if (phsize < 9)
            die("pattern %d has a %u-byte header", i, phsize);

        pat->rows = rows;
        total     = rows * m->channels;
        pat->cell = (XmCell *)xmalloc((size_t)total * sizeof(XmCell));

        d = need(b, off + phsize, packed, "pattern data");
        {
            size_t k = 0;
            while (cell_index < total && k < packed) {
                XmCell *c = &pat->cell[cell_index++];
                unsigned char mask = d[k];
                if (mask & 0x80) {
                    k++;
                    if ((mask & 0x01) && k < packed) c->note  = d[k++];
                    if ((mask & 0x02) && k < packed) c->instr = d[k++];
                    if ((mask & 0x04) && k < packed) c->vol   = d[k++];
                    if ((mask & 0x08) && k < packed) c->eff   = d[k++];
                    if ((mask & 0x10) && k < packed) c->par   = d[k++];
                } else {
                    if (k + 5 > packed)
                        break;
                    c->note  = d[k + 0]; c->instr = d[k + 1]; c->vol = d[k + 2];
                    c->eff   = d[k + 3]; c->par   = d[k + 4];
                    k += 5;
                }
            }
        }
        off += phsize + packed;
    }

    for (i = 0; i < m->num_instr; i++) {
        const unsigned char *q = need(b, off, 4, "instrument header");
        uint32_t ihsize = rd_u32le(q);
        XmInstr *ins = &m->instr[i];
        int ns;

        if (ihsize < 29)
            die("instrument %d has a %u-byte header", i, ihsize);
        q = need(b, off, ihsize, "instrument header");
        ns = rd_u16le(q + 27);
        if (ns > MAX_SAMPLES)
            die("instrument %d has %d samples (max %d)", i, ns, MAX_SAMPLES);
        ins->num_samples = ns;

        if (ns > 0) {
            if (ihsize < 0x9C)
                die("instrument %d has samples but a %u-byte header", i, ihsize);
            memcpy(ins->sample_of_note, q + 0x21, 96);
            for (j = 0; j < 96; j++)
                if (ins->sample_of_note[j] >= (uint8_t)ns)
                    ins->sample_of_note[j] = 0;
            parse_envelope(q, ins);
        }
        off += ihsize;

        /* All 40-byte sample headers come first, then every sample body back to
         * back -- the bodies are NOT interleaved with their headers. Lengths in
         * the header are byte counts; for 16-bit samples they are twice the
         * frame count, and the loop points are byte offsets too. */
        {
            int bits16[MAX_SAMPLES];

            for (j = 0; j < ns; j++) {
                const unsigned char *s = need(b, off, 40, "sample header");
                XmSample *sm = &ins->sample[j];
                int flags;
                sm->length        = (int32_t)rd_u32le(s);
                sm->loop_start    = (int32_t)rd_u32le(s + 4);
                sm->loop_length   = (int32_t)rd_u32le(s + 8);
                sm->volume        = s[12];
                sm->finetune      = (int8_t)s[13];
                flags             = s[14];
                sm->panning       = s[15];
                sm->relative_note = (int8_t)s[16];
                sm->loop_type     = flags & 3;
                bits16[j]         = (flags & 0x10) != 0;
                if (sm->length < 0)
                    die("instrument %d sample %d has a negative length", i, j);
                off += 40;
            }

            for (j = 0; j < ns; j++) {
                XmSample *sm = &ins->sample[j];
                int32_t nbytes = sm->length;
                int32_t frames = bits16[j] ? nbytes / 2 : nbytes;
                const unsigned char *body = need(b, off, (size_t)nbytes, "sample data");
                int32_t k;

                sm->data = (float *)xmalloc((size_t)(frames > 0 ? frames : 1)
                                            * sizeof(float));
                /* Sample data is stored as deltas, not absolute values. */
                if (bits16[j]) {
                    int32_t old = 0;
                    for (k = 0; k < frames; k++) {
                        int32_t d = (int16_t)rd_u16le(body + k * 2);
                        old = (int16_t)(old + d);
                        sm->data[k] = (float)old / 32768.0f;
                    }
                    sm->loop_start  /= 2;
                    sm->loop_length /= 2;
                } else {
                    int32_t old = 0;
                    for (k = 0; k < frames; k++) {
                        int32_t d = (int8_t)body[k];
                        old = (int8_t)(old + d);
                        sm->data[k] = (float)old / 128.0f;
                    }
                }
                sm->length = frames;
                if (sm->loop_start < 0 || sm->loop_start > frames)
                    sm->loop_start = 0;
                if (sm->loop_length <= 0 ||
                    sm->loop_start + sm->loop_length > frames)
                    sm->loop_length = frames - sm->loop_start;
                if (sm->loop_length <= 0)
                    sm->loop_type = 0;
                off += (size_t)nbytes;
            }
        }
    }
}

/* -------------------------------------------------------------- envelopes */

static float envelope_value(const XmInstr *ins, int frame)
{
    int n = ins->vol_points, i;
    if (n <= 0)
        return 1.0f;
    if (frame <= ins->vol_x[0])
        return (float)ins->vol_y[0] / 64.0f;
    if (frame >= ins->vol_x[n - 1])
        return (float)ins->vol_y[n - 1] / 64.0f;
    for (i = 0; i + 1 < n; i++) {
        int x0 = ins->vol_x[i], x1 = ins->vol_x[i + 1];
        if (frame >= x0 && frame <= x1) {
            float t = (x1 > x0) ? (float)(frame - x0) / (float)(x1 - x0) : 0.0f;
            return lerp((float)ins->vol_y[i], (float)ins->vol_y[i + 1], t) / 64.0f;
        }
    }
    return (float)ins->vol_y[n - 1] / 64.0f;
}

static void tick_envelope(XmChannel *c)
{
    const XmInstr *ins = c->instr;
    if (!ins) {
        c->env_vol_value = 1.0f;
        return;
    }
    if (!ins->vol_env_on) {
        /* No envelope: the note stays at full level until key-off, then the
         * fadeout alone takes it down. */
        c->env_vol_value = 1.0f;
        return;
    }
    c->env_vol_value = envelope_value(ins, c->env_vol_frame);

    if (ins->vol_loop_on && ins->vol_loop_end < ins->vol_points &&
        c->env_vol_frame >= ins->vol_x[ins->vol_loop_end]) {
        c->env_vol_frame = ins->vol_x[ins->vol_loop_start];
    }
    if (ins->vol_sustain_on && !c->key_off &&
        ins->vol_sustain < ins->vol_points &&
        c->env_vol_frame == ins->vol_x[ins->vol_sustain]) {
        /* hold */
    } else {
        c->env_vol_frame++;
    }
}

/* ---------------------------------------------------------------- playback */

typedef struct {
    const XmModule *m;
    XmChannel  ch[MAX_CHANNELS];
    int   order_index, row, tick;
    int   speed, bpm;
    int   pattern_break_row;   /* -1 when none pending */
    int   pattern_jump_order;  /* -1 when none pending */
    int   rate;
    long  unhandled[64];       /* per effect id */
} XmPlayer;

static void note_off(XmChannel *c) { c->key_off = 1; }

static void update_step(const XmModule *m, XmChannel *c, int rate)
{
    float f = frequency_of_period(m, c->period, c->arp_offset, c->vibrato_offset);
    c->step = f / (float)rate;
}

static void trigger_note(const XmModule *m, XmChannel *c, int rate, int keep_pos)
{
    if (!c->sample)
        return;
    if (!keep_pos) {
        c->pos = 0.0;
        c->direction = 1;
    }
    c->active = 1;
    c->key_off = 0;
    c->fadeout_volume = 1.0f;
    c->env_vol_frame = 0;
    c->vibrato_ticks = 0;
    c->vibrato_offset = 0.0f;
    update_step(m, c, rate);
}

static void set_note(const XmModule *m, XmChannel *c, int note, int rate)
{
    const XmSample *s = c->sample;
    float n;
    if (!s)
        return;
    n = (float)(note - 1) + (float)s->relative_note + (float)s->finetune / 128.0f;
    c->period = period_of_note(m, n);
    update_step(m, c, rate);
}

static void row_tick0(XmPlayer *pl, XmChannel *c, const XmCell *cell)
{
    const XmModule *m = pl->m;
    int has_note = (cell->note > 0 && cell->note < 97);
    int is_tone_porta = (cell->eff == 0x03) ||
                        (cell->eff == 0x05) ||
                        ((cell->vol >> 4) == 0x0F);

    /* instrument column: selects the instrument and resets volume/pan */
    if (cell->instr > 0 && cell->instr <= m->num_instr) {
        const XmInstr *ins = &m->instr[cell->instr - 1];
        c->instr = ins;
        c->instr_index = cell->instr;
        if (has_note || c->sample) {
            /* reload default volume/panning from the mapped sample */
            int ni = has_note ? cell->note : c->note;
            if (ni > 0 && ni < 97 && ins->num_samples > 0) {
                const XmSample *s = &ins->sample[ins->sample_of_note[ni - 1]];
                c->volume  = s->volume;
                c->panning = s->panning;
            }
        }
        if (!has_note && c->active) {
            /* instrument-only: restart the envelope, keep the note going */
            c->env_vol_frame = 0;
            c->fadeout_volume = 1.0f;
            c->key_off = 0;
        }
    }

    if (cell->note == 97) {
        note_off(c);
    } else if (has_note && c->instr) {
        const XmInstr *ins = c->instr;
        if (ins->num_samples > 0) {
            const XmSample *s = &ins->sample[ins->sample_of_note[cell->note - 1]];
            c->note = cell->note;
            if (is_tone_porta && c->active) {
                /* Tone portamento: aim at the new note, keep playing the old. */
                float n = (float)(cell->note - 1) + (float)s->relative_note
                        + (float)s->finetune / 128.0f;
                c->target_period = period_of_note(m, n);
            } else {
                c->sample = s;
                set_note(m, c, cell->note, pl->rate);
                c->target_period = c->period;
                trigger_note(m, c, pl->rate, 0);
            }
        }
    }

    /* volume column -- only set-volume occurs in these modules */
    if (cell->vol >= 0x10 && cell->vol <= 0x50)
        c->volume = cell->vol - 0x10;
    else if (cell->vol > 0x50 && cell->vol <= 0x5F)
        c->volume = 64;

    /* effect column, tick-0 half */
    switch (cell->eff) {
    case 0x00:
        c->arp_offset = 0.0f;
        break;
    case 0x01: if (cell->par) c->m_porta_up   = cell->par; break;
    case 0x02: if (cell->par) c->m_porta_down = cell->par; break;
    case 0x03: if (cell->par) c->m_tone_porta = cell->par; break;
    case 0x04:
        if (cell->par >> 4)   c->m_vibrato = (c->m_vibrato & 0x0F) | (cell->par & 0xF0);
        if (cell->par & 0x0F) c->m_vibrato = (c->m_vibrato & 0xF0) | (cell->par & 0x0F);
        break;
    case 0x0A: if (cell->par) c->m_volume_slide = cell->par; break;
    case 0x0C:
        c->volume = cell->par > 64 ? 64 : cell->par;
        break;
    case 0x0E:
        if ((cell->par >> 4) == 0x09) {
            c->m_retrig = cell->par & 0x0F;
            c->retrig_counter = 0;
        } else {
            pl->unhandled[0x0E]++;
        }
        break;
    case 0x0F:
        if (cell->par == 0)
            break;
        if (cell->par < 0x20) pl->speed = cell->par;
        else                  pl->bpm   = cell->par;
        break;
    case 0x0B:
        pl->pattern_jump_order = cell->par;
        break;
    case 0x0D:
        pl->pattern_break_row = (cell->par >> 4) * 10 + (cell->par & 0x0F);
        break;
    default:
        if (cell->eff || cell->par)
            pl->unhandled[cell->eff & 63]++;
        break;
    }
}

static void volume_slide(XmChannel *c, int param)
{
    int up = param >> 4, down = param & 0x0F;
    if (up)        c->volume += up;
    else if (down) c->volume -= down;
    if (c->volume < 0)  c->volume = 0;
    if (c->volume > 64) c->volume = 64;
}

static void row_tick_n(XmPlayer *pl, XmChannel *c, const XmCell *cell, int tick)
{
    const XmModule *m = pl->m;
    switch (cell->eff) {
    case 0x00:
        if (cell->par) {
            int k = tick % 3;
            c->arp_offset = (k == 0) ? 0.0f
                          : (k == 1) ? (float)(cell->par >> 4)
                                     : (float)(cell->par & 0x0F);
            update_step(m, c, pl->rate);
        }
        break;
    case 0x01:
        c->period -= 4.0f * (float)c->m_porta_up;
        if (c->period < 1.0f) c->period = 1.0f;
        update_step(m, c, pl->rate);
        break;
    case 0x02:
        c->period += 4.0f * (float)c->m_porta_down;
        update_step(m, c, pl->rate);
        break;
    case 0x03:
        if (c->target_period > c->period) {
            c->period += 4.0f * (float)c->m_tone_porta;
            if (c->period > c->target_period) c->period = c->target_period;
        } else if (c->target_period < c->period) {
            c->period -= 4.0f * (float)c->m_tone_porta;
            if (c->period < c->target_period) c->period = c->target_period;
        }
        update_step(m, c, pl->rate);
        break;
    case 0x04:
        c->vibrato_ticks += (c->m_vibrato >> 4);
        c->vibrato_offset = 2.0f * waveform_sine(c->vibrato_ticks)
                          * (float)(c->m_vibrato & 0x0F) / 15.0f;
        update_step(m, c, pl->rate);
        break;
    case 0x0A:
        volume_slide(c, c->m_volume_slide);
        break;
    case 0x0E:
        if ((cell->par >> 4) == 0x09 && c->m_retrig) {
            if (++c->retrig_counter >= c->m_retrig) {
                c->retrig_counter = 0;
                trigger_note(m, c, pl->rate, 0);
            }
        }
        break;
    default:
        break;
    }
}

/* ------------------------------------------------------------------- mixing */

static float sample_at(const XmSample *s, double pos)
{
    /* Linear interpolation. FT2 itself used no interpolation on most hardware,
     * but the N64 mixer did resample, and point sampling at 44.1 kHz adds
     * audible aliasing that would be baked into a lossless file forever. */
    int32_t i = (int32_t)pos;
    float   f = (float)(pos - (double)i);
    float   a, bb;
    if (i < 0) return 0.0f;
    if (i >= s->length) return 0.0f;
    a = s->data[i];
    if (i + 1 < s->length)
        bb = s->data[i + 1];
    else if (s->loop_type == 1)
        bb = s->data[s->loop_start];
    else
        bb = a;
    return lerp(a, bb, f);
}

static void advance_channel(XmChannel *c)
{
    const XmSample *s = c->sample;
    if (!c->active || !s)
        return;
    c->pos += (double)c->step * (double)c->direction;

    if (s->loop_type == 1) {
        double end = (double)(s->loop_start + s->loop_length);
        while (c->pos >= end)
            c->pos -= (double)s->loop_length;
        if (c->pos < 0.0)
            c->pos = 0.0;
    } else if (s->loop_type == 2) {
        double lo = (double)s->loop_start;
        double hi = (double)(s->loop_start + s->loop_length);
        for (;;) {
            if (c->pos >= hi)      { c->pos = hi - (c->pos - hi); c->direction = -1; }
            else if (c->pos < lo)  { c->pos = lo + (lo - c->pos); c->direction = 1; }
            else break;
        }
    } else {
        if (c->pos >= (double)s->length) {
            c->active = 0;
            c->pos = 0.0;
        }
    }
}


/* ------------------------------------------------------------------- driver */

typedef struct {
    int    rate, passes, fade_ms;
    double max_seconds;
} RenderCfg;

typedef struct {
    long   frames;
    long   loop_start_frame;
    long   restart_frame;   /* frame the restart order position begins at */
    long   clipped;
    double peak;            /* pre-gain, so pass 1 can size the gain */
} RenderStats;

static void player_init(XmPlayer *pl, const XmModule *m, int rate)
{
    int i;
    memset(pl, 0, sizeof(*pl));
    pl->m = m;
    pl->rate = rate;
    pl->speed = m->default_speed;
    pl->bpm = m->default_bpm;
    pl->pattern_break_row = -1;
    pl->pattern_jump_order = -1;
    pl->tick = 0;
    for (i = 0; i < m->channels; i++) {
        pl->ch[i].volume = 64;
        /* XM default channel panning is centre. */
        pl->ch[i].panning = 128;
        pl->ch[i].direction = 1;
        pl->ch[i].fadeout_volume = 1.0f;
        pl->ch[i].env_vol_value = 1.0f;
    }
}

/* Render the whole song. `out` may be NULL, in which case nothing is written and
 * the call only measures (that is pass 1, which establishes the peak level so
 * pass 2 can pick a gain that cannot clip).
 *
 * `total_frames` is the length pass 1 measured, needed to place the fade; pass 1
 * itself passes 0 and gets no fade. The player is deterministic -- no rand, no
 * time source -- so the two passes generate identical audio. */
static void render(XmPlayer *pl, const RenderCfg *cfg, FILE *out, double gain,
                   long total_frames, RenderStats *st)
{
    const XmModule *m = pl->m;
    double carry = 0.0;
    long   max_frames = (long)(cfg->max_seconds * cfg->rate);
    long   fade_frames = (long)cfg->fade_ms * cfg->rate / 1000;
    long   fade_start;
    int    pass = 0, done = 0, i;

    memset(st, 0, sizeof(*st));
    st->loop_start_frame = -1;
    st->restart_frame = -1;

    if (out == NULL || total_frames <= 0 || fade_frames <= 0)
        fade_start = -1;
    else {
        if (fade_frames > total_frames) fade_frames = total_frames;
        fade_start = total_frames - fade_frames;
    }

    while (!done && st->frames < max_frames) {
        double spt_f;
        long   spt;
        int    c;

        /* --- start of tick: row events on tick 0, continuous effects after --- */
        if (pl->tick == 0) {
            const XmPattern *pat;
            int pidx = m->order[pl->order_index];
            /* Frame at which the module's restart position is first entered.
             * A single-pass render ends where the song ends, so a player that
             * wants to loop the file has to jump BACK to here -- and for four
             * of the six ROM modules that is a point partway in, not zero.
             * Without this the rip cannot be looped correctly at all. */
            if (pass == 0 && st->restart_frame < 0 && pl->row == 0 &&
                pl->order_index == m->restart_position)
                st->restart_frame = st->frames;
            if (pidx >= m->num_patterns) {
                /* An order slot past the end of the pattern table is a legal
                 * "skip this slot" marker rather than an error. */
                pl->row = 0;
                pl->order_index++;
                if (pl->order_index >= m->song_length) {
                    pl->order_index = m->restart_position;
                    if (++pass >= cfg->passes) break;
                    if (pass == 1 && st->loop_start_frame < 0)
                        st->loop_start_frame = st->frames;
                }
                continue;
            }
            pat = &m->pattern[pidx];
            for (c = 0; c < m->channels; c++)
                row_tick0(pl, &pl->ch[c], &pat->cell[pl->row * m->channels + c]);
        } else {
            const XmPattern *pat = &m->pattern[m->order[pl->order_index]];
            for (c = 0; c < m->channels; c++)
                row_tick_n(pl, &pl->ch[c],
                           &pat->cell[pl->row * m->channels + c], pl->tick);
        }

        /* envelopes and fadeout advance once per tick, not once per frame */
        for (c = 0; c < m->channels; c++) {
            XmChannel *ch = &pl->ch[c];
            tick_envelope(ch);
            if (ch->key_off && ch->instr) {
                ch->fadeout_volume -= (float)ch->instr->fadeout / 32768.0f;
                if (ch->fadeout_volume <= 0.0f) {
                    ch->fadeout_volume = 0.0f;
                    ch->active = 0;
                }
            }
        }

        /* --- mix this tick. A tick is 2.5/bpm seconds; the fractional part is
         * carried so a non-integral frames-per-tick does not drift. --- */
        spt_f = (double)cfg->rate * 2.5 / (double)pl->bpm + carry;
        spt   = (long)spt_f;
        carry = spt_f - (double)spt;

        for (i = 0; i < spt && st->frames < max_frames; i++) {
            double l = 0.0, r = 0.0, g = gain;

            for (c = 0; c < m->channels; c++) {
                XmChannel *ch = &pl->ch[c];
                double v, s, pan;
                if (!ch->active || !ch->sample) continue;
                s = (double)sample_at(ch->sample, ch->pos);
                v = (double)ch->volume / 64.0
                  * (double)ch->env_vol_value
                  * (double)ch->fadeout_volume;
                pan = (double)ch->panning / 255.0;
                l += s * v * (1.0 - pan);
                r += s * v * pan;
                advance_channel(ch);
            }

            if (fabs(l) > st->peak) st->peak = fabs(l);
            if (fabs(r) > st->peak) st->peak = fabs(r);

            if (out != NULL) {
                unsigned char o[4];
                long il, ir;
                if (fade_start >= 0 && st->frames >= fade_start) {
                    double t = (double)(st->frames - fade_start) / (double)fade_frames;
                    if (t > 1.0) t = 1.0;
                    g *= 1.0 - t;
                }
                il = lrint(l * g * 32767.0);
                ir = lrint(r * g * 32767.0);
                if (il >  32767) { il =  32767; st->clipped++; }
                if (il < -32768) { il = -32768; st->clipped++; }
                if (ir >  32767) { ir =  32767; st->clipped++; }
                if (ir < -32768) { ir = -32768; st->clipped++; }
                o[0] = (unsigned char)(il & 0xFF);
                o[1] = (unsigned char)((il >> 8) & 0xFF);
                o[2] = (unsigned char)(ir & 0xFF);
                o[3] = (unsigned char)((ir >> 8) & 0xFF);
                if (fwrite(o, 1, 4, out) != 4)
                    die("write error");
            }
            st->frames++;
        }

        /* --- advance tick / row / order --- */
        if (++pl->tick >= pl->speed) {
            const XmPattern *pat = &m->pattern[m->order[pl->order_index]];
            int next_row = pl->row + 1;
            int advance_order = 0;
            pl->tick = 0;

            if (pl->pattern_jump_order >= 0 || pl->pattern_break_row >= 0) {
                if (pl->pattern_jump_order >= 0) {
                    pl->order_index = pl->pattern_jump_order;
                    if (pl->order_index >= m->song_length)
                        pl->order_index = m->restart_position;
                } else {
                    advance_order = 1;
                }
                pl->row = (pl->pattern_break_row >= 0) ? pl->pattern_break_row : 0;
                pl->pattern_jump_order = -1;
                pl->pattern_break_row = -1;
            } else if (next_row >= pat->rows) {
                advance_order = 1;
                pl->row = 0;
            } else {
                pl->row = next_row;
            }

            if (advance_order) {
                pl->order_index++;
                if (pl->order_index >= m->song_length) {
                    pl->order_index = m->restart_position;
                    pass++;
                    if (pass >= cfg->passes)
                        done = 1;
                    if (pass == 1 && st->loop_start_frame < 0)
                        st->loop_start_frame = st->frames;
                }
            }
            if (pl->order_index < m->song_length &&
                m->order[pl->order_index] < m->num_patterns &&
                pl->row >= m->pattern[m->order[pl->order_index]].rows)
                pl->row = 0;
        }
    }
}

int main(int argc, char **argv)
{
    const char *in_path = NULL, *out_path = NULL;
    RenderCfg   cfg;
    double      headroom_db = -1.0, fixed_gain = 0.0;
    int         i, measure_only = 0;

    XmModule   *m;
    XmPlayer    pl;
    Buf         buf;
    FILE       *fin, *fout;
    unsigned char *image;
    long        image_size;
    RenderStats measure, final_st;
    double      gain;

    cfg.rate = 44100;
    cfg.passes = 2;
    cfg.fade_ms = 4000;
    cfg.max_seconds = 900.0;

    for (i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "--rate") && i + 1 < argc)             cfg.rate = atoi(argv[++i]);
        else if (!strcmp(a, "--passes") && i + 1 < argc)      cfg.passes = atoi(argv[++i]);
        else if (!strcmp(a, "--fade-ms") && i + 1 < argc)     cfg.fade_ms = atoi(argv[++i]);
        else if (!strcmp(a, "--max-seconds") && i + 1 < argc) cfg.max_seconds = atof(argv[++i]);
        else if (!strcmp(a, "--headroom-db") && i + 1 < argc) headroom_db = atof(argv[++i]);
        else if (!strcmp(a, "--gain") && i + 1 < argc)        fixed_gain = atof(argv[++i]);
        else if (!strcmp(a, "--measure"))                     measure_only = 1;
        else if (a[0] == '-' && a[1])
            die("unknown option %s", a);
        else if (!in_path)  in_path = a;
        else if (!out_path) out_path = a;
        else die("too many arguments");
    }
    if (!in_path || (!out_path && !measure_only))
        die("usage: xm_render [--rate N] [--passes N] [--fade-ms N]\n"
            "                 [--headroom-db X] [--gain X] <in.xm> <out.raw>\n"
            "       xm_render [options] --measure <in.xm>");
    if (cfg.rate < 8000 || cfg.rate > 192000) die("silly sample rate %d", cfg.rate);
    if (cfg.passes < 1) cfg.passes = 1;
    if (cfg.fade_ms < 0) cfg.fade_ms = 0;

    fin = fopen(in_path, "rb");
    if (!fin) die("cannot open %s", in_path);
    fseek(fin, 0, SEEK_END);
    image_size = ftell(fin);
    fseek(fin, 0, SEEK_SET);
    if (image_size < 80) die("%s is too small to be a module", in_path);
    image = (unsigned char *)xmalloc((size_t)image_size);
    if (fread(image, 1, (size_t)image_size, fin) != (size_t)image_size)
        die("short read on %s", in_path);
    fclose(fin);

    buf.p = image;
    buf.size = (size_t)image_size;
    m = (XmModule *)xmalloc(sizeof(XmModule));
    parse_module(&buf, m);

    /* Pass 1: measure. XM has no global mix attenuation -- a module with many
     * simultaneous channels routinely sums past unity -- so the only way to emit
     * a clean lossless file is to learn the true peak first and then scale. */
    player_init(&pl, m, cfg.rate);
    render(&pl, &cfg, NULL, 1.0, 0, &measure);

    if (fixed_gain > 0.0) {
        gain = fixed_gain;
    } else if (measure.peak > 0.0) {
        gain = pow(10.0, headroom_db / 20.0) / measure.peak;
    } else {
        gain = 1.0;
    }

    /* --measure stops here and reports pass 1. The caller needs this to pick one
     * gain for a whole soundtrack: scaling each module to its own peak would
     * throw away the relative loudness the composer wrote, which in XM lives
     * entirely in the sample volumes, the volume column, Cxx and the envelopes
     * -- there is no module-level master volume to read it off. */
    if (measure_only) {
        final_st = measure;
    } else {
        fout = fopen(out_path, "wb");
        if (!fout) die("cannot open %s for writing", out_path);
        player_init(&pl, m, cfg.rate);
        render(&pl, &cfg, fout, gain, measure.frames, &final_st);
        if (fclose(fout) != 0)
            die("error closing %s", out_path);
    }

    /* Report for the extractor's manifest. `unhandled_effects` being non-empty
     * means the render is NOT trustworthy; the caller checks it. */
    printf("{\"name\":\"%s\",\"channels\":%d,\"patterns\":%d,\"instruments\":%d,"
           "\"order_length\":%d,\"restart\":%d,\"frequency\":\"%s\","
           "\"speed\":%d,\"bpm\":%d,\"rate\":%d,\"passes\":%d,"
           "\"frames\":%ld,\"seconds\":%.3f,\"loop_start_frame\":%ld,"
           "\"restart_frame\":%ld,"
           "\"raw_peak\":%.4f,\"gain\":%.6f,\"clipped_samples\":%ld,"
           "\"unhandled_effects\":[",
           m->name, m->channels, m->num_patterns, m->num_instr,
           m->song_length, m->restart_position,
           m->linear_freq ? "linear" : "amiga",
           m->default_speed, m->default_bpm, cfg.rate, cfg.passes,
           final_st.frames, (double)final_st.frames / cfg.rate,
           final_st.loop_start_frame, final_st.restart_frame,
           measure.peak, gain, final_st.clipped);
    {
        int first = 1;
        for (i = 0; i < 64; i++) {
            if (!pl.unhandled[i]) continue;
            printf("%s{\"effect\":%d,\"count\":%ld}", first ? "" : ",", i,
                   pl.unhandled[i]);
            first = 0;
        }
    }
    printf("]}\n");
    return 0;
}
