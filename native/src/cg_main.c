/* CG50 diagnostic for the recovered runner and real exported meshes.
 * This is not yet the full game; see native/README.md. */
#include <gint/clock.h>
#include <gint/display.h>
#include <gint/dma.h>
#include <gint/drivers/r61524.h>
#include <gint/gint.h>
#include <gint/keyboard.h>
#include <gint/rtc.h>
#include <gint/timer.h>
#include <stdint.h>
#include "assets.h"
#include "menu.h"
#include "present.h"
#include "save.h"
#include "session.h"

/* The clock runs faster than the simulation, to place frames between steps. */
#define CLOCK_HZ 480
#define STEP_UNITS (CLOCK_HZ / 30)
#define FRAME_UNITS (CLOCK_HZ / 60) /* draw at most 60 frames a second */
#define MAX_STEPS 30
/* The game over menu holds its selection still for this long. */
#define SETTLE_UNITS (CLOCK_HZ / 2)
#define SAVE_PATH "/TRunner.sav" /* in the calculator's storage memory */

_Static_assert(DWIDTH == TR_WIDTH && DHEIGHT == TR_HEIGHT &&
    TR_HEIGHT % TR_BAND_HEIGHT == 0, "frames fill the display in whole strips");
static TRBand band;
static volatile uint32_t clock_units;

static int count(void) { clock_units++; return TIMER_CONTINUE; }

/* Files can only be reached with the OS in charge of the machine, which must
 * not happen in the middle of a screen transfer. */
static void load_scores(TRScores *scores)
{
    int32_t stored = 0;
    gint_world_switch(GINT_CALL(tr_save_read, SAVE_PATH, &stored));
    tr_scores_start(scores, stored);
}

/* Stores a new high score, if there is one. Returns false if that failed. */
static bool save_scores(TRScores *scores)
{
    if (!scores->unsaved) return true;
    dma_transfer_wait(0);
    scores->unsaved = !gint_world_switch(GINT_CALL(tr_save_write, SAVE_PATH, scores->best));
    return !scores->unsaved;
}

static void draw_menu(const TRSession *session, const TRScores *scores, int selected,
    bool save_failed)
{
    bool over = session->runner.dead;
    int centre = DWIDTH / 2, first_item = 118;
    int bottom = first_item + 18 * tr_menu_count(over) + 2;
    drect_border(98, 46, DWIDTH - 99, bottom, C_RGB(3, 3, 3), 1, C_WHITE);
    dtext_opt(centre, 54, C_WHITE, C_NONE, DTEXT_CENTER, DTEXT_TOP,
        over ? "GAME OVER" : "PAUSED");
    dprint_opt(centre, 72, C_WHITE, C_NONE, DTEXT_CENTER, DTEXT_TOP,
        "SCORE %ld", (long)session->runner.score);
    dprint_opt(centre, 86, C_WHITE, C_NONE, DTEXT_CENTER, DTEXT_TOP,
        "HIGH SCORE %ld", (long)scores->best);
    if (save_failed)
        dtext_opt(centre, 100, C_RGB(31, 12, 8), C_NONE, DTEXT_CENTER, DTEXT_TOP,
            "COULD NOT SAVE IT");
    else if (over && scores->best > scores->previous)
        dtext_opt(centre, 100, C_RGB(31, 28, 0), C_NONE, DTEXT_CENTER, DTEXT_TOP,
            "NEW HIGH SCORE!");
    for (int i = 0; i < tr_menu_count(over); i++) {
        int y = first_item + 18 * i;
        if (i == selected) drect(118, y - 4, DWIDTH - 119, y + 12, C_WHITE);
        dtext_opt(centre, y, i == selected ? C_BLACK : C_WHITE, C_NONE,
            DTEXT_CENTER, DTEXT_TOP, tr_menu_label(tr_menu_item(over, i)));
    }
}

int main(void)
{
    /* Quitting leads to the calculator's menu. Without this, the OS would not
     * start the add-in again until another one had been opened. */
    gint_setrestart(1);
    /* Read the seed before loading, so that it does not depend on load time. */
    int seed = (int)rtc_ticks();
    const TRAssets *assets = tr_assets_load();
    static TRSession session;
    tr_session_reset(&session, seed);
    TRScores scores;
    load_scores(&scores);
    int timer = timer_configure(TIMER_ANY, 1000000 / CLOCK_HZ, GINT_CALL(count));
    if (timer < 0) return 1;
    timer_start(timer);

    TRPose current, next, view;
    tr_session_pose(&session, &current);
    next = current;
    TRInput input = {0};
    uint32_t simulated = clock_units, drawn = simulated - FRAME_UNITS;
    uint32_t second = simulated, ended = simulated;
    int frames = 0, rate = 0, selected = 0;
    bool redraw = true, show_rate = false, save_failed = false, quit = false;
    bool counting = false; /* measuring the frame rate of a moving run */
    while (!quit) {
        /* Collect presses until the next simulation step uses them. */
        bool to_calculator_menu = false;
        key_event_t ev;
        while ((ev = pollevent()).type != KEYEV_NONE) {
            if (ev.type != KEYEV_DOWN) continue;
            if (ev.key == KEY_EXIT) quit = true;
            if (ev.key == KEY_MENU) to_calculator_menu = true;
            if (ev.key == KEY_F6) { show_rate = !show_rate; redraw = true; }
            int key = ev.key == KEY_UP ? TR_KEY_UP : ev.key == KEY_DOWN ? TR_KEY_DOWN :
                ev.key == KEY_LEFT ? TR_KEY_LEFT : ev.key == KEY_RIGHT ? TR_KEY_RIGHT :
                ev.key == KEY_F1 ? TR_KEY_PAUSE : ev.key == KEY_EXE ? TR_KEY_SELECT : -1;
            if (key < 0) continue;
            bool paused = session.paused, over = session.runner.dead;
            if (paused || over) redraw = true;
            bool moves = !over || clock_units - ended >= SETTLE_UNITS;
            if (tr_menu_press(&selected, paused, over, moves, (TRKey)key, &input))
                quit = true;
        }
        if (quit) break;

        if (to_calculator_menu) {
            save_failed = !save_scores(&scores);
            dma_transfer_wait(0);
            gint_osmenu(); /* returns if the player opens the game again */
            /* Come back to the pause menu, not into a moving run. */
            if (!session.runner.dead && !session.paused) {
                session.paused = true;
                selected = 0;
            }
            input = (TRInput){0};
            tr_session_pose(&session, &current);
            next = current;
            simulated = clock_units;
            counting = false;
            redraw = true;
        }

        uint32_t now = clock_units;
        int steps = (int)((now - simulated) / STEP_UNITS);
        if (steps > 0) {
            simulated += (uint32_t)steps * STEP_UNITS;
            bool pressed = input.jump || input.slide || input.turn || input.restart ||
                input.toggle_pause;
            bool was_paused = session.paused, was_over = session.runner.dead;
            bool restarting = input.restart;
            if (restarting) {
                save_scores(&scores); /* a run abandoned from the pause menu */
                tr_scores_new_run(&scores);
                save_failed = false;
            }
            input.tilt = keydown(KEY_RIGHT) - keydown(KEY_LEFT);
            tr_session_advance(&session, input, steps > MAX_STEPS ? MAX_STEPS : steps,
                (int)rtc_ticks());
            tr_scores_reach(&scores, session.runner.score);
            tr_session_pose(&session, &current);
            tr_session_predict(&session, input.tilt, &next);
            input = (TRInput){0};
            /* Time spent saving is not part of the new run. */
            if (restarting) simulated = clock_units;
            /* A menu that has just appeared starts at its first entry. */
            if (session.runner.dead && !was_over) {
                selected = 0;
                ended = now;
                save_failed = false;
            }
            if (session.paused && !was_paused) selected = 0;
            bool still = session.paused || session.runner.dead;
            if (!still || !(was_paused || was_over) || pressed) redraw = true;
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
         * this one is prepared. Wait for it only before touching VRAM again.
         * The bottom strip is usually the busiest, so starting there gives the
         * transfer the longest time to finish. */
        tr_scene_build(&session.course, assets, &view, session.runner.sliding);
        for (int bottom = TR_HEIGHT; bottom > 0; bottom -= TR_BAND_HEIGHT) {
            TRTarget target = {band.pixels, bottom - TR_BAND_HEIGHT, bottom};
            tr_scene_draw(&target);
            dma_transfer_wait(0);
            tr_present_rows(band.pixels, gint_vram + target.top * DWIDTH, TR_BAND_HEIGHT);
        }
        dprint(8, 6, C_WHITE, "SCORE %ld", (long)session.runner.score);
        dprint(8, 22, C_WHITE, "COINS %d", session.runner.coins);
        if (show_rate) dprint(340, 6, C_WHITE, "FPS %d", rate);
        if (still) draw_menu(&session, &scores, selected, save_failed);
        r61524_display(gint_vram, 0, DHEIGHT, R61524_DMA);
        /* Menus are drawn only on change, so time spent there is left out. */
        if (still) counting = false;
        else if (!counting) {
            counting = true;
            second = now;
            frames = 0;
        } else if (++frames, now - second >= CLOCK_HZ) {
            rate = (int)(frames * CLOCK_HZ / (now - second));
            frames = 0;
            second = now;
        }
        /* Save once the game over screen is up, as writing takes a moment. */
        if (session.runner.dead && scores.unsaved && !save_failed) {
            save_failed = !save_scores(&scores);
            if (save_failed) redraw = true;
        }
    }
    save_scores(&scores);
    dma_transfer_wait(0);
    timer_stop(timer);
    return 0;
}
