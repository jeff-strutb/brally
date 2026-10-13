/* game_stubs.h: the game's side of the renderer, absent from the test harnesses
 * that compile brr_metal.m itself (lighttest.m, night_test.m); none of it is
 * reached by the tests.  One copy, so a renderer change that calls something
 * new from the game is stubbed once for every harness. */
int plat_flag(int flag) { (void)flag; return 0; }
int host_window_visible(void) { return 0; }
int light_capture_camera(LightCamera *c) { (void)c; return 0; }
int light_capture_eye(float eye[3]) { (void)eye; return 0; }
const LightTrack *light_game_track(void) { return NULL; }
int light_game_weather(void) { return 0; }
int light_glide_flip(void) { return 0; }
int light_scene_spots(LightSpot *out, int max) { (void)out; (void)max; return 0; }
#ifndef LT_HAVE_LIGHT_WORLD   /* night_test.m links the real light_world.c */
void light_world_gather(unsigned gen, const float *tri, int n, float share, const float rgb[3], const float eye[3])
{
    (void)gen, (void)tri, (void)n, (void)share, (void)rgb, (void)eye;
}
#endif
int brr_refl_tier(void) { return 0; }   /* reflections: off here (refl.sh tests them) */
void brr_refl_pass(id<MTLDevice> dev, id<MTLCommandBuffer> cb, const void *lu, size_t lusz, const BrrRtWorld *rt, id<MTLTexture> alb, id<MTLTexture> pos,
                   id<MTLTexture> nrm, id<MTLTexture> hdr, id<MTLTexture> sky, id<MTLTexture> bE) { }
int brr_rt_update(BrrRtWorld *w, id<MTLDevice> dev, id<MTLCommandQueue> q, const LightTrack *t)   /* the track's rays: rt.sh's */
{
    (void)w, (void)dev, (void)q, (void)t;
    return 0;
}
/* the irradiance probes: gi.sh's (no track here, so never reached) */
int brr_gi_init(BrrGi *g, id<MTLDevice> dev, int nx, int ny, int nz, int nr) { (void)g, (void)dev, (void)nx, (void)ny, (void)nz, (void)nr; return 0; }
void brr_gi_frame(BrrGi *g, const float eye[3], float sp, const float sun[3], const float sunc[3], const float sky[3], const float gnd[3])
{
    (void)g, (void)eye, (void)sp, (void)sun, (void)sunc, (void)sky, (void)gnd;
}
void brr_gi_encode(BrrGi *g, id<MTLCommandBuffer> cb, const BrrRtWorld *w, id<MTLBuffer> lset, int nlamp) { (void)g, (void)cb, (void)w, (void)lset, (void)nlamp; }
id<MTLTexture> brr_gi_irr(const BrrGi *g) { (void)g; return nil; }
id<MTLTexture> brr_gi_dst(const BrrGi *g) { (void)g; return nil; }

