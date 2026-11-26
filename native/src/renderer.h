#ifndef TEMPLE_RENDERER_H
#define TEMPLE_RENDERER_H

#include <stddef.h>
#include <stdint.h>
#include "course.h"
#include "raster.h"

#define TR_MESH_VERTEX_LIMIT 512

typedef struct {
    uint16_t corner[3]; /* indices into the mesh's positions */
    uint16_t floor;     /* lies flat in the temple floor */
    TRTexCoord uv[3];
} TRTriangle;

typedef struct {
    const int16_t (*positions)[3]; /* prefab-local, sixteenths of a unit */
    const TRTriangle *triangles;
    size_t position_count, triangle_count;
    const TRTexture *texture;      /* optional */
    /* Bounding sphere. Radius zero means never cull. */
    int16_t centre[3];
    int32_t radius;
} TRMesh;

/* The converter's vertices (position and UV pairs) and triangle indices. */
typedef struct {
    const int16_t (*positions)[3]; /* prefab-local, sixteenths of a unit */
    const int16_t (*uvs)[2];       /* 1/2048, v increasing upward */
    const uint16_t *indices;
    size_t vertex_count, index_count;
} TRMeshSource;

typedef struct {
    const TRMesh *path[2], *corner[4], *jump[2], *slide[2], *trip[2];
    const TRMesh *coin, *player;
    const TRMesh *hole[2], *narrow[2];
} TRAssets;

/* Prepares a mesh for drawing: shares equal positions between triangles, so
 * each is transformed once, and computes bounds. positions needs room for
 * vertex_count entries and triangles for index_count / 3. Triangles with
 * out-of-range indices are dropped. Returns the number of positions used. */
size_t tr_mesh_build(TRMesh *mesh, const TRMeshSource *source, const TRTexture *texture,
    int16_t (*positions)[3], TRTriangle *triangles);
/* The most triangles one frame can show; any further ones are left out. */
#define TR_SCENE_TRIANGLES 2304

/* Transforms what the camera sees into a list of screen triangles. */
void tr_scene_build(const TRCourse *course, const TRAssets *assets, const TRPose *pose,
    bool sliding);
/* Clears the target's rows to sky and ground and draws the listed triangles
 * there. A frame may be drawn in any number of targets. */
void tr_scene_draw(const TRTarget *target);
/* The number of triangles listed by the last tr_scene_build. */
size_t tr_scene_size(void);

#endif
