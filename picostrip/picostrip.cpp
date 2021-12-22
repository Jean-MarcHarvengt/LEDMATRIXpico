#define HAS_LEDSTRIP 1

#include "pico.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>
#include <cmath>
#ifdef HAS_LEDSTRIP
#include "WS2812.hpp"
#endif
#include "hardware/clocks.h"
#include "hardware/vreg.h"

#include "tft_t_dma.h"
#include "tft_font8x8.h"
#include "filesys.h"
#include "keyapi.h"

#include "gifdec.h"

static unsigned char font6x6[128][8];

static TFT_T_DMA tft;

#define LED_PIN         0
#define LED_LENGTH      512
#define BSHIFT          4

#define MATRIX_W        32
#define MATRIX_H        16
#define MATRIX_TFTDOT_W 6
#define MATRIX_TFTDOT_H 6

#define GIFDIR          "gifs/"
#define SCRIPTFILE      "script.txt"

#define BLUE            RGBVAL16(0, 0, 170)
#define LIGHT_BLUE      RGBVAL16(0, 136, 255)

typedef uint32_t matrix_pixel;
#define RGBVAL RGBVAL32
#define RR R32
#define GG G32
#define BB B32

static matrix_pixel pixels[MATRIX_W*MATRIX_H];

#define RASTER_HEIGHT  MATRIX_H


static matrix_pixel rastersR[RASTER_HEIGHT];
static matrix_pixel rastersG[RASTER_HEIGHT];
static matrix_pixel rastersB[RASTER_HEIGHT];
static matrix_pixel rastersY[RASTER_HEIGHT];
static matrix_pixel rastersM[RASTER_HEIGHT];
static matrix_pixel rastersC[RASTER_HEIGHT];

#define SINE_SAMPLES   MATRIX_W
#define SINE_AMP       2
static int16_t sinetable[SINE_SAMPLES];

#define BOUNCE_STEPS 32
#define SLIDE_STEPS  16

// Animation object type
#define AO_TEXT      0x00
#define AO_GIF       0x01
#define AO_RASTERS   0x02

// Animation object flags
#define AOF_NONE      0x00
#define AOF_SCROLL    0x01
#define AOF_BOUNCE    0x02
#define AOF_SLIDE     0x04
#define AOF_SINE      0x08 
#define AOF_FLASH     0x10
#define AOF_HCENTER   0x20 
#define AOF_VCENTER   0x40
#define AOF_HV        0x80

#define ANIME_MAX_OBJS 8 


static struct Anime {  
  uint8_t atype[ANIME_MAX_OBJS];
  void * data[ANIME_MAX_OBJS];
  matrix_pixel color[ANIME_MAX_OBJS];
  uint8_t flags[ANIME_MAX_OBJS];
  int16_t xoffset[ANIME_MAX_OBJS];
  int16_t yoffset[ANIME_MAX_OBJS];
  uint8_t speed_cnt[ANIME_MAX_OBJS];
  uint8_t speed_skp[ANIME_MAX_OBJS];
  int16_t scroll[ANIME_MAX_OBJS];
  int16_t scroll_max[ANIME_MAX_OBJS];
  int16_t angle[ANIME_MAX_OBJS];
  int16_t angle_step[ANIME_MAX_OBJS];
  uint8_t bounce_amp[ANIME_MAX_OBJS];
  uint8_t bounce_phase[ANIME_MAX_OBJS];
  int16_t intensity[ANIME_MAX_OBJS];
  int16_t intensity_step[ANIME_MAX_OBJS];
  uint8_t h[ANIME_MAX_OBJS];
  matrix_pixel bg_color;
  int nbObj;  
} anime;

#define KEYWORD_MAX    16
#define LABEL_MAX      16

static int script_nbr_seq;
static int script_cur_seq;
static char script_cur_name[LABEL_MAX];
static int script_cur_timeout_s;
static char line[512];
static char textualdata[ANIME_MAX_OBJS][484];
static int textualdata_index = 0; 

static uint32_t last_time_ms;


/************************************************
 * Convertion routines to TFT
 ************************************************/
static void draw_matrix_to_tft_dma(void) {
    for (int j=0;j<MATRIX_H;j++ ) {
        for (int l=0;l<MATRIX_TFTDOT_H;l++ ) {
            matrix_pixel * pixpt = &pixels[j*MATRIX_W];
            uint16_t * linebufpt = tft.getLineBuffer(j*7+l);
            for (int i=0;i<MATRIX_W;i++ ) {
                matrix_pixel pix = *pixpt++;
                uint16_t pix16 = RGBVAL16 ( RR(pix),GG(pix),BB(pix));
                *linebufpt++ = pix16;
                *linebufpt++ = pix16;
                *linebufpt++ = pix16;
                *linebufpt++ = pix16;
                *linebufpt++ = pix16;
                *linebufpt++ = pix16;
                *linebufpt++;
            }
        }
    }
}

// below functions not working
static void draw_matrix_to_tft(void) {
    //tft.allocate_frame_buffer();
    draw_matrix_to_tft_dma();
    uint16_t * linebufpt = tft.getLineBuffer(0);
    tft.copyBlockNoDma(0, 0, TFT_WIDTH, LINES_PER_BLOCK, linebufpt);
    linebufpt = tft.getLineBuffer(LINES_PER_BLOCK);
    tft.copyBlockNoDma(0, LINES_PER_BLOCK, TFT_WIDTH, LINES_PER_BLOCK, linebufpt);
}

static void draw_matrix_to_tft_slow(void) {
    for (int j=0;j<MATRIX_H;j++ ) {
        for (int l=0;l<MATRIX_TFTDOT_H;l++ ) {
            matrix_pixel * pixpt = &pixels[j*MATRIX_W];
            for (int i=0;i<MATRIX_W;i++ ) {
                tft.drawRectNoDma(i*7, j*7+l, MATRIX_TFTDOT_W, MATRIX_TFTDOT_H, *pixpt++);
            }
        }
    }
}


/************************************************
 * MATRIX draw routines 
 ************************************************/
static void fill_matrix(matrix_pixel pixcolor) {
    for (int j=0;j<MATRIX_H;j++ ) {
        for (int i=0;i<MATRIX_W;i++ ) {
            pixels[j*MATRIX_W+i] = pixcolor;
        }
    }
}

static void draw_text(const char * text, matrix_pixel pixcolor, int char_w, int char_h, int scroll, int y_offset, bool sinescroll) {
    uint16_t textleninpix = strlen(text)*8;
    if (scroll <= 0) return;
    if (scroll >= (textleninpix + MATRIX_W)) return;
    int yoff = y_offset;
    int textoff =  MATRIX_W - scroll;
    int outfirstcol = textoff;
    int infirstcol  = 0;
    if (textoff < 0) {
        textoff = -textoff;
        outfirstcol = 0;
        text += (textoff/char_w);
        infirstcol = textoff%char_w;
    }
    uint16_t c;
    while ((c = *text++)) {
        unsigned char * charpt;
        if (char_h >= 7) charpt=(unsigned char *)&font8x8[c][0];
        else charpt=&font6x6[c][0];
        for (int i=infirstcol;i<char_w;i++)
        {
            if (sinescroll) yoff = y_offset + (2*sinetable[outfirstcol])/SINE_AMP;
            uint8_t mask = 1<<i;
            for (int j=0;j<char_h;j++) {
                int ypos = yoff+j;
                if (ypos < 0) continue;
                if (ypos >= MATRIX_H) break;
                if (mask & charpt[j]) pixels[ypos*MATRIX_W+outfirstcol]=pixcolor;
            }
            outfirstcol++;
            if (outfirstcol == MATRIX_W) return;
        }
        infirstcol = 0; 
    }           
}

static void draw_rasters(matrix_pixel * rasters, int height, int y_offset, int intensity, bool sinescroll) {
    int yoff = y_offset;
    for (int i=0;i<MATRIX_W;i++)
    {
        if (sinescroll) yoff = y_offset + (2*sinetable[i])/SINE_AMP;
        for (int j=0;j<height;j++) {
            int ypos = yoff+j;
            if (ypos < 0) continue;
            if (ypos >= MATRIX_H) break;
            pixels[ypos*MATRIX_W+i]=rasters[(j*RASTER_HEIGHT)/height];
        }
    }

}

static void drawbitmap(gd_GIF *gif, int x_offset, int y_offset, bool sinescroll) {
    
    if (x_offset <= 0) return;
    if (x_offset >= (gif->width + MATRIX_W)) return;
    int yoff = y_offset;
    int xoff =  MATRIX_W - x_offset;
    int outfirstcol = xoff;
    int infirstcol  = 0;
    if (xoff < 0) {
        xoff = -xoff;
        outfirstcol = 0;
        infirstcol = xoff;
    }
    
    uint8_t index, *color;
    for (int i=infirstcol;i<gif->width;i++)
    {
        if (sinescroll) yoff = y_offset + (2*sinetable[outfirstcol])/SINE_AMP;
        for (int j=0;j<gif->height;j++) {
            int ypos = yoff+j;
            if (ypos < 0) continue;
            if (ypos >= MATRIX_H) break;
            index = gif->frame[(gif->fy + j) * gif->width + gif->fx + i];
            if (!gif->gce.transparency || index != gif->gce.tindex) {
                color = &gif->palette->colors[index*3];
                uint8_t r = *color++;
                uint8_t g = *color++;
                uint8_t b = *color++;
                pixels[ypos*MATRIX_W+outfirstcol] = RGBVAL(r,g,b);
            }
        }
        
        outfirstcol++;
        if (outfirstcol == MATRIX_W) return;
    }           
    
}

/************************************************
 * ANIMATION handlers 
 ************************************************/
static void animNew(matrix_pixel color) {
    anime.bg_color = color;
    anime.nbObj = 0;
}


static void animInitRaster(matrix_pixel * raspt, matrix_pixel color) {
    uint8_t r,g,b;
    for (int i=0; i<RASTER_HEIGHT; i++) {
        int intensity = (255*i)/RASTER_HEIGHT;
        if (i>=RASTER_HEIGHT/2)
            intensity = 255-intensity; 
        r = ((color>>16 & 0xff)*intensity)>>8;
        g = ((color>>8 & 0xff)*intensity)>>8;
        b = ((color & 0xff)*intensity)>>8;
        *raspt++ = RGBVAL(r,g,b);
    }
}

static void animCommonSetup(int8_t id, int16_t xoff, int16_t yoff, uint16_t width, uint16_t height, uint8_t spdskip, uint8_t flags, uint8_t amp, uint8_t phase) {
    anime.flags[id]        = flags;
    anime.bounce_amp[id]   = amp;
    anime.bounce_phase[id] = phase;
    anime.speed_cnt[id]    = 0;
    anime.speed_skp[id]    = spdskip;
    anime.scroll[id]       = 0;

    if (flags & AOF_HCENTER) {
        anime.xoffset[id] = (MATRIX_W - width)/2+width;
    }
    else {
        anime.xoffset[id] = xoff;
    }
    if (flags & AOF_VCENTER) {
        anime.yoffset[id] = (MATRIX_H - height)/2;
    }
    else {
        anime.yoffset[id] = yoff;
    }

    if (flags & AOF_SCROLL) {
        if (anime.flags[id] & AOF_HV) {
            anime.yoffset[id] -= height;
            anime.scroll_max[id] = height+MATRIX_H*2;
        }
        else {
            anime.scroll_max[id] = width+MATRIX_W*2;
        }
    }
    else if (flags & AOF_SLIDE) {
        if (anime.flags[id] & AOF_HV) {
            anime.yoffset[id] -= height;
            anime.scroll_max[id] = (MATRIX_H - height)/2-anime.yoffset[id];
        }
        else {
            anime.scroll_max[id] = (MATRIX_W - width)/2+width;
        }
    }

    anime.intensity[id] = 0xff;
    if (flags & AOF_FLASH) {
        anime.intensity_step[id] = 8;
    }
    else {
        anime.intensity_step[id] = 0;
    } 

    anime.angle[id] = 0;
    if (flags & AOF_BOUNCE) {
        anime.angle_step[id] = 1;
    }
    else {
        anime.angle_step[id] = 0;
    }
}

static void animAddText(const char * text, uint8_t fntheight, int16_t xoff, int16_t yoff, matrix_pixel color, uint8_t spdskip, uint8_t flags, uint8_t amp, uint8_t phase) {
    if ( anime.nbObj < ANIME_MAX_OBJS ) {
        anime.atype[anime.nbObj]        = AO_TEXT;
        anime.data[anime.nbObj]         = (void*)text;
        anime.color[anime.nbObj]        = color;
        anime.h[anime.nbObj]            = fntheight;
        animCommonSetup(anime.nbObj, xoff, yoff, strlen(text)*8, fntheight, spdskip, flags, amp, phase);
        anime.nbObj++;
    }
}

static void animAddRaster(const matrix_pixel * rasters, uint8_t height, int16_t xoff, int16_t yoff, uint8_t spdskip, uint8_t flags, uint8_t amp, uint8_t phase) {
    if ( anime.nbObj < ANIME_MAX_OBJS ) {        
        anime.atype[anime.nbObj] = AO_RASTERS;
        anime.data[anime.nbObj]  = (void*)rasters;
        anime.h[anime.nbObj]     = height;
        animCommonSetup(anime.nbObj, xoff, yoff, 0, height, spdskip, flags, amp, phase);
        anime.nbObj++;
    }
}


static void animAddGif(const char * filename, int16_t xoff, int16_t yoff, uint8_t spdskip, uint8_t flags, uint8_t amp, uint8_t phase) {
    if ( anime.nbObj < ANIME_MAX_OBJS ) {        
        char filepath[64];
        strcpy(filepath, GIFDIR);
        strcat(filepath, filename);
        anime.atype[anime.nbObj] = AO_GIF;
        gd_GIF *gif = gd_open_gif(filepath);
        if (gif) {
            gd_get_frame(gif);
            anime.data[anime.nbObj] = (void*)gif;
            animCommonSetup(anime.nbObj, xoff, yoff, gif->width, gif->height, spdskip, flags, amp, phase);
            anime.nbObj++;
        }
    }
}

static void handleAnims(void) {
    uint8_t r,g,b;
    fill_matrix(anime.bg_color);
    for (int o=0;o<anime.nbObj; o++) {
        int16_t xoff = anime.xoffset[o];
        int16_t yoff = anime.yoffset[o];
        if (anime.flags[o] & AOF_SCROLL) {
            if (anime.flags[o] & AOF_HV) {
                yoff += anime.scroll[o];
            }
            else {
                xoff += anime.scroll[o];
            }    
        }
        if (anime.flags[o] & AOF_BOUNCE) {
            if (anime.flags[o] & AOF_HV) {
                xoff += anime.bounce_amp[o]*sin((2*3.1416*((anime.angle[o]+anime.bounce_phase[o])&(BOUNCE_STEPS-1)))/BOUNCE_STEPS);
            }
            else {
                yoff += anime.bounce_amp[o]*sin((2*3.1416*((anime.angle[o]+anime.bounce_phase[o])&(BOUNCE_STEPS-1)))/BOUNCE_STEPS);
            }    
        }
        if (anime.flags[o] & AOF_SLIDE) {
            if (anime.flags[o] & AOF_HV) {
                yoff += anime.scroll_max[o]*sin((3.1416/2*anime.scroll[o])/SLIDE_STEPS);
            }
            else {
                xoff += anime.scroll_max[o]*sin((3.1416/2*anime.scroll[o])/SLIDE_STEPS);
            }    
        }
        switch(anime.atype[o]) {
            case AO_TEXT:
                r = ((anime.color[o]>>16 & 0xff)*anime.intensity[o])>>8;
                g = ((anime.color[o]>>8 & 0xff)*anime.intensity[o])>>8;
                b = ((anime.color[o] & 0xff)*anime.intensity[o])>>8;
                draw_text((const char *)anime.data[o], RGBVAL(r,g,b), 8,anime.h[o], xoff, yoff, ((anime.flags[o] & AOF_SINE)?true:false));
                break;

            case AO_GIF:
                gd_GIF * gif; 
                gif = (gd_GIF *)anime.data[o];
                //if (gd_get_frame(gif)) {
                drawbitmap(gif, xoff, yoff, ((anime.flags[o] & AOF_SINE)?true:false));                 
                break;

            case AO_RASTERS:
                r = ((anime.color[o]>>16 & 0xff)*anime.intensity[o])>>8;
                g = ((anime.color[o]>>8 & 0xff)*anime.intensity[o])>>8;
                b = ((anime.color[o] & 0xff)*anime.intensity[o])>>8;
                draw_rasters((matrix_pixel *)anime.data[o], anime.h[o], yoff, anime.intensity[o], ((anime.flags[o] & AOF_SINE)?true:false));                
                break;

        }

        if (anime.speed_cnt[o] == 0) {
            anime.speed_cnt[o] = anime.speed_skp[o];

            if (anime.flags[o] & AOF_SCROLL) {
                anime.scroll[o] += 1;
                if (anime.scroll[o] >= anime.scroll_max[o]) 
                {
                    anime.scroll[o] = 0;
                    //anime.intensity[o] = 0xff;
                    //anime.intensity_step[o] = 0x8;  
                } 
            }
            else if (anime.flags[o] & AOF_SLIDE) {
                if (anime.scroll[o] < SLIDE_STEPS  ) 
                {
                    anime.scroll[o] += 1;
                } 
            }

            anime.intensity[o] += anime.intensity_step[o];
            if (anime.intensity[o] > 0xff) {
             anime.intensity_step[o] = -anime.intensity_step[o];
             anime.intensity[o] += anime.intensity_step[o];
            }
            else if (anime.intensity[o] < 0x20) {
             anime.intensity_step[o] = -anime.intensity_step[o];
             anime.intensity[o] += anime.intensity_step[o];
            }

            anime.angle[o] += anime.angle_step[o];
            anime.angle[o] &= (BOUNCE_STEPS-1);

        }
        else {
            anime.speed_cnt[o] -= 1;    
        }
    }
}

static void closeAnims(void) {
    for (int o=0;o<anime.nbObj; o++) {
        switch(anime.atype[o]) {
            case AO_TEXT:
                break;
            case AO_GIF:
                gd_GIF * gif; 
                gif = (gd_GIF *)anime.data[o];   
                if (gif) {
                    gd_close_gif(gif);
                }
                break;
            case AO_RASTERS:
                break;
        }
        anime.data[o] = NULL;
    }
}

/************************************************
 * SCRIPT parser 
 ************************************************/
static int initScript(void) {
    script_nbr_seq = 0;
    char keyword[KEYWORD_MAX];
    char c;
    int i;   
    int size = emu_FileSize(SCRIPTFILE);  
    int fd = emu_FileOpen(SCRIPTFILE);
    if (fd >=0)  {
        printf("file opened\n"); 
        i = 0;

        //while ((c = emu_FileGetc(fd) ) != EOF) {
        while (size>0) {
            c = emu_FileGetc(fd);
            size--;
            //printf("0x%02x %c\n",c,c);
            if (c == '\n') { 
                line[i] = 0;
                if (strlen(&line[0])) {
                    //printf("%s\n",&line[0]);
                    sscanf(&line[0], "%s", &keyword[0]); 
                    int kewordlen = strlen(&keyword[0]);
                    if (!strncmp("begin",keyword,kewordlen)) {
                        script_nbr_seq++;
                    }
                }
                i = 0;  
            }
            else { 
                line[i++] = c;
            }
        }  

        emu_FileClose(fd);
    }

    return (script_nbr_seq);
}

static void loadScriptSeq(int seq) {
    closeAnims();    
    int fd = emu_FileOpen(SCRIPTFILE);
    if (fd >=0)  {
        char keyword[KEYWORD_MAX];
        int cur_seq = 0;       
        char c;
        int i;         
        animNew(RGBVAL(0x00,0x00,0x00));
        printf("file opened\n"); 
        i = 0;
        while ((c = emu_FileGetc(fd) ) != EOF) {
            //printf("0x%02x %c\n",c,c);
            if (c == '\n') { 
                line[i] = 0;
                if (strlen(&line[0])) {
                    int xoff;
                    int yoff;
                    int speed;
                    int amp;
                    int phase;
                    int opts;                    
                    //printf("%s\n",&line[0]);
                    sscanf(&line[0], "%s", &keyword[0]); 
                    int kewordlen = strlen(&keyword[0]);
                    //printf("%s %d\n",&keyword[0], kewordlen);
                    if (!strncmp("//",keyword,kewordlen)) {
                        //printf("comment\n");
                    }
                    else if (!strncmp("begin",keyword,kewordlen)) {
                        if  (cur_seq == seq) {
                            sscanf(&line[0], "%s %s", &keyword[0], &script_cur_name[0]);                      
                            //printf("begin %s\n", &script_cur_name[0]);
                            textualdata_index = 0;
                        }
                    }
                    else if (!strncmp("end",keyword,kewordlen)) { 
                        if (cur_seq == seq ) {
                            sscanf(&line[0], "%s %d", &keyword[0], &script_cur_timeout_s);                      
                            //printf("end %d\n", script_cur_timeout_s);
                            break;
                        }
                        else {
                            cur_seq++;
                        }     
                    }
                    else if ( (!strncmp("text",keyword,kewordlen)) && (cur_seq == seq) ) {
                        int fnth;
                        int r,g,b;
                        int nbparam = sscanf(&line[0], "%s %d, %d,%d, %d, 0x%02x, %d,%d,%d,%d,%d,%[^\n]", &keyword[0], &fnth, &xoff,&yoff,&speed, &opts, &r,&g,&b, &amp, &phase, &textualdata[textualdata_index][0]);                      
                        animAddText(&textualdata[textualdata_index][1],fnth, xoff,yoff, RGBVAL(r,g,b), speed, opts, amp, phase);
                        textualdata_index++;
                    }
                    else if ( (!strncmp("gif",keyword,kewordlen)) && (cur_seq == seq) ){ 
                        int nbparam = sscanf(&line[0], "%s %d,%d ,%d, 0x%02x, %d,%d,%[^\n]", &keyword[0], &xoff,&yoff, &speed, &opts, &amp, &phase, &textualdata[textualdata_index][0]);
                        animAddGif(&textualdata[textualdata_index][1], xoff,yoff, speed, opts, amp, phase);
                        textualdata_index++;
                    }
                    else if ( (!strncmp("raster",keyword,kewordlen)) && (cur_seq == seq) ) { 
                        int height;
                        int nbparam = sscanf(&line[0], "%s %d, %d,%d, %d, 0x%02x ,%d,%d,%[^\n]", &keyword[0], &height, &xoff,&yoff, &speed, &opts, &amp, &phase, &textualdata[textualdata_index][0]);
                        matrix_pixel* rasters = rastersB;
                        switch(textualdata[textualdata_index][1]) {
                            case 'R':
                                rasters = rastersR;
                                break;
                            case 'G':
                                rasters = rastersG;
                                break;
                            case 'Y':
                                rasters = rastersY;
                                break;
                            case 'M':
                                rasters = rastersM;
                                break;
                            case 'C':
                                rasters = rastersC;
                                break;
                            case 'B':
                            default:
                                break;
                        }
                        animAddRaster(rasters, height, xoff,yoff, speed, opts, amp, phase);
                        textualdata_index++;
                    }
                }
                i = 0;  
            }
            else { 
                line[i++] = c;
            }
        }  
        emu_FileClose(fd);
        tft.fillScreen( RGBVAL16(0x00,0x00,0x00) );
        tft.drawText(8*12,128,script_cur_name,BLUE,LIGHT_BLUE,true);
        last_time_ms = to_ms_since_boot (get_absolute_time());
    }
}

/************************************************
 * MAIN 
 ************************************************/
int main(void) {
//    vreg_set_voltage(VREG_VOLTAGE_1_05);
    set_sys_clock_khz(125000, true);    
//    set_sys_clock_khz(150000, true);    
//    set_sys_clock_khz(133000, true);    
//    set_sys_clock_khz(200000, true);    
//    set_sys_clock_khz(210000, true);    
//    set_sys_clock_khz(230000, true);    
//    set_sys_clock_khz(225000, true);    
//    set_sys_clock_khz(250000, true);  
    stdio_init_all();

    printf("starting...\n");   

    tft.begin();
#ifdef PICOMPUTER
#else
    tft.flipscreen(false);    
    emu_InitJoysticks();
#endif    
    emu_FilesysInit();

    tft.fillScreenNoDma( RGBVAL16(0x00,0x00,0x00) );
    tft.startDMA();
    tft.fillScreen( RGBVAL16(0x00,0x00,0x00) );

#ifdef HAS_LEDSTRIP
    WS2812 ledStrip(
        LED_PIN,            // Data line is connected to pin 0. (GP0)
        LED_LENGTH,         // Strip is 6 LEDs long.
        pio0,               // Use PIO 0 for creating the state machine.
        0,                  // Index of the state machine that will be created for controlling the LED strip
                            // You can have 4 state machines per PIO-Block up to 8 overall.
                            // See Chapter 3 in: https://datasheets.raspberrypi.org/rp2040/rp2040-datasheet.pdf
        WS2812::FORMAT_GRB  // Pixel format used by the LED strip
    );
#endif

    // 6x6 font generation
    for (int c=0;c<128;c++) {
        unsigned char * fontsrc=(unsigned char *)&font8x8[c][0];
        unsigned char * fontdst=&font6x6[c][0];
        *fontdst++ = *fontsrc++;
        fontsrc++;
        *fontdst++ = *fontsrc++;
        *fontdst++ = *fontsrc++;
        *fontdst++ = *fontsrc++;
        fontsrc++;
        *fontdst++ = *fontsrc++;
        *fontdst++ = 0x00; 
        *fontdst++ = 0x00; 
        *fontdst++ = 0x00; 
    }


    int a=0;
    for (int i=0;i<MATRIX_W;i++) {
        sinetable[i] = SINE_AMP*sin((6.2832*a)/SINE_SAMPLES);
        a += 1;
        a &= SINE_SAMPLES-1;
    }    
 

    animInitRaster(rastersR, RGBVAL(0xff,0x00,0x00));
    animInitRaster(rastersG, RGBVAL(0x00,0xff,0x00));
    animInitRaster(rastersB, RGBVAL(0x00,0x00,0xff));
    animInitRaster(rastersY, RGBVAL(0xff,0xff,0x00));
    animInitRaster(rastersC, RGBVAL(0x00,0xff,0xff));
    animInitRaster(rastersM, RGBVAL(0xff,0x00,0xff));

    anime.nbObj = 0;
    animNew(RGBVAL(0x00,0x00,0x00));

    initScript();
    if (script_nbr_seq) {
        loadScriptSeq(script_cur_seq);
    }
    else { 

        animAddRaster(rastersB, 16, 0,0, 0, AOF_VCENTER, 0,0);
        animAddText("Stop dumping your trash in front of the house!!!",8, 0,0, RGBVAL(0xff,0xff,0xff), 0, AOF_SCROLL, 0,0);   
        animAddText("GO!!",8, 0,8, RGBVAL(0xff,0xff,0), 0, AOF_SLIDE|AOF_FLASH, 0,0);   

        //animAddRaster(rastersG, 8, 0,0, 0, AOF_SINE|AOF_VCENTER, 0,0);
        //animAddText("Let's go!",8, 0,0, RGBVAL(0,200,200), 0, AOF_SLIDE|AOF_FLASH|AOF_VCENTER, 0,0);   
        
        //animAddGif("tree32.gif", 0,0, 1, AOF_SCROLL|AOF_HCENTER|AOF_BOUNCE|AOF_HV, 4, 0);
        //animAddGif("snowmankiss32.gif", 0,0, 2, AOF_SCROLL|AOF_HCENTER|AOF_BOUNCE|AOF_HV, 4, 0);
        //animAddGif("snowman128.gif", 0,0, 1, AOF_SCROLL|AOF_VCENTER|AOF_BOUNCE, 16, 0);

        /*
        animAddRaster(rastersG, 8, 0,0, 0, AOF_SINE, 0,0);
        animAddGif("pernotrans150.gif", 0,0, 0, AOF_SCROLL|AOF_VCENTER|AOF_BOUNCE|AOF_SINE, 4, 0);
        animAddText("NO TRASH, NO BAG!",8, 0,0, RGBVAL(0xff,0xff,0xff), 0, AOF_SCROLL, 0,0);   
        */

        /*
        //animAddGif("perno150.gif", 0,0, 0, AOF_HCENTER|AOF_SLIDE|AOF_HV, 0, 0);
        animAddText("AB",5, 0,0, RGBVAL(0xff,0xff,0xff), 0, AOF_SLIDE|AOF_FLASH, 0,0);   
        animAddText("ABC",5, 0,5, RGBVAL(0xff,0xff,0xff), 0, AOF_SLIDE|AOF_FLASH, 0,0);   
        animAddText("ABCD",5, 0,10, RGBVAL(0xff,0xff,0xff), 0, AOF_SLIDE|AOF_FLASH, 0,0);   
        //animAddText("ABCDEFGH",8, 0,0, RGBVAL(0xff,0xff,0x00), 0, AOF_HCENTER|AOF_VCENTER, 0,0);   
        //animAddText("ABCDEFGH",8, 0,0, RGBVAL(0xff,0xff,0x00), 0, AOF_HCENTER|AOF_SLIDE|AOF_HV, 0,0);   
        //animAddGif("potion16.gif", 0,0, 0, AOF_HCENTER|AOF_VCENTER, 0, 0);
        //animAddGif("potion16.gif", 0,0, 0, AOF_HCENTER|AOF_SLIDE|AOF_HV, 0, 0);
        */

        /*
        animAddRaster(rastersR, 2, 0,4, 0, AOF_BOUNCE, 8,0);
        animAddRaster(rastersG, 2, 0,4, 0, AOF_BOUNCE, 8,2);
        animAddRaster(rastersB, 2, 0,4, 0, AOF_BOUNCE, 8,4);
        animAddRaster(rastersC, 2, 0,4, 0, AOF_BOUNCE, 8,6);
        animAddRaster(rastersR, 2, 0,4, 0, AOF_BOUNCE, 8,8);
        animAddRaster(rastersG, 2, 0,4, 0, AOF_BOUNCE, 8,10);
        animAddRaster(rastersB, 2, 0,4, 0, AOF_BOUNCE, 8,12);
        animAddRaster(rastersY, 2, 0,4, 0, AOF_BOUNCE, 8,14);
        */
        /*
        animAddRaster(rastersR, 4, 0,4, 0, AOF_BOUNCE, 8,0);
        animAddRaster(rastersG, 4, 0,4, 0, AOF_BOUNCE, 8,4);
        animAddRaster(rastersB, 4, 0,4, 0, AOF_BOUNCE, 8,8);
        animAddGif("potion16.gif", 0,0, 0, AOF_SCROLL|AOF_BOUNCE|AOF_VCENTER, 4, 0);
        animAddGif("coin16.gif", -16,0, 0, AOF_SCROLL|AOF_BOUNCE|AOF_VCENTER, 4, 4);
        animAddGif("crystal16.gif", -32,0, 0, AOF_SCROLL|AOF_BOUNCE|AOF_VCENTER, 4, 8);
        animAddGif("candy16.gif", -48,0, 0, AOF_SCROLL|AOF_BOUNCE|AOF_VCENTER, 4, 12);
        animAddGif("present16.gif", -64,0, 0, AOF_SCROLL|AOF_BOUNCE|AOF_VCENTER, 4, 14);
        */
        /*
        animAddGif("perno150.gif", 0,-16, 2, AOF_SCROLL|AOF_BOUNCE, 16,0);
        //animAddRaster(rastersG, 8, 0,0, 3, AOF_SINE, 0,0);
        //animAddRaster(rastersR, 8, 0,4, 2, AOF_BOUNCE|AOF_FLASH, 16,0);
        //animAddRaster(rastersB, 8, 0,8, 1, AOF_BOUNCE|AOF_FLASH, 16,0);
        animAddText("NO TRASH, NO BAG!",5, 0,2, RGBVAL(0xff,0xff,0xff), 2, AOF_SCROLL|AOF_SINE, 8,0);   
        animAddText("SALUT LES MILOUTES ... ",5, 0,10, RGBVAL(0xff,0xff,0x00), 4, AOF_SCROLL|AOF_FLASH, 0,0);
        */
    }    
    last_time_ms = to_ms_since_boot (get_absolute_time());

    while (true) {
#ifdef PICOMPUTER
#else
        // handle keys
        if (script_nbr_seq) {
            uint16_t bClick = emu_DebounceLocalKeys();
            if (bClick & MASK_JOY2_UP) {
                if (script_cur_seq == 0 ) {
                    script_cur_seq = script_nbr_seq-1;
                }
                else {
                    script_cur_seq--;
                }
                loadScriptSeq(script_cur_seq);
            }
            else if (bClick & MASK_JOY2_DOWN) {
                script_cur_seq++;
                if (script_cur_seq == script_nbr_seq ) {
                    script_cur_seq = 0;
                }  
                loadScriptSeq(script_cur_seq);
            }
            else if (bClick & MASK_JOY2_BTN) {
            }
        }
#endif
        // handle anims
        handleAnims();

        // update screens
        draw_matrix_to_tft_dma();
#ifdef HAS_LEDSTRIP        
        // draw_matrix_to_leds
        int ledIndex = 0;
        for (int j=MATRIX_H-1;j>=0;j-- ) {
            matrix_pixel * pixpt = &pixels[j*MATRIX_W];
            if (j & 1) { 
                for (int i=0;i<MATRIX_W;i++ ) {
                    matrix_pixel rgb = pixpt[i];
                    ledStrip.setPixelColor(ledIndex++, WS2812::RGB(RR(rgb)>>BSHIFT, GG(rgb)>>BSHIFT, BB(rgb)>>BSHIFT));
                }
            }
            else { 
                for (int i=MATRIX_W-1;i>=0;i-- ) {
                    matrix_pixel rgb = pixpt[i];
                    ledStrip.setPixelColor(ledIndex++, WS2812::RGB(RR(rgb)>>BSHIFT, GG(rgb)>>BSHIFT, BB(rgb)>>BSHIFT));
                }
            }

        }
        ledStrip.show();
#endif        
        sleep_ms(20);

        if (script_nbr_seq) {
            uint32_t time_ms = to_ms_since_boot (get_absolute_time());
            if ( time_ms > (last_time_ms + script_cur_timeout_s*1000) ) {
                last_time_ms = time_ms;
                script_cur_seq++;
                if (script_cur_seq == script_nbr_seq ) {
                    script_cur_seq = 0;
                }  
                loadScriptSeq(script_cur_seq);
            }
        }
    }


    // TEST
    // Set half LEDs to red and half to blue!
    //ledStrip.fill( WS2812::RGB(255, 0, 0), 0, LED_LENGTH / 2 );
    //ledStrip.fill( WS2812::RGB(0, 0, 255), LED_LENGTH / 2 );
    //ledStrip.show();
}






