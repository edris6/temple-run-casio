#include "menu.h"
#include "save.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int selected;

static bool press(const TRSession *session, bool moves, TRKey key, TRInput *input)
{
    *input = (TRInput){0};
    return tr_menu_press(&selected, session->paused, session->runner.dead, moves, key, input);
}

static bool empty(TRInput input)
{
    return !input.jump && !input.slide && !input.restart && !input.toggle_pause &&
        !input.turn && !input.tilt;
}

static void check_entries(void)
{
    assert(tr_menu_count(false) == 3 && tr_menu_count(true) == 2);
    assert(tr_menu_item(false, 0) == TR_ITEM_RESUME);
    assert(tr_menu_item(false, 1) == TR_ITEM_RESTART);
    assert(tr_menu_item(false, 2) == TR_ITEM_QUIT);
    assert(tr_menu_item(true, 0) == TR_ITEM_RESTART);
    assert(tr_menu_item(true, 1) == TR_ITEM_QUIT);
    assert(!strcmp(tr_menu_label(TR_ITEM_RESUME), "RESUME"));
    assert(!strcmp(tr_menu_label(TR_ITEM_RESTART), "RESTART"));
    assert(!strcmp(tr_menu_label(TR_ITEM_QUIT), "QUIT"));
}

static void check_playing_keys(void)
{
    TRSession s;
    tr_session_reset(&s, 1);
    TRInput input;
    selected = 0;
    assert(!press(&s, true, TR_KEY_UP, &input) && input.jump && !input.slide);
    assert(!press(&s, true, TR_KEY_DOWN, &input) && input.slide && !input.jump);
    assert(!press(&s, true, TR_KEY_LEFT, &input) && input.turn == -1);
    assert(!press(&s, true, TR_KEY_RIGHT, &input) && input.turn == 1);
    assert(!press(&s, true, TR_KEY_PAUSE, &input) && input.toggle_pause);
    /* EXE does nothing during a run, and nothing moves the hidden selection. */
    assert(!press(&s, true, TR_KEY_SELECT, &input) && empty(input) && selected == 0);
}

/* Keys pressed on the pause menu, carried through to the session. */
static void check_pause_menu(void)
{
    TRSession s;
    tr_session_reset(&s, 1);
    tr_session_advance(&s, (TRInput){.toggle_pause = true}, 1, 1);
    assert(s.paused);
    TRInput input;
    selected = 0;
    assert(!press(&s, true, TR_KEY_UP, &input) && empty(input) && selected == 0);
    assert(!press(&s, true, TR_KEY_DOWN, &input) && empty(input) && selected == 1);
    press(&s, true, TR_KEY_DOWN, &input);
    press(&s, true, TR_KEY_DOWN, &input);
    assert(selected == 2); /* stops at the last entry */
    assert(press(&s, true, TR_KEY_SELECT, &input)); /* quit */
    assert(!press(&s, true, TR_KEY_LEFT, &input) && empty(input));
    assert(!press(&s, true, TR_KEY_RIGHT, &input) && empty(input));

    selected = 0; /* resume */
    assert(!press(&s, true, TR_KEY_SELECT, &input) && input.toggle_pause);
    TRSession resumed = s;
    tr_session_advance(&resumed, input, 1, 1);
    assert(!resumed.paused);
    assert(!press(&s, true, TR_KEY_PAUSE, &input) && input.toggle_pause);

    selected = 1; /* restart */
    s.runner.score = 500;
    assert(!press(&s, true, TR_KEY_SELECT, &input) && input.restart);
    tr_session_advance(&s, input, 5, 2);
    assert(!s.paused && !s.runner.dead && s.runner.score == 0 && s.runner.forward == -35);
}

static void check_game_over_menu(void)
{
    TRSession s;
    tr_session_reset(&s, 1);
    s.runner.dead = true;
    TRInput input;
    selected = 0;
    /* Just after dying, a late slide must not move to Quit; EXE restarts. */
    assert(!press(&s, false, TR_KEY_DOWN, &input) && empty(input) && selected == 0);
    assert(!press(&s, false, TR_KEY_SELECT, &input) && input.restart);
    assert(!press(&s, true, TR_KEY_PAUSE, &input) && empty(input));
    press(&s, true, TR_KEY_DOWN, &input);
    press(&s, true, TR_KEY_DOWN, &input);
    assert(selected == 1);
    assert(press(&s, true, TR_KEY_SELECT, &input));
    press(&s, true, TR_KEY_UP, &input);
    assert(selected == 0);
    assert(!press(&s, true, TR_KEY_SELECT, &input) && input.restart);
    tr_session_advance(&s, input, 5, 2);
    assert(!s.runner.dead);
}

static void check_scores(void)
{
    TRScores scores;
    tr_scores_start(&scores, 300);
    tr_scores_reach(&scores, 200);
    assert(scores.best == 300 && !scores.unsaved);
    tr_scores_reach(&scores, 300);
    assert(!scores.unsaved);
    tr_scores_reach(&scores, 450);
    tr_scores_reach(&scores, 500);
    assert(scores.best == 500 && scores.previous == 300 && scores.unsaved);
    /* Saving part way through a run still leaves it a new high score. */
    scores.unsaved = false;
    tr_scores_reach(&scores, 600);
    assert(scores.unsaved && scores.best > scores.previous);
    tr_scores_new_run(&scores);
    assert(scores.previous == 600 && scores.best == 600);
}

static void write_file(const char *path, const void *bytes, size_t size)
{
    FILE *f = fopen(path, "wb");
    assert(f && fwrite(bytes, 1, size, f) == size);
    assert(!fclose(f));
}

static void check_save(void)
{
    uint8_t bytes[TR_RECORD_BYTES];
    int32_t score = -7;
    tr_record_encode(0x01020304, bytes);
    static const uint8_t expected[] = {'T','R','H','S', 1,2,3,4, 254,253,252,251};
    assert(!memcmp(bytes, expected, sizeof expected));
    assert(tr_record_decode(bytes, sizeof bytes, &score) && score == 0x01020304);
    /* Any damaged byte, a short record and a negative score are refused. */
    score = -7;
    for (int i = 0; i < TR_RECORD_BYTES; i++) {
        bytes[i] ^= 0x10;
        assert(!tr_record_decode(bytes, sizeof bytes, &score));
        bytes[i] ^= 0x10;
    }
    assert(!tr_record_decode(bytes, sizeof bytes - 1, &score));
    tr_record_encode(-1, bytes);
    assert(!tr_record_decode(bytes, sizeof bytes, &score) && score == -7);

    const char *path = "build/test_menu.sav";
    remove(path);
    assert(!tr_save_read(path, &score) && score == -7);
    assert(tr_save_write(path, 123456) && tr_save_read(path, &score) && score == 123456);
    assert(tr_save_write(path, 2147483647) && tr_save_read(path, &score) &&
        score == 2147483647);
    assert(tr_save_write(path, 0) && tr_save_read(path, &score) && score == 0);
    score = -7;
    write_file(path, "TRHS", 4);
    assert(!tr_save_read(path, &score) && score == -7);
    write_file(path, "", 0);
    assert(!tr_save_read(path, &score) && score == -7);
    /* A damaged file is repaired by the next save. */
    assert(tr_save_write(path, 42) && tr_save_read(path, &score) && score == 42);
    assert(!tr_save_write("build/no-such-folder/test.sav", 1));
    remove(path);
}

int main(void)
{
    check_entries();
    check_playing_keys();
    check_pause_menu();
    check_game_over_menu();
    check_scores();
    check_save();
    puts("Menu and high score checks passed");
    return 0;
}
