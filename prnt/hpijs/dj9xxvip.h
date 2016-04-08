/*****************************************************************************\
  dj9xxvip.h : Interface for the DJ9xxVIP class

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


#ifndef APDK_DJ9XXVIP_H
#define APDK_DJ9XXVIP_H

APDK_BEGIN_NAMESPACE

//extern char *ModelString[MAX_ID_STRING];

/*!
\internal
*/
class DJ9xxVIP : public Printer
{
public:
    DJ9xxVIP(SystemServices* pSS, BOOL proto=FALSE);

    Header* SelectHeader(PrintContext* pc);
    DRIVER_ERROR VerifyPenInfo();
    DRIVER_ERROR ParsePenInfo(PEN_TYPE& ePen, BOOL QueryPrinter=TRUE);
    DISPLAY_STATUS ParseError(BYTE status_reg);

    Compressor* CreateCompressor(unsigned int RasterSize);

    virtual BOOL UseGUIMode(PrintMode* pPrintMode);
    BOOL UseCMYK(unsigned int iPrintMode) { return FALSE; }

    DRIVER_ERROR CleanPen();

    virtual PAPER_SIZE MandatoryPaperSize();

    virtual DRIVER_ERROR CheckInkLevel();

    virtual BOOL PhotoTrayPresent(BOOL bQueryPrinter);

    virtual PHOTOTRAY_STATE PhotoTrayEngaged (BOOL bQueryPrinter);

    inline virtual BOOL SupportSeparateBlack (PrintMode *pCurrentMode)
    {
        if (pCurrentMode->medium == mediaAuto ||
            pCurrentMode->medium == mediaPlain)
        {
            return TRUE;
        }
        return FALSE;
    }

    virtual DRIVER_ERROR SetHint (PRINTER_HINT eHint, int iValue)
    {
        switch (eHint)
        {
            case PAGES_IN_DOC_HINT:
            {
                m_iNumPages = iValue;
                break;
            }
            case SPEED_MECH_HINT:
            {
                return SendSpeedMechCmd (iValue);
            }
            case EXTRA_DRYTIME_HINT:
            {
                m_cExtraDryTime = (BYTE) (iValue & 0xFF);
                break;
            }
            case LEFT_OVERSPRAY_HINT:
            {
                m_iLeftOverspray = iValue;
                break;
            }
            case RIGHT_OVERSPRAY_HINT:
            {
                m_iRightOverspray = iValue;
                break;
            }
            case TOP_OVERSPRAY_HINT:
            {
                m_iTopOverspray = iValue;
                break;
            }
            case BOTTOM_OVERSPRAY_HINT:
            {
                m_iBottomOverspray = iValue;
                break;
            }

            default:
                break;
        }
        return NO_ERROR;
    }
    virtual int GetHint (PRINTER_HINT eHint)
    {
        if (eHint == EXTRA_DRYTIME_HINT)
        {
            return (int) m_cExtraDryTime;
        }
        return 0;
    }

    virtual DRIVER_ERROR AddPJLHeader ();
protected:

#ifdef APDK_HP_UX
    virtual DJ9xxVIP & operator = (Printer& rhs)
    {
        return *this;
    }
#endif

    BOOL PCL3acceptsDriverware;
    virtual BYTE PhotoTrayStatus(BOOL bQueryPrinter);
	int		iNumMissingPens;
    int     m_iLeftOverspray;
    int     m_iTopOverspray;
    int     m_iRightOverspray;
    int     m_iBottomOverspray;

private:
    BOOL IsPCL3DriverwareAvailable();
    virtual DRIVER_ERROR SendSpeedMechCmd (BOOL bLastPage)
    {
        return NO_ERROR;
    }
    BYTE    m_cExtraDryTime;
}; //DJ9xxVIP


class DJ990Mode : public PrintMode
{
public:
    DJ990Mode();
}; //AladdenMode


class GrayModeDJ990 : public GrayMode
{
public:
    GrayModeDJ990 (uint32_t *map, BOOL PCL3OK);
}; //GrayModeDJ990

class DJ990CMYGrayMode : public PrintMode
{
public:
    DJ990CMYGrayMode ();
}; //DJ990CMYGrayMode

#ifdef APDK_EXTENDED_MEDIASIZE
class DJ990KGrayMode : public PrintMode
{
public:
    DJ990KGrayMode ();
}; //DJ990KGrayMode

class DJ9902400Mode : public PrintMode
{
public:
    DJ9902400Mode ();
    virtual inline BOOL MediaCompatible (MEDIATYPE eMedia)
    {
        return (eMedia == pmMediaType || eMedia == MEDIA_PHOTO);
    }
}; // DJ9902400Mode

class DJ990DraftMode : public PrintMode
{
public:
    DJ990DraftMode ();
}; // DJ990DraftMode

#endif // APDK_EXTENDED_MEDIASIZE

class DJ990BestMode : public PrintMode
{
public:
    DJ990BestMode ();
};  // DJ990BestMode

class DJ990PhotoNormalMode : public PrintMode
{
public:
    DJ990PhotoNormalMode ();
}; // DJ990 PhotoNormalMode

#ifdef APDK_HIGH_RES_MODES
    const int VIP_BASE_RES = 600;
#else
    const int VIP_BASE_RES = 300;
#endif

/////////////////////////////
#define kWhite 0x00FFFFFE
#define GetRed(x) (((x >> 16) & 0x0FF))
#define GetGreen(x) (((x >> 8) & 0x0FF))
#define GetBlue(x) ((x & 0x0FF))

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

#ifndef MIN
#define MIN(a,b)    (((a)>=(b))?(b):(a))
#endif
#ifndef MAX
#define MAX(a,b)    (((a)<=(b))?(b):(a))
#endif

/*
We don't actually support 4-byte RGB for anything excpet VIP printers.
So implimenting this and documenting it as a build option is misleading.
We must make halftoning support 4-byte RGB before we can publish this.
If we hear from someone that they need support for 4-byte RGB (and it's
worth it) then we will add it to halftoning, put the define in config.h,
and uncomment the #if here.  1/17/2002 - JLM

#if defined(APDK_4BYTE_RGB)
    #define getPixel get4Pixel
    #define putPixel put4Pixel
    const unsigned int BYTES_PER_PIXEL = 4;
#else
*/
    #define getPixel get3Pixel
    #define putPixel put3Pixel
    const unsigned int BYTES_PER_PIXEL = 3;
//#endif


/*!
\internal
*/
class Mode10 : public Compressor
{
public:
    Mode10(SystemServices* pSys, Printer* pPrinter, unsigned int RasterSize);
    virtual ~Mode10();
    BOOL Process(RASTERDATA* input);
	BYTE* NextOutputRaster(COLORTYPE color);
    void Flush();

private:
    Printer* thePrinter;

    inline uint32_t get4Pixel(unsigned char *pixAddress, unsigned int bPrint = FALSE)
        {
            #ifdef APDK_LITTLE_ENDIAN
                return (((unsigned int*)pixAddress)[0]) & kWhite;
            #else
                return (((unsigned int*)pixAddress)[0]) & 0xFFFFFF00;
            #endif
        }


    inline uint32_t get4Pixel(unsigned char *pixAddress, int pixelOffset)
        {
            #ifdef APDK_LITTLE_ENDIAN
                return ((unsigned int*)pixAddress)[pixelOffset] & kWhite;
            #else
                return ((unsigned int*)pixAddress)[pixelOffset] & 0xFFFFFF00;
            #endif
        }


    inline void put4Pixel(unsigned char *pixAddress, int pixelOffset, uint32_t pixel)
        {
            #ifdef APDK_LITTLE_ENDIAN
                (((unsigned int*)pixAddress)[pixelOffset] = pixel & kWhite);
            #else
                (((unsigned int*)pixAddress)[pixelOffset] = pixel & 0xFFFFFF00);
            #endif
        }


    inline void outputVLIBytesConsecutively(int number, unsigned char *&compressedDataPtr)
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


    void put3Pixel(BYTE* pixAddress, int pixelOffset, uint32_t pixel);
    inline uint32_t get3Pixel(BYTE* pixAddress, int pixelOffset);
    unsigned short ShortDelta(uint32_t lastPixel, uint32_t lastUpperPixel);


}; //Mode10

#ifdef APDK_DJ9xxVIP
//! DJ9xxVIPProxy
/*!
******************************************************************************/
class DJ9xxVIPProxy : public PrinterProxy
{
public:
    DJ9xxVIPProxy() : PrinterProxy(
        "DJ9xxVIP",                     // family name
        "DESKJET 96\0"                              // DeskJet 96x series
        "DESKJET 98\0"                              // DeskJet 98x series
        "DESKJET 99\0"                              // DeskJet 99x series
        "deskjet 6122\0"                            // deskjet 6122
        "deskjet 6127\0"                            // deskjet 6127
        "PHOTOSMART 1115\0"
        "PHOTOSMART 1215\0"                         // PSP 1215
        "PHOTOSMART 1218\0"                         // PSP 1218
        "PHOTOSMART 1115\0"                         // PSP 1115
        "PHOTOSMART 1315\0"                         // PSP 1315
        "HP BUSINESS INKJET 22\0"                   // 2200, 2230, 2250, 2280
        "cp1160\0"                                  // CP 1160
        "HP Color Inkjet CP1700\0"                  // CP 1700
        "HP BUSINESS INKJET 22\0"
        "dj450\0"
		"deskjet 450\0"
#ifdef APDK_MLC_PRINTER
        "officejet d\0"                             // officejet d series
		"officejet 7100\0"                          // offjetjet 7100 series
#endif
    ) {m_iPrinterType = eDJ9xxVIP;}
    inline Printer* CreatePrinter(SystemServices* pSS) const { return new DJ9xxVIP(pSS); }
	inline PRINTER_TYPE GetPrinterType() const { return eDJ9xxVIP;}
	inline unsigned int GetModelBit() const { return 0x10000;}
};
#endif

APDK_END_NAMESPACE

#endif //APDK_DJ9XXVIP_H
