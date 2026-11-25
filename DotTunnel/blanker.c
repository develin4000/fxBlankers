/*
->========================================================<-
->= fxBlankers - DotTunnel - (c) Copyright 2026 OnyxSoft =<-
->========================================================<-
->= Version  : 1.4                                       =<-
->= File     : blanker.c                                 =<-
->= Author   : Stefan Blixth                             =<-
->= Compiled : 2026-11-26                                =<-
->========================================================<-

Based on the "2nd Reality Tunnel Effect for MS DOS" (dostunnl) by Arne Schmitz : https://github.com/root42/dostunnl
and the "Simple blanker example" by Guido Mersmann and Michal Wozniak in the MorphOS SDK.
*/

#define  VERSION  1
#define  REVISION 4


#define BLANKERLIBNAME  "dottunnel.btd"
#define BLANKERNAME     "DotTunnel"
#define AUTHORNAME      "Stefan Blixth, OnyxSoft"
#define DESCRIPTION     "Dot Tunnel Effect"
#define BLANKERID       MAKE_ID('D','T','U','N')


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
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/cybergraphics.h>
#include <math.h>
#include <string.h>

//#define  USEDEBUG 1
#include "debug.h"

#define I2S(x) I2S2(x)
#define I2S2(x) #x

#define DEF_TUNNELDELAY   0x00  // 25ms
char *DEF_CYDELAY[] = {"25", "50", "75", "100", "125", "150", "175", "200", "250", NULL};

#define DEF_TUNNELSPEED   0x01  // Normal
char *DEF_CYSPEED[] = {"Slow", "Normal", "Fast", NULL};
static const double TUNNEL_SPEEDS[] = {35.0, 70.0, 140.0};   // tunnel steps per second (original: one per 70Hz VGA frame)

#define DEF_TUNNELTHEME   0x00  // Gray
char *DEF_CYTHEME[] = {"Gray", "Blue", "Green", "GreenBlue", "Red", "Purple", "Yellow", NULL};


#define THEME_GRAY      0x01
#define THEME_BLUE      0x02
#define THEME_GREEN     0x03
#define THEME_GREENBLUE 0x04
#define THEME_RED       0x05
#define THEME_PURPLE    0x06
#define THEME_YELLOW    0x07


// blanker settings window definition
#define nTAG(o)            (BTD_Client + (o))
#define MYTAG_TUNNELDELAY  nTAG(0)
#define MYTAG_TUNNELTHEME  nTAG(1)
#define MYTAG_TUNNELSPEED  nTAG(2)

#define DATA_WIDTH   320          // the original runs in VGA mode 13h (320x200)
#define DATA_HEIGHT  200
#define DATA_SIZE    (DATA_WIDTH * DATA_HEIGHT)
#define RENDER_RGB   3

#define MKRGB(r,g,b) (((r) << 16) | ((g) << 8) | (b))


/*---------------------------- Tunnel (dostunnl) -----------------------------*/

// fix.h / fix.c - 24.8 fixed point maths
#define FIX_PREC 8
#define TO_FIX(x)  ((long)((x)*(1<<FIX_PREC)))
#define TO_LONG(x) ((x)/(1<<FIX_PREC))

static inline long fix_mul(long a, long b) { return (a * b) >> FIX_PREC; }
static inline long fix_div(long a, long b) { return (a << FIX_PREC) / b; }

#define DT_PI       3.14159265358979323846
#define SIN_SIZE    512
#define COS_OFF     128
#define LEVELS      128           // rings in the tunnel
#define POINTS      64            // dots per ring

static long  dt_sin_tab[SIN_SIZE + COS_OFF];
static long *dt_cos_tab = dt_sin_tab + COS_OFF;

static int   tun_coords[LEVELS][POINTS][2];

// The original never sets a palette, so the colour indices 16..31 it uses are
// the grey ramp of the default VGA palette (6 bit values, scaled to 8 bit here).
static const unsigned char vga_grey[16] =
{
   0x00, 0x14, 0x20, 0x2c, 0x38, 0x45, 0x51, 0x61,
   0x71, 0x82, 0x92, 0xa2, 0xb6, 0xcb, 0xe3, 0xff
};

static void init_sin(void)
{
   int i;
   for(i = 0; i < SIN_SIZE + COS_OFF; ++i)
      dt_sin_tab[i] = TO_FIX(sin(2.0 * DT_PI * i / (double)SIN_SIZE));
}

// Lissajous path the tunnel sways along
static void liss(int *x, int *y, unsigned long t)
{
   const long scale = TO_FIX(50);
   *x = TO_LONG(fix_mul(dt_cos_tab[(t*2+64) % SIN_SIZE], scale));
   *y = TO_LONG(fix_mul(dt_cos_tab[(t*3) % SIN_SIZE], scale));
}

static void init_tunnel(void)
{
   int i, j;
   long scale_x = TO_FIX(50);
   long scale_y = TO_FIX(31);    /* 50 * 320/200 = 50 * 0.625 ~= 31 */
   long proj;

   for(i = 0; i < LEVELS; ++i)
   {
      proj = fix_div(TO_FIX(i+1), TO_FIX(LEVELS));
      for(j = 0; j < POINTS; ++j)
      {
         tun_coords[i][j][0] = TO_LONG(fix_div(fix_mul(dt_cos_tab[j * (SIN_SIZE/POINTS)], scale_x), proj)) + DATA_WIDTH/2;
         tun_coords[i][j][1] = TO_LONG(fix_div(fix_mul(dt_sin_tab[j * (SIN_SIZE/POINTS)], scale_y), proj)) + DATA_HEIGHT/2;
      }
   }
}

/*----------------------------------------------------------------------------*/


static struct BTDCycle OBJ_TunnelDelay = {{MYTAG_TUNNELDELAY, "Tunnel delay (ms)", BTDPT_CYCLE }, DEF_TUNNELDELAY, DEF_CYDELAY};
static struct BTDCycle OBJ_TunnelSpeed = {{MYTAG_TUNNELSPEED, "Speed", BTDPT_CYCLE }, DEF_TUNNELSPEED, DEF_CYSPEED};
static struct BTDCycle OBJ_TunnelTheme = {{MYTAG_TUNNELTHEME, "Style", BTDPT_CYCLE }, DEF_TUNNELTHEME, DEF_CYTHEME};

struct BTDNode *BlankerGUI[] =
{
   (APTR) &OBJ_TunnelDelay,
   (APTR) &OBJ_TunnelSpeed,
   (APTR) &OBJ_TunnelTheme,
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
   IPTR                 bd_Settings_TunnelDelay;
   IPTR                 bd_Settings_TunnelTheme;
   IPTR                 bd_Settings_TunnelSpeed;

   // Runtime data
   BOOL                 Active;
   unsigned char        rcomp, gcomp, bcomp;
   ULONG                start_s, start_us;
   unsigned char        *renderdata;
};

// global stuff
struct Library       *CyberGfxBase  = NULL;
struct GfxBase       *GfxBase       = NULL;
struct IntuitionBase *IntuitionBase = NULL;
struct Library       *UtilityBase   = NULL;
struct DosLibrary    *DOSBase       = NULL;


/*=----------------------------- Tunnel_Time() -------------------------------*
 * Seconds since the blanker was started                                      *
 *----------------------------------------------------------------------------*/
static double Tunnel_Time(struct BlankerData *bd)
{
   ULONG s, us;
   CurrentTime(&s, &us);
   return (double)(s - bd->start_s) + ((double)us - (double)bd->start_us) / 1000000.0;
}
/*=*/

/*=----------------------------- Render_Pixel() ------------------------------*
 *                                                                            *
 *----------------------------------------------------------------------------*/
static inline void Render_Pixel(struct BlankerData *bd, unsigned int xpos, unsigned int ypos, unsigned long pen)
{
   if ((xpos < DATA_WIDTH) && (ypos < DATA_HEIGHT))
   {
      unsigned char *p = bd->renderdata + ((DATA_WIDTH * RENDER_RGB) * ypos) + (xpos * RENDER_RGB);

      p[0] = (unsigned char)(pen >> 16);
      p[1] = (unsigned char)(pen >> 8);
      p[2] = (unsigned char)pen;
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

   debug_print("%s : %s (%d)\n", __FILE__ , __func__, __LINE__);

   if(CyberGfxBase && UtilityBase && IntuitionBase && GfxBase && DOSBase)
   {
      // The tables only depend on constants, so build them once per library open
      init_sin();
      init_tunnel();
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

      if(bd->bd_TimerRequest.tr_node.io_Message.mn_ReplyPort &&
         !OpenDevice("timer.device", UNIT_MICROHZ, (struct IORequest *) &bd->bd_TimerRequest, 0))
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
         bd->bd_Settings_TunnelDelay = GetTagData(MYTAG_TUNNELDELAY, (ULONG) DEF_TUNNELDELAY, TagList) + 1;
         bd->bd_Settings_TunnelTheme = GetTagData(MYTAG_TUNNELTHEME, (ULONG) DEF_TUNNELTHEME, TagList) + 1;
         bd->bd_Settings_TunnelSpeed = GetTagData(MYTAG_TUNNELSPEED, (ULONG) DEF_TUNNELSPEED, TagList);

         if (bd->bd_Settings_TunnelSpeed >= sizeof(TUNNEL_SPEEDS) / sizeof(TUNNEL_SPEEDS[0]))
            bd->bd_Settings_TunnelSpeed = DEF_TUNNELSPEED;

         bd->rcomp = bd->gcomp = bd->bcomp = 0x00;

         switch (bd->bd_Settings_TunnelTheme)
         {
               case THEME_BLUE      : bd->bcomp = 0xff; break;
               case THEME_GREEN     : bd->gcomp = 0xff; break;
               case THEME_GREENBLUE : bd->gcomp = bd->bcomp = 0xff; break;
               case THEME_RED       : bd->rcomp = 0xff; break;
               case THEME_PURPLE    : bd->rcomp = bd->bcomp = 0xff; break;
               case THEME_YELLOW    : bd->rcomp = bd->gcomp = 0xff; break;
               default              : bd->rcomp = bd->gcomp = bd->bcomp = 0xff; break;
         }

         bd->Active = FALSE;
         CurrentTime(&bd->start_s, &bd->start_us);
         bd->renderdata = AllocVecTaskPooled(DATA_SIZE * RENDER_RGB);

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
   unsigned long t;
   int i, j, x, y, lx, ly;
   unsigned long color;

   debug_print("%s : %s (%d) - PreviewMode : %d\n", __FILE__ , __func__, __LINE__, bd->bd_PreviewMode);

   bd->bd_TimerRequest.tr_time.tv_micro = (bd->bd_Settings_TunnelDelay*25)*1000;
   bd->bd_TimerRequest.tr_time.tv_secs  = 0;
   bd->bd_TimerRequest.tr_node.io_Command = TR_ADDREQUEST;

   DoIO((struct IORequest *) &bd->bd_TimerRequest);

   if(!bd->Active)
   {
      FillPixelArray(bd->bd_RastPort, bd->bd_Left, bd->bd_Top, bd->bd_Width, bd->bd_Height, 0x00000000);
      bd->Active = TRUE;
   }

   if(bd->renderdata)
   {
      memset(bd->renderdata, 0, DATA_SIZE * RENDER_RGB);

      // The original advances t by one per VGA frame. Here t follows the clock,
      // so the tunnel moves at the same speed whatever delay is selected.
      t = (unsigned long)(Tunnel_Time(bd) * TUNNEL_SPEEDS[bd->bd_Settings_TunnelSpeed]);

      for(i = 0; i < LEVELS; ++i)
      {
         // 5 rings shown, 5 rings hidden - gives the moving bands
         if((t+i) % 10 < 5) continue;

         // Colour index 31 (white) nearest the viewer down to 16 (black) at the far end
         {
            unsigned long c = vga_grey[15 - i / (LEVELS/16)];
            color = MKRGB(c & bd->rcomp, c & bd->gcomp, c & bd->bcomp);
         }

         // The original keeps a ring buffer of sway offsets, where ring i ends up
         // using the Lissajous position from step t+i+1. Computing it directly
         // stays correct even when several steps pass between two frames.
         liss(&lx, &ly, t + i + 1);

         for(j = 0; j < POINTS; ++j)
         {
            x = tun_coords[i][j][0] + lx;
            y = tun_coords[i][j][1] + ly;

            if(x >= 0 && y >= 0 && x < DATA_WIDTH && y < DATA_HEIGHT)
               Render_Pixel(bd, (unsigned int)x, (unsigned int)y, color);
         }
      }

      ScalePixelArray(bd->renderdata, DATA_WIDTH, DATA_HEIGHT, DATA_WIDTH*RENDER_RGB, bd->bd_RastPort, bd->bd_Left, bd->bd_Top, bd->bd_Width, bd->bd_Height, RECTFMT_RGB);
   }
}
/*=*/
