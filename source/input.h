#pragma once
#include "noise.h"
void input_init(void);
void input_update(NoiseApp *app);
void input_next_track(NoiseApp *app);
void input_previous_track(NoiseApp *app);
void input_play_selected(NoiseApp *app);
void input_toggle_play(NoiseApp *app);
void input_toggle_like(NoiseApp *app);
