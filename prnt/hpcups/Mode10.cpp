/*****************************************************************************\
  Mode10.cpp : Implementation of Mode10 class

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

#include "Mode10.h"

Mode10::Mode10 (unsigned int PlaneSize) : Compressor (PlaneSize, true)
{
    if (constructor_error != NO_ERROR)  // if error in base constructor
    {
        return;
    }

    union
    {
        short    s;
        char     c[2];
    } uEndian;
    uEndian.s = 0x0A0B;
    m_eEndian = LITTLEENDIAN;
    if (uEndian.c[0] == 0x0A)
        m_eEndian = BIGENDIAN;

	// In the worst case, compression expands data by 50%
	compressBuf = new BYTE[(PlaneSize + PlaneSize/2)];
	if (compressBuf == NULL)
		constructor_error = ALLOCMEM_ERROR;

    memset (SeedRow, 0xFF, PlaneSize);
} //Mode10

Mode10::~Mode10()
{ }

void Mode10::Flush ()
{
    if (!seeded)
    {
        return;
    }
	compressedsize = 0;
    iRastersReady = 0;
    seeded = false;
    memset (SeedRow, 0xFF, inputsize);
} //Flush

inline uint32_t Mode10::get3Pixel (BYTE *pixAddress, int pixelOffset)
{
    pixAddress += ((pixelOffset << 1) + pixelOffset);     //pixAddress += pixelOffset * 3;

    BYTE r = *(pixAddress);
    BYTE g = *(pixAddress + 1);
    BYTE b = *(pixAddress + 2);

    return (kWhite & ((r << 16) | (g << 8) | (b)));

} //get3Pixel

void Mode10::put3Pixel (BYTE *pixAddress, int pixelOffset, uint32_t pixel)
{
    pixAddress += ((pixelOffset << 1) + pixelOffset);     //pixAddress += pixelOffset * 3;

    unsigned int temp = (pixel & kWhite);

    *(pixAddress)     = ((temp >> 16) & 0x000000FF);
    *(pixAddress + 1) = ((temp >> 8) & 0x000000FF);
    *(pixAddress + 2) = (temp & 0x000000FF);

} //put3Pixel


unsigned short Mode10::ShortDelta (uint32_t lastPixel, uint32_t lastUpperPixel)
{
    int dr, dg, db;
    int result;

    dr = GetRed (lastPixel) - GetRed (lastUpperPixel);
    dg = GetGreen (lastPixel) - GetGreen (lastUpperPixel);
    db = GetBlue (lastPixel) - GetBlue (lastUpperPixel);

    if ((dr <= 15) && (dr >= -16) && (dg <= 15) && (dg >= -16) && (db <= 30) && (db >= -32))
    {   // Note db is divided by 2 to double it's range from -16..15 to -32..30
        result = ((dr << 10) & 0x007C00) | (((dg << 5) & 0x0003E0) | ((db >> 1) & 0x01F) | 0x8000);   // set upper bit to signify short delta
    }
    else
    {
        result = 0;  // upper bit is zero to signify delta won't work
    }

    return (unsigned short) result;
}

bool Mode10::Process (RASTERDATA *input)
/****************************************************************************
Initially written by Elden Wood
August 1998

Similar to mode 9, though tailored for pixel data.
For more information see the Bert Compression Format document.

This function compresses a single row per call.
****************************************************************************/
{
    if (input == NULL || 
       (input->rasterdata[COLORTYPE_COLOR] == NULL && input->rasterdata[COLORTYPE_BLACK] == NULL))    // flushing pipeline
    {
        Flush ();
        return false;
    }

    if (myplane == COLORTYPE_BLACK || input->rasterdata[COLORTYPE_COLOR]==NULL)
    {
        iRastersReady = 1;
        compressedsize = 0;
        return true;
    }
    unsigned int originalsize = input->rastersize[myplane];
    unsigned int size = input->rastersize[myplane];

    unsigned char *seedRowPtr = (unsigned char *) SeedRow;

    unsigned char *compressedDataPtr = compressBuf;
    unsigned char *curRowPtr =  (unsigned char *) input->rasterdata[myplane];
    unsigned int rowWidthInBytes = size;
    ASSERT(curRowPtr);
    ASSERT(seedRowPtr);
    ASSERT(compressedDataPtr);
    ASSERT(rowWidthInBytes >= BYTES_PER_PIXEL);
    ASSERT((rowWidthInBytes % BYTES_PER_PIXEL) == 0);

    unsigned char *compressedDataStart = compressedDataPtr;
    unsigned int lastPixel = (rowWidthInBytes / BYTES_PER_PIXEL) - 1;

    // Setup sentinal value to replace last pixel of curRow. Simplifies future end condition checking.
    uint32_t realLastPixel = getPixel(curRowPtr, lastPixel);

    uint32_t newLastPixel = realLastPixel;
    while ((getPixel (curRowPtr, lastPixel - 1) == newLastPixel) ||
           (getPixel (seedRowPtr, lastPixel) == newLastPixel))
    {
        putPixel (curRowPtr, lastPixel, newLastPixel += 0x100); // add one to green.
    }
    unsigned int curPixel = 0;
    unsigned int seedRowPixelCopyCount;
    unsigned int cachedColor = kWhite;

    do // all pixels in row
    {
        unsigned char CMDByte = 0;
        int replacementCount;

        // Find seedRowPixelCopyCount for upcoming copy
        seedRowPixelCopyCount = curPixel;
        while (getPixel (seedRowPtr, curPixel) == getPixel (curRowPtr, curPixel))
        {
            curPixel++;
        }

        seedRowPixelCopyCount = curPixel - seedRowPixelCopyCount;
        ASSERT (curPixel <= lastPixel);

        int pixelSource = 0;

        if (curPixel == lastPixel) // On last pixel of row. RLE could also leave us on the last pixel of the row from the previous iteration.
        {
            putPixel(curRowPtr, lastPixel, realLastPixel);

            if (getPixel(seedRowPtr, curPixel) == realLastPixel)
            {
                goto mode10rtn;
            }
            else // code last pix as a literal
            {

                CMDByte = eLiteral;
                pixelSource = eeNewPixel;
                replacementCount = 1;
                curPixel++;
            }
        }
        else // prior to last pixel of row
        {
            ASSERT(curPixel < lastPixel);

            replacementCount = curPixel;
            uint32_t RLERun = getPixel (curRowPtr, curPixel);

            curPixel++; // Adjust for next pixel.
            while (RLERun == getPixel (curRowPtr, curPixel)) // RLE
            {
                curPixel++;
            }
            curPixel--; // snap back to current.
            replacementCount = curPixel - replacementCount;
            ASSERT(replacementCount >= 0);

            if (replacementCount > 0) // Adjust for total occurance and move to next pixel to do.
            {
                curPixel++;
                replacementCount++;

                if (cachedColor == RLERun)
                {
                    pixelSource = eeCachedColor;
                }
                else if (getPixel (seedRowPtr, curPixel-replacementCount + 1) == RLERun)
                {
                    pixelSource = eeNEPixel;
                }
                else if ((curPixel-replacementCount > 0) &&
                         (getPixel (curRowPtr, curPixel-replacementCount - 1) == RLERun))
                {
                    pixelSource = eeWPixel;
                }
                else
                {
                    pixelSource = eeNewPixel;
                    cachedColor = RLERun;
                }

                CMDByte = eRLE; // Set default for later.

            }

            if (curPixel == lastPixel)
            {
                ASSERT(replacementCount > 0); // Already found some RLE pixels

                if (realLastPixel == RLERun) // Add to current RLE. Otherwise it'll be part of the literal from the seedrow section above on the next iteration.
                {
                    putPixel (curRowPtr, lastPixel, realLastPixel);
                    replacementCount++;
                    curPixel++;
                }
            }

            if (0 == replacementCount) // no RLE so it's a literal by default.
            {
                uint32_t tempPixel = getPixel (curRowPtr, curPixel);

                ASSERT(tempPixel != getPixel (curRowPtr, curPixel + 1)); // not RLE
                ASSERT(tempPixel != getPixel (seedRowPtr, curPixel)); // not seedrow copy

                CMDByte = eLiteral;

                if (cachedColor == tempPixel)
                {
                    pixelSource = eeCachedColor;

                }
                else if (getPixel (seedRowPtr, curPixel + 1) == tempPixel)
                {
                    pixelSource = eeNEPixel;

                }
                else if ((curPixel > 0) &&  (getPixel (curRowPtr, curPixel-1) == tempPixel))
                {
                    pixelSource = eeWPixel;

                }
                else
                {

                    pixelSource = eeNewPixel;
                    cachedColor = tempPixel;
                }

                replacementCount = curPixel;
                uint32_t cachePixel;
                uint32_t nextPixel = getPixel (curRowPtr, curPixel+1);
                do
                {
                    if (++curPixel == lastPixel)
                    {
                        putPixel (curRowPtr, lastPixel, realLastPixel);
                        curPixel++;
                        break;
                    }
                    cachePixel = nextPixel;
                }
                while ((cachePixel != (nextPixel = getPixel (curRowPtr, curPixel+1))) &&
                       (cachePixel != getPixel (seedRowPtr, curPixel)));

                replacementCount = curPixel - replacementCount;

                ASSERT(replacementCount > 0);
            }
        }

        //ASSERT(seedRowPixelCopyCount >= 0);

        // Write out compressed data next.
        if (eLiteral == CMDByte)
        {
            ASSERT(replacementCount >= 1);

            replacementCount -= 1; // normalize it

            CMDByte |= pixelSource; // Could put this directly into CMDByte above.
            CMDByte |= MIN(3, seedRowPixelCopyCount) << 3;
            CMDByte |= MIN(7, replacementCount);

            *compressedDataPtr++ = CMDByte;

            if (seedRowPixelCopyCount >= 3)
            {
                outputVLIBytesConsecutively (seedRowPixelCopyCount - 3, compressedDataPtr);
            }

            replacementCount += 1; // denormalize it

            int totalReplacementCount = replacementCount;
            int upwardPixelCount = 1;

            if (eeNewPixel != pixelSource)
            {
                replacementCount -= 1; // Do not encode 1st pixel of run since it comes from an alternate location.
                upwardPixelCount = 2;
            }

            for ( ; upwardPixelCount <= totalReplacementCount; upwardPixelCount++)
            {
                ASSERT(totalReplacementCount >= upwardPixelCount);

                unsigned short compressedPixel = ShortDelta (getPixel (curRowPtr, curPixel - replacementCount),
                                                             getPixel (seedRowPtr, curPixel - replacementCount));
                if (compressedPixel)
                {
                    *compressedDataPtr++ = compressedPixel >> 8;
                    *compressedDataPtr++ = (unsigned char)compressedPixel;

                }
                else
                {
                    uint32_t uncompressedPixel = getPixel (curRowPtr, curPixel - replacementCount);

                    uncompressedPixel >>= 1; // Lose the lsb of blue and zero out the msb of the 3 bytes.

                    *compressedDataPtr++ = (BYTE) (uncompressedPixel >> 16);
                    *compressedDataPtr++ = (BYTE) (uncompressedPixel >> 8);
                    *compressedDataPtr++ = (BYTE) (uncompressedPixel);

                }

                if (((upwardPixelCount-8) % 255) == 0)  // See if it's time to spill a single VLI byte.
                {
                    *compressedDataPtr++ = MIN(255, totalReplacementCount - upwardPixelCount);
                }

                replacementCount--;
            }
        }
        else // RLE
        {
            ASSERT(replacementCount >= 2);

            replacementCount -= 2; // normalize it

            CMDByte |= pixelSource; // Could put this directly into CMDByte above.
            CMDByte |= MIN(3, seedRowPixelCopyCount) << 3;
            CMDByte |= MIN(7, replacementCount);

            *compressedDataPtr++ = CMDByte;

            if (seedRowPixelCopyCount >= 3)
            {
                outputVLIBytesConsecutively (seedRowPixelCopyCount - 3, compressedDataPtr);
            }

            replacementCount += 2; // denormalize it

            if (eeNewPixel == pixelSource)
            {
                unsigned short compressedPixel = ShortDelta(getPixel (curRowPtr, curPixel - replacementCount),
                                                            getPixel (seedRowPtr, curPixel - replacementCount));
                if (compressedPixel)
                {
                    *compressedDataPtr++ = compressedPixel >> 8;
                    *compressedDataPtr++ = (unsigned char) compressedPixel;
                }
                else
                {
                    uint32_t uncompressedPixel = getPixel (curRowPtr, curPixel - replacementCount);

                    uncompressedPixel >>= 1;

                    *compressedDataPtr++ = (BYTE) (uncompressedPixel >> 16);
                    *compressedDataPtr++ = (BYTE) (uncompressedPixel >> 8);
                    *compressedDataPtr++ = (BYTE) (uncompressedPixel);
                }
            }

            if (replacementCount - 2 >= 7) outputVLIBytesConsecutively (replacementCount - (7+2), compressedDataPtr);
        }
    } while (curPixel <= lastPixel);
mode10rtn:
    size = static_cast<int>(compressedDataPtr - compressedDataStart); // return # of compressed bytes.
    compressedsize = size;
    memcpy (SeedRow, input->rasterdata[myplane], originalsize);
    seeded = true;
    iRastersReady = 1;
    return true;
} //Process

bool Mode10::NextOutputRaster (RASTERDATA& next_raster)
{
    if (iRastersReady == 0)
    {
        return false;
    }

    if (myplane == COLORTYPE_COLOR && compressedsize != 0)
    {
        next_raster.rastersize[COLORTYPE_COLOR] = compressedsize;
        next_raster.rasterdata[COLORTYPE_COLOR] = compressBuf;
    }
    else
    {
        next_raster.rastersize[COLORTYPE_COLOR] = 0;
        next_raster.rasterdata[COLORTYPE_COLOR] = raster.rasterdata[COLORTYPE_COLOR];
    }

    next_raster.rastersize[COLORTYPE_BLACK] = raster.rastersize[COLORTYPE_BLACK];
    next_raster.rasterdata[COLORTYPE_BLACK] = raster.rasterdata[COLORTYPE_BLACK];

    iRastersReady = 0;
    return true;
}

