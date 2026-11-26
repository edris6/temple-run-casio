/* CG50 diagnostic for the recovered runner and real exported meshes.
 * This is not yet the full game; see native/README.md. */
#include <gint/clock.h>
#include <gint/display.h>
#include <gint/dma.h>
#include <gint/drivers/r61524.h>
#include <gint/keyboard.h>
#include <gint/rtc.h>
#include <gint/timer.h>
#include <stdint.h>
#include "assets.h"
#include "present.h"
#include "session.h"

/* The clock runs faster than the simulation, to place frames between steps. */
#define CLOCK_HZ 480
#define STEP_UNITS (CLOCK_HZ / 30)
#define FRAME_UNITS (CLOCK_HZ / 60) /* draw at most 60 frames a second */
#define MAX_STEPS 30

_Static_assert(DWIDTH == TR_SCREEN_WIDTH && DHEIGHT == TR_SCREEN_HEIGHT,
    "The presenter expects the full CG50 display");
static TRFramebuffer frame;
static volatile uint32_t clock_units;

static int count(void) { clock_units++; return TIMER_CONTINUE; }

int main(void)
{
    /* Read the seed before loading, so that it does not depend on load time. */
    int seed = (int)rtc_ticks();
    const TRAssets *assets = tr_assets_load();
    static TRSession session;
    tr_session_reset(&session, seed);
    int timer = timer_configure(TIMER_ANY, 1000000 / CLOCK_HZ, GINT_CALL(count));
    if (timer < 0) return 1;
    /* Clear the border once; later frames only replace the interior. */
    dclear(C_BLACK);
    dupdate();
    timer_start(timer);

    TRPose current, next, view;
    tr_session_pose(&session, &current);
    next = current;
    TRInput input = {0};
    uint32_t simulated = clock_units, drawn = simulated - FRAME_UNITS;
    uint32_t second = simulated;
    int frames = 0, rate = 0;
    bool redraw = true, show_rate = false;
    int running = 1;
    while (running) {
        /* Collect presses until the next simulation step uses them. */
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
            if (ev.key == KEY_F6) { show_rate = !show_rate; redraw = true; }
        }
        if (!running) break;

        uint32_t now = clock_units;
        int steps = (int)((now - simulated) / STEP_UNITS);
        if (steps > 0) {
            simulated += (uint32_t)steps * STEP_UNITS;
            bool pressed = input.jump || input.slide || input.turn || input.restart ||
                input.toggle_pause;
            bool was_still = session.paused || session.runner.dead;
            input.tilt = keydown(KEY_6) - keydown(KEY_4);
            if (!input.tilt) input.tilt = keydown(KEY_RIGHT) - keydown(KEY_LEFT);
            tr_session_advance(&session, input, steps > MAX_STEPS ? MAX_STEPS : steps,
                (int)rtc_ticks());
            tr_session_pose(&session, &current);
            tr_session_predict(&session, input.tilt, &next);
            input = (TRInput){0};
            bool still = session.paused || session.runner.dead;
            if (!still || !was_still || pressed) redraw = true;
        }

        /* A paused or finished run only needs drawing when it changes. */
        bool still = session.paused || session.runner.dead;
        if ((still && !redraw) || now - drawn < FRAME_UNITS) {
            sleep();
            continue;
        }
        drawn = now;
        redraw = false;
        if (still) view = current;
        else {
            uint32_t since = now - simulated;
            tr_pose_between(&current, &next,
                since >= STEP_UNITS ? 1.0f : (float)since / STEP_UNITS, &view);
        }
        /* The previous frame is still being sent to the screen by DMA while
         * this one is drawn. Wait for it only before touching VRAM again. */
        tr_render_world(&frame, &session.course, assets, &view, session.runner.sliding);
        dma_transfer_wait(0);
        tr_present_scale(&frame, gint_vram);
        dprint(8, 6, C_WHITE, "SCORE %ld", (long)session.runner.score);
        dprint(8, 22, C_WHITE, "COINS %d", session.runner.coins);
        if (show_rate) dprint(340, 6, C_WHITE, "FPS %d", rate);
        if (session.paused) dtext(150, 42, C_WHITE, "PAUSED - F1: RESUME");
        if (session.runner.dead) dtext(100, 42, C_WHITE, "GAME OVER - EXE: RESTART");
        r61524_display(gint_vram, 0, DHEIGHT, R61524_DMA);
        frames++;
        if (now - second >= CLOCK_HZ) {
            rate = (int)(frames * CLOCK_HZ / (now - second));
            frames = 0;
            second = now;
        }
    }
    dma_transfer_wait(0);
    timer_stop(timer);
    return 0;
}
