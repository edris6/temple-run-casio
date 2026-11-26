/* Desktop-only diagnostic: render a frame with the user's converted meshes.
 * The optional step count first runs the game that far without any input. */
#include "assets.h"
#include "session.h"
#include <stdio.h>
#include <stdlib.h>

static TRFramebuffer fb;
static const TRTarget screen = {fb.pixels, 0, TR_HEIGHT};
static TRSession session;

int main(int argc, char **argv)
{
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "usage: %s frame.ppm [steps]\n", argv[0]);
        return 2;
    }
    const TRAssets *assets = tr_assets_load();
    tr_session_reset(&session, 1);
    for (int step = argc == 3 ? atoi(argv[2]) : 0; step > 0; step--)
        tr_session_advance(&session, (TRInput){0}, 1, 1);
    TRPose pose;
    tr_session_pose(&session, &pose);
    tr_scene_build(&session.course, assets, &pose, session.runner.sliding);
    tr_scene_draw(&screen);
    FILE *f = fopen(argv[1], "wb");
    if (!f) { perror(argv[1]); return 1; }
    fprintf(f, "P6\n%d %d\n255\n", TR_WIDTH, TR_HEIGHT);
    for (int i = 0; i < TR_WIDTH * TR_HEIGHT; i++) {
        unsigned c = tr_pixel_color(fb.pixels[i]);
        unsigned char rgb[] = {(c >> 11) * 255 / 31, ((c >> 5) & 63) * 255 / 63,
                               (c & 31) * 255 / 31};
        fwrite(rgb, 1, 3, f);
    }
    return fclose(f) != 0;
}
