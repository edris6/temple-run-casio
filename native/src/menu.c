#include "menu.h"

/* A finished run cannot be resumed, so it starts one entry further down. */
int tr_menu_count(bool over) { return over ? 2 : 3; }

TRMenuItem tr_menu_item(bool over, int index)
{
    return (TRMenuItem)(index + (over ? TR_ITEM_RESTART : TR_ITEM_RESUME));
}

const char *tr_menu_label(TRMenuItem item)
{
    return item == TR_ITEM_RESUME ? "RESUME" : item == TR_ITEM_RESTART ? "RESTART" : "QUIT";
}

bool tr_menu_press(int *selected, bool paused, bool over, bool moves, TRKey key,
    TRInput *input)
{
    if (!paused && !over) {
        if (key == TR_KEY_UP) input->jump = true;
        if (key == TR_KEY_DOWN) input->slide = true;
        if (key == TR_KEY_LEFT) input->turn = -1;
        if (key == TR_KEY_RIGHT) input->turn = 1;
        if (key == TR_KEY_PAUSE) input->toggle_pause = !input->toggle_pause;
        return false;
    }
    if (key == TR_KEY_UP && moves && *selected > 0) --*selected;
    if (key == TR_KEY_DOWN && moves && *selected < tr_menu_count(over) - 1) ++*selected;
    if (key == TR_KEY_PAUSE && !over) input->toggle_pause = true;
    if (key == TR_KEY_SELECT) switch (tr_menu_item(over, *selected)) {
        case TR_ITEM_RESUME: input->toggle_pause = true; break;
        case TR_ITEM_RESTART: input->restart = true; break;
        case TR_ITEM_QUIT: return true;
    }
    return false;
}

void tr_scores_start(TRScores *scores, int32_t stored)
{
    scores->best = scores->previous = stored;
    scores->unsaved = false;
}

void tr_scores_reach(TRScores *scores, int32_t score)
{
    if (score <= scores->best) return;
    scores->best = score;
    scores->unsaved = true;
}

void tr_scores_new_run(TRScores *scores) { scores->previous = scores->best; }
