/* host_sky.m -- the Remastered skies (port code).
 *
 * One panorama per track environment and weather, drawn by host_fx.m's
 * relight pass in place of the game's 64x64 painted sky texture: so it is on
 * exactly when Remastered is on (the ~ key), like the lighting and the car.
 *
 * Environments follow the sky file the game loads for the track
 * (BrTrackLoad 0x100311C0 indexes cargfx/skytex*.lut4 by the chosen track,
 * g_brCfgChosenTrack 0x100B3014): mirror tracks share their base track's sky,
 * the race track and the bonus track get their own.  Weather is
 * g_brCarPhysWeather 0x104B15E8 (0 sunny, 1 fog, 2 storm, 3 snow, 4 rain at
 * night).
 *
 * Files: <dir>/<env>_<weather>.png, written by ports/brally-wasm/tools/remaster_sky.py;
 * <dir> is BR_SKY_DIR, the app's Resources/sky, or ports/common/models/sky/pack.
 * Each picture spans 180 degrees of heading (host_fx.m wraps it twice) from
 * the horizon on its bottom row upward.
 *
 * Only the sky in use is resident (a 2560x1728 panorama with mips is ~24 MB).
 * It is decoded off the render thread; until it arrives host_fx.m keeps the
 * sky it drew before.
 */
#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <ImageIO/ImageIO.h>
#include "host.h"
#include <stdlib.h>
#include <string.h>

static const char *const ENV[16] = {
    "desert", "mountain", "coast", "mine", "amazon", "race",
    "desert", "mountain", "coast", "mine", "amazon", "race",
    "desert", "bonus", "bonus", "desert"
};
static const char *const WEATHER[5] = { "clear", "fog", "storm", "snow", "night" };

/* the environment of the track loaded: by host_env.m's reading of the track
 * itself where it has one, else by the chosen-track setting */
const char *henv_track_name(void);   /* host_env.m */
static int env_index(u32 track)
{
    const char *n = henv_track_name();
    int i;
    if (n && n[0]) for (i = 0; i < 16; i++) if (!strcmp(n, ENV[i])) return i;
    return (int)(track & 15);
}

static NSString *sky_dir(void)
{
    const char *e = getenv("BR_SKY_DIR");
    NSString *r;
    if (e) return [NSString stringWithUTF8String:e];
    r = [[NSBundle mainBundle].resourcePath stringByAppendingPathComponent:@"sky"];
    if ([[NSFileManager defaultManager] fileExistsAtPath:r]) return r;
    return @"ports/common/models/sky/pack";
}

static id<MTLTexture> decode(id<MTLDevice> dev, NSString *path)
{
    CGImageSourceRef src = CGImageSourceCreateWithURL((__bridge CFURLRef)[NSURL fileURLWithPath:path], NULL);
    CGImageRef img;
    CGContextRef cx;
    CGColorSpaceRef cs;
    MTLTextureDescriptor *td;
    id<MTLTexture> t;
    size_t w, h;
    void *px;
    if (!src) return nil;
    img = CGImageSourceCreateImageAtIndex(src, 0, NULL);
    CFRelease(src);
    if (!img) return nil;
    w = CGImageGetWidth(img); h = CGImageGetHeight(img);
    px = calloc(w * h, 4);
    cs = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    cx = CGBitmapContextCreate(px, w, h, 8, w * 4, cs, kCGImageAlphaNoneSkipLast | kCGBitmapByteOrder32Big);
    CGColorSpaceRelease(cs);
    if (!cx) { CGImageRelease(img); free(px); return nil; }
    CGContextDrawImage(cx, CGRectMake(0, 0, w, h), img);
    CGContextRelease(cx);
    CGImageRelease(img);
    /* sRGB storage: the shader samples linear light */
    td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm_sRGB
                                                            width:w height:h mipmapped:YES];
    td.usage = MTLTextureUsageShaderRead;
    t = [dev newTextureWithDescriptor:td];
    [t replaceRegion:MTLRegionMake2D(0, 0, w, h) mipmapLevel:0 withBytes:px bytesPerRow:w * 4];
    free(px);
    {
        id<MTLCommandQueue> q = [dev newCommandQueue];
        id<MTLCommandBuffer> cb = [q commandBuffer];
        id<MTLBlitCommandEncoder> b = [cb blitCommandEncoder];
        [b generateMipmapsForTexture:t];
        [b endEncoding];
        [cb commit];
        [cb waitUntilCompleted];
    }
    return t;
}

static NSObject *g_lock;
static id<MTLTexture> g_tex;         /* the sky for g_key, once decoded */
static int g_key = -1;               /* env * 8 + weather wanted */
static int g_have = -1;              /* the key g_tex holds */

/* The panorama for the track and weather being raced, or nil (not decoded
 * yet, or no file).  Called once a frame from host_fx.m. */
id<MTLTexture> hsky_tex(id<MTLDevice> dev, int weather)
{
    u32 track = H32(0x100B3014u);
    const char *env;
    int key;
    id<MTLTexture> t;
    if (!g_lock) g_lock = [NSObject new];
    if (weather < 0 || weather > 4) weather = 0;
    env = ENV[env_index(track)];
    {   const char *ov = getenv("BR_SKY_ENV");   /* check any sky on any track */
        int i;
        if (ov) for (i = 0; i < 16; i++) if (!strcmp(ov, ENV[i])) { env = ENV[i]; break; } }
    for (key = 0; key < 16 && ENV[key] != env; key++) ;
    key = key * 8 + weather;
    @synchronized (g_lock) {
        if (key != g_key) {
            NSString *path = [sky_dir() stringByAppendingPathComponent:
                              [NSString stringWithFormat:@"%s_%s.png", env, WEATHER[weather]]];
            g_key = key;
            fprintf(stderr, "sky: track %u -> %s_%s\n", (unsigned)track, env, WEATHER[weather]);
            dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
                id<MTLTexture> nt = decode(dev, path);
                if (!nt) fprintf(stderr, "sky: no %s\n", path.UTF8String);
                @synchronized (g_lock) {
                    if (g_key == key) { g_tex = nt; g_have = key; }
                }
            });
        }
        t = g_have == g_key ? g_tex : nil;
    }
    return t;
}

/* The track's horizon band: the distant land along the horizon, drawn over
 * the sky (<dir>/<env>_band.png, RGBA, ports/brally-wasm/tools/remaster_sky_band.py),
 * one for every weather of the track; or nil.  Alpha kept, straight (not
 * premultiplied): CoreGraphics draws RGBA premultiplied, so it is undone here. */
static id<MTLTexture> decode_rgba(id<MTLDevice> dev, NSString *path)
{
    CGImageSourceRef src = CGImageSourceCreateWithURL((__bridge CFURLRef)[NSURL fileURLWithPath:path], NULL);
    CGImageRef img;
    CGContextRef cx;
    CGColorSpaceRef cs;
    MTLTextureDescriptor *td;
    id<MTLTexture> t;
    size_t w, h, i;
    unsigned char *px;
    if (!src) return nil;
    img = CGImageSourceCreateImageAtIndex(src, 0, NULL);
    CFRelease(src);
    if (!img) return nil;
    w = CGImageGetWidth(img); h = CGImageGetHeight(img);
    px = calloc(w * h, 4);
    cs = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    cx = CGBitmapContextCreate(px, w, h, 8, w * 4, cs, kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
    CGColorSpaceRelease(cs);
    if (!cx) { CGImageRelease(img); free(px); return nil; }
    CGContextSetBlendMode(cx, kCGBlendModeCopy);
    CGContextDrawImage(cx, CGRectMake(0, 0, w, h), img);
    CGContextRelease(cx);
    CGImageRelease(img);
    for (i = 0; i < w * h; i++) {
        unsigned a = px[4 * i + 3], k;
        if (a && a < 255) for (k = 0; k < 3; k++) { unsigned v = px[4 * i + k] * 255u / a; px[4 * i + k] = (unsigned char)(v > 255 ? 255 : v); }
    }
    td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm_sRGB width:w height:h mipmapped:YES];
    td.usage = MTLTextureUsageShaderRead;
    t = [dev newTextureWithDescriptor:td];
    [t replaceRegion:MTLRegionMake2D(0, 0, w, h) mipmapLevel:0 withBytes:px bytesPerRow:w * 4];
    free(px);
    {
        id<MTLCommandQueue> q = [dev newCommandQueue];
        id<MTLCommandBuffer> cb = [q commandBuffer];
        id<MTLBlitCommandEncoder> b = [cb blitCommandEncoder];
        [b generateMipmapsForTexture:t];
        [b endEncoding];
        [cb commit];
        [cb waitUntilCompleted];
    }
    return t;
}

static id<MTLTexture> g_band;
static int g_band_key = -1, g_band_have = -1;
id<MTLTexture> hsky_band(id<MTLDevice> dev)
{
    u32 track = H32(0x100B3014u);
    int key = env_index(track);
    id<MTLTexture> t;
    if (!g_lock) g_lock = [NSObject new];
    {   const char *ov = getenv("BR_SKY_ENV");
        int i;
        if (ov) for (i = 0; i < 16; i++) if (!strcmp(ov, ENV[i])) { key = i; break; } }
    for (int i = 0; i < 16; i++) if (!strcmp(ENV[i], ENV[key])) { key = i; break; }   /* mirror tracks share it */
    @synchronized (g_lock) {
        if (key != g_band_key) {
            NSString *path = [sky_dir() stringByAppendingPathComponent:[NSString stringWithFormat:@"%s_band.png", ENV[key]]];
            int want = key;
            g_band_key = key;
            dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
                id<MTLTexture> nt = [[NSFileManager defaultManager] fileExistsAtPath:path] ? decode_rgba(dev, path) : nil;
                @synchronized (g_lock) {
                    if (g_band_key == want) { g_band = nt; g_band_have = want; }
                }
            });
        }
        t = g_band_have == g_band_key ? g_band : nil;
    }
    return t;
}
