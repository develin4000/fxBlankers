/*
->====================================================<-
->= fxBlankers - DotFlag - © Copyright 2023 OnyxSoft =<-
->====================================================<-
->= Version  : 1.3                                   =<-
->= File     : blanker.c                             =<-
->= Author   : Stefan Blixth                         =<-
->= Compiled : 2023-07-10                            =<-
->====================================================<-

Based on retro-demoeffect dotflag source by Johan Gardhage : https://github.com/johangardhage/retro-demoeffects/blob/master/src/dotflag.cpp
and the "Simple blanker example" by Guido Mersmann and Michal Wozniak in the MorphOS SDK.
*/

#define  VERSION  1
#define  REVISION 3

#define BLANKERLIBNAME  "dotflag.btd"
#define BLANKERNAME     "DotFlag"
#define AUTHORNAME      "Stefan Blixth, OnyxSoft"
#define DESCRIPTION     "Flag effect"
#define BLANKERID       MAKE_ID('F','L','A','G')

/***************************************************************************/

#include <cybergraphx/cybergraphics.h>
#include <dos/dos.h>
#include <libraries/iffparse.h>
#include <libraries/query.h>
#include <utility/hooks.h>
#include <utility/tagitem.h>
#include <intuition/blankerapi.h>
#include <graphics/rpattr.h>
#include <intuition/intuitionbase.h>

#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/timer.h>
#include <proto/graphics.h>
#include <proto/utility.h>
#include <proto/cybergraphics.h>
#include <proto/random.h>
#include <math.h>

//#define  USEDEBUG 1
#include "debug.h"

#define I2S(x) I2S2(x)
#define I2S2(x) #x

#define DEF_FLAGDELAY     0x03  // 100ms
char *DEF_CYDELAY[] = {"25", "50", "75", "100", "125", "150", "175", "200", "250", NULL};

#define DEF_FLAGTHEME   0x00 // Flag
char *DEF_CYTHEME[] = {"Sweden", "Finland", "Denmark", "Norway", "Germany", "Ukraine", "Poland", "France", "Italy", "Greek", NULL};

// blanker settings window definition
#define nTAG(o)          (BTD_Client + (o))
#define MYTAG_FLAGDELAY  nTAG(0)
#define MYTAG_FLAGTHEME  nTAG(1)

#define DATA_WIDTH   320
#define DATA_HEIGHT  200
#define DATA_SIZE    DATA_WIDTH * DATA_HEIGHT
#define RENDER_RGB   3
#define RENDER_RGBA  4

#define FLAG_CURVE   125
#define FLAG_SIZE    4
#define FLAG_WIDTH   16
#define FLAG_HEIGHT  10
#define SINE_VALUES  255


unsigned long SwedishFlag[FLAG_HEIGHT][FLAG_WIDTH] = {
   {0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xfffcff33, 0xfffcff33, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff},
   {0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xfffcff33, 0xfffcff33, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff},
   {0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xfffcff33, 0xfffcff33, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff},
   {0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xfffcff33, 0xfffcff33, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff},
   {0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33},
   {0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33},
   {0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xfffcff33, 0xfffcff33, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff},
   {0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xfffcff33, 0xfffcff33, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff},
   {0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xfffcff33, 0xfffcff33, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff},
   {0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xfffcff33, 0xfffcff33, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff}
};

unsigned long FinnishFlag[FLAG_HEIGHT][FLAG_WIDTH] = {
   {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xff005dff, 0xff005dff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff},
   {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xff005dff, 0xff005dff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff},
   {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xff005dff, 0xff005dff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff},
   {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xff005dff, 0xff005dff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff},
   {0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff},
   {0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff},
   {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xff005dff, 0xff005dff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff},
   {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xff005dff, 0xff005dff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff},
   {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xff005dff, 0xff005dff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff},
   {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xff005dff, 0xff005dff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff}
};

unsigned long NorwegianFlag[FLAG_HEIGHT][FLAG_WIDTH] = {
   {0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffffffff, 0xff005dff, 0xffffffff, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000},
   {0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffffffff, 0xff005dff, 0xffffffff, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000},
   {0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffffffff, 0xff005dff, 0xffffffff, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000},
   {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xff005dff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff},
   {0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff},
   {0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff},
   {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xff005dff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff},
   {0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffffffff, 0xff005dff, 0xffffffff, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000},
   {0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffffffff, 0xff005dff, 0xffffffff, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000},
   {0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffffffff, 0xff005dff, 0xffffffff, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000}
};

unsigned long DanishFlag[FLAG_HEIGHT][FLAG_WIDTH] = {
   {0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffffffff, 0xffffffff, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e},
   {0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffffffff, 0xffffffff, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e},
   {0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffffffff, 0xffffffff, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e},
   {0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffffffff, 0xffffffff, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e},
   {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff},
   {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff},
   {0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffffffff, 0xffffffff, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e},
   {0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffffffff, 0xffffffff, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e},
   {0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffffffff, 0xffffffff, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e},
   {0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffffffff, 0xffffffff, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e, 0xffc8102e}
};

unsigned long PolishFlag[FLAG_HEIGHT][FLAG_WIDTH] = {
   {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff},
   {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff},
   {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff},
   {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff},
   {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff},   
   {0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000},
   {0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000},
   {0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000},
   {0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000},
   {0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000}
};

unsigned long UkrainanFlag[FLAG_HEIGHT][FLAG_WIDTH] = {
   {0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff},
   {0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff},
   {0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff},
   {0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff},
   {0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff, 0xff005dff},
   {0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33},
   {0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33},
   {0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33},
   {0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33},
   {0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33, 0xfffcff33} 
};

unsigned long GermanFlag[FLAG_HEIGHT][FLAG_WIDTH] = {
   {0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000},
   {0xff363636, 0xff363636, 0xff363636, 0xff363636, 0xff363636, 0xff363636, 0xff363636, 0xff363636, 0xff363636, 0xff363636, 0xff363636, 0xff363636, 0xff363636, 0xff363636, 0xff363636, 0xff363636},
   {0xff363636, 0xff363636, 0xff363636, 0xff363636, 0xff363636, 0xff363636, 0xff363636, 0xff363636, 0xff363636, 0xff363636, 0xff363636, 0xff363636, 0xff363636, 0xff363636, 0xff363636, 0xff363636},
   {0xff363636, 0xff363636, 0xff363636, 0xff363636, 0xff363636, 0xff363636, 0xff363636, 0xff363636, 0xff363636, 0xff363636, 0xff363636, 0xff363636, 0xff363636, 0xff363636, 0xff363636, 0xff363636},
   {0xffdd0000, 0xffdd0000, 0xffdd0000, 0xffdd0000, 0xffdd0000, 0xffdd0000, 0xffdd0000, 0xffdd0000, 0xffdd0000, 0xffdd0000, 0xffdd0000, 0xffdd0000, 0xffdd0000, 0xffdd0000, 0xffdd0000, 0xffdd0000},
   {0xffdd0000, 0xffdd0000, 0xffdd0000, 0xffdd0000, 0xffdd0000, 0xffdd0000, 0xffdd0000, 0xffdd0000, 0xffdd0000, 0xffdd0000, 0xffdd0000, 0xffdd0000, 0xffdd0000, 0xffdd0000, 0xffdd0000, 0xffdd0000},
   {0xffdd0000, 0xffdd0000, 0xffdd0000, 0xffdd0000, 0xffdd0000, 0xffdd0000, 0xffdd0000, 0xffdd0000, 0xffdd0000, 0xffdd0000, 0xffdd0000, 0xffdd0000, 0xffdd0000, 0xffdd0000, 0xffdd0000, 0xffdd0000},
   {0xffffce00, 0xffffce00, 0xffffce00, 0xffffce00, 0xffffce00, 0xffffce00, 0xffffce00, 0xffffce00, 0xffffce00, 0xffffce00, 0xffffce00, 0xffffce00, 0xffffce00, 0xffffce00, 0xffffce00, 0xffffce00},
   {0xffffce00, 0xffffce00, 0xffffce00, 0xffffce00, 0xffffce00, 0xffffce00, 0xffffce00, 0xffffce00, 0xffffce00, 0xffffce00, 0xffffce00, 0xffffce00, 0xffffce00, 0xffffce00, 0xffffce00, 0xffffce00},
   {0xffffce00, 0xffffce00, 0xffffce00, 0xffffce00, 0xffffce00, 0xffffce00, 0xffffce00, 0xffffce00, 0xffffce00, 0xffffce00, 0xffffce00, 0xffffce00, 0xffffce00, 0xffffce00, 0xffffce00, 0xffffce00}
};

unsigned long FrenchFlag[FLAG_HEIGHT][FLAG_WIDTH] = {
   {0xff0055a4, 0xff0055a4, 0xff0055a4, 0xff0055a4, 0xff0055a4, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffef4135, 0xffef4135, 0xffef4135, 0xffef4135, 0xffef4135, 0xff000000},
   {0xff0055a4, 0xff0055a4, 0xff0055a4, 0xff0055a4, 0xff0055a4, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffef4135, 0xffef4135, 0xffef4135, 0xffef4135, 0xffef4135, 0xff000000},
   {0xff0055a4, 0xff0055a4, 0xff0055a4, 0xff0055a4, 0xff0055a4, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffef4135, 0xffef4135, 0xffef4135, 0xffef4135, 0xffef4135, 0xff000000},
   {0xff0055a4, 0xff0055a4, 0xff0055a4, 0xff0055a4, 0xff0055a4, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffef4135, 0xffef4135, 0xffef4135, 0xffef4135, 0xffef4135, 0xff000000},
   {0xff0055a4, 0xff0055a4, 0xff0055a4, 0xff0055a4, 0xff0055a4, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffef4135, 0xffef4135, 0xffef4135, 0xffef4135, 0xffef4135, 0xff000000},
   {0xff0055a4, 0xff0055a4, 0xff0055a4, 0xff0055a4, 0xff0055a4, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffef4135, 0xffef4135, 0xffef4135, 0xffef4135, 0xffef4135, 0xff000000},
   {0xff0055a4, 0xff0055a4, 0xff0055a4, 0xff0055a4, 0xff0055a4, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffef4135, 0xffef4135, 0xffef4135, 0xffef4135, 0xffef4135, 0xff000000},
   {0xff0055a4, 0xff0055a4, 0xff0055a4, 0xff0055a4, 0xff0055a4, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffef4135, 0xffef4135, 0xffef4135, 0xffef4135, 0xffef4135, 0xff000000},
   {0xff0055a4, 0xff0055a4, 0xff0055a4, 0xff0055a4, 0xff0055a4, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffef4135, 0xffef4135, 0xffef4135, 0xffef4135, 0xffef4135, 0xff000000},
   {0xff0055a4, 0xff0055a4, 0xff0055a4, 0xff0055a4, 0xff0055a4, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffef4135, 0xffef4135, 0xffef4135, 0xffef4135, 0xffef4135, 0xff000000}
};

unsigned long ItalianFlag[FLAG_HEIGHT][FLAG_WIDTH] = {
   {0xff009246, 0xff009246, 0xff009246, 0xff009246, 0xff009246, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffef4135, 0xffef4135, 0xffef4135, 0xffef4135, 0xffef4135, 0xff000000},
   {0xff009246, 0xff009246, 0xff009246, 0xff009246, 0xff009246, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffef4135, 0xffef4135, 0xffef4135, 0xffef4135, 0xffef4135, 0xff000000},
   {0xff009246, 0xff009246, 0xff009246, 0xff009246, 0xff009246, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffef4135, 0xffef4135, 0xffef4135, 0xffef4135, 0xffef4135, 0xff000000},
   {0xff009246, 0xff009246, 0xff009246, 0xff009246, 0xff009246, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffef4135, 0xffef4135, 0xffef4135, 0xffef4135, 0xffef4135, 0xff000000},
   {0xff009246, 0xff009246, 0xff009246, 0xff009246, 0xff009246, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffef4135, 0xffef4135, 0xffef4135, 0xffef4135, 0xffef4135, 0xff000000},
   {0xff009246, 0xff009246, 0xff009246, 0xff009246, 0xff009246, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffef4135, 0xffef4135, 0xffef4135, 0xffef4135, 0xffef4135, 0xff000000},
   {0xff009246, 0xff009246, 0xff009246, 0xff009246, 0xff009246, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffef4135, 0xffef4135, 0xffef4135, 0xffef4135, 0xffef4135, 0xff000000},
   {0xff009246, 0xff009246, 0xff009246, 0xff009246, 0xff009246, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffef4135, 0xffef4135, 0xffef4135, 0xffef4135, 0xffef4135, 0xff000000},
   {0xff009246, 0xff009246, 0xff009246, 0xff009246, 0xff009246, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffef4135, 0xffef4135, 0xffef4135, 0xffef4135, 0xffef4135, 0xff000000},
   {0xff009246, 0xff009246, 0xff009246, 0xff009246, 0xff009246, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffef4135, 0xffef4135, 0xffef4135, 0xffef4135, 0xffef4135, 0xff000000}
};

unsigned long GreekFlag[FLAG_HEIGHT][FLAG_WIDTH] = {
   {0xff0000ff, 0xff0000ff, 0xffffffff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff},
   {0xff0000ff, 0xff0000ff, 0xffffffff, 0xff0000ff, 0xff0000ff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff},
   {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff},
   {0xff0000ff, 0xff0000ff, 0xffffffff, 0xff0000ff, 0xff0000ff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff},
   {0xff0000ff, 0xff0000ff, 0xffffffff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff},
   {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff},
   {0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff},
   {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff},
   {0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff},
   {0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000}
};


float SinTable[SINE_VALUES];


static struct BTDCycle OBJ_FlagDelay = {{MYTAG_FLAGDELAY, "Flag delay (ms)", BTDPT_CYCLE }, DEF_FLAGDELAY, DEF_CYDELAY};
static struct BTDCycle OBJ_FlagTheme = {{MYTAG_FLAGTHEME, "Country", BTDPT_CYCLE }, DEF_FLAGTHEME, DEF_CYTHEME};

struct BTDNode *BlankerGUI[] =
{
   (APTR) &OBJ_FlagDelay,
   (APTR) &OBJ_FlagTheme,
   NULL,
};

// compose strings for multiple usage
UBYTE BlankerVersionID[]   = "$VER: " BLANKERLIBNAME " " I2S(VERSION) "." I2S(REVISION) " (" __AMIGADATE__ ") © " __COPYRIGHTYEAR__ " " AUTHORNAME;
UBYTE BlankerLibName[]     = BLANKERLIBNAME;
UBYTE BlankerName[]        = BLANKERNAME;
UBYTE BlankerAuthor[]      = AUTHORNAME;
UBYTE BlankerDescription[] = DESCRIPTION;

// blanker info struct
struct BTDInfo BlankerInfo =
{
   BTDI_Revision,                 /* API version */
   BLANKERID,                     /* ID for prefs window snapshot */
   BlankerName,                   /* blanker name */
   BlankerDescription,            /* description */
   BlankerAuthor,                 /* author(s) */
   BTDIF_DoNotWait,               /* flags */
   BlankerGUI,                    /* config options */
};

// lib query tag information
struct TagItem MyBlankerQueryTags[] =
{
   { QUERYINFOATTR_NAME,        (ULONG) BlankerName },
   { QUERYINFOATTR_DESCRIPTION, (ULONG) BlankerDescription },
   { QUERYINFOATTR_AUTHOR,      (ULONG) BlankerAuthor },
   { QUERYINFOATTR_VERSION,     (ULONG)VERSION},
   { QUERYINFOATTR_REVISION,    (ULONG)REVISION},
   { TAG_DONE,0}
};

// blanker data structure
struct BlankerData
{
   // Timer device data
   struct Library       *bd_timerbase;
   struct timerequest   bd_TimerRequest;

   // BTDDrawInfo
   struct Screen        *bd_Screen;
   struct ViewPort      *bd_ViewPort;
   struct RastPort      *bd_RastPort;
   LONG                 bd_Width;
   LONG                 bd_Height;
   LONG                 bd_Left;
   LONG                 bd_Top;
   IPTR                 bd_PreviewMode;

   // Settings
   IPTR                 bd_Settings_FlagDelay;
   IPTR                 bd_Settings_FlagTheme;

   // Runtime data
   IPTR                 bd_StopRendering;
   BOOL                 Active;
   unsigned char        *renderdata;
};

#define TimerBase    bd->bd_timerbase

// global stuff
struct Library       *CyberGfxBase  = NULL;
struct GfxBase       *GfxBase       = NULL;
struct IntuitionBase *IntuitionBase = NULL;
struct Library       *UtilityBase   = NULL;
struct DosLibrary    *DOSBase       = NULL;
struct Library       *RandomBase    = NULL;


/*=----------------------------- Render_Fill() -------------------------------*
 *                                                                            *
 *----------------------------------------------------------------------------*/
void Render_Fill(struct BlankerData *bd, unsigned long pen)
{
   unsigned char r, g, b;
   unsigned long rpos=0, rtot=0;

   r = (unsigned char)(pen >> 16);
   g = (unsigned char)(pen >> 8);
   b = (unsigned char)pen;

   rtot = DATA_SIZE*RENDER_RGB;

   while (rpos < (rtot-RENDER_RGB))
   {
      bd->renderdata[rpos++] = r;
      bd->renderdata[rpos++] = g;
      bd->renderdata[rpos++] = b;
   }
}
/*=*/

/*=----------------------------- Render_Pixel() ------------------------------*
 *                                                                            *
 *----------------------------------------------------------------------------*/
void Render_Pixel(struct BlankerData *bd, unsigned int xpos, unsigned int ypos, unsigned long pen)
{
   unsigned char r, g, b;
   long pos;

   if ((xpos < DATA_WIDTH) && (ypos < DATA_HEIGHT))
   {
      pos = ((DATA_WIDTH * RENDER_RGB) * ypos) + (xpos * RENDER_RGB);

      r = (unsigned char)(pen >> 16);
      g = (unsigned char)(pen >> 8);
      b = (unsigned char)pen;

      bd->renderdata[pos] = r;
      bd->renderdata[pos+1] = g;
      bd->renderdata[pos+2] = b;
   }
}
/*=*/

/*=----------------------------- BlankerLibFree() ----------------------------*
 *                                                                            *
 *----------------------------------------------------------------------------*/
void BlankerLibFree( void )
{
   debug_print("%s : %s (%d)\n", __FILE__ , __func__, __LINE__);

   if(CyberGfxBase)
   {
      CloseLibrary(CyberGfxBase);
      CyberGfxBase = NULL;
   }
   if(UtilityBase)
   {
      CloseLibrary(UtilityBase);
      UtilityBase = NULL;
   }
   if(IntuitionBase)
   {
      CloseLibrary((APTR) IntuitionBase);
      IntuitionBase = NULL;
   }
   if(GfxBase)
   {
      CloseLibrary((APTR) GfxBase);
      GfxBase = NULL;
   }
   if(DOSBase)
   {
      CloseLibrary((APTR) DOSBase);
      DOSBase = NULL;
   }
   if (RandomBase)
   {
      CloseLibrary(RandomBase);
      RandomBase = NULL;
   }
}
/*=*/

/*=----------------------------- BlankerLibInit() ----------------------------*
 *                                                                            *
 *----------------------------------------------------------------------------*/
ULONG BlankerLibInit( void )
{
   CyberGfxBase  =        OpenLibrary("cybergraphics.library", 0 );
   UtilityBase   =        OpenLibrary("utility.library"      , 0 );
   IntuitionBase = (APTR) OpenLibrary("intuition.library"    , 0 );
   GfxBase       = (APTR) OpenLibrary("graphics.library"     , 0 );
   DOSBase       = (APTR) OpenLibrary("dos.library"          , 0 );
   RandomBase    =        OpenLibrary("random.library"       , 1 );

   debug_print("%s : %s (%d)\n", __FILE__ , __func__, __LINE__);

   if(CyberGfxBase && UtilityBase && IntuitionBase && GfxBase && DOSBase && RandomBase)
   {
      return(1);
   } 
   else
   {
      BlankerLibFree();
      return(0);
   }
}
/*=*/

/*=----------------------------- QueryMyBlanker() ----------------------------*
 *                                                                            *
 *----------------------------------------------------------------------------*/
struct BTDInfo *QueryMyBlanker(void)
{
   debug_print("%s : %s (%d)\n", __FILE__ , __func__, __LINE__);
   return(&BlankerInfo);
}
/*=*/

/*=----------------------------- EndMyBlanker() ------------------------------*
 *                                                                            *
 *----------------------------------------------------------------------------*/
void EndMyBlanker(struct BlankerData *bd)
{
   debug_print("%s : %s (%d)\n", __FILE__ , __func__, __LINE__);

   if(bd)
   {
      // close timer first
      if(bd->bd_timerbase)
      {
         CloseDevice((struct IORequest *) &bd->bd_TimerRequest);
      }

      // delete message port
      if(bd->bd_TimerRequest.tr_node.io_Message.mn_ReplyPort)
      {
         DeleteMsgPort(bd->bd_TimerRequest.tr_node.io_Message.mn_ReplyPort);
      }

      if (bd->renderdata)
      {
         FreeVecTaskPooled(bd->renderdata);
         bd->renderdata = NULL;
      }

      // now we free our base structure
      FreeVec(bd);
   }
}
/*=*/

/*=----------------------------- InitMyBlanker() -----------------------------*
 *                                                                            *
 *----------------------------------------------------------------------------*/
struct BlankerData *InitMyBlanker(struct TagItem *TagList)
{
   int i;
   struct BlankerData *bd           = NULL;
   struct BTDDrawInfo *BTDDrawInfo  = (struct BTDDrawInfo *) GetTagData(BTD_DrawInfo, 0, TagList);
   ULONG              dummy, *error = (ULONG *)              GetTagData(BTD_Error, (ULONG) &dummy, TagList);

   debug_print("%s : %s (%d)\n", __FILE__ , __func__, __LINE__);

   // make sure we did not get bogus data
   if(!(BTDDrawInfo && error))
   {
      return(NULL);
   }

   if((BTDDrawInfo->BDI_Width < 40) || (BTDDrawInfo->BDI_Height < 30))
   {
      *error = BTDERR_Size;
      return(NULL);
   }

   // if something goes wrong, it is probably low memory
   *error = BTDERR_Memory;

   // we allocate our basic blanker structure
   if( (bd = AllocVec(sizeof(struct BlankerData), MEMF_PUBLIC|MEMF_CLEAR)))
   {
      // setup timer io request
      bd->bd_TimerRequest.tr_node.io_Message.mn_Node.ln_Type = NT_REPLYMSG;
      bd->bd_TimerRequest.tr_node.io_Message.mn_ReplyPort    = CreateMsgPort();
      bd->bd_TimerRequest.tr_node.io_Message.mn_Length       = sizeof(bd->bd_TimerRequest);
      bd->bd_TimerRequest.tr_node.io_Flags                   = IOF_QUICK;

      if(!OpenDevice("timer.device", UNIT_MICROHZ, (struct IORequest *) &bd->bd_TimerRequest, 0))
      {
         bd->bd_timerbase = (struct Library *) bd->bd_TimerRequest.tr_node.io_Device;

         // copy draw info values for easy and quick usage during rendering
         bd->bd_Screen   = BTDDrawInfo->BDI_Screen;
         bd->bd_ViewPort = BTDDrawInfo->BDI_Screen ? &BTDDrawInfo->BDI_Screen->ViewPort : NULL;
         bd->bd_RastPort = BTDDrawInfo->BDI_RPort;
         bd->bd_Width    = BTDDrawInfo->BDI_Width;
         bd->bd_Height   = BTDDrawInfo->BDI_Height;
         bd->bd_Left     = BTDDrawInfo->BDI_Left;
         bd->bd_Top      = BTDDrawInfo->BDI_Top;

         bd->bd_PreviewMode = GetTagData(BTD_PreviewMode, FALSE, TagList);
         bd->bd_Settings_FlagDelay   = GetTagData(MYTAG_FLAGDELAY, (ULONG) DEF_FLAGDELAY, TagList) + 1;
         bd->bd_Settings_FlagTheme   = GetTagData(MYTAG_FLAGTHEME, (ULONG) DEF_FLAGTHEME, TagList) + 1;

         bd->Active = FALSE;
         bd->renderdata = AllocVecTaskPooled((DATA_WIDTH*(DATA_HEIGHT+4))*3); //Render_Alloc(DATA_WIDTH, DATA_HEIGHT+4, RENDER_RGB);

         // Init sin table
         for (i = 0; i < SINE_VALUES; i++)
         {
            SinTable[i] = sin(i * 4 * M_PI / SINE_VALUES) * 10;
         }

         if (bd->renderdata)
         {
            FillPixelArray(bd->bd_RastPort, bd->bd_Left, bd->bd_Top, bd->bd_Width, bd->bd_Height, 0x00000000);
            return(bd);
         }
      }
   }

   // something went wrong.
   // Call shut down to release all data we may have already gathered during init process
   EndMyBlanker(bd);

   // we failed
   return(NULL);
}
/*=*/

/*=----------------------------- AnimMyBlanker() -----------------------------*
 *                                                                            *
 *----------------------------------------------------------------------------*/
void AnimMyBlanker(struct BlankerData *bd)
{
   int x1, x2, y1, y2, xp, yp, xc, yc, x, y;
   unsigned long color;
   int frame;
   static double frame_counter = 0;
   float deltatime = 5.0f;

   frame_counter += deltatime * 100;
   frame = frame_counter;


   debug_print("%s : %s (%d) - PrevieMode : %d\n", __FILE__ , __func__, __LINE__, bd->bd_PreviewMode);

   bd->bd_TimerRequest.tr_time.tv_micro = (bd->bd_Settings_FlagDelay*25)*1000;
   bd->bd_TimerRequest.tr_time.tv_secs  = 0;
   bd->bd_TimerRequest.tr_node.io_Command = TR_ADDREQUEST;

   DoIO((struct IORequest *) &bd->bd_TimerRequest);

   // Do the magic...
   if(!bd->Active)
   {
      FillPixelArray(bd->bd_RastPort, bd->bd_Left, bd->bd_Top, bd->bd_Width, bd->bd_Height, 0x00000000);
      bd->Active = TRUE;
   }

   if(bd->renderdata)
   {
      Render_Fill(bd, 0xff000000); // Clear the render...

      // Draw flag
      for (y1 = 0; y1 < FLAG_HEIGHT; y1++) 
      {
         for (x1 = 0; x1 < FLAG_WIDTH; x1++)
         {
            switch (bd->bd_Settings_FlagTheme)
            {
               case 2  : color = FinnishFlag[y1][x1]; break;
               case 3  : color = DanishFlag[y1][x1]; break;
               case 4  : color = NorwegianFlag[y1][x1]; break;
               case 5  : color = GermanFlag[y1][x1]; break;
               case 6  : color = UkrainanFlag[y1][x1]; break;
               case 7  : color = PolishFlag[y1][x1]; break;
               case 8  : color = FrenchFlag[y1][x1]; break;
               case 9  : color = ItalianFlag[y1][x1]; break;
               case 10 : color = GreekFlag[y1][x1]; break;
               default : color = SwedishFlag[y1][x1]; break;
            }

            for (y2 = 0; y2 < FLAG_SIZE; y2++)
            {
               for (x2 = 0; x2 < FLAG_SIZE; x2++)
               {
                  xp = x1 * FLAG_SIZE + x2;
                  yp = y1 * FLAG_SIZE + y2;

                  xc = (DATA_WIDTH / 2) - (FLAG_WIDTH * FLAG_SIZE * FLAG_SIZE) / 2;
                  yc = (DATA_HEIGHT / 2) - (FLAG_HEIGHT * FLAG_SIZE * FLAG_SIZE) / 2;

                  x = xc + xp * FLAG_SIZE + SinTable[(frame + xp * FLAG_CURVE + yp * FLAG_CURVE + xp) % SINE_VALUES];
                  y = yc + yp * FLAG_SIZE + SinTable[(frame + xp * FLAG_CURVE + yp * FLAG_CURVE + yp) % SINE_VALUES];

                  Render_Pixel(bd, x, y, color);
               }
            }
         }
      }

      ScalePixelArray(bd->renderdata, DATA_WIDTH, DATA_HEIGHT, DATA_WIDTH*3, bd->bd_RastPort, 0, 0, bd->bd_Width, bd->bd_Height, RECTFMT_RGB);
   }
}
/*=*/

