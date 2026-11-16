#pragma once
#include "noise.h"
bool audio_init(void);
bool audio_open(const NoiseTrack *track);
void audio_update(void);
void audio_pause(void);
void audio_resume(void);
void audio_stop(void);
void audio_set_volume(uint8_t volume);
bool audio_is_playing(void);
bool audio_finished(void);
uint32_t audio_position_ms(void);
uint32_t audio_output_rate(void);
/* Decoder-to-mixer single-producer/single-consumer ring interface. */
void audio_push_pcm(const int16_t *interleaved_stereo,uint32_t frames);
uint32_t audio_ring_used(void);
uint32_t audio_ring_free(void);
void audio_ring_reset(void);
