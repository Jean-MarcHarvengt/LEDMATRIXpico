#ifndef IOPINS_H
#define IOPINS_H

#include "platform_config.h"

//#define PICOMPUTER      1

// TFT
#define TFT_SPIREG      spi0
#define TFT_SPIDREQ     DREQ_SPI0_TX
#define TFT_SCLK        18
#define TFT_MOSI        19
#define TFT_MISO        255 // Not required, used for DC... 
#define TFT_DC          16
#define TFT_RST         21 //255
#define TFT_CS          255
#define TFT_BACKLIGHT   20


// SD (see SPI0 in code!!!)
#define SD_SPIREG       spi1
#ifdef PICOMPUTER
#define SD_SCLK         10
#define SD_MOSI         11
#else
#define SD_SCLK         14
#define SD_MOSI         15
#endif
#define SD_MISO         12 
#define SD_CS           13
#define SD_DETECT       255 // 22

#define PIN_JOY2_1      9  // UP (PREV)
#define PIN_JOY2_BTN    8  // OK
#define PIN_JOY2_2      7  // DOWN (NEXT)

#endif
