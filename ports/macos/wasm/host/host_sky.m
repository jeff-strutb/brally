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
 * Files: <dir>/<env>_<weather>.png, written by ports/macos/tools/remaster_sky.py;
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
    env = ENV[track & 15];
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
