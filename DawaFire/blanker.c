/*
->=====================================================<-
->= fxBlankers - DawaFire - © Copyright 2021 OnyxSoft =<-
->=====================================================<-
->= Version  : 1.3                                    =<-
->= File     : blanker.c                              =<-
->= Author   : Stefan Blixth                          =<-
->= Compiled : 2021-12-27                             =<-
->=====================================================<-

Based on AROS demo source : https://github.com/ezrec/AROS-mirror/tree/ABI_V1/contrib/Demo/DawaFire
and the "Simple blanker example" by Guido Mersmann and Michal Wozniak in the MorphOS SDK.

*/

#define  VERSION  1
#define  REVISION 3

#define BLANKERLIBNAME  "dawafire.btd"
#define BLANKERNAME     "DawaFire"
#define AUTHORNAME      "Stefan Blixth, OnyxSoft"
#define DESCRIPTION     "Fire effect"
#define BLANKERID       MAKE_ID('D','A','W','A')

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

//#define  USEDEBUG 1
#include <math.h>
#include "debug.h"

#define I2S(x) I2S2(x)
#define I2S2(x) #x

#define DEF_FIREDELAY    0x03  // 50ms
char *DEF_CYFIREDELAY[] = {"20", "30", "40", "50", "60", "70", "80", "90", "100", NULL};

#define DEF_FIRETHEME   0x00 // Fire
char *DEF_CYFIRETHEME[] = {"Fire", "Purple", "Green", "Blue", NULL};

#define THEME_FIRE      0x01
#define THEME_PURPLE    0x02
#define THEME_GREEN     0x03
#define THEME_BLUE      0x04

// blanker settings window definition
#define nTAG(o)            (BTD_Client + (o))
#define MYTAG_FIREDELAY    nTAG(0)
#define MYTAG_FIRETHEME    nTAG(1)

#define DATA_WIDTH   320
#define DATA_HEIGHT  200
#define DATA_SIZE    DATA_WIDTH * DATA_HEIGHT

static struct BTDCycle OBJ_FireDelay = {{MYTAG_FIREDELAY, "Delay (ms) :", BTDPT_CYCLE}, DEF_FIREDELAY, DEF_CYFIREDELAY};
static struct BTDCycle OBJ_FireTheme = {{MYTAG_FIRETHEME, "Theme :", BTDPT_CYCLE}, DEF_FIRETHEME, DEF_CYFIRETHEME};

struct BTDNode *BlankerGUI[] =
{
   (APTR) &OBJ_FireDelay,
   (APTR) &OBJ_FireTheme,
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
   IPTR                 bd_Settings_FireDelay;
   IPTR                 bd_Settings_FireTheme;
   
   // Runtime data
   IPTR                 bd_StopRendering;
   ULONG                cgfx_coltab[256];
   BOOL                 Active;
   unsigned char        *firebuffer;
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

struct particle
{
  int X0,Y0;
  int V0x,V0y;
  int power;
  int time;
  struct particle *next;
};

typedef struct particle particle;

struct particle *firstparticle;

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
      UtilityBase	= NULL;
   }
   if(IntuitionBase)
   {
      CloseLibrary((APTR) IntuitionBase);
      IntuitionBase = NULL;
   }
   if(GfxBase)
   {
      CloseLibrary( (APTR) GfxBase);
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

      // Clean the buffers allocated
      if (bd->firebuffer)
      {
         FreeVecTaskPooled(bd->firebuffer);
         bd->firebuffer = NULL;
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
   int i;
   int r,g,b;
   ULONG cntr;

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
         bd->bd_Settings_FireDelay = GetTagData(MYTAG_FIREDELAY, (ULONG) DEF_FIREDELAY, TagList) + 1;
         bd->bd_Settings_FireTheme = GetTagData(MYTAG_FIRETHEME, (ULONG) DEF_FIRETHEME, TagList) + 1;

         bd->Active = FALSE;
         bd->firebuffer = AllocVecTaskPooled(DATA_WIDTH*DATA_HEIGHT);
         bd->renderdata = AllocVecTaskPooled((DATA_WIDTH*DATA_HEIGHT)*3); // RGB-data for rendering...

         debug_print("%s : %s (%d) | bd->Width = %d, bd->Height = %d\n", __FILE__ , __func__, __LINE__, bd->bd_Width, bd->bd_Height);

         if (bd->firebuffer)
         {
            for (cntr=0; cntr < DATA_WIDTH*DATA_HEIGHT; cntr++)
               bd->firebuffer[cntr] = 1;

            // Init the colormap
            for (i=0 ; i<256 ; i++)
            {  
               r = g = b = 0 ;

               if ((i > 7) && (i < 32))
                  r = 10 * (i - 7);
               if (i > 31)
                  r = 255;

               if ((i > 32) && (i < 57))
                  g = 10 * (i - 32);
               if (i > 56)
                  g = 255;

               if (i < 8)
                  b = 8 * i;
               if ((i > 7) && (i < 17))
                  b = 8 * (16 - i);
               if ((i > 57) && (i < 82))
                  b = 10 * (i - 57);
               if (i > 81)
                  b = 255;

               switch (bd->bd_Settings_FireTheme)
               {
                  case THEME_PURPLE:
                     bd->cgfx_coltab[i] = (r<<16) + (g<<8) + (r);
                     break;

                  case THEME_GREEN:
                     bd->cgfx_coltab[i] = (g<<16) + (r<<8) + (b);
                     break;

                  case THEME_BLUE:
                     bd->cgfx_coltab[i] = (b<<16) + (g<<8) + (r);
                     break;

                  default : // THEME_FIRE
                     bd->cgfx_coltab[i] = (r<<16) + (g<<8) + (b);
                     break;
               }
            }

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
   static double k=0;
   int i, ii;
   unsigned char *blur = bd->firebuffer;
   struct particle *precurrentparticle;
   struct particle *currentparticle;
   long cntr = 0, dpos = 0;
   unsigned long colrgb = 0;

   debug_print("%s : %s (%d) - PrevieMode : %d\n", __FILE__ , __func__, __LINE__, bd->bd_PreviewMode);

   bd->bd_TimerRequest.tr_time.tv_micro = bd->bd_Settings_FireDelay*10000;
   bd->bd_TimerRequest.tr_time.tv_secs  = 0;
   bd->bd_TimerRequest.tr_node.io_Command = TR_ADDREQUEST;

   DoIO((struct IORequest *) &bd->bd_TimerRequest);


   /* Add new particles */
   for (i=0; i<8; i++)
   {
      for (ii=0; ii<8; ii++)
      {
         precurrentparticle = (struct particle *) AllocMem(sizeof(struct particle), MEMF_ANY);
         precurrentparticle->V0x = (Random() & 31) - 16;
         precurrentparticle->V0y = (Random() & 31);

         precurrentparticle->X0 = DATA_WIDTH/2+
                                  DATA_WIDTH/2.5*
                                  sin((20*sin(k/20))+i*70)*
                                  (sin(10+k/(10+i))+0.2)*
                                  cos((k+i*25)/10);
                                  
         precurrentparticle->Y0 = DATA_HEIGHT/2+
                                  DATA_HEIGHT/2.5*
                                  cos((20*sin(k/(20+i)))+i*70)*
                                  (sin(10+k/10)+0.2)*
                                  cos((k+i*25)/10);

         precurrentparticle->power = 110+50*sin(k/3);
         precurrentparticle->time = 0;
         precurrentparticle->next = firstparticle;
      
         firstparticle = precurrentparticle;
      }
      k += 0.02;
   }

   /*
   *  Physics:
   *
   *  Vx(t) = V0x
   *  Vy(t) = V0y - G.t
   *
   *  X(t) = X0 + V0x.t
   *  Y(t) = Y0 + V0y.t - G.t^2
   *
   */
   currentparticle = firstparticle->next;

   while (currentparticle != NULL)
   {
      int dawa;
      int tx,ty;
      int time;

      time = currentparticle->time / 16;

      tx = (currentparticle->X0) + time*(currentparticle->V0x);
      ty = (currentparticle->Y0) + time*(currentparticle->V0y) - time*time*2;
      ty = DATA_HEIGHT-ty;

      if ((tx<DATA_WIDTH) && (ty>3) && (ty<DATA_HEIGHT))
      {
         dawa = bd->firebuffer[ty*DATA_WIDTH+tx];
         dawa += currentparticle->power;
         if (dawa>255)
            dawa = 255;
         bd->firebuffer[ty*DATA_WIDTH+tx] = dawa;
      }
    
      currentparticle->time += 1;
      currentparticle->power -= ((currentparticle->power >> 4) + 1);

      if (currentparticle->power <= 1)
      {
         precurrentparticle->next = currentparticle->next;
         FreeMem(currentparticle, sizeof(struct particle));
      } 
      else
         precurrentparticle = currentparticle;
    
      currentparticle = precurrentparticle->next;
   }

   /*
   * Blur :
   *
   *  &&&
   *  &@&
   *   &
   */
   blur += 2*DATA_WIDTH;

   for (i=0; i<DATA_WIDTH*(DATA_HEIGHT-3); i++)
   {
      * (blur) = ( * (blur-DATA_WIDTH-1)     +
                   * (blur-DATA_WIDTH)       +
                   * (blur-DATA_WIDTH+1)     +
                   * (blur-1)       +
                   * (blur)         +
                   ( (*blur) >> 1 ) +
                   ( (*blur) >> 2 ) +
                   * (blur+1)       +
                   * (blur+DATA_WIDTH) ) >> 3;
      blur++;
   }

   if(!bd->Active)
   {
      FillPixelArray(bd->bd_RastPort, bd->bd_Left, bd->bd_Top, bd->bd_Width, bd->bd_Height, 0x00000000);
      bd->Active = TRUE;
   }

   if(bd->renderdata)
   {
      for (cntr = 0; cntr < DATA_SIZE; cntr++)
      {
         colrgb = bd->cgfx_coltab[bd->firebuffer[cntr]];

         bd->renderdata[dpos++] = (unsigned char)(colrgb>>16); // R
         bd->renderdata[dpos++] = (unsigned char)(colrgb>>8);  // G
         bd->renderdata[dpos++] = (unsigned char)(colrgb);     // B
      }

      ScalePixelArray(bd->renderdata, DATA_WIDTH, DATA_HEIGHT, DATA_WIDTH*3, bd->bd_RastPort, 0, bd->bd_Top, bd->bd_Width, bd->bd_Height, RECTFMT_RGB);
   }
}
/*=*/

