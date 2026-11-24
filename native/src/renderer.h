#ifndef TEMPLE_RENDERER_H
#define TEMPLE_RENDERER_H

#include <stddef.h>
#include <stdint.h>
#include "course.h"

#define TR_WIDTH 192
#define TR_HEIGHT 108

typedef struct {
    uint16_t pixels[TR_WIDTH * TR_HEIGHT];
    uint16_t depth[TR_WIDTH * TR_HEIGHT];
} TRFramebuffer;

typedef struct {
    const float (*vertices)[3];
    const uint16_t *indices;
    size_t vertex_count, index_count;
    const float (*uvs)[2];
    const uint16_t *texture; /* 128x128 RGB565, optional */
    const int16_t (*fixed_vertices)[3], (*fixed_uvs)[2];
} TRMesh;

typedef struct {
    const TRMesh *path[2], *corner[4], *jump[2], *slide[2], *trip[2];
    const TRMesh *coin, *player;
    const TRMesh *hole[2], *narrow[2];
} TRAssets;

/* Path and player are the exported Object81-Pivot and player meshes. */
void tr_render_frame(TRFramebuffer *fb, const TRMesh *path, const TRMesh *player,
    float forward, float height, float lateral);
void tr_render_world(TRFramebuffer *fb, const TRCourse *course, const TRAssets *assets,
    float height);

#endif
