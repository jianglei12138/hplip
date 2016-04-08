/*****************************************************************************\
  Mode10.h : Interface for the Mode10 class

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
 
#ifndef MODE10_H
#define MODE10_H

#include "CommonDefinitions.h"
#include "Processor.h"
#include "Compressor.h"

//#define kUseOldBert 0

#define kBertDecompressPixelSize 3

// Follows are all the masks for the command byte.
#define kTypeMask           0x80
#define kTypeShiftAmount    7

#define kCacheLiteralBitsMask 0x60
#define kCacheLiteralBitsShiftAmount 5

#define kCacheBitsMask 0x60
#define kCacheBitsShiftAmount 5

#define kRoffMask           0x18
#define kRoffShiftAmount    3

#define kReplace_countMask  0x07

// Now have the compiler check to make sure none of the masks overlap/underlap bits accidently.
#if ((kTypeMask | kCacheLiteralBitsMask | kRoffMask | kReplace_countMask) != 255)
#error "Your mask bits are messed up!"
#endif

#if ((kTypeMask | kCacheBitsMask | kRoffMask | kReplace_countMask) != 255)
#error "Your mask bits are messed up!"
#endif

enum
{
    eLiteral = 0,
    eRLE = 0x80
};

enum
{
    eeNewPixel = 0x0,
    eeWPixel = 0x20,
    eeNEPixel = 0x40,
    eeCachedColor = 0x60
};

enum
{
    eNewColor       = 0x0,
    eWestColor      = 0x1,
    eNorthEastColor = 0x2,
    eCachedColor    = 0x3
};

// Literal
#define M10_MAX_OFFSET0         2       /* Largest unscaled value an offset can have before extra byte is needed. */
#define M10_MAX_COUNT0          6       /* Largest unscaled and unbiased value a count can have before extra byte is needed */
#define M10_COUNT_START0        1       /* What a count of zero has a value of. */

// RLE
#define M10_MAX_OFFSET1         2
#define M10_MAX_COUNT1          6
#define M10_COUNT_START1        2

#define getPixel get3Pixel
#define putPixel put3Pixel
#define BYTES_PER_PIXEL 3

class Mode10 : public Compressor 
{
    public:
        Mode10 (unsigned int RasterSize);
        virtual ~Mode10 ();
        //!@todo: need to fix this so it remains virtual????
        //~Mode10();
        bool Process (RASTERDATA *input);
        bool NextOutputRaster (RASTERDATA& next_raster);
        void Flush ();

    private:
        inline uint32_t get4Pixel (unsigned char *pixAddress, bool bPrint = false)
        {
            if (m_eEndian == LITTLEENDIAN)
                return (((unsigned int*)pixAddress)[0]) & kWhite;
            else
                return (((unsigned int*)pixAddress)[0]) & 0xFFFFFF00;
        }


        inline uint32_t get4Pixel (unsigned char *pixAddress, int pixelOffset)
        {
            if (m_eEndian == LITTLEENDIAN)
                return ((unsigned int*)pixAddress)[pixelOffset] & kWhite;
            else
                return ((unsigned int*)pixAddress)[pixelOffset] & 0xFFFFFF00;
        }


        inline void put4Pixel (unsigned char *pixAddress, int pixelOffset, uint32_t pixel)
        {
            if (m_eEndian == LITTLEENDIAN)
                (((unsigned int*)pixAddress)[pixelOffset] = pixel & kWhite);
            else
                (((unsigned int*)pixAddress)[pixelOffset] = pixel & 0xFFFFFF00);
        }


        inline void outputVLIBytesConsecutively (int number, unsigned char *&compressedDataPtr)
        {
            do
            {
                *compressedDataPtr++ = MIN(number, 255);
                if (255 == number)
                {
                    *compressedDataPtr++ = 0;
                }
                number -= MIN(number,255);
            } while (number);
        }


        void        put3Pixel (BYTE* pixAddress, int pixelOffset, uint32_t pixel);
        inline      uint32_t get3Pixel (BYTE* pixAddress, int pixelOffset);
        unsigned    short ShortDelta (uint32_t lastPixel, uint32_t lastUpperPixel);
        ENDIAN_TYPE m_eEndian;


}; // Mode10

#endif // MODE10_H

