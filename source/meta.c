#include "meta.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static uint8_t frame_data[65536];
static uint32_t synchsafe(const uint8_t *p){return ((uint32_t)(p[0]&127)<<21)|((uint32_t)(p[1]&127)<<14)|((uint32_t)(p[2]&127)<<7)|(p[3]&127);}
static void trim(char *s){size_t n=strlen(s);while(n&&(s[n-1]==' '||s[n-1]=='\r'||s[n-1]=='\n'))s[--n]=0;}
static void latin1_to_utf8(const uint8_t *src,size_t n,char *out,size_t cap){size_t o=0;for(size_t i=0;i<n&&o+2<cap;i++){unsigned c=src[i];if(c<128)out[o++]=(char)c;else{out[o++]=(char)(0xC0|(c>>6));out[o++]=(char)(0x80|(c&63));}}out[o]=0;}
static void copy_text(const uint8_t *p,size_t n,char *out,size_t cap){
    out[0]=0;if(!n||cap<2)return;int enc=*p++;n--;
    if(enc==0){latin1_to_utf8(p,n,out,cap);return;}
    if(enc==3){size_t k=n<cap-1?n:cap-1;memcpy(out,p,k);out[k]=0;return;}
    size_t i=0,o=0;bool little=false;
    if(enc==1&&n>=2){if(p[0]==0xFF&&p[1]==0xFE){little=true;i=2;}else if(p[0]==0xFE&&p[1]==0xFF){little=false;i=2;}}
    for(;i+1<n&&o+1<cap;i+=2){unsigned c=little?(p[i]|(p[i+1]<<8)):((p[i]<<8)|p[i+1]);if(c==0)break;if(c<128)out[o++]=(char)c;else if(c<2048&&o+2<cap){out[o++]=(char)(0xC0|(c>>6));out[o++]=(char)(0x80|(c&63));}else if(o+3<cap){out[o++]=(char)(0xE0|(c>>12));out[o++]=(char)(0x80|((c>>6)&63));out[o++]=(char)(0x80|(c&63));}}
    out[o]=0;
}
static void fallback_title(NoiseTrack *t){const char*p=strrchr(t->path,'/');p=p?p+1:t->path;snprintf(t->title,sizeof(t->title),"%s",p);char*d=strrchr(t->title,'.');if(d)*d=0;}
static void read_id3v1(FILE*f,NoiseTrack*t){if(fseek(f,-128,SEEK_END)!=0)return;uint8_t v[128];if(fread(v,1,128,f)!=128||memcmp(v,"TAG",3))return;if(!t->title[0])latin1_to_utf8(v+3,30,t->title,sizeof(t->title));if(!t->artist[0])latin1_to_utf8(v+33,30,t->artist,sizeof(t->artist));if(!t->album[0])latin1_to_utf8(v+63,30,t->album,sizeof(t->album));}
static uint32_t be32(const uint8_t*p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static void read_duration(FILE*f,NoiseTrack*t,long mp3_start){
    if(t->duration)return;
    if(fseek(f,mp3_start,SEEK_SET)!=0)return;
    uint8_t buf[4096];size_t n=fread(buf,1,sizeof(buf),f);
    static const uint16_t br1[16]={0,32,40,48,56,64,80,96,112,128,160,192,224,256,320,0};
    static const uint16_t br2[16]={0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,0};
    static const uint32_t sr[3][3]={{11025,12000,8000},{22050,24000,16000},{44100,48000,32000}};
    for(size_t i=0;i+40<n;i++){
        if(buf[i]!=0xFF||(buf[i+1]&0xE0)!=0xE0)continue;
        int ver=(buf[i+1]>>3)&3,layer=(buf[i+1]>>1)&3,bi=(buf[i+2]>>4)&15,si=(buf[i+2]>>2)&3;
        if(ver==1||layer!=1||bi==0||bi==15||si==3)continue;
        int version_row=ver==3?2:ver==2?1:0;uint32_t rate=sr[version_row][si];
        int mono=((buf[i+3]>>6)&3)==3;int side=(ver==3?(mono?17:32):(mono?9:17));int crc=(buf[i+1]&1)?0:2;size_t xoff=i+4+crc+side;
        if(xoff+12<n&&(!memcmp(buf+xoff,"Xing",4)||!memcmp(buf+xoff,"Info",4))){uint32_t flags=be32(buf+xoff+4);if(flags&1){uint32_t frames=be32(buf+xoff+8);uint32_t per_frame=ver==3?1152:576;if(frames&&rate){t->duration=(uint32_t)(((uint64_t)frames*per_frame)/rate);return;}}}
        uint32_t kb=ver==3?br1[bi]:br2[bi];if(kb){t->duration=(uint32_t)(((uint64_t)t->bytes*8)/(kb*1000));return;}
    }
}
void meta_read(NoiseTrack*t){
    FILE*f=fopen(t->path,"rb");if(!f){fallback_title(t);return;}
    long mp3_start=0;uint8_t h[10];
    if(fread(h,1,10,f)==10&&!memcmp(h,"ID3",3)){uint32_t size=synchsafe(h+6);mp3_start=10+(long)size;long end=mp3_start;while(ftell(f)+10<=end){uint8_t fh[10];if(fread(fh,1,10,f)!=10)break;if(fh[0]==0)break;uint32_t n=(h[3]==4)?synchsafe(fh+4):((uint32_t)fh[4]<<24)|((uint32_t)fh[5]<<16)|((uint32_t)fh[6]<<8)|fh[7];if(n==0)continue;if(ftell(f)+(long)n>end)break;if(n>sizeof(frame_data)){fseek(f,(long)n,SEEK_CUR);continue;}if(fread(frame_data,1,n,f)!=n)break;if(!memcmp(fh,"TIT2",4))copy_text(frame_data,n,t->title,sizeof(t->title));else if(!memcmp(fh,"TPE1",4))copy_text(frame_data,n,t->artist,sizeof(t->artist));else if(!memcmp(fh,"TALB",4))copy_text(frame_data,n,t->album,sizeof(t->album));}}
    else rewind(f);
    read_id3v1(f,t);read_duration(f,t,mp3_start);fclose(f);
    if(!t->title[0])fallback_title(t);
    trim(t->title);trim(t->artist);trim(t->album);
    if(!t->artist[0])strcpy(t->artist,"Unknown artist");
    if(!t->album[0])strcpy(t->album,"Unknown album");
}
