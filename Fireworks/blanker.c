/*
->======================================================<-
->= fxBlankers - Fireworks - © Copyright 2021 OnyxSoft =<-
->======================================================<-
->= Version  : 1.0                                     =<-
->= File     : blanker.c                               =<-
->= Author   : Stefan Blixth                           =<-
->= Compiled : 2021-09-07                              =<-
->======================================================<-

Based on AROS demo source : https://github.com/ezrec/AROS-mirror/tree/ABI_V1/contrib/Demo/Firework
and the "Simple blanker example" by Guido Mersmann and Michal Wozniak in the MorphOS SDK.

*/

#define  VERSION  1
#define  REVISION 0

#define BLANKERLIBNAME  "fireworks.btd"
#define BLANKERNAME     "Fireworks"
#define AUTHORNAME      "Stefan Blixth, OnyxSoft"
#define DESCRIPTION     "Fireworks effect"
#define BLANKERID       MAKE_ID('F','I','R','E')

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

#define DEF_FIREDELAY      0x03  // 50ms
char *DEF_CYFIREDELAY[]    = {"20", "30", "40", "50", "60", "70", "80", "90", "100", NULL};
#define FIREWORK_PARTICLES 4096 //1024 //

// blanker settings window definition
#define nTAG(o)            (BTD_Client + (o))
#define MYTAG_FIREDELAY    nTAG(0)

#define DATA_WIDTH   320
#define DATA_HEIGHT  200
#define DATA_SIZE    DATA_WIDTH * DATA_HEIGHT

static struct BTDCycle OBJ_FireDelay = {{MYTAG_FIREDELAY, "Flame delay (ms)", BTDPT_CYCLE}, DEF_FIREDELAY, DEF_CYFIREDELAY};

struct BTDNode *BlankerGUI[] =
{
   (APTR) &OBJ_FireDelay,
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

struct particle
{
    WORD speed;
    WORD counter;
    WORD ypos;
    WORD xpos;
    UBYTE particlecolor;
};
#define xspeed counter
#define yspeed speed

typedef struct particle particle;

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

   // Runtime data
   IPTR                 bd_StopRendering;
   ULONG                cgfx_coltab[256];
   struct               particle particles[FIREWORK_PARTICLES];
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
   static UBYTE pal[256*3];
   ULONG ecx = 1024;
   ULONG eax = 0xA000;
   ULONG temp;
   ULONG palindex = 0;

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

         bd->Active = FALSE;
         bd->firebuffer = AllocVecTaskPooled(DATA_WIDTH*(DATA_HEIGHT + 2));
         bd->renderdata = AllocVecTaskPooled((DATA_WIDTH*(DATA_HEIGHT + 2))*3); // RGB-data for rendering...

         debug_print("%s : %s (%d) | bd->Width = %d, bd->Height = %d\n", __FILE__ , __func__, __LINE__, bd->bd_Width, bd->bd_Height);

         if (bd->firebuffer)
         {
            // Init the colormap
            for(ecx = 1024; ecx; ecx--)
            {
               eax = (eax >> 8) | (eax & 0xFF) << 24;
               if ((eax & 255) >= 63)
               {
                  eax &= ~0xFF; eax |= 63;
                  temp = ((eax & 0xFF00) + 0x400) & 0xFF00;
                  eax &= ~0xFF00; eax |= temp;
               }

               if (ecx & 3)
               {
                  pal[palindex++] = (UBYTE)eax;
               }
            }

            for(palindex = 0; palindex < 256; palindex++)
            {
               ULONG red   = pal[palindex*3] * 4;
               ULONG green = pal[palindex*3+1] * 4;
               ULONG blue  = pal[palindex*3+2] * 4;

               bd->cgfx_coltab[palindex] = (red << 16) + (green << 8) + blue;
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

#define FIREWORK_RANDOM    ((WORD)(Random() & 0xFFFF))

/*=----------------------------- AnimMyBlanker() -----------------------------*
 *                                                                            *
 *----------------------------------------------------------------------------*/
void AnimMyBlanker(struct BlankerData *bd)
{
   static ULONG tick;
   static ULONG ebx;
   static ULONG si = 0x100;
   ULONG buffsize = (DATA_WIDTH * DATA_HEIGHT);

   WORD x, y;
   ULONG ecx;
   long cntr = 0, dpos = 0;
   unsigned long colrgb = 0;

   debug_print("%s : %s (%d) - PrevieMode : %d\n", __FILE__ , __func__, __LINE__, bd->bd_PreviewMode);

   bd->bd_TimerRequest.tr_time.tv_micro = bd->bd_Settings_FireDelay*10000;
   bd->bd_TimerRequest.tr_time.tv_secs  = 0;
   bd->bd_TimerRequest.tr_node.io_Command = TR_ADDREQUEST;

   DoIO((struct IORequest *) &bd->bd_TimerRequest);

   if(!bd->Active)
   {
      FillPixelArray(bd->bd_RastPort, bd->bd_Left, bd->bd_Top, bd->bd_Width, bd->bd_Height, 0x00000000);
      bd->Active = TRUE;
   }

   tick++;

   if (!(tick & 3))
   {
      // New explosion
      x = FIREWORK_RANDOM / 2;
      y = FIREWORK_RANDOM / 2;

      for(ecx = 256; ecx; ecx--)
      {
         struct particle *p;
         float f;

         WORD xspeed;
         WORD yspeed;

         p = &bd->particles[ebx];
         ebx = (ebx + 1) & (FIREWORK_PARTICLES - 1);

         p->speed = FIREWORK_RANDOM >> 6;
         p->counter = ecx;

         f = (float)((ULONG)p->speed + (ULONG)p->counter * 65536);

         p->ypos = y;

         xspeed = sin(f) * p->speed;
         yspeed = cos(f) * p->speed;

         p->xpos = x;

         p->particlecolor = 58;
         p->yspeed = ((UWORD)yspeed) * 2;
         p->xspeed = xspeed;
      }
   }

   // Draw/move all particles
   for(ecx = FIREWORK_PARTICLES; ecx; ecx--)
   {
      struct particle *p = &bd->particles[si];
      WORD xp, yp;

      p->yspeed += 16;
      p->ypos += p->yspeed; p->yspeed -= (p->yspeed / 16);
      p->xpos += p->xspeed; p->xspeed -= (p->xspeed / 16);

      yp = p->ypos / 256;
      xp = p->xpos / 128;

      if ((yp >= -99) && (yp <= 99) && (xp >= -159) && (xp <= 159))
      {
         ULONG dest = (yp + 100) * DATA_WIDTH + xp + 160;
         UBYTE col = p->particlecolor--;

         if (dest < buffsize)
         {
            if (col > bd->firebuffer[dest])
            {
               bd->firebuffer[dest] = col;
            }
         }
      }

      si = (si + 1) & (FIREWORK_PARTICLES - 1);
   }

   // blur
   for(ecx = 0; ecx < DATA_WIDTH * DATA_HEIGHT; ecx++)
      for(ecx = 0; ecx < buffsize; ecx++)
      {
         UBYTE col = bd->firebuffer[ecx];

         col += bd->firebuffer[ecx + 1];
         if (ecx >= DATA_WIDTH)
            col += bd->firebuffer[ecx - DATA_WIDTH];
         col += bd->firebuffer[ecx + DATA_WIDTH];
         col /= 4;

         if ((col > 0) && (tick & 1))
         {
            col--;
         }

         bd->firebuffer[ecx] = col;
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

      ScalePixelArray(bd->renderdata, DATA_WIDTH, DATA_HEIGHT, DATA_WIDTH*3, bd->bd_RastPort, 0, bd->bd_Top, bd->bd_Width, bd->bd_Height+20, RECTFMT_RGB);
   }
}
/*=*/

