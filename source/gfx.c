#include "gfx.h"
#include "font8x8.h"
#include <nds/cothread.h>
#include <string.h>
#include <stdlib.h>

#define W 256
#define H 192
#define BG_STRIDE 256
static int bg_top, bg_bottom;
static uint16_t *top_fb, *bottom_fb;
static uint16_t top_buffer[256*192];
static uint16_t bottom_buffer[256*192];

void gfx_init(void) {
    videoSetMode(MODE_5_2D | DISPLAY_BG2_ACTIVE);
    vramSetBankA(VRAM_A_MAIN_BG);
    bg_top = bgInit(2, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    top_fb = (uint16_t*)bgGetGfxPtr(bg_top);
    videoSetModeSub(MODE_5_2D | DISPLAY_BG2_ACTIVE);
    vramSetBankC(VRAM_C_SUB_BG);
    bg_bottom = bgInitSub(2, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    bottom_fb = (uint16_t*)bgGetGfxPtr(bg_bottom);
    memset(top_fb,0,256*256*2); memset(bottom_fb,0,256*256*2);
    memset(top_buffer,0,sizeof(top_buffer)); memset(bottom_buffer,0,sizeof(bottom_buffer));
}
uint16_t *gfx_top(void){return top_buffer;}
uint16_t *gfx_bottom(void){return bottom_buffer;}
void gfx_clear(uint16_t *fb,uint16_t c){ for(int y=0;y<H;y++) for(int x=0;x<W;x++) fb[y*BG_STRIDE+x]=c; }
void gfx_rect(uint16_t *fb,int x,int y,int w,int h,uint16_t c){
    if(w<=0||h<=0)return;
    if(x<0){w+=x;x=0;}
    if(y<0){h+=y;y=0;}
    if(x+w>W)w=W-x;
    if(y+h>H)h=H-y;
    if(w<=0||h<=0)return;
    for(int j=0;j<h;j++){uint16_t *p=fb+(y+j)*BG_STRIDE+x;for(int i=0;i<w;i++)p[i]=c;}
}
void gfx_round(uint16_t *fb,int x,int y,int w,int h,int r,uint16_t c){
    gfx_rect(fb,x+r,y,w-2*r,h,c); gfx_rect(fb,x,y+r,w,h-2*r,c);
    for(int yy=0;yy<r;yy++)for(int xx=0;xx<r;xx++){int dx=r-xx,dy=r-yy;if(dx*dx+dy*dy<=r*r){if(x+xx>=0&&y+yy>=0&&x+xx<W&&y+yy<H)fb[(y+yy)*BG_STRIDE+x+xx]=c;if(x+w-1-xx>=0&&y+yy>=0&&x+w-1-xx<W&&y+yy<H)fb[(y+yy)*BG_STRIDE+x+w-1-xx]=c;if(x+xx>=0&&y+h-1-yy>=0&&x+xx<W&&y+h-1-yy<H)fb[(y+h-1-yy)*BG_STRIDE+x+xx]=c;if(x+w-1-xx>=0&&y+h-1-yy>=0&&x+w-1-xx<W&&y+h-1-yy<H)fb[(y+h-1-yy)*BG_STRIDE+x+w-1-xx]=c;}}
}
void gfx_line(uint16_t *fb,int x0,int y0,int x1,int y1,uint16_t c){int dx=abs(x1-x0),sx=x0<x1?1:-1,dy=-abs(y1-y0),sy=y0<y1?1:-1,err=dx+dy;for(;;){if(x0>=0&&x0<W&&y0>=0&&y0<H)fb[y0*BG_STRIDE+x0]=c;if(x0==x1&&y0==y1)break;int e2=2*err;if(e2>=dy){err+=dy;x0+=sx;}if(e2<=dx){err+=dx;y0+=sy;}}}
void gfx_circle(uint16_t *fb,int cx,int cy,int r,uint16_t c){int x=r,y=0,err=1-r;while(x>=y){int px[8]={cx+x,cx+y,cx-y,cx-x,cx-x,cx-y,cx+y,cx+x};int py[8]={cy+y,cy+x,cy+x,cy+y,cy-y,cy-x,cy-x,cy-y};for(int i=0;i<8;i++)if(px[i]>=0&&px[i]<W&&py[i]>=0&&py[i]<H)fb[py[i]*BG_STRIDE+px[i]]=c;y++;if(err<0)err+=2*y+1;else{x--;err+=2*(y-x)+1;}}}
void gfx_fill_circle(uint16_t *fb,int cx,int cy,int r,uint16_t c){for(int y=-r;y<=r;y++){int x=0;while((x+1)*(x+1)+y*y<=r*r)x++;gfx_line(fb,cx-x,cy+y,cx+x,cy+y,c);}}
static uint32_t utf8_next(const unsigned char **p){uint32_t c=*(*p)++;if(c<0x80)return c;if((c&0xE0)==0xC0){uint32_t b=*(*p)++;return ((c&31)<<6)|(b&63);}if((c&0xF0)==0xE0){uint32_t b=*(*p)++,d=*(*p)++;return ((c&15)<<12)|((b&63)<<6)|(d&63);}if((c&0xF8)==0xF0){(*p)+=3;return '?';}return '?';}
void gfx_text(uint16_t *fb,int x,int y,const char *s,uint16_t c,int scale){
    const unsigned char *p=(const unsigned char*)s; while(*p){uint32_t cp=utf8_next(&p);if(cp=='\n'){y+=8*scale;x=0;continue;}if(cp<32||cp>255)cp='?';const uint8_t *g=NOISE_FONT8X8[cp-32];for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++)if(g[yy]&(1<<xx))gfx_rect(fb,x+xx*scale,y+yy*scale,scale,scale,c);x+=8*scale;}
}
int gfx_text_width(const char *s,int scale){int n=0;const unsigned char*p=(const unsigned char*)s;while(*p){utf8_next(&p);n++;}return n*8*scale;}
void gfx_blit(uint16_t *fb,int x,int y,int w,int h,const uint16_t *src,int sw,int sh){for(int yy=0;yy<h;yy++)for(int xx=0;xx<w;xx++){int sx=xx*sw/w,sy=yy*sh/h,dx=x+xx,dy=y+yy;if(dx>=0&&dx<W&&dy>=0&&dy<H)fb[dy*BG_STRIDE+dx]=src[sy*sw+sx];}}
void gfx_present(void){DC_FlushRange(top_buffer,sizeof(top_buffer));DC_FlushRange(bottom_buffer,sizeof(bottom_buffer));cothread_yield_irq(IRQ_VBLANK);dmaCopy(top_buffer,top_fb,sizeof(top_buffer));dmaCopy(bottom_buffer,bottom_fb,sizeof(bottom_buffer));}
