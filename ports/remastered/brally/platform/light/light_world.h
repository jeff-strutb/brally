/* light_world.h: the track's own lights at night (light_world.c) */
#ifndef BR_LIGHT_WORLD_H
#define BR_LIGHT_WORLD_H

#include "light.h"

#define LIGHT_WORLD_CELL 4.0f     /* metres: emitters closer than this, facing the same way, are one light */
#define LIGHT_WORLD_OFF  0.3f     /* metres in front of the surface */
#define LIGHT_WORLD_MAX  96       /* the most the clustered set takes from the track */
#define LIGHT_LH_DEG_S   37.5f    /* the lighthouse: 1.25 degrees a step at the race's 30 Hz, a turn in 9.6 s */

typedef struct LightEmitter {
    float p[3], n[3];
    float area;                   /* m2 */
    float lum;                    /* cd/m2 */
    float rgb[3];
} LightEmitter;

int   light_world_cluster(const LightEmitter *e, int ne, LightSpot *out, int max);
float light_world_flux(const LightSpot *s, int n);   /* lumens of the point lights */
void  light_world_lighthouse(LightSpot *o, const float pos[3], float t, float deg_s);
void  light_world_set(const LightSpot *s, int n);
void  light_world_set_lighthouse(const float *pos);  /* NULL: none */
int   light_world_spots(LightSpot *out, int max, float t);
/* the renderer's glowing track draws (n triangles, 9 floats each), lum
 * their luminance (cd/m2), rgb their colour */
void  light_world_gather(unsigned gen, const float *tri, int n, float lum, const float rgb[3], const float eye[3]);
int   light_world_refresh(void);   /* clusters what was gathered when it grew */
int   light_world_emitters(void);
void  light_world_beam(LightSpot *o, const float pos[3], const float xaxis[3]);
/* the lighthouse beams of the track (light_game.c): each one's position and
 * its model's x axis, live (the race turns them) */
int   light_game_beams(float pos[][3], float xaxis[][3], int max);
/* a beam instance by its record: flag bytes 0x4C, 0x4D (Coastline's 64 and 142: 6f 06) */
static inline int light_beam_record(unsigned char f4c, unsigned char f4d) { return f4c == 0x6f && f4d == 0x06; }

#endif
