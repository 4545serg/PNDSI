#include <nds.h>
#include <fat.h>
#include <filesystem.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "noise.h"
#include "gfx.h"
#include "ui.h"
#include "library.h"
#include "queue.h"
#include "audio.h"
#include "lyrics.h"
#include "input.h"
#include "settings.h"

NoiseApp g_app;
void noise_status(const char*msg){snprintf(g_app.status,sizeof(g_app.status),"%s",msg?msg:"");g_app.status_frames=75;}

static void make_data_dirs(void){mkdir("sd:/3ds",0777);mkdir("sd:/3ds/PROJECT_NOISE",0777);mkdir("sd:/3ds/PROJECT_NOISE/lyrics",0777);mkdir("sd:/Music",0777);}

int main(void){
    defaultExceptionHandler();
    memset(&g_app,0,sizeof(g_app));
    g_app.playing=-1;g_app.volume=224;g_app.repeat_mode=0;g_app.lyrics_animation=true;g_app.lyrics_font=1;g_app.offline_only=false;g_app.sort_mode=0;g_app.theme_mode=0;g_app.tab=TAB_ALL;g_app.page=PAGE_LIBRARY;
    gfx_init();
    ui_boot_screen();
    if(!fatInitDefault()){noise_status("SD CARD INIT FAILED");for(;;){ui_draw(&g_app);swiWaitForVBlank();}}
    make_data_dirs();
    settings_load(&g_app);
    if(!nitroFSInit(NULL))noise_status("NITROFS / HTTPS CA ERROR");
    input_init();
    if(!audio_init())noise_status("AUDIO ENGINE INIT FAILED");
    audio_set_volume(g_app.volume);
    lyrics_init();
    if(!library_scan(&g_app,"sd:/Music"))noise_status("ADD MP3 FILES TO SD:/MUSIC");
    queue_load(&g_app);
    library_rebuild_visible(&g_app);
    noise_status(g_app.track_count?"LIBRARY READY":"NO MP3 FILES FOUND");
    for(;;){
        input_update(&g_app);
        audio_update();
        lyrics_update();
        if(lyrics_matches_ready()&&g_app.page!=PAGE_LYRICS&&g_app.page!=PAGE_SETTINGS){g_app.page=PAGE_LYRICS;g_app.selected=0;g_app.search_editing=false;noise_status("SELECT LYRIC MATCH");}
        if(audio_finished()){if(g_app.repeat_mode==2&&g_app.playing>=0){int again=g_app.playing;if(audio_open(&g_app.tracks[again])){g_app.playing_now=true;lyrics_load_for_track(&g_app.tracks[again]);if(lyrics_count()==0&&!g_app.offline_only)lyrics_request_for_track(&g_app.tracks[again]);}else input_next_track(&g_app);}else input_next_track(&g_app);}
        g_app.frame++;
        ui_draw(&g_app);
    }
    return 0;
}
