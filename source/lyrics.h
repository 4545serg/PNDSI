#pragma once
#include "noise.h"
typedef struct { uint32_t ms; char text[NOISE_TEXT]; } LyricLine;
void lyrics_init(void);
void lyrics_load_for_track(const NoiseTrack *track);
void lyrics_request_for_track(const NoiseTrack *track);
void lyrics_update(void);
int lyrics_line_at(uint32_t ms);
const LyricLine *lyrics_lines(void);
int lyrics_count(void);
bool lyrics_synced(void);
bool lyrics_busy(void);
const char *lyrics_message(void);
const char *lyrics_wifi_status(void);
void lyrics_clear_cache(void);
bool lyrics_matches_ready(void);
int lyrics_match_count(void);
const char *lyrics_match_title(int index);
const char *lyrics_match_artist(int index);
void lyrics_select_match(int index);
void lyrics_dismiss_matches(void);
