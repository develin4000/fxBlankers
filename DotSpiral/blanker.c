/*
->========================================================<-
->= fxBlankers - DotSpiral - (c) Copyright 2026 OnyxSoft =<-
->========================================================<-
->= Version  : 1.4                                       =<-
->= File     : blanker.c                                 =<-
->= Author   : Stefan Blixth                             =<-
->= Compiled : 2026-11-26                                =<-
->========================================================<-

Based on retro-demoeffects dottunnel source by Johan Gardhage : https://github.com/johangardhage/retro-demoeffects/blob/master/src/dottunnel.cpp and the "Simple blanker example" by Guido Mersmann and Michal Wozniak in the MorphOS SDK.
*/

#define  VERSION  1
#define  REVISION 4


#define BLANKERLIBNAME  "dotspiral.btd"
#define BLANKERNAME     "DotSpiral"
#define AUTHORNAME      "Stefan Blixth, OnyxSoft"
#define DESCRIPTION     "Dot Spiral Effect"
#define BLANKERID       MAKE_ID('S','P','I','R')


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

#define DEF_SPIRALDELAY     0x03  // 100ms
char *DEF_CYDELAY[] = {"25", "50", "75", "100", "125", "150", "175", "200", "250", NULL};

#define DEF_SPIRALTHEME   0x00 // Gray
char *DEF_CYTHEME[] = {"Gray", "Blue", "Green", "GreenBlue", "Red", "Purple", "Yellow", NULL};


#define THEME_GRAY      0x01
#define THEME_BLUE      0x02
#define THEME_GREEN     0x03
#define THEME_GREENBLUE 0x04
#define THEME_RED       0x05
#define THEME_PURPLE    0x06
#define THEME_YELLOW    0x07


// blanker settings window definition
#define nTAG(o)          (BTD_Client + (o))
#define MYTAG_SPIRALDELAY  nTAG(0)
#define MYTAG_SPIRALTHEME  nTAG(1)

#define DATA_WIDTH   320
#define DATA_HEIGHT  240
#define DATA_SIZE    DATA_WIDTH * DATA_HEIGHT
#define RENDER_RGB   3
#define RENDER_RGBA  4

#define MKRGB(r,g,b) (((r) << 16) | ((g) << 8) | (b))



#define DT_RING_COUNT    40       // rings between the eye and the mouth
#define DT_RING_DOTS     60       // 6 degrees per dot
#define DT_RING_WIDTH    740.0
#define DT_RING_HEIGHT   (788.0 * DATA_HEIGHT / 240.0)   // original is made for 320x240
#define DT_TUNNEL_DEPTH  20.0
#define DT_TUNNEL_SPEED  12.0     // depth travelled per second
#define DT_ANGLES        256.0    // angle units per full turn
#define DT_TWIST_PER_RING (DT_ANGLES / 24.0)
#define DT_SWAY_RADIUS   260.0
#define DT_TWIST_SPEED   (DT_ANGLES * 1.0)
#define DT_PI            3.14159265358979323846

static double dt_ringx[DT_RING_DOTS];
static double dt_ringy[DT_RING_DOTS];
static ULONG  dt_start_s = 0, dt_start_us = 0;

static double dt_sin(double a) { return sin((a * 2.0 * DT_PI) / DT_ANGLES); }
static double dt_cos(double a) { return cos((a * 2.0 * DT_PI) / DT_ANGLES); }

unsigned char rcomp = 0xff;
unsigned char gcomp = 0xff;
unsigned char bcomp = 0xff;


static struct BTDCycle OBJ_SpiralDelay = {{MYTAG_SPIRALDELAY, "Spiral delay (ms)", BTDPT_CYCLE }, DEF_SPIRALDELAY, DEF_CYDELAY};
static struct BTDCycle OBJ_SpiralTheme = {{MYTAG_SPIRALTHEME, "Style", BTDPT_CYCLE }, DEF_SPIRALTHEME, DEF_CYTHEME};

struct BTDNode *BlankerGUI[] =
{
   (APTR) &OBJ_SpiralDelay,
   (APTR) &OBJ_SpiralTheme,
   NULL,
};

// compose strings for multiple usage
UBYTE BlankerVersionID[]   = "$VER: " BLANKERLIBNAME " " I2S(VERSION) "." I2S(REVISION) " (" __AMIGADATE__ ") (c) " __COPYRIGHTYEAR__ " " AUTHORNAME;
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
   IPTR                 bd_Settings_SpiralDelay;
   IPTR                 bd_Settings_SpiralTheme;

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


// Seconds since init_dottunnel()
static double dt_time(void)
{
   ULONG s, us;
   CurrentTime(&s, &us);
   return (double)(s - dt_start_s) + ((double)us - (double)dt_start_us) / 1000000.0;
}


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
         bd->bd_Settings_SpiralDelay   = GetTagData(MYTAG_SPIRALDELAY, (ULONG) DEF_SPIRALDELAY, TagList) + 1;
         bd->bd_Settings_SpiralTheme   = GetTagData(MYTAG_SPIRALTHEME, (ULONG) DEF_SPIRALTHEME, TagList) + 1;

         rcomp = gcomp = bcomp = 0x00;

         switch (bd->bd_Settings_SpiralTheme)
         {
               case THEME_BLUE      : bcomp = 0xff; break;
               case THEME_GREEN     : gcomp = 0xff; break;
               case THEME_GREENBLUE : gcomp = bcomp = 0xff; break;
               case THEME_RED       : rcomp = 0xff; break;
               case THEME_PURPLE    : rcomp = bcomp = 0xff; break;
               case THEME_YELLOW    : rcomp = gcomp = 0xff; break;
               default              : rcomp = gcomp = bcomp = 0xff; break;
         }

         bd->Active = FALSE;
         bd->renderdata = AllocVecTaskPooled((DATA_WIDTH*(DATA_HEIGHT+4))*3);

         // Init the rings...
         int i;
         for (i = 0; i < DT_RING_DOTS; i++)
         {
            double angle = i * DT_ANGLES / DT_RING_DOTS;
            dt_ringx[i] = DT_RING_WIDTH  * dt_sin(angle);
            dt_ringy[i] = DT_RING_HEIGHT * dt_cos(angle);
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

   bd->bd_TimerRequest.tr_time.tv_micro = (bd->bd_Settings_SpiralDelay*25)*1000;
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
      double total = dt_time();

      double spacing = DT_TUNNEL_DEPTH / DT_RING_COUNT;
      double phase   = fmod(total * DT_TUNNEL_SPEED, spacing);
      double twist   = fmod(-total * DT_TWIST_SPEED, DT_ANGLES);
      int i, j;

      if (twist < 0) twist += DT_ANGLES;

      for (i = 0; i < DT_RING_COUNT; i++)
      {
         double along = i + phase / spacing;
         double depth = DT_TUNNEL_DEPTH + spacing - i * spacing - phase;
         double angle = twist + along * DT_TWIST_PER_RING;
         double swayx = DT_SWAY_RADIUS * dt_sin(angle);
         double swayy = DT_SWAY_RADIUS * dt_cos(angle);
         long   c     = (long)(255.0 * along / DT_RING_COUNT);

         if (c < 0) c = 0;
         if (c > 255) c = 255;
         color = MKRGB((unsigned long)c & rcomp, (unsigned long)c & gcomp, (unsigned long)c & bcomp);

         for (j = 0; j < DT_RING_DOTS; j++)
         {
            double x = (dt_ringx[j] + swayx) / depth + DATA_WIDTH  / 2.0;
            double y = (dt_ringy[j] + swayy) / depth + DATA_HEIGHT / 2.0;

            if (x >= 0.0 && y >= 0.0 && x < DATA_WIDTH && y < DATA_HEIGHT)
               Render_Pixel(bd, (unsigned int)x, (unsigned int)y, color);
         }
      }

      ScalePixelArray(bd->renderdata, DATA_WIDTH, DATA_HEIGHT, DATA_WIDTH*3, bd->bd_RastPort, 0, 0, bd->bd_Width, bd->bd_Height, RECTFMT_RGB);
   }
}
/*=*/
