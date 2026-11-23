/* CG50 diagnostic for the recovered runner and real exported meshes.
 * This is not yet the full game; see native/README.md. */
#include <gint/display.h>
#include <gint/keyboard.h>
#include <gint/timer.h>
#include <stdint.h>
#include "renderer.h"
#include "recovered.h"
#include "player.h"
#include "path.h"
#include "player_tex.h"
#include "path_tex.h"

static TRFramebuffer frame;
static volatile int ticks;

/* Keep elapsed simulation time when a frame takes longer than 1/30 second. */
static int tick(void) { if (ticks < 30) ticks++; return TIMER_CONTINUE; }

int main(void)
{
    TRMesh path = {path_vertices,path_indices,PATH_VERTEX_COUNT,PATH_INDEX_COUNT,path_uvs,path_texture};
    TRMesh player = {player_vertices,player_indices,PLAYER_VERTEX_COUNT,PLAYER_INDEX_COUNT,player_uvs,player_texture};
    TRRunner runner;
    float lateral = 0;
    tr_reset(&runner);
    int timer = timer_configure(TIMER_ANY, 33333, GINT_CALL(tick));
    if (timer < 0) return 1;
    timer_start(timer);
    int running = 1;
    while (running) {
        if (!ticks) continue;
        int jump = 0, slide = 0, restart = 0;
        key_event_t ev;
        while ((ev = pollevent()).type != KEYEV_NONE) {
            if (ev.type != KEYEV_DOWN) continue;
            if (ev.key == KEY_EXIT) running = 0;
            if (ev.key == KEY_UP) jump = 1;
            if (ev.key == KEY_DOWN) slide = 1;
            if (ev.key == KEY_EXE) restart = 1;
        }
        if (!running) break;
        int elapsed = ticks;
        ticks = 0;
        for (int i = 0; i < elapsed; i++) {
            if (keydown(KEY_4)) lateral -= 0.6f;
            if (keydown(KEY_6)) lateral += 0.6f;
            if (lateral < -7) lateral = -7;
            if (lateral > 7) lateral = 7;
            tr_step(&runner, 1.0f/30.0f, true, 0);
        }
        if (restart && runner.dead) tr_reset(&runner);
        if (jump) tr_jump(&runner);
        if (slide) tr_slide(&runner);
        tr_render_frame(&frame, &path, &player, runner.forward, runner.height, lateral);
        dclear(C_BLACK);
        /* gint exposes a 396x224 RGB565 framebuffer, with no extra row stride. */
        for (int y = 0; y < TR_HEIGHT; y++) for (int x = 0; x < TR_WIDTH; x++) {
            uint16_t c = frame.pixels[y * TR_WIDTH + x];
            int pixel = (y * 2 + 4) * DWIDTH + x * 2 + 6;
            gint_vram[pixel] = gint_vram[pixel + 1] = c;
            gint_vram[pixel + DWIDTH] = gint_vram[pixel + DWIDTH + 1] = c;
        }
        dprint(8, 6, C_WHITE, "SCORE %ld", (long)runner.score);
        if (runner.dead) dtext(100, 28, C_WHITE, "EXE: RESTART");
        dupdate();
    }
    timer_stop(timer);
    return 0;
}
