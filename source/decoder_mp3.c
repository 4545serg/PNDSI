#include "decoder_mp3.h"
#include "audio.h"
#include <mad.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define INPUT_CHUNK 8192
#define INPUT_BUFFER (INPUT_CHUNK + MAD_BUFFER_GUARD)
#define MAD_MAX_SAMPLES 1152
#define MAX_OUTPUT_FRAMES 4800
#define OUTPUT_RATE_MAX 32768
#define PUMP_FRAMES 2

static struct mad_stream stream;
static struct mad_frame frame;
static struct mad_synth synth;
static unsigned char input[INPUT_BUFFER];
static FILE *file;
static bool opened, need_refill, input_eof, decode_done, first_frame;
static uint32_t input_rate, output_rate;
static uint64_t input_total, output_total;
static int16_t pcm_out[MAX_OUTPUT_FRAMES*2];

static int16_t scale_sample(mad_fixed_t sample){
    sample+=(1L<<(MAD_F_FRACBITS-16));
    if(sample>=MAD_F_ONE)sample=MAD_F_ONE-1;
    else if(sample< -MAD_F_ONE)sample= -MAD_F_ONE;
    sample>>=(MAD_F_FRACBITS+1-16);
    return (int16_t)sample;
}

static bool refill_input(void){
    size_t remaining=0;
    if(stream.next_frame&&stream.bufend&&stream.next_frame<=stream.bufend){
        remaining=(size_t)(stream.bufend-stream.next_frame);
        if(remaining>INPUT_CHUNK)remaining=INPUT_CHUNK;
        if(remaining)memmove(input,stream.next_frame,remaining);
    }
    size_t got=fread(input+remaining,1,INPUT_CHUNK-remaining,file);
    if(got==0){
        input_eof=true;
        if(remaining==0)return false;
        memset(input+remaining,0,MAD_BUFFER_GUARD);
        mad_stream_buffer(&stream,input,(unsigned long)(remaining+MAD_BUFFER_GUARD));
        return true;
    }
    mad_stream_buffer(&stream,input,(unsigned long)(remaining+got));
    return true;
}

static bool frame_has_vbr_header(void){
    if(!stream.this_frame||!stream.next_frame||stream.next_frame<=stream.this_frame)return false;
    size_t n=(size_t)(stream.next_frame-stream.this_frame);
    if(n>160)n=160;
    for(size_t i=0;i+4<=n;i++){
        const unsigned char*p=stream.this_frame+i;
        if(!memcmp(p,"Xing",4)||!memcmp(p,"Info",4)||!memcmp(p,"VBRI",4))return true;
    }
    return false;
}

static void resample_and_push(const struct mad_pcm *pcm){
    if(!pcm->length||!pcm->samplerate)return;
    uint32_t in_rate=pcm->samplerate;
    if(!input_rate){input_rate=in_rate;output_rate=in_rate>OUTPUT_RATE_MAX?OUTPUT_RATE_MAX:in_rate;}
    if(in_rate!=input_rate){input_rate=in_rate;output_rate=in_rate>OUTPUT_RATE_MAX?OUTPUT_RATE_MAX:in_rate;input_total=output_total=0;}
    const mad_fixed_t*left=pcm->samples[0];
    const mad_fixed_t*right=pcm->channels==2?pcm->samples[1]:NULL;
    uint32_t n=pcm->length;if(n>MAD_MAX_SAMPLES)n=MAD_MAX_SAMPLES;
    uint64_t new_input_total=input_total+n;
    uint64_t target_output_total=(new_input_total*output_rate)/input_rate;
    uint32_t out_frames=(uint32_t)(target_output_total-output_total);
    if(out_frames>MAX_OUTPUT_FRAMES)out_frames=MAX_OUTPUT_FRAMES;
    for(uint32_t i=0;i<out_frames;i++){
        uint64_t out_index=output_total+i;
        uint64_t src_q16=((out_index*(uint64_t)input_rate)<<16)/output_rate;
        uint64_t src_index=src_q16>>16;
        uint32_t frac=(uint32_t)(src_q16&65535);
        uint32_t rel=(src_index<input_total)?0:(uint32_t)(src_index-input_total);
        if(rel>=n)rel=n-1;
        uint32_t next=rel+1<n?rel+1:rel;
        int32_t l0=scale_sample(left[rel]),l1=scale_sample(left[next]);
        int32_t r0=right?scale_sample(right[rel]):l0;
        int32_t r1=right?scale_sample(right[next]):l1;
        pcm_out[i*2]=(int16_t)(l0+(((l1-l0)*(int32_t)frac)>>16));
        pcm_out[i*2+1]=(int16_t)(r0+(((r1-r0)*(int32_t)frac)>>16));
    }
    if(out_frames)audio_push_pcm(pcm_out,out_frames);
    input_total=new_input_total;
    output_total=target_output_total;
}

bool decoder_mp3_open(const char*path){
    decoder_mp3_close();
    file=fopen(path,"rb");if(!file)return false;
    unsigned char id3[10];
    if(fread(id3,1,10,file)==10&&!memcmp(id3,"ID3",3)){
        uint32_t tag=((uint32_t)(id3[6]&127)<<21)|((uint32_t)(id3[7]&127)<<14)|((uint32_t)(id3[8]&127)<<7)|(id3[9]&127);
        if(fseek(file,(long)(10+tag),SEEK_SET)!=0){fclose(file);file=NULL;return false;}
    }else rewind(file);
    mad_stream_init(&stream);mad_frame_init(&frame);mad_synth_init(&synth);
    opened=true;need_refill=true;input_eof=false;decode_done=false;first_frame=true;
    input_rate=output_rate=0;input_total=output_total=0;
    return true;
}

void decoder_mp3_close(void){
    if(opened){mad_synth_finish(&synth);mad_frame_finish(&frame);mad_stream_finish(&stream);opened=false;}
    if(file){fclose(file);file=NULL;}
    need_refill=true;input_eof=false;decode_done=false;first_frame=true;
    input_rate=output_rate=0;input_total=output_total=0;
}

void decoder_mp3_pump(void){
    if(!opened||decode_done)return;
    for(int count=0,attempts=0;count<PUMP_FRAMES&&attempts<64;){
        attempts++;
        if(audio_ring_free()<MAX_OUTPUT_FRAMES)break;
        if(need_refill){if(!refill_input()){decode_done=true;break;}need_refill=false;}
        if(mad_frame_decode(&frame,&stream)==-1){
            if(stream.error==MAD_ERROR_BUFLEN){if(input_eof){decode_done=true;break;}need_refill=true;continue;}
            if(MAD_RECOVERABLE(stream.error))continue;
            decode_done=true;break;
        }
        bool skip_header=first_frame&&frame_has_vbr_header();
        first_frame=false;
        if(!skip_header){mad_synth_frame(&synth,&frame);resample_and_push(&synth.pcm);}
        count++;
    }
}

void decoder_mp3_prefill(void){for(int i=0;i<32&&audio_ring_used()<8192&&!decode_done;i++)decoder_mp3_pump();}
bool decoder_mp3_done(void){return decode_done;}
uint32_t decoder_mp3_rate(void){return output_rate;}
