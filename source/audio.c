#include "audio.h"
#include "decoder_mp3.h"
#include <maxmod9.h>
#include <string.h>
#include <nds/interrupts.h>

#define PCM_RING_FRAMES 32768
static int16_t ring[PCM_RING_FRAMES*2];
static volatile uint32_t ring_read,ring_write;
static volatile uint32_t supplied_frames;
static uint32_t base_position_frames;
static uint32_t output_rate;
static uint8_t master_volume=224;
static bool audio_ready,stream_open,paused;
static mm_stream stream_info;

uint32_t audio_ring_used(void){uint32_t r=ring_read,w=ring_write;return (w-r)&(PCM_RING_FRAMES-1);}
uint32_t audio_ring_free(void){return (PCM_RING_FRAMES-1)-audio_ring_used();}
void audio_ring_reset(void){ring_read=ring_write=supplied_frames=0;}
void audio_push_pcm(const int16_t*samples,uint32_t frames){
    uint32_t free_frames=audio_ring_free();if(frames>free_frames)frames=free_frames;
    for(uint32_t i=0;i<frames;i++){uint32_t w=ring_write;ring[w*2]=samples[i*2];ring[w*2+1]=samples[i*2+1];__asm__ volatile("" ::: "memory");ring_write=(w+1)&(PCM_RING_FRAMES-1);}
}
static mm_word stream_callback(mm_word length,mm_addr dest,mm_stream_formats fmt){
    int16_t*out=(int16_t*)dest;
    if(fmt!=MM_STREAM_16BIT_STEREO){memset((void*)dest,0,length*2);return length;}
    for(uint32_t i=0;i<length;i++){
        uint32_t r=ring_read,w=ring_write;
        if(r!=w){out[i*2]=ring[r*2];out[i*2+1]=ring[r*2+1];ring_read=(r+1)&(PCM_RING_FRAMES-1);supplied_frames++;}
        else{out[i*2]=0;out[i*2+1]=0;}
    }
    return length;
}
static void stream_open_now(void){
    stream_info.sampling_rate=output_rate;stream_info.buffer_length=4096;
    stream_info.callback=stream_callback;stream_info.format=MM_STREAM_16BIT_STEREO;
    stream_info.timer=MM_TIMER0;stream_info.manual=false;
    mmStreamOpen(&stream_info);mmStreamVolume(master_volume);stream_open=true;paused=false;
}
bool audio_init(void){soundEnable();audio_ready=mmInitNoSoundbank();return audio_ready;}
void audio_stop(void){if(stream_open){irqDisable(IRQ_TIMER0);mmStreamClose();stream_open=false;irqEnable(IRQ_TIMER0);}decoder_mp3_close();audio_ring_reset();output_rate=0;base_position_frames=0;paused=false;}
bool audio_open(const NoiseTrack*t){if(!audio_ready||!t)return false;audio_stop();if(!decoder_mp3_open(t->path))return false;decoder_mp3_prefill();output_rate=decoder_mp3_rate();if(!output_rate||audio_ring_used()<256){audio_stop();return false;}stream_open_now();return true;}
void audio_update(void){if(stream_open&&!paused)decoder_mp3_pump();}
void audio_pause(void){if(!stream_open||paused)return;irqDisable(IRQ_TIMER0);uint32_t played=mmStreamGetPosition();uint32_t ahead=supplied_frames>played?supplied_frames-played:0;uint32_t r=ring_read;ring_read=(r-ahead)&(PCM_RING_FRAMES-1);base_position_frames+=played;mmStreamClose();stream_open=false;supplied_frames=0;paused=true;irqEnable(IRQ_TIMER0);}
void audio_resume(void){if(!audio_ready||stream_open||!paused)return;stream_open_now();}
void audio_set_volume(uint8_t v){master_volume=v;if(stream_open)mmStreamVolume(master_volume);}
bool audio_is_playing(void){return stream_open&&!paused;}
uint32_t audio_position_ms(void){if(!output_rate)return 0;uint32_t frames=base_position_frames+(stream_open?mmStreamGetPosition():0);return (uint32_t)(((uint64_t)frames*1000)/output_rate);}
uint32_t audio_output_rate(void){return output_rate;}
bool audio_finished(void){if(!decoder_mp3_done()||audio_ring_used()!=0||paused||!stream_open)return false;return mmStreamGetPosition()>=supplied_frames;}
