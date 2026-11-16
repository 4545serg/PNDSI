#include "input.h"
#include "library.h"
#include "queue.h"
#include "audio.h"
#include "lyrics.h"
#include "ui.h"
#include "settings.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static bool x_pending;
static uint8_t x_timer;
static int x_track=-1;
static uint8_t up_repeat,down_repeat;
static void play_track(NoiseApp*a,int idx){if(idx<0||idx>=a->track_count)return;a->playing_now=false;a->paused=false;if(!audio_open(&a->tracks[idx])){a->playing=-1;noise_status("MP3 OPEN / DECODE FAILED");return;}if(a->tab==TAB_QUEUE)queue_remove_track(a,(uint16_t)idx);a->playing=idx;a->playing_now=true;a->paused=false;lyrics_load_for_track(&a->tracks[idx]);if(lyrics_count()==0&&!a->offline_only)lyrics_request_for_track(&a->tracks[idx]);noise_status("NOW PLAYING");}
static int selected_track(NoiseApp*a){if(!a->visible_count||a->selected>=a->visible_count)return -1;return a->visible[a->selected];}
void input_next_track(NoiseApp*a){int n=-1;if(a->queue_count)n=queue_pop(a);else if(a->track_count){if(a->shuffle)n=rand()%a->track_count;else if(a->playing<0)n=0;else if(a->playing+1<a->track_count)n=a->playing+1;else if(a->repeat_mode==1)n=0;}if(n>=0)play_track(a,n);else{audio_stop();a->playing_now=false;a->paused=false;noise_status("PLAYBACK FINISHED");}}
void input_previous_track(NoiseApp*a){if(!a->track_count)return;int n=a->playing<0?0:(a->playing+ a->track_count-1)%a->track_count;play_track(a,n);}
void input_play_selected(NoiseApp*a){int idx=selected_track(a);if(idx<0)return;if(a->paused&&a->playing==idx){audio_resume();a->paused=false;a->playing_now=true;}else play_track(a,idx);}
void input_toggle_play(NoiseApp*a){int idx=selected_track(a);if(idx<0)return;if(a->playing<0||idx!=a->playing){play_track(a,idx);return;}if(a->paused){audio_resume();a->paused=false;a->playing_now=true;}else{audio_pause();a->paused=true;a->playing_now=false;}}
void input_toggle_like(NoiseApp*a){int t=a->playing>=0?a->playing:selected_track(a);if(t>=0){library_toggle_like(a,t);noise_status(a->tracks[t].liked?"LIKED SONG":"UNLIKED SONG");}}
void input_init(void){x_pending=false;x_timer=0;x_track=-1;up_repeat=down_repeat=0;}
static void move_selection(NoiseApp*a,int d){if(!a->visible_count)return;int n=(int)a->selected+d;if(n<0)n=0;if(n>=a->visible_count)n=a->visible_count-1;a->selected=(uint16_t)n;}
static void commit_x(NoiseApp*a,bool next){if(x_track<0||x_track>=a->track_count)return;queue_add(a,(uint16_t)x_track,next);noise_status(next?"ADDED TO PLAY NEXT":"ADDED TO QUEUE");}
void input_update(NoiseApp*a){scanKeys();uint32_t down=keysDown(),held=keysHeld();
    if(a->status_frames)a->status_frames--;
    if(down&KEY_X){if(x_pending&&x_timer>0){x_pending=false;x_timer=0;commit_x(a,true);x_track=-1;}else{if(x_pending){commit_x(a,false);}x_pending=true;x_timer=15;x_track=a->playing>=0?a->playing:selected_track(a);}}
    else if(x_pending){if(x_timer)x_timer--;else{commit_x(a,false);x_pending=false;x_track=-1;}}
    if(down&KEY_L)input_previous_track(a);
    if(down&KEY_R)input_next_track(a);
    if(down&KEY_A){if(a->page==PAGE_LYRICS){lyrics_select_match(a->selected);a->page=PAGE_LIBRARY;noise_status("FETCHING LYRIC MATCH");}else if(a->page==PAGE_SETTINGS){if(a->selected==0)a->shuffle=!a->shuffle;else if(a->selected==1)a->repeat_mode=(a->repeat_mode+1)%3;else if(a->selected==2)a->lyrics_animation=!a->lyrics_animation;else if(a->selected==3)a->lyrics_font=(a->lyrics_font+1)%3;else if(a->selected==4){a->volume=(uint8_t)(a->volume+32);audio_set_volume(a->volume);}else if(a->selected==5){a->sort_mode=(a->sort_mode+1)%3;library_rebuild_visible(a);}else if(a->selected==6)a->offline_only=!a->offline_only;else if(a->selected==7)lyrics_clear_cache();else if(a->selected==8)a->theme_mode=(a->theme_mode+1)%4;settings_save(a);}else input_play_selected(a);}
    if(down&KEY_B){audio_pause();a->paused=true;a->playing_now=false;noise_status("PAUSED");}
    if(down&KEY_Y)input_toggle_like(a);
    if(down&KEY_UP){if(a->page==PAGE_SETTINGS||a->page==PAGE_LYRICS){if(a->selected>0)a->selected--;}else move_selection(a,-1);up_repeat=9;}else if((held&KEY_UP)&&up_repeat==0){if(a->page==PAGE_SETTINGS||a->page==PAGE_LYRICS){if(a->selected>0)a->selected--;}else move_selection(a,-1);up_repeat=4;}else if(up_repeat)up_repeat--;
    if(down&KEY_DOWN){if(a->page==PAGE_SETTINGS){if(a->selected<8)a->selected++;}else if(a->page==PAGE_LYRICS){if(a->selected+1<lyrics_match_count())a->selected++;}else move_selection(a,1);down_repeat=9;}else if((held&KEY_DOWN)&&down_repeat==0){if(a->page==PAGE_SETTINGS){if(a->selected<8)a->selected++;}else if(a->page==PAGE_LYRICS){if(a->selected+1<lyrics_match_count())a->selected++;}else move_selection(a,1);down_repeat=4;}else if(down_repeat)down_repeat--;
    if(a->search_editing&&a->page!=PAGE_SETTINGS){if(down&KEY_LEFT){if(a->search_cursor)a->search_cursor--;}if(down&KEY_RIGHT){if(a->search_cursor<a->search_len)a->search_cursor++;}}
    else if(a->page==PAGE_LIBRARY&&down&KEY_LEFT){a->tab=(a->tab==TAB_ALL)?TAB_QUEUE:(a->tab==TAB_QUEUE)?TAB_LIKED:TAB_ALL;a->selected=0;library_rebuild_visible(a);}
    else if(a->page==PAGE_LIBRARY&&down&KEY_RIGHT){a->tab=(a->tab==TAB_ALL)?TAB_LIKED:(a->tab==TAB_LIKED)?TAB_QUEUE:TAB_ALL;a->selected=0;library_rebuild_visible(a);}
    if(down&KEY_SELECT){if(a->page==PAGE_LYRICS)lyrics_dismiss_matches();a->page=(a->page==PAGE_SETTINGS)?PAGE_LIBRARY:PAGE_SETTINGS;a->selected=0;noise_status(a->page==PAGE_SETTINGS?"SETTINGS":"LIBRARY");}
    if(down&KEY_START){if(a->page==PAGE_LYRICS){lyrics_dismiss_matches();a->page=PAGE_LIBRARY;}else{a->search_editing=false;a->page=PAGE_LIBRARY;a->tab=TAB_ALL;a->search_len=0;a->search[0]=0;library_rebuild_visible(a);}}
    if(down&KEY_TOUCH){touchPosition p;touchRead(&p);bool lyric_page=(a->page==PAGE_LYRICS);ui_touch(a,p.px,p.py);if(!lyric_page&&p.py>=48&&p.py<114&&!a->search_editing&&a->page!=PAGE_SETTINGS){int row=(p.py-48)/22;int base=a->selected>0?a->selected-1:0;int idx=base+row;if(idx>=0&&idx<a->visible_count)play_track(a,a->visible[idx]);}}
}
