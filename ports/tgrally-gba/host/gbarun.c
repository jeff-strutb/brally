/* gbarun.c -- run a GBA ROM headless under mGBA (libmgba), save the screen
 * at chosen frames as PNG and print a word of the ROM's statistics block
 * once a second.  GBARUN_RAW=FILE writes every frame from GBARUN_RAW_FROM on as
 * raw 240x160 RGB24 (for a video).  GBARUN_MEM=FILE saves EWRAM at the end.
 * GBARUN_IWRAM=FILE@FRAME: IWRAM after that frame.
 * GBARUN_PROBE=ADDR:FRAME:X:Y holds the camera at FRAME, unless GBARUN_REALTIME
 * (the ROM's own pacing) or GBARUN_STEP (every recorded frame in turn) is set.
 * GBARUN_WAV=FILE: the sound, from GBARUN_RAW_FROM on (16-bit stereo WAV).
 * GBARUN_PROF=FILE@FROM: from frame FROM on, the cycles spent at each code
 * address (IWRAM and ROM), stepped an instruction at a time, into FILE.
 *
 *   gbarun ROM FRAMES STATS_ADDR OUTDIR [SHOT_FRAME...]
 *
 * STATS_ADDR (hex) is the ROM's struct of counters (see gba/main.c: g_stats);
 * each line printed is frame, then its words.  */
#include <mgba/flags.h>
#include <mgba/core/core.h>
#include <mgba/core/config.h>
#include <mgba-util/vfs.h>
#include <mgba-util/audio-buffer.h>
#include <mgba/internal/arm/arm.h>
#include <mgba/internal/gba/gba.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

static void wr32be(unsigned char *p, uint32_t v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }

static void chunk(FILE *f, const char *t, const unsigned char *d, uint32_t n)
{
    unsigned char b[4];
    uint32_t crc = crc32(0, (const unsigned char *)t, 4);
    crc = crc32(crc, d, n);
    wr32be(b, n);
    fwrite(b, 1, 4, f);
    fwrite(t, 1, 4, f);
    fwrite(d, 1, n, f);
    wr32be(b, crc);
    fwrite(b, 1, 4, f);
}

static void png(const char *path, const mColor *px, int w, int h, int stride, int scale)
{
    int W = w * scale, H = h * scale, x, y;
    uLongf zn = compressBound((uLong)(W * 3 + 1) * H);
    unsigned char *raw = malloc((size_t)(W * 3 + 1) * H), *z = malloc(zn), hdr[13];
    FILE *f = fopen(path, "wb");
    for (y = 0; y < H; y++) {
        unsigned char *r = raw + (size_t)y * (W * 3 + 1);
        r[0] = 0;
        for (x = 0; x < W; x++) {
            uint32_t c = (uint32_t)px[(y / scale) * stride + x / scale];
            r[1 + x * 3] = c & 0xFF;
            r[2 + x * 3] = (c >> 8) & 0xFF;
            r[3 + x * 3] = (c >> 16) & 0xFF;
        }
    }
    compress2(z, &zn, raw, (uLong)(W * 3 + 1) * H, 6);
    fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
    wr32be(hdr, W);
    wr32be(hdr + 4, H);
    hdr[8] = 8; hdr[9] = 2; hdr[10] = hdr[11] = hdr[12] = 0;
    chunk(f, "IHDR", hdr, 13);
    chunk(f, "IDAT", z, (uint32_t)zn);
    chunk(f, "IEND", NULL, 0);
    fclose(f);
    free(raw);
    free(z);
}

int main(int argc, char **argv)
{
    static mColor buf[256 * 160];
    struct mCore *core;
    int frames, f, i, nshot = argc - 5;
    uint32_t stats;
    if (argc < 5) {
        fprintf(stderr, "usage: gbarun ROM FRAMES STATS_ADDR OUTDIR [SHOT_FRAME...]\n");
        return 2;
    }
    frames = atoi(argv[2]);
    stats = (uint32_t)strtoul(argv[3], NULL, 16);
    core = mCoreFind(argv[1]);
    if (!core || !core->init(core))
        return 1;
    core->setVideoBuffer(core, buf, 256);
    if (!mCoreLoadFile(core, argv[1]))
        return 1;
    mCoreInitConfig(core, NULL);
    core->reset(core);
    FILE *raw = getenv("GBARUN_RAW") ? fopen(getenv("GBARUN_RAW"), "wb") : NULL;   /* every frame, RGB24 */
    int raw_from = getenv("GBARUN_RAW_FROM") ? atoi(getenv("GBARUN_RAW_FROM")) : 1;
    unsigned pa = 0, pf = 0, px = 0, py = 0;
    if (getenv("GBARUN_PROBE"))                       /* ADDR:FRAME:X:Y -- g_probe (gba/main.c) */
        sscanf(getenv("GBARUN_PROBE"), "%x:%u:%u:%u", &pa, &pf, &px, &py);
    static uint64_t prof_iw[0x4000], prof_rom[0x200000];
    FILE *wav = getenv("GBARUN_WAV") ? fopen(getenv("GBARUN_WAV"), "wb") : NULL;
    uint32_t wav_n = 0;
    if (wav) {
        fwrite("RIFF\0\0\0\0WAVEfmt \x10\0\0\0\x01\0\x02\0\0\0\0\0\0\0\0\0\x04\0\x10\0data\0\0\0\0", 1, 44, wav);
        core->setAudioBufferSize(core, 4096);
    }
    char prof[512] = "";
    int prof_from = 0;
    if (getenv("GBARUN_PROF"))                        /* FILE@FROM */
        sscanf(getenv("GBARUN_PROF"), "%511[^@]@%d", prof, &prof_from);
    for (f = 1; f <= frames; f++) {
        if (pa && f <= 30) {                          /* until the program has started */
            if (getenv("GBARUN_STEP"))               /* g_probe[2]: every recorded camera frame in turn */
                core->busWrite32(core, pa + 8, 1);
            if (!getenv("GBARUN_REALTIME") && !getenv("GBARUN_STEP")) {   /* the camera held at FRAME */
                core->busWrite32(core, pa, pf + 1);
                core->busWrite32(core, pa + 4, px | py << 16);
            }
        }
        if (prof && f >= prof_from) {
            struct GBA *gba = (struct GBA *)core->board;
            uint32_t fc = core->frameCounter(core);
            while (core->frameCounter(core) == fc) {
                uint32_t pc = (uint32_t)gba->cpu->gprs[15] - (gba->cpu->executionMode == MODE_THUMB ? 4 : 8);
                int32_t t = mTimingCurrentTime(&gba->timing);
                core->step(core);
                t = mTimingCurrentTime(&gba->timing) - t;
                if (pc >= 0x03000000 && pc < 0x03008000)
                    prof_iw[(pc - 0x03000000) >> 1] += (uint64_t)t;
                else if (pc >= 0x08000000 && pc < 0x08400000)
                    prof_rom[(pc - 0x08000000) >> 1] += (uint64_t)t;
            }
        } else
            core->runFrame(core);
        if (getenv("GBARUN_IWRAM")) {
            char path[512];
            int at = 0;
            if (sscanf(getenv("GBARUN_IWRAM"), "%511[^@]@%d", path, &at) == 2 && at == f) {
                FILE *m = fopen(path, "wb");
                for (i = 0; i < 0x8000; i += 4) {
                    uint32_t w = core->busRead32(core, 0x03000000 + i);
                    fwrite(&w, 4, 1, m);
                }
                fclose(m);
            }
        }
        {
            struct mAudioBuffer *ab = core->getAudioBuffer(core);
            int16_t tmp[2 * 2048];
            size_t got;
            while ((got = mAudioBufferRead(ab, tmp, 2048)) > 0)
                if (wav && f >= raw_from) {
                    fwrite(tmp, 4, got, wav);
                    wav_n += (uint32_t)got;
                }
        }
        if (raw && f >= raw_from) {
            int x, y;
            for (y = 0; y < 160; y++)
                for (x = 0; x < 240; x++) {
                    uint32_t c = (uint32_t)buf[y * 256 + x];
                    fputc(c & 0xFF, raw);
                    fputc((c >> 8) & 0xFF, raw);
                    fputc((c >> 16) & 0xFF, raw);
                }
        }
        for (i = 0; i < nshot; i++)
            if (atoi(argv[5 + i]) == f) {
                char path[512];
                snprintf(path, sizeof path, "%s/gba_%05d.png", argv[4], f);
                png(path, buf, 240, 160, 256, 3);
                if (stats)
                    printf("shot %d frame %u\n", f, core->busRead32(core, stats));
            }
        if (stats && f % 60 == 0) {
            printf("%d", f);
            for (i = 0; i < 12; i++)
                printf(" %u", core->busRead32(core, stats + i * 4));
            printf("\n");
        }
    }
    if (pa)
        printf("probe triangle %u\n", core->busRead32(core, pa + 8));
    if (wav) {
        uint32_t rate = core->audioSampleRate(core), v;
        fseek(wav, 4, SEEK_SET);
        v = 36 + wav_n * 4;
        fwrite(&v, 4, 1, wav);
        fseek(wav, 24, SEEK_SET);
        fwrite(&rate, 4, 1, wav);
        v = rate * 4;
        fwrite(&v, 4, 1, wav);
        fseek(wav, 40, SEEK_SET);
        v = wav_n * 4;
        fwrite(&v, 4, 1, wav);
        fclose(wav);
    }
    if (*prof) {
        FILE *pf_ = fopen(prof, "w");
        for (i = 0; i < 0x4000; i++)
            if (prof_iw[i])
                fprintf(pf_, "%08X %llu\n", 0x03000000 + i * 2, (unsigned long long)prof_iw[i]);
        for (i = 0; i < 0x200000; i++)
            if (prof_rom[i])
                fprintf(pf_, "%08X %llu\n", 0x08000000 + i * 2, (unsigned long long)prof_rom[i]);
        fclose(pf_);
    }
    if (getenv("GBARUN_MEM")) {                       /* FILE: EWRAM at the end, for a look at the ROM's lists */
        FILE *m = fopen(getenv("GBARUN_MEM"), "wb");
        for (i = 0; i < 0x40000; i += 4) {
            uint32_t w = core->busRead32(core, 0x02000000 + i);
            fwrite(&w, 4, 1, m);
        }
        fclose(m);
    }
    core->deinit(core);
    return 0;
}
