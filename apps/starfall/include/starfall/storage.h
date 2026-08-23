#ifndef STARFALL_STORAGE_H
#define STARFALL_STORAGE_H

#include <stdint.h>

uint32_t starfall_high_score_load(void);
int starfall_high_score_save(uint32_t score);

#endif
