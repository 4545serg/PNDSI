#pragma once
#include "noise.h"
void queue_add(NoiseApp *a,uint16_t track,bool next);
int queue_pop(NoiseApp *a);
void queue_save(const NoiseApp *a);
void queue_remove_track(NoiseApp *a,uint16_t track);
void queue_load(NoiseApp *a);
