#include "save.h"
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

static void put_word(uint8_t *bytes, uint32_t word)
{
    for (int i = 0; i < 4; i++) bytes[i] = (uint8_t)(word >> (24 - 8 * i));
}

static uint32_t get_word(const uint8_t *bytes)
{
    uint32_t word = 0;
    for (int i = 0; i < 4; i++) word = word << 8 | bytes[i];
    return word;
}

void tr_record_encode(int32_t score, uint8_t bytes[TR_RECORD_BYTES])
{
    memcpy(bytes, "TRHS", 4);
    put_word(bytes + 4, (uint32_t)score);
    put_word(bytes + 8, ~(uint32_t)score);
}

bool tr_record_decode(const uint8_t *bytes, size_t size, int32_t *score)
{
    if (size < TR_RECORD_BYTES || memcmp(bytes, "TRHS", 4)) return false;
    uint32_t value = get_word(bytes + 4);
    if (get_word(bytes + 8) != ~value || value > INT32_MAX) return false;
    *score = (int32_t)value;
    return true;
}

bool tr_save_read(const char *path, int32_t *score)
{
    int file = open(path, O_RDONLY);
    if (file < 0) return false;
    uint8_t bytes[TR_RECORD_BYTES];
    long size = (long)read(file, bytes, sizeof bytes);
    close(file);
    return size == TR_RECORD_BYTES && tr_record_decode(bytes, sizeof bytes, score);
}

bool tr_save_write(const char *path, int32_t score)
{
    int file = open(path, O_WRONLY | O_CREAT, 0644);
    if (file < 0) return false;
    uint8_t bytes[TR_RECORD_BYTES];
    tr_record_encode(score, bytes);
    bool written = write(file, bytes, sizeof bytes) == TR_RECORD_BYTES;
    return close(file) == 0 && written;
}
