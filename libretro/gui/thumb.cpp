/*
	modded for libretro-frodo
*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "libretro-core.h"

#include "graph.h"

typedef unsigned  int  PIXEL;

#define AVERAGE(a, b)   ( (((a) & 0xfffefefe) + ((b) & 0xfffefefe)) >> 1 )

void ScaleLine(PIXEL *Target, PIXEL *Source, int SrcWidth, int TgtWidth)
{
   int NumPixels = TgtWidth;
   int IntPart = SrcWidth / TgtWidth;
   int FractPart = SrcWidth % TgtWidth;
   int E = 0;

   while (NumPixels-- > 0)
   {
      *Target++ = *Source;
      Source += IntPart;
      E += FractPart;
      if (E >= TgtWidth)
      {
         E -= TgtWidth;
         Source++;
      } /* if */
   } /* while */
}

void ScaleRect(PIXEL *Target, PIXEL *Source, int SrcWidth, int SrcHeight,
      int TgtWidth, int TgtHeight)
{
   int NumPixels = TgtHeight;
   int IntPart = (SrcHeight / TgtHeight) * SrcWidth;
   int FractPart = SrcHeight % TgtHeight;
   int E = 0;
   PIXEL *PrevSource = NULL;

   while (NumPixels-- > 0)
   {
      if (Source == PrevSource)
         memcpy(Target, Target-TgtWidth, TgtWidth*sizeof(*Target));
      else
      {
         ScaleLine(Target, Source, SrcWidth, TgtWidth);
         PrevSource = Source;
      } /* if */
      Target += TgtWidth;
      Source += IntPart;
      E += FractPart;
      if (E >= TgtHeight)
      {
         E -= TgtHeight;
         Source += SrcWidth;
      } /* if */
   } /* while */
}

void ScaleMinifyByTwo(PIXEL *Target, PIXEL *Source, int SrcWidth, int SrcHeight)
{
   int x, y, x2, y2;
   int TgtWidth, TgtHeight;
   PIXEL p, q;

   TgtWidth = SrcWidth / 2;
   TgtHeight = SrcHeight / 2;
   for (y = 0; y < TgtHeight; y++)
   {
      y2 = 2 * y;
      for (x = 0; x < TgtWidth; x++)
      {
         x2 = 2 * x;
         p = AVERAGE(Source[y2*SrcWidth + x2], Source[y2*SrcWidth + x2 + 1]);
         q = AVERAGE(Source[(y2+1)*SrcWidth + x2], Source[(y2+1)*SrcWidth + x2 + 1]);
         Target[y*TgtWidth + x] = AVERAGE(p, q);
      } /* for */
   } /* for */
}

