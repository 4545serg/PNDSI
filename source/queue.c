#include "queue.h"
#include "library.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
void queue_add(NoiseApp*a,uint16_t t,bool next){if(t>=a->track_count)return;for(int i=0;i<a->queue_count;i++)if(a->queue[i]==t){for(int j=i;j<a->queue_count-1;j++)a->queue[j]=a->queue[j+1];a->queue_count--;break;}if(a->queue_count>=NOISE_MAX_QUEUE)return;int at=next?0:a->queue_count;for(int i=a->queue_count;i>at;i--)a->queue[i]=a->queue[i-1];a->queue[at]=t;a->queue_count++;queue_save(a);library_rebuild_visible(a);}
void queue_remove_track(NoiseApp*a,uint16_t t){for(int i=0;i<a->queue_count;i++)if(a->queue[i]==t){for(int j=i;j<a->queue_count-1;j++)a->queue[j]=a->queue[j+1];a->queue_count--;queue_save(a);library_rebuild_visible(a);return;}}
int queue_pop(NoiseApp*a){if(!a->queue_count)return -1;int t=a->queue[0];for(int i=0;i<a->queue_count-1;i++)a->queue[i]=a->queue[i+1];a->queue_count--;queue_save(a);library_rebuild_visible(a);return t;}
void queue_save(const NoiseApp*a){mkdir("sd:/3ds",0777);mkdir("sd:/3ds/PROJECT_NOISE",0777);FILE*f=fopen("sd:/3ds/PROJECT_NOISE/queue.txt","w");if(!f)return;for(int i=0;i<a->queue_count;i++)if(a->queue[i]<a->track_count)fprintf(f,"%s\n",a->tracks[a->queue[i]].path);fclose(f);}
void queue_load(NoiseApp*a){FILE*f=fopen("sd:/3ds/PROJECT_NOISE/queue.txt","r");if(!f)return;char line[NOISE_PATH];while(fgets(line,sizeof(line),f)&&a->queue_count<NOISE_MAX_QUEUE){line[strcspn(line,"\r\n")]=0;for(int i=0;i<a->track_count;i++)if(!strcmp(line,a->tracks[i].path)){a->queue[a->queue_count++]=i;break;}}fclose(f);}
