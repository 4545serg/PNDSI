#include "settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
void settings_load(NoiseApp*a){FILE*f=fopen("sd:/3ds/PROJECT_NOISE/settings.cfg","r");if(!f)return;char line[96];while(fgets(line,sizeof(line),f)){char*k=line,*v=strchr(line,'=');if(!v)continue;*v++=0;v[strcspn(v,"\r\n")]=0;if(!strcmp(k,"shuffle"))a->shuffle=atoi(v)!=0;else if(!strcmp(k,"repeat"))a->repeat_mode=(uint8_t)(atoi(v)%3);else if(!strcmp(k,"volume")){int x=atoi(v);a->volume=(uint8_t)(x<0?0:x>255?255:x);}else if(!strcmp(k,"lyrics_animation"))a->lyrics_animation=atoi(v)!=0;else if(!strcmp(k,"lyrics_font"))a->lyrics_font=(uint8_t)(atoi(v)%3);else if(!strcmp(k,"sort"))a->sort_mode=(uint8_t)(atoi(v)%3);else if(!strcmp(k,"offline"))a->offline_only=atoi(v)!=0;else if(!strcmp(k,"theme"))a->theme_mode=(uint8_t)(atoi(v)%4);}fclose(f);}
void settings_save(const NoiseApp*a){mkdir("sd:/3ds",0777);mkdir("sd:/3ds/PROJECT_NOISE",0777);FILE*f=fopen("sd:/3ds/PROJECT_NOISE/settings.cfg","w");if(!f)return;fprintf(f,"shuffle=%d\nrepeat=%u\nvolume=%u\nlyrics_animation=%d\nlyrics_font=%u\nsort=%u\noffline=%d\ntheme=%u\n",a->shuffle,a->repeat_mode,a->volume,a->lyrics_animation,a->lyrics_font,a->sort_mode,a->offline_only,a->theme_mode);fclose(f);}
