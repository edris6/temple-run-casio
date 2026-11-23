#ifndef TEMPLE_RENDERER_H
#define TEMPLE_RENDERER_H

#include <stddef.h>
#include <stdint.h>

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
} TRMesh;

/* Path and player are the exported Object81-Pivot and player meshes. */
void tr_render_frame(TRFramebuffer *fb, const TRMesh *path, const TRMesh *player,
    float forward, float height, float lateral);

#endif
