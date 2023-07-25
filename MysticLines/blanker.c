/*
->========================================================<-
->= fxBlankers - MysticLines - © Copyright 2023 OnyxSoft =<-
->========================================================<-
->= Version  : 1.3                                       =<-
->= File     : blanker.c                                 =<-
->= Author   : Stefan Blixth                             =<-
->= Compiled : 2023-07-25                                =<-
->========================================================<-

Using line-render routine from flightcrank : https://github.com/flightcrank/demo-effects/blob/master/stars/stars_1/renderer.c
and June Tate-Gans from Nybbles and Bytes Mystify QBasic example from the live stream : https://youtu.be/bl0td3qV7WM?t=480
*/

#define  VERSION  1
#define  REVISION 3

#define BLANKERLIBNAME  "mysticlines.btd"
#define BLANKERNAME     "MysticLines"
#define AUTHORNAME      "Stefan Blixth, OnyxSoft"
#define DESCRIPTION     "Mystical line effect"
#define BLANKERID       MAKE_ID('M','Y','L','N')

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

#define DEF_LINECOUNT     0x02  // 20 lines
char *DEF_CYLINES[] = {"5", "10", "20", "30", "40", NULL};

#define DEF_LINETYPE      0x00 // Effect
char *DEF_CYTYPE[] = {"Separate", "Attached", NULL};

#define DEF_LINECOLOR      0x00 // Color
char *DEF_CYCOLOR[] = {"Static", "Cycle", NULL};

// blanker settings window definition
#define nTAG(o)          (BTD_Client + (o))
#define MYTAG_LINECOUNT  nTAG(0)
#define MYTAG_LINETYPE   nTAG(1)
#define MYTAG_LINECOLOR  nTAG(2)

#define DATA_WIDTH   320
#define DATA_HEIGHT  200
#define DATA_SIZE    DATA_WIDTH * DATA_HEIGHT
#define RENDER_RGB   3
#define RENDER_RGBA  4

#define MaxNumVectors   50

#define ABS(a) ((a)>0?(a):-(a))

static struct BTDCycle OBJ_LineCount = {{MYTAG_LINECOUNT, "Lines", BTDPT_CYCLE }, DEF_LINECOUNT, DEF_CYLINES};
static struct BTDCycle OBJ_LineType = {{MYTAG_LINETYPE, "Type", BTDPT_CYCLE }, DEF_LINETYPE, DEF_CYTYPE};
static struct BTDCycle OBJ_LineColor = {{MYTAG_LINECOLOR, "Color", BTDPT_CYCLE }, DEF_LINECOLOR, DEF_CYCOLOR};

struct BTDNode *BlankerGUI[] =
{
   (APTR) &OBJ_LineCount,
   (APTR) &OBJ_LineType,
   (APTR) &OBJ_LineColor,
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
   IPTR                 bd_Settings_LineCount;
   IPTR                 bd_Settings_LineType;
   IPTR                 bd_Settings_LineColor;

   // Runtime data
   IPTR                 bd_StopRendering;
   BOOL                 Active;
   unsigned char        *renderdata;
   int                  x[MaxNumVectors];
   int                  y[MaxNumVectors];
   int                  ox[MaxNumVectors];
   int                  oy[MaxNumVectors];
   int                  dx[MaxNumVectors];
   int                  dy[MaxNumVectors];
   int                  col[MaxNumVectors];
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
   int cnt = 0;

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
         bd->bd_Settings_LineCount  = GetTagData(MYTAG_LINECOUNT, (ULONG) DEF_LINECOUNT, TagList) + 1;
         bd->bd_Settings_LineType   = GetTagData(MYTAG_LINETYPE,  (ULONG) DEF_LINETYPE, TagList) + 1;
         bd->bd_Settings_LineColor  = GetTagData(MYTAG_LINECOLOR, (ULONG) DEF_LINECOLOR, TagList) + 1;

         bd->Active = FALSE;
         bd->renderdata = AllocVecTaskPooled((DATA_WIDTH*(DATA_HEIGHT+4))*3); //Render_Alloc(DATA_WIDTH, DATA_HEIGHT+4, RENDER_RGB);

         if (bd->renderdata)
         {
            // Init random positions and colors...
            for (cnt = 0; cnt < MaxNumVectors; cnt++)
            {
               bd->x[cnt] = Random() % DATA_WIDTH;
               bd->y[cnt] = Random() % DATA_HEIGHT;
               bd->dx[cnt] = -1;
               bd->dy[cnt] = -1;
               bd->col[cnt] = (Random() % 0x00ffffff);
            }

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
   int vectors, i, j;

   debug_print("%s : %s (%d) - PrevieMode : %d\n", __FILE__ , __func__, __LINE__, bd->bd_PreviewMode);

   bd->bd_TimerRequest.tr_time.tv_micro = 5000;
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
      if (bd->bd_Settings_LineCount >= 1)
         vectors = (bd->bd_Settings_LineCount*10);
      else
         vectors = 5;

      for (i = 0; i < vectors; ((bd->bd_Settings_LineType==1) ? i+=2 : i++)) // Type : 1 = Separate, 2 = Attached...
      {
         if (bd->bd_Settings_LineType == 1)
            j = (i+1);
         else
            j = ((i+1) % vectors);

         Render_Line(bd, bd->ox[i], bd->oy[i], bd->ox[j], bd->oy[j], 0xff000000);
         Render_Line(bd, bd->x[i], bd->y[i], bd->x[j], bd->y[j], ((bd->bd_Settings_LineColor==1) ? bd->col[i] : (Random() % 0x00ffffff)));
      }

      for (i = 0; i < vectors; i++)
      {
         bd->ox[i] = bd->x[i];
         bd->oy[i] = bd->y[i];

         bd->x[i] = bd->x[i] + bd->dx[i];
         bd->y[i] = bd->y[i] + bd->dy[i];

         if ((bd->x[i] < 1) || (bd->x[i] > DATA_WIDTH-2))
            bd->dx[i] = -bd->dx[i];

         if ((bd->y[i] < 1) || (bd->y[i] > DATA_HEIGHT-2))
            bd->dy[i] = -bd->dy[i];
      }

      ScalePixelArray(bd->renderdata, DATA_WIDTH, DATA_HEIGHT, DATA_WIDTH*3, bd->bd_RastPort, 0, 0, bd->bd_Width, bd->bd_Height, RECTFMT_RGB);
   }
}
/*=*/

