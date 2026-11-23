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
#include <stdio.h>

static TRFramebuffer fb;
int main(int argc, char **argv)
{
    if (argc != 2) { fprintf(stderr, "usage: %s frame.ppm\n", argv[0]); return 2; }
    TRMesh path = {path_vertices,path_indices,PATH_VERTEX_COUNT,PATH_INDEX_COUNT,path_uvs,path_texture};
    TRMesh player = {player_vertices,player_indices,PLAYER_VERTEX_COUNT,PLAYER_INDEX_COUNT,player_uvs,player_texture};
    TRMesh jump = {jump_vertices,jump_indices,JUMP_VERTEX_COUNT,JUMP_INDEX_COUNT,jump_uvs,tree_texture};
    TRMesh slide = {slide_vertices,slide_indices,SLIDE_VERTEX_COUNT,SLIDE_INDEX_COUNT,slide_uvs,tree_texture};
    TRMesh trip = {trip_vertices,trip_indices,TRIP_VERTEX_COUNT,TRIP_INDEX_COUNT,trip_uvs,tree_texture};
    TRMesh coin = {coin_vertices,coin_indices,COIN_VERTEX_COUNT,COIN_INDEX_COUNT,coin_uvs,path_texture};
    TRCourse course;
    tr_course_init(&course, 1);
    tr_render_frame(&fb, &path, &player, 0, 0, 0);
    tr_render_course(&fb, &course, &jump, &slide, &trip, &coin, 0, 0);
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
