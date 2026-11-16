#pragma once
#include <nds.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define NOISE_MAX_TRACKS 512
#define NOISE_MAX_QUEUE 64
#define NOISE_MAX_LYRICS 384
#define NOISE_PATH 256
#define NOISE_TEXT 96

typedef struct {
    char path[NOISE_PATH];
    char title[NOISE_TEXT];
    char artist[NOISE_TEXT];
    char album[NOISE_TEXT];
    uint32_t duration;
    uint32_t bytes;
    bool liked;
} NoiseTrack;

typedef enum { PAGE_LIBRARY=0, PAGE_LIKED, PAGE_QUEUE, PAGE_SEARCH, PAGE_LYRICS, PAGE_SETTINGS } NoisePage;
typedef enum { TAB_ALL=0, TAB_LIKED, TAB_QUEUE } NoiseTab;

typedef struct {
    NoiseTrack tracks[NOISE_MAX_TRACKS];
    uint16_t track_count;
    uint16_t visible[NOISE_MAX_TRACKS];
    uint16_t visible_count;
    uint16_t selected;
    int16_t playing;
    uint16_t queue[NOISE_MAX_QUEUE];
    uint8_t queue_count;
    NoisePage page;
    NoiseTab tab;
    char search[48];
    uint8_t search_len;
    uint8_t search_cursor;
    bool search_editing;
    bool playing_now;
    bool paused;
    bool shuffle;
    uint8_t repeat_mode;
    uint8_t volume;
    bool lyrics_animation;
    bool offline_only;
    uint8_t lyrics_font;
    uint8_t sort_mode;
    uint8_t theme_mode;
    char status[48];
    uint8_t status_frames;
    uint32_t frame;
} NoiseApp;

extern NoiseApp g_app;
void noise_status(const char *msg);
