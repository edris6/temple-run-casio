#ifndef TEMPLE_SAVE_H
#define TEMPLE_SAVE_H

/* The high score file. It uses the Unix file functions, which gint provides on
 * the calculator; there they must be called through gint_world_switch(). */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* "TRHS", the score, then its complement, both big-endian. */
#define TR_RECORD_BYTES 12

void tr_record_encode(int32_t score, uint8_t bytes[TR_RECORD_BYTES]);
/* False, leaving *score alone, unless the bytes hold an intact record. */
bool tr_record_decode(const uint8_t *bytes, size_t size, int32_t *score);

bool tr_save_read(const char *path, int32_t *score);
/* Overwrites the record in place, creating the file if needed. */
bool tr_save_write(const char *path, int32_t score);

#endif
