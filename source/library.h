#pragma once
#include "noise.h"
bool library_scan(NoiseApp *app,const char *root);
void library_rebuild_visible(NoiseApp *app);
void library_toggle_like(NoiseApp *app,int track);
void library_save_likes(const NoiseApp *app);
void library_load_likes(NoiseApp *app);
void library_search(NoiseApp *app);
