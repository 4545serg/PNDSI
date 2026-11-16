#pragma once
#include <nds.h>
#include <stdint.h>
#include <stdbool.h>
void gfx_init(void);
uint16_t *gfx_top(void);
uint16_t *gfx_bottom(void);
void gfx_clear(uint16_t *fb, uint16_t color);
void gfx_rect(uint16_t *fb,int x,int y,int w,int h,uint16_t c);
void gfx_round(uint16_t *fb,int x,int y,int w,int h,int r,uint16_t c);
void gfx_line(uint16_t *fb,int x0,int y0,int x1,int y1,uint16_t c);
void gfx_circle(uint16_t *fb,int cx,int cy,int r,uint16_t c);
void gfx_fill_circle(uint16_t *fb,int cx,int cy,int r,uint16_t c);
void gfx_text(uint16_t *fb,int x,int y,const char *s,uint16_t c,int scale);
int gfx_text_width(const char *s,int scale);
void gfx_blit(uint16_t *fb,int x,int y,int w,int h,const uint16_t *src,int sw,int sh);
void gfx_present(void);
