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
#include "path_h.h"
#include "corner_c.h"
#include "corner_d.h"
#include "jump_h.h"
#include "slide_h.h"
#include "trip_h.h"
#include "hole.h"
#include "hole_h.h"
#include "narrow.h"
#include "narrow_h.h"

static TRFramebuffer frame;
static volatile int ticks;

/* Keep elapsed simulation time when a frame takes longer than 1/30 second. */
static int tick(void) { if (ticks < 30) ticks++; return TIMER_CONTINUE; }

int main(void)
{
    TRMesh path = {path_vertices,path_indices,PATH_VERTEX_COUNT,PATH_INDEX_COUNT,path_uvs,path_texture,path_fixed_vertices,path_fixed_uvs};
    TRMesh player = {player_vertices,player_indices,PLAYER_VERTEX_COUNT,PLAYER_INDEX_COUNT,player_uvs,player_texture,player_fixed_vertices,player_fixed_uvs};
    TRMesh jump_mesh = {jump_vertices,jump_indices,JUMP_VERTEX_COUNT,JUMP_INDEX_COUNT,jump_uvs,tree_texture,jump_fixed_vertices,jump_fixed_uvs};
    TRMesh slide_mesh = {slide_vertices,slide_indices,SLIDE_VERTEX_COUNT,SLIDE_INDEX_COUNT,slide_uvs,tree_texture,slide_fixed_vertices,slide_fixed_uvs};
    TRMesh trip_mesh = {trip_vertices,trip_indices,TRIP_VERTEX_COUNT,TRIP_INDEX_COUNT,trip_uvs,tree_texture,trip_fixed_vertices,trip_fixed_uvs};
    TRMesh coin_mesh = {coin_vertices,coin_indices,COIN_VERTEX_COUNT,COIN_INDEX_COUNT,coin_uvs,path_texture,coin_fixed_vertices,coin_fixed_uvs};
    TRMesh corner_left = {left_corner_vertices,left_corner_indices,LEFT_CORNER_VERTEX_COUNT,LEFT_CORNER_INDEX_COUNT,left_corner_uvs,path_texture,left_corner_fixed_vertices,left_corner_fixed_uvs};
    TRMesh corner_right = {right_corner_vertices,right_corner_indices,RIGHT_CORNER_VERTEX_COUNT,RIGHT_CORNER_INDEX_COUNT,right_corner_uvs,path_texture,right_corner_fixed_vertices,right_corner_fixed_uvs};
    TRMesh path_horizontal = {path_h_vertices,path_h_indices,PATH_H_VERTEX_COUNT,PATH_H_INDEX_COUNT,path_h_uvs,path_texture,path_h_fixed_vertices,path_h_fixed_uvs};
    TRMesh corner_c_mesh = {corner_c_vertices,corner_c_indices,CORNER_C_VERTEX_COUNT,CORNER_C_INDEX_COUNT,corner_c_uvs,path_texture,corner_c_fixed_vertices,corner_c_fixed_uvs};
    TRMesh corner_d_mesh = {corner_d_vertices,corner_d_indices,CORNER_D_VERTEX_COUNT,CORNER_D_INDEX_COUNT,corner_d_uvs,path_texture,corner_d_fixed_vertices,corner_d_fixed_uvs};
    TRMesh jump_horizontal = {jump_h_vertices,jump_h_indices,JUMP_H_VERTEX_COUNT,JUMP_H_INDEX_COUNT,jump_h_uvs,tree_texture,jump_h_fixed_vertices,jump_h_fixed_uvs};
    TRMesh slide_horizontal = {slide_h_vertices,slide_h_indices,SLIDE_H_VERTEX_COUNT,SLIDE_H_INDEX_COUNT,slide_h_uvs,tree_texture,slide_h_fixed_vertices,slide_h_fixed_uvs};
    TRMesh trip_horizontal = {trip_h_vertices,trip_h_indices,TRIP_H_VERTEX_COUNT,TRIP_H_INDEX_COUNT,trip_h_uvs,tree_texture,trip_h_fixed_vertices,trip_h_fixed_uvs};
    TRMesh hole_mesh = {hole_vertices,hole_indices,HOLE_VERTEX_COUNT,HOLE_INDEX_COUNT,hole_uvs,path_texture,hole_fixed_vertices,hole_fixed_uvs};
    TRMesh hole_horizontal = {hole_h_vertices,hole_h_indices,HOLE_H_VERTEX_COUNT,HOLE_H_INDEX_COUNT,hole_h_uvs,path_texture,hole_h_fixed_vertices,hole_h_fixed_uvs};
    TRMesh narrow_mesh = {narrow_vertices,narrow_indices,NARROW_VERTEX_COUNT,NARROW_INDEX_COUNT,narrow_uvs,path_texture,narrow_fixed_vertices,narrow_fixed_uvs};
    TRMesh narrow_horizontal = {narrow_h_vertices,narrow_h_indices,NARROW_H_VERTEX_COUNT,NARROW_H_INDEX_COUNT,narrow_h_uvs,path_texture,narrow_h_fixed_vertices,narrow_h_fixed_uvs};
    TRAssets assets = {{&path,&path_horizontal}, {&corner_left,&corner_right,&corner_c_mesh,&corner_d_mesh},
        {&jump_mesh,&jump_horizontal}, {&slide_mesh,&slide_horizontal},
        {&trip_mesh,&trip_horizontal}, &coin_mesh, &player,
        {&hole_mesh,&hole_horizontal}, {&narrow_mesh,&narrow_horizontal}};
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
            if (ev.key == KEY_LEFT) input.turn = -1;
            if (ev.key == KEY_RIGHT) input.turn = 1;
            if (ev.key == KEY_EXE) input.restart = true;
            if (ev.key == KEY_F1) input.toggle_pause = !input.toggle_pause;
        }
        if (!running) break;
        int elapsed = ticks;
        ticks = 0;
        input.tilt = keydown(KEY_6) - keydown(KEY_4);
        if (!input.tilt) input.tilt = keydown(KEY_RIGHT) - keydown(KEY_LEFT);
        tr_session_advance(&session, input, elapsed, (int)rtc_ticks());
        tr_render_world(&frame, &session.course, &assets, session.runner.height);
        dclear(C_BLACK);
        /* gint exposes a 396x224 RGB565 framebuffer, with no extra row stride. */
        for (int y = 0; y < TR_HEIGHT; y++) for (int x = 0; x < TR_WIDTH; x++) {
            uint16_t c = frame.pixels[y * TR_WIDTH + x];
            int pixel = (y * 3 + 4) * DWIDTH + x * 3 + 6;
            for (int dy = 0; dy < 3; dy++) {
                gint_vram[pixel + dy*DWIDTH] = c;
                gint_vram[pixel + dy*DWIDTH + 1] = c;
                gint_vram[pixel + dy*DWIDTH + 2] = c;
            }
        }
        dprint(8, 6, C_WHITE, "SCORE %ld", (long)session.runner.score);
        dprint(8, 22, C_WHITE, "COINS %d", session.runner.coins);
        if (session.paused) dtext(150, 42, C_WHITE, "PAUSED - F1: RESUME");
        if (session.runner.dead) dtext(100, 42, C_WHITE, "GAME OVER - EXE: RESTART");
        dupdate();
    }
    timer_stop(timer);
    return 0;
}
