#ifndef TEMPLE_MENU_H
#define TEMPLE_MENU_H

/* The pause and game over menus and the high score. This is the port's own
 * menu, not the original game's; cg_main.c draws it. */
#include "session.h"

typedef enum { TR_ITEM_RESUME, TR_ITEM_RESTART, TR_ITEM_QUIT } TRMenuItem;
typedef enum {
    TR_KEY_UP, TR_KEY_DOWN, TR_KEY_LEFT, TR_KEY_RIGHT, TR_KEY_PAUSE, TR_KEY_SELECT
} TRKey;

/* The entries offered while paused, or once the run is over. */
int tr_menu_count(bool over);
TRMenuItem tr_menu_item(bool over, int index);
const char *tr_menu_label(TRMenuItem item);

/* Applies a key press. While paused or over, the keys work the menu: moving
 * *selected or adding the chosen entry to input. Otherwise they play. moves is
 * false just after a run ends, so that a late jump or slide does not move the
 * selection. Returns true if the player chose to quit. */
bool tr_menu_press(int *selected, bool paused, bool over, bool moves, TRKey key,
    TRInput *input);

typedef struct {
    int32_t best;     /* highest score reached, including the current run */
    int32_t previous; /* highest score before the current run */
    bool unsaved;     /* best has not been stored yet */
} TRScores;

void tr_scores_start(TRScores *scores, int32_t stored);
/* The current run has reached score. */
void tr_scores_reach(TRScores *scores, int32_t score);
void tr_scores_new_run(TRScores *scores);

#endif
