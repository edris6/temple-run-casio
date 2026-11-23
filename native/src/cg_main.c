/* CG50 diagnostic for the recovered runner and real exported meshes.
 * This is not yet the full game; see native/README.md. */
#include <gint/display.h>
#include <gint/keyboard.h>
#include <gint/rtc.h>
#include <gint/timer.h>
#include <stdint.h>
#include "renderer.h"
#include "recovered.h"
#include "session.h"
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

static TRFramebuffer frame;
static volatile int ticks;

/* Keep elapsed simulation time when a frame takes longer than 1/30 second. */
static int tick(void) { if (ticks < 30) ticks++; return TIMER_CONTINUE; }

int main(void)
{
    TRMesh path = {path_vertices,path_indices,PATH_VERTEX_COUNT,PATH_INDEX_COUNT,path_uvs,path_texture};
    TRMesh player = {player_vertices,player_indices,PLAYER_VERTEX_COUNT,PLAYER_INDEX_COUNT,player_uvs,player_texture};
    TRMesh jump_mesh = {jump_vertices,jump_indices,JUMP_VERTEX_COUNT,JUMP_INDEX_COUNT,jump_uvs,tree_texture};
    TRMesh slide_mesh = {slide_vertices,slide_indices,SLIDE_VERTEX_COUNT,SLIDE_INDEX_COUNT,slide_uvs,tree_texture};
    TRMesh trip_mesh = {trip_vertices,trip_indices,TRIP_VERTEX_COUNT,TRIP_INDEX_COUNT,trip_uvs,tree_texture};
    TRMesh coin_mesh = {coin_vertices,coin_indices,COIN_VERTEX_COUNT,COIN_INDEX_COUNT,coin_uvs,path_texture};
    TRMesh corner_left = {left_corner_vertices,left_corner_indices,LEFT_CORNER_VERTEX_COUNT,LEFT_CORNER_INDEX_COUNT,left_corner_uvs,path_texture};
    TRMesh corner_right = {right_corner_vertices,right_corner_indices,RIGHT_CORNER_VERTEX_COUNT,RIGHT_CORNER_INDEX_COUNT,right_corner_uvs,path_texture};
    TRSession session;
    tr_session_reset(&session, (int)rtc_ticks());
    int timer = timer_configure(TIMER_ANY, 33333, GINT_CALL(tick));
    if (timer < 0) return 1;
    timer_start(timer);
    int running = 1;
    while (running) {
        if (!ticks) continue;
        TRInput input = {0};
        key_event_t ev;
        while ((ev = pollevent()).type != KEYEV_NONE) {
            if (ev.type != KEYEV_DOWN) continue;
            if (ev.key == KEY_EXIT) running = 0;
            if (ev.key == KEY_UP) input.jump = true;
            if (ev.key == KEY_DOWN) input.slide = true;
            if (ev.key == KEY_EXE) input.restart = true;
            if (ev.key == KEY_F1) input.toggle_pause = !input.toggle_pause;
        }
        if (!running) break;
        int elapsed = ticks;
        ticks = 0;
        input.tilt = keydown(KEY_6) - keydown(KEY_4);
        tr_session_advance(&session, input, elapsed, (int)rtc_ticks());
        tr_render_opening(&frame, &session.course, &path, &player, &corner_left, &corner_right,
                          session.runner.forward, session.runner.height, session.lateral);
        tr_render_course(&frame, &session.course, &jump_mesh, &slide_mesh, &trip_mesh, &coin_mesh,
                         session.runner.forward, session.lateral);
        dclear(C_BLACK);
        /* gint exposes a 396x224 RGB565 framebuffer, with no extra row stride. */
        for (int y = 0; y < TR_HEIGHT; y++) for (int x = 0; x < TR_WIDTH; x++) {
            uint16_t c = frame.pixels[y * TR_WIDTH + x];
            int pixel = (y * 2 + 4) * DWIDTH + x * 2 + 6;
            gint_vram[pixel] = gint_vram[pixel + 1] = c;
            gint_vram[pixel + DWIDTH] = gint_vram[pixel + DWIDTH + 1] = c;
        }
        dprint(8, 6, C_WHITE, "SCORE %ld", (long)session.runner.score);
        dprint(8, 22, C_WHITE, "COINS %d", session.runner.coins);
        if (session.paused) dtext(150, 42, C_WHITE, "PAUSED - F1: RESUME");
        if (session.runner.dead) dtext(100, 42, C_WHITE, "GAME OVER - EXE: RESTART");
        if (session.course.corner_reached) dtext(55, 42, C_WHITE, "CORNER NOT PORTED - EXE: RESTART");
        dupdate();
    }
    timer_stop(timer);
    return 0;
}
