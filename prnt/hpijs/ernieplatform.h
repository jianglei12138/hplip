/*****************************************************************************\
  ernieplatform.h : Interface for ernie

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


APDK_BEGIN_NAMESPACE

// Endianess of the architecture we're on
#ifdef APDK_LITTLE_ENDIAN
    #define bigEndian       0
    #define littleEndian    1
    #define GetRed(x) (((x >> 16) & 0x0FF))
    #define GetGreen(x) (((x >> 8) & 0x0FF))
    #define GetBlue(x) ((x & 0x0FF))
#else
    #define bigEndian       1
    #define littleEndian    0
    #define GetRed(x) (((x >> 24) & 0x0FF))
    #define GetGreen(x) (((x >> 16) & 0x0FF))
    #define GetBlue(x) (((x >> 8) & 0x0FF))
#endif

/*#define GetRed(x) (((x >> 24) & 0x0FF))
#define GetGreen(x) (((x >> 16) & 0x0FF))
#define GetBlue(x) (((x >> 8) & 0x0FF))*/

// Slow down the system and gather stats or not.
#define kGatherStats    0
#define kDecompressStats 0

// ********** THIS IS NOT TRUE **************
// While it may seem like the need to be both
// big endian and little endian versions of
// these macros, due to the face that the
// shift command in general knows about the
// endian order, you get the same result
// from these commands no matter what order
// the bytes are in.  Strange isn't it.
//#define GetRed(x) (((x >> 16) & 0x0FF))
//#define GetRed(x) (((x >> 24) & 0x0FF))
//#define GetGreen(x) (((x >> 8) & 0x0FF))
//#define GetGreen(x) (((x >> 16) & 0x0FF))
//#define GetBlue(x) ((x & 0x0FF))
//#define GetBlue(x) (((x >> 8) & 0x0FF))
// ******************************************

#ifndef MIN
#define MIN(a,b)    (((a)>=(b))?(b):(a))
#endif
#ifndef MAX
#define MAX(a,b)    (((a)<=(b))?(b):(a))
#endif

#define kWhite 0x00FFFFFE

//inline unsigned int get4Pixel(unsigned char *pixAddress);
//inline unsigned int get4Pixel(unsigned char *pixAddress)
//inline uint32_t get4Pixel(unsigned char *pixAddress);

inline uint32_t get4Pixel(unsigned char *pixAddress)
{
//  slower simple code
//  unsigned int toReturn = *((unsigned int*)pixAddress); // load toReturn with XRGB
//  toReturn &= kWhite; // Strip off unwanted X. EGW stripped lsb blue.
//
//  return toReturn;

#ifdef APDK_LITTLE_ENDIAN
    return (((unsigned int*)pixAddress)[0]) & kWhite;
#else
    return (((unsigned int*)pixAddress)[0]) & 0xFFFFFF00;
#endif
}


//inline unsigned int get4Pixel(unsigned char *pixAddress, int pixelOffset);
//inline unsigned int get4Pixel(unsigned char *pixAddress, int pixelOffset)
//inline uint32_t get4Pixel(unsigned char *pixAddress, int pixelOffset);

inline uint32_t get4Pixel(unsigned char *pixAddress, int pixelOffset)
{
//  slower simple code
//  unsigned int *uLongPtr = (unsigned int *)pixAddress;
//  uLongPtr += pixelOffset;
//
//  return *uLongPtr & kWhite;

#ifdef APDK_LITTLE_ENDIAN
    return ((unsigned int*)pixAddress)[pixelOffset] & kWhite;
#else
    return ((unsigned int*)pixAddress)[pixelOffset] & 0xFFFFFF00;
#endif
}

//void put3Pixel(unsigned char *pixAddress, int pixelOffset, unsigned int pixel);
void put3Pixel(unsigned char *pixAddress, int pixelOffset, uint32_t pixel);

//inline void put4Pixel(unsigned char *pixAddress, int pixelOffset, unsigned int pixel);
//inline void put4Pixel(unsigned char *pixAddress, int pixelOffset, unsigned int pixel)
//inline void put4Pixel(unsigned char *pixAddress, int pixelOffset, uint32_t pixel);

inline void put4Pixel(unsigned char *pixAddress, int pixelOffset, uint32_t pixel)
{
#ifdef APDK_LITTLE_ENDIAN
    (((unsigned int*)pixAddress)[pixelOffset] = pixel & kWhite);
#else
    (((unsigned int*)pixAddress)[pixelOffset] = pixel & 0xFFFFFF00);
#endif
}

APDK_END_NAMESPACE
