/* compile_test.m: the renderer's whole shader source (as brr_open() puts it
 * together: shader_src.py) compiled by this Mac's Metal compiler, as the game
 * compiles it at start-up, and the lighting's pipelines built from it with the
 * renderer's formats.  No game run.
 *
 *   compile_test LIT.metal */
#import <Metal/Metal.h>
#include <stdio.h>

static int g_fail;

static void pipe_ok(id<MTLDevice> dev, id<MTLLibrary> lib, NSString *vs, NSString *fs, const MTLPixelFormat *f, int nf, int depth)
{
    MTLRenderPipelineDescriptor *d = [MTLRenderPipelineDescriptor new];
    NSError *err = nil;
    int i;
    d.vertexFunction = [lib newFunctionWithName:vs];
    d.fragmentFunction = fs ? [lib newFunctionWithName:fs] : nil;
    for (i = 0; i < nf; i++)
        d.colorAttachments[i].pixelFormat = f[i];
    if (depth)
        d.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
    if (!d.vertexFunction || (fs && !d.fragmentFunction) || ![dev newRenderPipelineStateWithDescriptor:d error:&err]) {
        printf("FAIL pipeline %s/%s: %s\n", [vs UTF8String], fs ? [fs UTF8String] : "-", err ? [[err description] UTF8String] : "no function");
        g_fail++;
    } else {
        printf("  pipeline %s/%s built\n", [vs UTF8String], fs ? [fs UTF8String] : "-");
    }
}

int main(int argc, char **argv)
{
    @autoreleasepool {
        static const MTLPixelFormat gb[3] = { MTLPixelFormatRGBA8Unorm, MTLPixelFormatRGBA32Float, MTLPixelFormatRGBA16Float };
        static const MTLPixelFormat hf = MTLPixelFormatRGBA16Float;
        id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
        NSError *err = nil;
        NSString *src;
        id<MTLLibrary> lib;
        if (argc < 2 || !dev)
            return 2;
        src = [NSString stringWithContentsOfFile:[NSString stringWithUTF8String:argv[1]] encoding:NSUTF8StringEncoding error:&err];
        lib = src ? [dev newLibraryWithSource:src options:nil error:&err] : nil;
        if (!lib) {
            printf("FAIL the renderer's shader source: %s\n", [[err description] UTF8String]);
            return 1;
        }
        printf("compile: the renderer's shader source (%lu bytes) compiled by %s's Metal compiler, %lu functions\n",
               (unsigned long)[src length], [[dev name] UTF8String], (unsigned long)[[lib functionNames] count]);
        pipe_ok(dev, lib, @"vs", @"fsgb", gb, 3, 1);
        pipe_ok(dev, lib, @"lvs", @"lfs", &hf, 1, 0);
        pipe_ok(dev, lib, @"vs", @"fsf", &hf, 1, 1);
        pipe_ok(dev, lib, @"svs", nil, NULL, 0, 1);
        pipe_ok(dev, lib, @"lcclr", nil, NULL, 0, 1);
        pipe_ok(dev, lib, @"lcvs", nil, NULL, 0, 1);
        {   /* the BRDF's albedo table, made at start-up */
            static const MTLPixelFormat rg = MTLPixelFormatRG16Float;
            pipe_ok(dev, lib, @"lvs", @"brdf_lut", &rg, 1, 0);
            pipe_ok(dev, lib, @"lvs", @"brdf_avg", &rg, 1, 0);
        }
        pipe_ok(dev, lib, @"lvs", @"vol_apply", &hf, 1, 0);   /* the volumetric fog (brr_metal_vol.h) */
        for (NSString *k in @[ @"vol_inject", @"vol_integrate" ]) {
            id<MTLFunction> fn = [lib newFunctionWithName:k];
            if (!fn || ![dev newComputePipelineStateWithFunction:fn error:&err]) {
                printf("FAIL kernel %s: %s\n", [k UTF8String], err ? [[err description] UTF8String] : "no function");
                g_fail++;
            } else {
                printf("  kernel %s built\n", [k UTF8String]);
            }
        }
    }
    printf(g_fail ? "compile_test: %d FAILED\n" : "compile_test: all passed\n", g_fail);
    return g_fail != 0;
}
