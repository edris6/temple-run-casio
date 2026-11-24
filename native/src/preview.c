/* Desktop-only diagnostic: render one frame with the user's converted meshes. */
#include "renderer.h"
#include "player.h"
#include "path.h"
#include "player_tex.h"
#include "path_tex.h"
#include "jump.h"
#include "slide.h"
#include "trip.h"
#include "coin.h"
#include "tree_tex.h"
#include "left_corner.h"
#include "right_corner.h"
#include "corner_c.h"
#include "corner_d.h"
#include "path_h.h"
#include "jump_h.h"
#include "slide_h.h"
#include "trip_h.h"
#include "hole.h"
#include "hole_h.h"
#include "narrow.h"
#include "narrow_h.h"
#include <stdio.h>

static TRFramebuffer fb;
int main(int argc, char **argv)
{
    if (argc != 2) { fprintf(stderr, "usage: %s frame.ppm\n", argv[0]); return 2; }
    TRMesh path = {path_vertices,path_indices,PATH_VERTEX_COUNT,PATH_INDEX_COUNT,path_uvs,path_texture,path_fixed_vertices,path_fixed_uvs};
    TRMesh player = {player_vertices,player_indices,PLAYER_VERTEX_COUNT,PLAYER_INDEX_COUNT,player_uvs,player_texture,player_fixed_vertices,player_fixed_uvs};
    TRMesh jump = {jump_vertices,jump_indices,JUMP_VERTEX_COUNT,JUMP_INDEX_COUNT,jump_uvs,tree_texture,jump_fixed_vertices,jump_fixed_uvs};
    TRMesh slide = {slide_vertices,slide_indices,SLIDE_VERTEX_COUNT,SLIDE_INDEX_COUNT,slide_uvs,tree_texture,slide_fixed_vertices,slide_fixed_uvs};
    TRMesh trip = {trip_vertices,trip_indices,TRIP_VERTEX_COUNT,TRIP_INDEX_COUNT,trip_uvs,tree_texture,trip_fixed_vertices,trip_fixed_uvs};
    TRMesh coin = {coin_vertices,coin_indices,COIN_VERTEX_COUNT,COIN_INDEX_COUNT,coin_uvs,path_texture,coin_fixed_vertices,coin_fixed_uvs};
    TRMesh corners[] = {
        {left_corner_vertices,left_corner_indices,LEFT_CORNER_VERTEX_COUNT,LEFT_CORNER_INDEX_COUNT,left_corner_uvs,path_texture,left_corner_fixed_vertices,left_corner_fixed_uvs},
        {right_corner_vertices,right_corner_indices,RIGHT_CORNER_VERTEX_COUNT,RIGHT_CORNER_INDEX_COUNT,right_corner_uvs,path_texture,right_corner_fixed_vertices,right_corner_fixed_uvs},
        {corner_c_vertices,corner_c_indices,CORNER_C_VERTEX_COUNT,CORNER_C_INDEX_COUNT,corner_c_uvs,path_texture,corner_c_fixed_vertices,corner_c_fixed_uvs},
        {corner_d_vertices,corner_d_indices,CORNER_D_VERTEX_COUNT,CORNER_D_INDEX_COUNT,corner_d_uvs,path_texture,corner_d_fixed_vertices,corner_d_fixed_uvs}};
    TRMesh path_horizontal = {path_h_vertices,path_h_indices,PATH_H_VERTEX_COUNT,PATH_H_INDEX_COUNT,path_h_uvs,path_texture,path_h_fixed_vertices,path_h_fixed_uvs};
    TRMesh jh = {jump_h_vertices,jump_h_indices,JUMP_H_VERTEX_COUNT,JUMP_H_INDEX_COUNT,jump_h_uvs,tree_texture,jump_h_fixed_vertices,jump_h_fixed_uvs};
    TRMesh sh = {slide_h_vertices,slide_h_indices,SLIDE_H_VERTEX_COUNT,SLIDE_H_INDEX_COUNT,slide_h_uvs,tree_texture,slide_h_fixed_vertices,slide_h_fixed_uvs};
    TRMesh th = {trip_h_vertices,trip_h_indices,TRIP_H_VERTEX_COUNT,TRIP_H_INDEX_COUNT,trip_h_uvs,tree_texture,trip_h_fixed_vertices,trip_h_fixed_uvs};
    TRMesh hole_mesh = {hole_vertices,hole_indices,HOLE_VERTEX_COUNT,HOLE_INDEX_COUNT,hole_uvs,path_texture,hole_fixed_vertices,hole_fixed_uvs};
    TRMesh hole_horizontal = {hole_h_vertices,hole_h_indices,HOLE_H_VERTEX_COUNT,HOLE_H_INDEX_COUNT,hole_h_uvs,path_texture,hole_h_fixed_vertices,hole_h_fixed_uvs};
    TRMesh narrow_mesh = {narrow_vertices,narrow_indices,NARROW_VERTEX_COUNT,NARROW_INDEX_COUNT,narrow_uvs,path_texture,narrow_fixed_vertices,narrow_fixed_uvs};
    TRMesh narrow_horizontal = {narrow_h_vertices,narrow_h_indices,NARROW_H_VERTEX_COUNT,NARROW_H_INDEX_COUNT,narrow_h_uvs,path_texture,narrow_h_fixed_vertices,narrow_h_fixed_uvs};
    TRAssets assets = {{&path,&path_horizontal}, {&corners[0],&corners[1],&corners[2],&corners[3]},
        {&jump,&jh}, {&slide,&sh}, {&trip,&th}, &coin, &player,
        {&hole_mesh,&hole_horizontal}, {&narrow_mesh,&narrow_horizontal}};
    TRCourse course;
    tr_course_init(&course, 1);
    tr_render_world(&fb, &course, &assets, 0);
    FILE *f = fopen(argv[1], "wb");
    if (!f) { perror(argv[1]); return 1; }
    fprintf(f, "P6\n%d %d\n255\n", TR_WIDTH, TR_HEIGHT);
    for (int i = 0; i < TR_WIDTH * TR_HEIGHT; i++) {
        unsigned c = fb.pixels[i];
        unsigned char rgb[] = {(c >> 11) * 255 / 31, ((c >> 5) & 63) * 255 / 63,
                               (c & 31) * 255 / 31};
        fwrite(rgb, 1, 3, f);
    }
    return fclose(f) != 0;
}
