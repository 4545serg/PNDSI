#include "lyrics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <time.h>
#include <curl/curl.h>
#include <dswifi9.h>

#define LYRIC_BODY_MAX 32768
#define LYRIC_CACHE_DIR "sd:/3ds/PROJECT_NOISE/lyrics"
#define MAX_MATCHES 5

typedef enum { LY_IDLE, LY_WIFI, LY_EXACT, LY_WAIT_SEARCH, LY_SEARCH, LY_GET_ID, LY_MATCHES } LyState;
typedef struct { long id; char title[NOISE_TEXT]; char artist[NOISE_TEXT]; } LyMatch;

static LyricLine lines[NOISE_MAX_LYRICS];
static LyMatch matches[MAX_MATCHES];
static int nlines, match_count;
static bool is_synced;
static char message[48]="LYRICS NOT LOADED";
static char body[LYRIC_BODY_MAX], synced_buf[LYRIC_BODY_MAX], plain_buf[LYRIC_BODY_MAX];
static size_t body_len;
static CURLM *multi;
static CURL *easy;
static LyState state=LY_IDLE;
static char cache_path[NOISE_PATH];
static char pending_url[1024];
static char pending_title[NOISE_TEXT], pending_artist[NOISE_TEXT], pending_album[NOISE_TEXT];
static uint32_t pending_duration;
static bool wifi_started;
static uint32_t wifi_wait_frames, retry_wait_frames;
static uint16_t search_delay_frames;
static long retry_after;

static uint32_t hash_path(const char*s){uint32_t h=2166136261u;while(*s){h^=(uint8_t)*s++;h*=16777619u;}return h;}
static size_t write_cb(char*ptr,size_t sz,size_t nm,void*ud){(void)ud;size_t n=sz*nm;if(body_len+n>=sizeof(body))return 0;memcpy(body+body_len,ptr,n);body_len+=n;body[body_len]=0;return n;}
static size_t header_cb(char*ptr,size_t sz,size_t nm,void*ud){(void)ud;size_t n=sz*nm;if(n>12&&!strncasecmp(ptr,"Retry-After:",12)){const char*v=ptr+12;while(*v==' '||*v=='\t')v++;char*end=NULL;long sec=strtol(v,&end,10);if(end!=v)retry_after=sec;else{time_t when=curl_getdate(v,NULL),now=time(NULL);retry_after=when>now?(long)(when-now):30;}}return n;}

static void json_string(const char*json,const char*key,char*out,size_t cap){
    char needle[48];snprintf(needle,sizeof(needle),"\"%s\"",key);const char*p=strstr(json,needle);out[0]=0;if(!p)return;p=strchr(p,':');if(!p)return;p++;while(*p&&isspace((unsigned char)*p))p++;if(*p!='\"')return;p++;
    size_t o=0;while(*p&&*p!='\"'&&o+4<cap){unsigned char c=(unsigned char)*p++;if(c=='\\'){c=(unsigned char)*p++;if(c=='n')c='\n';else if(c=='r')c='\r';else if(c=='t')c='\t';else if(c=='u'){unsigned v=0;for(int i=0;i<4&&isxdigit((unsigned char)p[i]);i++){char q=p[i];v=(v<<4)|(q<='9'?q-'0':(tolower(q)-'a'+10));}p+=4;if(v<0x80)out[o++]=(char)v;else if(v<0x800){out[o++]=(char)(0xC0|(v>>6));out[o++]=(char)(0x80|(v&63));}else{out[o++]=(char)(0xE0|(v>>12));out[o++]=(char)(0x80|((v>>6)&63));out[o++]=(char)(0x80|(v&63));}continue;}}out[o++]=(char)c;}out[o]=0;
}
static long json_number(const char*json,const char*key){char needle[48];snprintf(needle,sizeof(needle),"\"%s\"",key);const char*p=strstr(json,needle);if(!p||(p=strchr(p,':'))==NULL)return 0;return strtol(p+1,NULL,10);}
static const char *object_end(const char*p){int depth=0;bool quoted=false,escape=false;for(;*p;p++){if(quoted){if(escape)escape=false;else if(*p=='\\')escape=true;else if(*p=='\"')quoted=false;continue;}if(*p=='\"'){quoted=true;continue;}if(*p=='{')depth++;else if(*p=='}'&&--depth==0)return p;}return NULL;}
static int parse_matches(const char*json){match_count=0;const char*p=json;while((p=strchr(p,'{'))&&match_count<MAX_MATCHES){const char*end=object_end(p);if(!end)break;char title[NOISE_TEXT],artist[NOISE_TEXT];json_string(p,"trackName",title,sizeof(title));json_string(p,"artistName",artist,sizeof(artist));long id=json_number(p,"id");if(id>0){matches[match_count].id=id;snprintf(matches[match_count].title,sizeof(matches[match_count].title),"%s",title[0]?title:"Unknown title");snprintf(matches[match_count].artist,sizeof(matches[match_count].artist),"%s",artist[0]?artist:"Unknown artist");match_count++;}p=end+1;}return match_count;}

static void parse_lrc(const char*src){
    nlines=0;is_synced=false;const char*p=src;
    while(*p&&nlines<NOISE_MAX_LYRICS){const char*e=strchr(p,'\n');const char*line_end=e?e:p+strlen(p);const char*q=p;uint32_t stamps[8];int ns=0;
        while(q<line_end&&*q=='['&&ns<8){const char*close=memchr(q,']',(size_t)(line_end-q));if(!close)break;int mm=0,ss=0;if(sscanf(q+1,"%d:%d",&mm,&ss)!=2||mm<0||ss<0||ss>=60)break;uint32_t frac=0;const char*dot=memchr(q+1,'.',(size_t)(close-(q+1)));if(dot){const char*r=dot+1;int digits=0;while(r<close&&isdigit((unsigned char)*r)&&digits<3){frac=frac*10+(uint32_t)(*r-'0');r++;digits++;}if(digits==1)frac*=100;else if(digits==2)frac*=10;}stamps[ns++]=(uint32_t)(mm*60000+ss*1000)+frac;q=close+1;}
        if(ns){while(q<line_end&&(*q==' '||*q=='\t'))q++;size_t n=(size_t)(line_end-q);if(n>=NOISE_TEXT)n=NOISE_TEXT-1;for(int k=0;k<ns&&nlines<NOISE_MAX_LYRICS;k++){LyricLine*l=&lines[nlines++];l->ms=stamps[k];memcpy(l->text,q,n);l->text[n]=0;is_synced=true;}}
        if(!e)break;
        p=e+1;
    }
    for(int i=1;i<nlines;i++){LyricLine v=lines[i];int j=i-1;while(j>=0&&lines[j].ms>v.ms){lines[j+1]=lines[j];j--;}lines[j+1]=v;}
}
static void parse_plain(const char*src){nlines=0;is_synced=false;const char*p=src;while(*p&&nlines<NOISE_MAX_LYRICS){const char*e=strchr(p,'\n');size_t n=e?(size_t)(e-p):strlen(p);while(n&&p[n-1]=='\r')n--;if(n){LyricLine*l=&lines[nlines++];l->ms=0;if(n>=sizeof(l->text))n=sizeof(l->text)-1;memcpy(l->text,p,n);l->text[n]=0;}if(!e)break;p=e+1;}}
static void cache_write(const char*data){mkdir("sd:/3ds",0777);mkdir("sd:/3ds/PROJECT_NOISE",0777);mkdir(LYRIC_CACHE_DIR,0777);FILE*f=fopen(cache_path,"w");if(f){fputs(data,f);fclose(f);}}
static bool cache_read(const NoiseTrack*t){static char b[LYRIC_BODY_MAX];snprintf(cache_path,sizeof(cache_path),LYRIC_CACHE_DIR"/%08lx.lrc",(unsigned long)hash_path(t->path));FILE*f=fopen(cache_path,"r");bool plain=false;if(!f){snprintf(cache_path,sizeof(cache_path),LYRIC_CACHE_DIR"/%08lx.txt",(unsigned long)hash_path(t->path));f=fopen(cache_path,"r");plain=true;}if(!f)return false;size_t n=fread(b,1,sizeof(b)-1,f);fclose(f);b[n]=0;if(plain)parse_plain(b);else parse_lrc(b);strcpy(message,"CACHED LYRICS");return nlines>0;}

static void cancel_pending(void){if(easy){if(multi)curl_multi_remove_handle(multi,easy);curl_easy_cleanup(easy);easy=NULL;}state=LY_IDLE;match_count=0;}
void lyrics_init(void){curl_global_init(CURL_GLOBAL_DEFAULT);multi=NULL;easy=NULL;state=LY_IDLE;}
void lyrics_load_for_track(const NoiseTrack*t){cancel_pending();nlines=0;is_synced=false;match_count=0;if(!t){strcpy(message,"NO TRACK SELECTED");return;}if(cache_read(t))return;char p[NOISE_PATH];snprintf(p,sizeof(p),"%s",t->path);char*d=strrchr(p,'.');if(d)strcpy(d,".lrc");FILE*f=fopen(p,"r");if(f){static char b[LYRIC_BODY_MAX];size_t n=fread(b,1,sizeof(b)-1,f);fclose(f);b[n]=0;parse_lrc(b);strcpy(message,"LOCAL LYRICS");if(nlines){snprintf(cache_path,sizeof(cache_path),LYRIC_CACHE_DIR"/%08lx.lrc",(unsigned long)hash_path(t->path));cache_write(b);return;}}strcpy(message,"LYRICS NOT FOUND");}

static void url_encode(const char*src,char*dst,size_t cap){static const char hex[]="0123456789ABCDEF";size_t o=0;for(const unsigned char*p=(const unsigned char*)src;*p&&o+4<cap;p++){unsigned char c=*p;if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~')dst[o++]=c;else if(c==' ')dst[o++]='+';else{dst[o++]='%';dst[o++]=hex[c>>4];dst[o++]=hex[c&15];}}dst[o]=0;}
static void make_url(bool search){char a[256],t[256],l[256];url_encode(pending_artist,a,sizeof(a));url_encode(pending_title,t,sizeof(t));url_encode(pending_album,l,sizeof(l));if(search)snprintf(pending_url,sizeof(pending_url),"https://lrclib.net/api/search?track_name=%s&artist_name=%s&album_name=%s",t,a,l);else snprintf(pending_url,sizeof(pending_url),"https://lrclib.net/api/get?track_name=%s&artist_name=%s&album_name=%s&duration=%lu",t,a,l,(unsigned long)pending_duration);}
static void start_url(const char*url,LyState new_state,const char*label){if(easy){curl_multi_remove_handle(multi,easy);curl_easy_cleanup(easy);easy=NULL;}if(!multi)multi=curl_multi_init();body_len=0;body[0]=0;retry_after=0;easy=curl_easy_init();curl_easy_setopt(easy,CURLOPT_URL,url);curl_easy_setopt(easy,CURLOPT_USERAGENT,"PROJECT NOISE/1.0 (Nintendo DSi homebrew)");curl_easy_setopt(easy,CURLOPT_ACCEPT_ENCODING,"gzip");curl_easy_setopt(easy,CURLOPT_WRITEFUNCTION,write_cb);curl_easy_setopt(easy,CURLOPT_HEADERFUNCTION,header_cb);curl_easy_setopt(easy,CURLOPT_CONNECTTIMEOUT,8L);curl_easy_setopt(easy,CURLOPT_TIMEOUT,14L);curl_easy_setopt(easy,CURLOPT_NOSIGNAL,1L);curl_easy_setopt(easy,CURLOPT_CAINFO,"nitro:/cacert.pem");curl_easy_setopt(easy,CURLOPT_SSL_VERIFYPEER,1L);curl_easy_setopt(easy,CURLOPT_SSL_VERIFYHOST,2L);curl_multi_add_handle(multi,easy);state=new_state;snprintf(message,sizeof(message),"%s",label);}
static void start_request(bool search){make_url(search);start_url(pending_url,search?LY_SEARCH:LY_EXACT,search?"SEARCHING LRCLIB":"MATCHING LRCLIB");}
static void start_id_request(long id){char url[128];snprintf(url,sizeof(url),"https://lrclib.net/api/get/%ld",id);start_url(url,LY_GET_ID,"FETCHING LYRIC MATCH");}
void lyrics_request_for_track(const NoiseTrack*t){
    if(!t)return;
    if(retry_wait_frames){strcpy(message,"LRCLIB RATE LIMIT WAIT");return;}
    lyrics_load_for_track(t);if(nlines)return;
    snprintf(pending_title,sizeof(pending_title),"%s",t->title);snprintf(pending_artist,sizeof(pending_artist),"%s",t->artist);snprintf(pending_album,sizeof(pending_album),"%s",t->album);pending_duration=t->duration;snprintf(cache_path,sizeof(cache_path),LYRIC_CACHE_DIR"/%08lx.lrc",(unsigned long)hash_path(t->path));
    if(!wifi_started){if(!Wifi_InitDefault(INIT_ONLY|WIFI_ATTEMPT_DSI_MODE)){strcpy(message,"WIFI INIT FAILED");return;}Wifi_AutoConnect();wifi_started=true;wifi_wait_frames=0;state=LY_WIFI;strcpy(message,"CONNECTING WIFI");}
    else if(Wifi_AssocStatus()==ASSOCSTATUS_ASSOCIATED)start_request(false);
    else{wifi_wait_frames=0;state=LY_WIFI;strcpy(message,"CONNECTING WIFI");}
}

static void handle_lyrics_response(void){
    json_string(body,"syncedLyrics",synced_buf,sizeof(synced_buf));json_string(body,"plainLyrics",plain_buf,sizeof(plain_buf));
    if(synced_buf[0]){parse_lrc(synced_buf);cache_write(synced_buf);strcpy(message,"LRCLIB SYNCED LYRICS");}
    else if(plain_buf[0]){parse_plain(plain_buf);char old[NOISE_PATH];snprintf(old,sizeof(old),"%s",cache_path);char*ext=strrchr(cache_path,'.');if(ext)strcpy(ext,".txt");cache_write(plain_buf);snprintf(cache_path,sizeof(cache_path),"%s",old);strcpy(message,"LRCLIB PLAIN LYRICS");}
    else strcpy(message,"NO LYRICS FOUND");
}
void lyrics_update(void){
    if(retry_wait_frames)retry_wait_frames--;
    if(state==LY_WAIT_SEARCH){if(search_delay_frames)search_delay_frames--;else start_request(true);return;}
    if(state==LY_WIFI){int s=Wifi_AssocStatus();if(s==ASSOCSTATUS_ASSOCIATED)start_request(false);else if(s==ASSOCSTATUS_CANNOTCONNECT||++wifi_wait_frames>900){state=LY_IDLE;strcpy(message,"WIFI NOT CONNECTED");}return;}
    if(state!=LY_EXACT&&state!=LY_SEARCH&&state!=LY_GET_ID)return;
    int running=0;if(curl_multi_perform(multi,&running)!=CURLM_OK){state=LY_IDLE;strcpy(message,"NETWORK ERROR");return;}
    int q;CURLMsg*m;while((m=curl_multi_info_read(multi,&q))){if(m->msg!=CURLMSG_DONE)continue;long code=0;curl_easy_getinfo(easy,CURLINFO_RESPONSE_CODE,&code);LyState completed=state;curl_multi_remove_handle(multi,easy);curl_easy_cleanup(easy);easy=NULL;
        if(code==404&&completed==LY_EXACT){search_delay_frames=18;state=LY_WAIT_SEARCH;strcpy(message,"SEARCHING LRCLIB");return;}
        if(code==429){retry_wait_frames=(uint32_t)(retry_after>0?retry_after:30)*60;snprintf(message,sizeof(message),"RATE LIMITED %lds",retry_after);state=LY_IDLE;return;}
        if(code>=200&&code<300){
            if(completed==LY_SEARCH){int n=parse_matches(body);if(n==1){start_id_request(matches[0].id);return;}if(n>1){state=LY_MATCHES;strcpy(message,"SELECT LYRIC MATCH");return;}state=LY_IDLE;strcpy(message,"NO LYRICS FOUND");return;}
            handle_lyrics_response();state=LY_IDLE;return;
        }
        snprintf(message,sizeof(message),"LRCLIB HTTP %ld",code);state=LY_IDLE;return;
    }
}

int lyrics_line_at(uint32_t ms){if(!nlines)return -1;if(!is_synced)return 0;int i=0;while(i+1<nlines&&lines[i+1].ms<=ms)i++;return i;}
const LyricLine*lyrics_lines(void){return lines;}
int lyrics_count(void){return nlines;}
bool lyrics_synced(void){return is_synced;}
bool lyrics_busy(void){return state!=LY_IDLE&&state!=LY_MATCHES;}
const char*lyrics_message(void){return message;}
const char*lyrics_wifi_status(void){if(!wifi_started)return "OFFLINE";int s=Wifi_AssocStatus();return s==ASSOCSTATUS_ASSOCIATED?"CONNECTED":s==ASSOCSTATUS_CANNOTCONNECT?"OFFLINE":"CONNECTING";}
void lyrics_clear_cache(void){DIR*d=opendir(LYRIC_CACHE_DIR);if(!d)return;struct dirent*e;char p[NOISE_PATH];while((e=readdir(d))){if(e->d_name[0]=='.')continue;snprintf(p,sizeof(p),LYRIC_CACHE_DIR"/%s",e->d_name);unlink(p);}closedir(d);nlines=0;is_synced=false;strcpy(message,"LYRICS CACHE CLEARED");}
bool lyrics_matches_ready(void){return state==LY_MATCHES;}
int lyrics_match_count(void){return match_count;}
const char*lyrics_match_title(int i){return i>=0&&i<match_count?matches[i].title:"";}
const char*lyrics_match_artist(int i){return i>=0&&i<match_count?matches[i].artist:"";}
void lyrics_select_match(int i){if(state!=LY_MATCHES||i<0||i>=match_count)return;start_id_request(matches[i].id);}
void lyrics_dismiss_matches(void){if(state==LY_MATCHES){state=LY_IDLE;match_count=0;strcpy(message,"LYRIC MATCHES CLOSED");}}
