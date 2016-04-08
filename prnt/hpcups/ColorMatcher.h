/*****************************************************************************\
  colormatch.h : Interface for the ColorMatcher class

  Copyright (c) 1996 - 2015, HP Co.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
  3. Neither the name of HP nor the names of its
     contributors may be used to endorse or promote products derived
     from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR IMPLIED
  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
  NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
  TO, PATENT INFRINGEMENT; PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
  ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
\*****************************************************************************/

#ifndef COLORMATCHER_H
#define COLORMATCHER_H

#define TESTMODE 0
#define MAP17CUBE 1
#define OCT_MASK1 0x10
#define OCT_MASK2 0x8
#define OCT_MASK3 0x4
#define OCT_MASK4 0x2
#define OCT_MASK5 0x1

class ColorMatcher : public Processor
{
public:
    ColorMatcher(ColorMap cm, unsigned int DyeCount,
                 unsigned int iInputWidth);
    virtual ~ColorMatcher();

    bool Process(RASTERDATA* InputRaster=NULL);
    void Flush();
    DRIVER_ERROR constructor_error;
    void Restart();         // set up for new page or blanks

    // items required by Processor
    unsigned int GetMaxOutputWidth();
    bool NextOutputRaster(RASTERDATA &next_raster);

    unsigned int ColorPlaneCount;
    unsigned int InputWidth;    // # of pixels input
    unsigned int StartPlane;    // since planes are ordered KCMY, if no K, this is 1
    unsigned int EndPlane;      // usually Y, could be Mlight
    unsigned int ResBoost;

    DRIVER_ERROR MakeGrayMap(const uint32_t *colormap, uint32_t* graymap);

private:
    void FreeBuffers();
    void ColorMatch(unsigned long width, const uint32_t *map,
                     unsigned char *rgb, unsigned char *kplane,
		     unsigned char *cplane, unsigned char *mplane,
                     unsigned char *yplane);
    void Interpolate(const uint32_t *map,
                     unsigned char r,unsigned char g,unsigned char b,
                     unsigned char *blackout, unsigned char *cyanout,
                     unsigned char *magentaout, unsigned char *yellowout);

    void ColorMatch(unsigned long width, const unsigned char *map,
                    unsigned char *rgb, unsigned char *kplane,
		    unsigned char *cplane, unsigned char *mplane,
                    unsigned char *yplane);
    void Interpolate(const unsigned char *map,
                     unsigned char r,unsigned char g,unsigned char b,
                     unsigned char *blackout, unsigned char *cyanout,
                     unsigned char *magentaout, unsigned char *yellowout);


    inline unsigned char GetYellowValue(uint32_t cmyk)
    { return( ((unsigned char)((cmyk)>>24) & 0xFF) ); }

    inline unsigned char GetMagentaValue(uint32_t cmyk)
    { return( ((unsigned char)((cmyk)>>16) & 0xFF) ); }

    inline unsigned char GetCyanValue(uint32_t cmyk)
    { return( ((unsigned char)(((int)(cmyk))>>8) & 0xFF) ); }

    inline unsigned char GetBlackValue(uint32_t cmyk)
    { return( ((unsigned char)(cmyk) & 0xFF) ); }

    bool Forward16PixelsNonWhite(BYTE *inputPtr)
    {
//        return ((*(uint32_t *)(inputPtr) != 0x0) || (*(((uint32_t *)(inputPtr)) + 1) != 0x0)  ||
//            (*(((uint32_t *)(inputPtr)) + 2) != 0x0) || (*(((uint32_t *)(inputPtr)) + 3) != 0x0));
    for (int i=0; i < 16; i++)
    {
        if ((*inputPtr++)!=0)
            return true;
    }

    return false;
    }

    bool Backward16PixelsNonWhite(BYTE *inputPtr)
    {
//        return ((*(uint32_t *)(inputPtr) != 0x0) || (*(((uint32_t *)(inputPtr)) - 1) != 0x0)  ||
//            (*(((uint32_t *)(inputPtr)) - 2) != 0x0) || (*(((uint32_t *)(inputPtr)) - 3) != 0x0));
    for (int i=0; i < 16; i++)
    {
        if ((*inputPtr--)!=0)
            return true;
    }

    return false;
    }


    unsigned char* Contone; // containing byte-per-pixel CMYK values

    ColorMap cmap;

//    void PixelMultiply(unsigned char* buffer, unsigned int width, unsigned int factor);

    unsigned int PlaneCount();          // tells how many layers (colors,hifipe,multirow)

    bool started;

}; // ColorMatcher

#endif  // COLORMATCHER_H

