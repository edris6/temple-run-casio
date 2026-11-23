/* Desktop-only diagnostic: render one frame with the user's converted meshes. */
#include "renderer.h"
#include "player.h"
#include "path.h"
#include "player_tex.h"
#include "path_tex.h"
#include <stdio.h>

static TRFramebuffer fb;
int main(int argc, char **argv)
{
    if (argc != 2) { fprintf(stderr, "usage: %s frame.ppm\n", argv[0]); return 2; }
    TRMesh path = {path_vertices,path_indices,PATH_VERTEX_COUNT,PATH_INDEX_COUNT,path_uvs,path_texture};
    TRMesh player = {player_vertices,player_indices,PLAYER_VERTEX_COUNT,PLAYER_INDEX_COUNT,player_uvs,player_texture};
    tr_render_frame(&fb, &path, &player, 0, 0, 0);
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
