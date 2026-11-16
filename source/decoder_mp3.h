#pragma once
#include <stdbool.h>
#include <stdint.h>
bool decoder_mp3_open(const char *path);
void decoder_mp3_close(void);
void decoder_mp3_prefill(void);
void decoder_mp3_pump(void);
bool decoder_mp3_done(void);
uint32_t decoder_mp3_rate(void);
