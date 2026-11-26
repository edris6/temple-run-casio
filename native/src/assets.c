/* The converted meshes and textures (see tools/import_assets.py). The add-in's
 * ROM is slow to read, so everything used while drawing is copied to RAM. */
#include "assets.h"
#include <string.h>
#include "player.h"
#include "path.h"
#include "path_h.h"
#include "left_corner.h"
#include "right_corner.h"
#include "corner_c.h"
#include "corner_d.h"
#include "hole.h"
#include "hole_h.h"
#include "narrow.h"
#include "narrow_h.h"
#include "jump.h"
#include "jump_h.h"
#include "slide.h"
#include "slide_h.h"
#include "trip.h"
#include "trip_h.h"
#include "coin.h"
#include "player_tex.h"
#include "path_tex.h"
#include "tree_tex.h"

#define MESHES(X) \
    X(path, PATH, WALL) X(path_h, PATH_H, WALL) \
    X(left_corner, LEFT_CORNER, WALL) X(right_corner, RIGHT_CORNER, WALL) \
    X(corner_c, CORNER_C, WALL) X(corner_d, CORNER_D, WALL) \
    X(hole, HOLE, WALL) X(hole_h, HOLE_H, WALL) \
    X(narrow, NARROW, WALL) X(narrow_h, NARROW_H, WALL) \
    X(jump, JUMP, TREE) X(jump_h, JUMP_H, TREE) \
    X(slide, SLIDE, TREE) X(slide_h, SLIDE_H, TREE) \
    X(trip, TRIP, TREE) X(trip_h, TRIP_H, TREE) \
    X(coin, COIN, WALL) X(player, PLAYER, RUNNER)

enum { WALL, TREE, RUNNER, TEXTURES };

#define DECLARE(name, NAME, texture) static TRMesh name##_mesh; \
    _Static_assert(NAME##_VERTEX_COUNT <= TR_MESH_VERTEX_LIMIT, "mesh too large: " #name);
MESHES(DECLARE)

#define VERTICES(name, NAME, texture) + NAME##_VERTEX_COUNT
#define TRIANGLES(name, NAME, texture) + NAME##_INDEX_COUNT / 3
static int16_t positions[0 MESHES(VERTICES)][3];
static TRTriangle triangles[0 MESHES(TRIANGLES)];
static uint16_t texture_pixels[TEXTURES][TR_TEXTURE_SIZE * TR_TEXTURE_SIZE];
static uint16_t mip_pixels[TEXTURES][TR_MIP_PIXELS];
static TRTexture textures[TEXTURES];

const TRAssets *tr_assets_load(void)
{
    static const uint16_t *const source[TEXTURES] = {path_texture, tree_texture, player_texture};
    for (int i = 0; i < TEXTURES; i++) {
        memcpy(texture_pixels[i], source[i], sizeof texture_pixels[i]);
        tr_texture_build(&textures[i], texture_pixels[i], mip_pixels[i]);
    }
    size_t used_positions = 0, used_triangles = 0;
#define LOAD(name, NAME, texture_id) { \
        TRMeshSource data = {name##_fixed_vertices, name##_fixed_uvs, name##_indices, \
            NAME##_VERTEX_COUNT, NAME##_INDEX_COUNT}; \
        used_positions += tr_mesh_build(&name##_mesh, &data, &textures[texture_id], \
            positions + used_positions, triangles + used_triangles); \
        used_triangles += name##_mesh.triangle_count; }
    MESHES(LOAD)
    static const TRAssets assets = {
        .path = {&path_mesh, &path_h_mesh},
        .corner = {&left_corner_mesh, &right_corner_mesh, &corner_c_mesh, &corner_d_mesh},
        .jump = {&jump_mesh, &jump_h_mesh},
        .slide = {&slide_mesh, &slide_h_mesh},
        .trip = {&trip_mesh, &trip_h_mesh},
        .coin = &coin_mesh,
        .player = &player_mesh,
        .hole = {&hole_mesh, &hole_h_mesh},
        .narrow = {&narrow_mesh, &narrow_h_mesh},
    };
    return &assets;
}
