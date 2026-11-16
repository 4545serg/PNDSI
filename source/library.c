#include "library.h"
#include "meta.h"
#include "ui.h"
#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

static uint32_t scan_entries;
static bool contains_ci(const char *hay,const char *needle){if(!needle||!*needle)return true;size_t n=strlen(needle);for(const char*p=hay;*p;p++)if(!strncasecmp(p,needle,n))return true;return false;}
static bool is_mp3(const char *p){const char*d=strrchr(p,'.');return d&&(!strcasecmp(d,".mp3"));}
static void scan_dir(NoiseApp*a,const char*dir,int depth){if(depth>10||a->track_count>=NOISE_MAX_TRACKS)return;DIR*d=opendir(dir);if(!d)return;struct dirent*e;while((e=readdir(d))&&a->track_count<NOISE_MAX_TRACKS){if((++scan_entries&31)==0){char msg[48];snprintf(msg,sizeof(msg),"SCANNING MUSIC %u",a->track_count);noise_status(msg);ui_draw(a);}if(e->d_name[0]=='.')continue;char p[NOISE_PATH];snprintf(p,sizeof(p),"%s/%s",dir,e->d_name);struct stat st;if(stat(p,&st)!=0)continue;if(S_ISDIR(st.st_mode)){scan_dir(a,p,depth+1);continue;}if(!S_ISREG(st.st_mode)||!is_mp3(p))continue;NoiseTrack*t=&a->tracks[a->track_count];memset(t,0,sizeof(*t));snprintf(t->path,sizeof(t->path),"%s",p);t->bytes=(uint32_t)st.st_size;meta_read(t);a->track_count++;}closedir(d);}
bool library_scan(NoiseApp*a,const char*root){a->track_count=0;scan_entries=0;noise_status("SCANNING MUSIC");ui_draw(a);scan_dir(a,root,0);library_load_likes(a);library_rebuild_visible(a);return a->track_count>0;}
void library_rebuild_visible(NoiseApp*a){a->visible_count=0;if(a->tab==TAB_QUEUE){for(int i=0;i<a->queue_count;i++)if(a->queue[i]<a->track_count)a->visible[a->visible_count++]=a->queue[i];}else{for(int i=0;i<a->track_count;i++){if(a->tab==TAB_LIKED&&!a->tracks[i].liked)continue;if(a->page==PAGE_SEARCH&&a->search_len){char hay[300];snprintf(hay,sizeof(hay),"%s %s %s",a->tracks[i].title,a->tracks[i].artist,a->tracks[i].path);if(!contains_ci(hay,a->search))continue;}a->visible[a->visible_count++]=i;}}
    if(a->tab!=TAB_QUEUE){for(int i=1;i<a->visible_count;i++){uint16_t v=a->visible[i];int j=i-1;while(j>=0){const char*x=a->sort_mode==1?a->tracks[a->visible[j]].artist:a->sort_mode==2?a->tracks[a->visible[j]].album:a->tracks[a->visible[j]].title;const char*y=a->sort_mode==1?a->tracks[v].artist:a->sort_mode==2?a->tracks[v].album:a->tracks[v].title;if(strcasecmp(x,y)<=0)break;a->visible[j+1]=a->visible[j];j--;}a->visible[j+1]=v;}}
    if(a->visible_count==0)a->selected=0;else if(a->selected>=a->visible_count)a->selected=a->visible_count-1;}
void library_toggle_like(NoiseApp*a,int idx){if(idx<0||idx>=a->track_count)return;a->tracks[idx].liked=!a->tracks[idx].liked;library_save_likes(a);library_rebuild_visible(a);}
void library_save_likes(const NoiseApp*a){mkdir("sd:/3ds",0777);mkdir("sd:/3ds/PROJECT_NOISE",0777);FILE*f=fopen("sd:/3ds/PROJECT_NOISE/favorites.txt","w");if(!f)return;for(int i=0;i<a->track_count;i++)if(a->tracks[i].liked)fprintf(f,"%s\n",a->tracks[i].path);fclose(f);}
void library_load_likes(NoiseApp*a){FILE*f=fopen("sd:/3ds/PROJECT_NOISE/favorites.txt","r");if(!f)return;char line[NOISE_PATH];while(fgets(line,sizeof(line),f)){line[strcspn(line,"\r\n")]=0;for(int i=0;i<a->track_count;i++)if(!strcmp(line,a->tracks[i].path)){a->tracks[i].liked=true;break;}}fclose(f);}
void library_search(NoiseApp*a){a->page=PAGE_SEARCH;a->tab=TAB_ALL;a->selected=0;library_rebuild_visible(a);}
