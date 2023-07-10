/*
->=========================================================<-
->= fxBlankers - RandomPixels - © Copyright 2023 OnyxSoft =<-
->=========================================================<-
->= Version  : 1.2                                        =<-
->= File     : blanker.c                                  =<-
->= Author   : Stefan Blixth                              =<-
->= Compiled : 2023-07-10                                 =<-
->=========================================================<-

Using line-render routine from flightcrank : https://github.com/flightcrank/demo-effects/blob/master/stars/stars_1/renderer.c
and the "Simple blanker example" by Guido Mersmann and Michal Wozniak in the MorphOS SDK.

*/

#define  VERSION  1
#define  REVISION 2

#define BLANKERLIBNAME  "RandomPixels.btd"
#define BLANKERNAME     "RandomPixels"
#define AUTHORNAME      "Stefan Blixth, OnyxSoft"
#define DESCRIPTION     "Random pixel effect"
#define BLANKERID       MAKE_ID('R','N','P','X')

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

#define DEF_RANDDELAY     0x03  // 100ms
char *DEF_CYDELAY[] = {"25", "50", "75", "100", "125", "150", "175", "200", "250", NULL};

#define DEF_RANDTHEME   0x00 // Effect
char *DEF_CYTHEME[] = {"Boxes", "Lines", "Pixels", NULL};

// blanker settings window definition
#define nTAG(o)          (BTD_Client + (o))
#define MYTAG_RANDDELAY  nTAG(0)
#define MYTAG_RANDTHEME  nTAG(1)

#define DATA_WIDTH   320
#define DATA_HEIGHT  200
#define DATA_SIZE    DATA_WIDTH * DATA_HEIGHT
#define RENDER_RGB   3
#define RENDER_RGBA  4

#define ABS(a) ((a)>0?(a):-(a))

static struct BTDCycle OBJ_RandDelay = {{MYTAG_RANDDELAY, "Refresh delay (ms)", BTDPT_CYCLE }, DEF_RANDDELAY, DEF_CYDELAY};
static struct BTDCycle OBJ_RandTheme = {{MYTAG_RANDTHEME, "Effect", BTDPT_CYCLE }, DEF_RANDTHEME, DEF_CYTHEME};

struct BTDNode *BlankerGUI[] =
{
   (APTR) &OBJ_RandDelay,
   (APTR) &OBJ_RandTheme,
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
   IPTR                 bd_Settings_RandDelay;
   IPTR                 bd_Settings_RandTheme;

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

/*=----------------------------- Render_Line() -------------------------------*
 *                                                                            *
 *----------------------------------------------------------------------------*/
void Render_Line(struct BlankerData *bd, unsigned int x0, unsigned int y0, unsigned int x1, unsigned int y1, unsigned long pen)
{
   int dx, dy;

   Render_Pixel(bd, x0, y0, pen);
   
   if (x0 > x1)
   {
      int temp_x = x0;
      int temp_y = y0;

      x0 = x1;
      y0 = y1;

      x1 = temp_x;
      y1 = temp_y;
   }

   dx = x1 - x0;
   dy = y1 - y0;

   //the length of the line is greater along the X axis
   if (dx >= ABS(dy)) 
   {
      float slope = (float) dy / dx;

      //line travels from top to bottom
      if (y0 <= y1)
      {

         float ideal_y = y0 + slope;
         int y = (int) round(ideal_y);
         float error = ideal_y - y;

         int i = 0;

         //loop through all the X values
         for(i = 1; i <= dx; i++)
         {
            int x = x0 + i;

            Render_Pixel(bd, x, y, pen);

            error += slope;

            if (error  >= 0.5)
            {
               y++;
               error -= 1;
            }
         }
      }

      //line travels from bottom to top
      if (y0 > y1)
      {
         float ideal_y = y0 + slope;
         int y = (int) round(ideal_y);
         float error = ideal_y - y;

         int i = 0;

         //loop through all the x values
         for(i = 1; i <= dx; i++)
         {
            int x = x0 + i;

            Render_Pixel(bd, x, y, pen);

            error += slope;

            if (error  <= -0.5)
            {
               y--;
               error += 1;
            }
         }
      }
   }

   //the length of the line is greater along the Y axis
   if (ABS(dy) > dx)
   {
      float slope = (float) dx / dy;

      //line travels from top to bottom
      if (y0 < y1)
      {
         float ideal_x = x0 + slope;
         int x = (int) round(ideal_x);
         float error = ideal_x - x;

         int i = 0;

         //loop through all the y values
         for(i = 1; i <= dy; i++)
         {
            int y = y0 + i;

            Render_Pixel(bd, x, y, pen);

            error += slope;

            if (error  >= 0.5)
            {
               x++;
               error -= 1;
            }
         }
      }

      //draw line from bottom to top
      if (y0 > y1)
      {
         float ideal_x = x0 - slope;
         int x = (int) round(ideal_x);
         float error = ideal_x - x;

         int i = 0;

         //loop through all the y values
         for(i = 1; i <= ABS(dy); i++)
         {
            int y = y0 - i;

            Render_Pixel(bd, x, y, pen);

            error += slope;

            if (error  <= -0.5)
            {
               x++;
               error += 1;
            }
         }
      }
   }
}
/*=*/

/*=----------------------------- Render_Square() -----------------------------*
 *                                                                            *
 *----------------------------------------------------------------------------*/
void Render_Square(struct BlankerData *bd, unsigned int x0, unsigned int y0, unsigned int x1, unsigned int y1, unsigned long pen)
{
   unsigned long rpos = 0;
   
   for (rpos = y0; rpos <= y1; rpos++)
   {
      Render_Line(bd, x0, rpos, x1, rpos, pen);
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
         bd->bd_Settings_RandDelay   = GetTagData(MYTAG_RANDDELAY, (ULONG) DEF_RANDDELAY, TagList) + 1;
         bd->bd_Settings_RandTheme   = GetTagData(MYTAG_RANDTHEME, (ULONG) DEF_RANDTHEME, TagList) + 1;

         bd->Active = FALSE;
         bd->renderdata = AllocVecTaskPooled((DATA_WIDTH*(DATA_HEIGHT+4))*3); //Render_Alloc(DATA_WIDTH, DATA_HEIGHT+4, RENDER_RGB);

         if (bd->renderdata)
         {
            Render_Fill(bd, 0xff000000);
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
   //int x1, x2, y1, y2, xp, yp, xc, yc, x, y;
   //unsigned long color;
   int frame;
   static double frame_counter = 0;
   float deltatime = 5.0f;

   frame_counter += deltatime * 100;
   frame = frame_counter;


   debug_print("%s : %s (%d) - PrevieMode : %d\n", __FILE__ , __func__, __LINE__, bd->bd_PreviewMode);

   bd->bd_TimerRequest.tr_time.tv_micro = (bd->bd_Settings_RandDelay*25)*1000;
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
      // Draw the Random effect
      switch(bd->bd_Settings_RandTheme)
      {
         case 2 : // Line...
            Render_Line(bd, (Random() % DATA_WIDTH), (Random() % DATA_HEIGHT), (Random() % DATA_WIDTH), (Random() % DATA_HEIGHT), (Random() % 0xffffffff));
            break;

         case 3 : // Pixels...
            Render_Pixel(bd, (Random() % DATA_WIDTH), (Random() % DATA_HEIGHT), (Random() % 0xffffffff));
            break;

         default : // Boxes...
            Render_Square(bd, (Random() % DATA_WIDTH), (Random() % DATA_HEIGHT), (Random() % DATA_WIDTH), (Random() % DATA_HEIGHT), (Random() % 0xffffffff));
            break;
      }

      ScalePixelArray(bd->renderdata, DATA_WIDTH, DATA_HEIGHT, DATA_WIDTH*3, bd->bd_RastPort, 0, 0, bd->bd_Width, bd->bd_Height, RECTFMT_RGB);
   }
}
/*=*/

