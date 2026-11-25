/*!   
   Copyright (c) OnyxSoft 2010

   \file    debug.h
   \brief   Take care and prints out our debug data for us 

   \author  Stefan Blixth
   \date    2010-06-29
   \version 0.1

   \todo    -
*/

 
#ifndef DEBUG_H_
#define DEBUG_H_

#ifndef __MORPHOS__
   #include <stdio.h>
   #ifdef USEDEBUG
      #include "SDI_stdarg.h"
   #endif
#endif

#ifdef USEDEBUG
   #ifdef __MORPHOS__
      #include <clib/debug_protos.h>
      #define debug_print(args...) { KPrintF((CONST_STRPTR)args); }
   #else
      #define debug_print(args...) { fprintf(stderr, args); }
   #endif
#else
   #define debug_print(...)
#endif

#endif /* DEBUG_H_ */