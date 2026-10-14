/* light_smooth.h: smooth shading normals for the game's low-polygon meshes,
 * made each frame from the triangles the camera draws (plain C; the Metal
 * renderer feeds its lit draws through it, lighttest checks it on shapes
 * whose normals are known).
 *
 * A corner's normal is the sum of the face normals of every triangle drawn
 * with a corner at the same point (within LIGHT_SMOOTH_WELD metres, so
 * separate draws of one surface join), each weighted by its angle at that
 * corner (Thurmer and Wuthrich 1998: independent of how the surface is cut
 * into triangles), counting only faces within the crease angle of the
 * corner's own face (a sharper edge stays hard).  The game draws faces
 * either way round and from either side: a neighbour facing the other way
 * is turned over before it is compared and added.  The result points to the
 * same side as the corner's own face; a triangle with no area keeps none (0). */
#ifndef BR_LIGHT_SMOOTH_H
#define BR_LIGHT_SMOOTH_H

#ifdef __cplusplus
extern "C" {
#endif

#define LIGHT_SMOOTH_WELD 0.01f       /* metres */
#define LIGHT_SMOOTH_CREASE 50.0f     /* degrees: low-polygon hills and banks smooth, kerbs and walls hard */

/* p: ncorner x 3 floats, three corners a triangle (ncorner a multiple of 3);
 * n: ncorner x 3 floats out.  cos_crease: the cosine of the crease angle.
 * 0 on success, -1 out of memory (n zeroed) */
int light_smooth_normals(const float *p, int ncorner, float cos_crease, float *n);

#ifdef __cplusplus
}
#endif

#endif
