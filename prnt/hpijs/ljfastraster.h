/*****************************************************************************\
  LJFastRaster.h : Interface for the LJFastRaster class

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


#ifndef APDK_LJFASTRASTER_H
#define APDK_LJFASTRASTER_H


APDK_BEGIN_NAMESPACE
/*!
\internal
*/
class LJFastRaster : public Printer
{
public:
    LJFastRaster (SystemServices* pSS,int numfonts=0, BOOL proto=FALSE);
    ~LJFastRaster ();

    virtual Header* SelectHeader (PrintContext* pc);
    virtual DRIVER_ERROR VerifyPenInfo ();
    virtual DRIVER_ERROR ParsePenInfo (PEN_TYPE& ePen, BOOL QueryPrinter=TRUE);
    virtual DISPLAY_STATUS ParseError (BYTE status_reg);
	virtual DRIVER_ERROR Flush (int FlushSize)
	{
		return NO_ERROR;
	}

    virtual BOOL GetMargins (PAPER_SIZE ps, float *fMargins)
    {
        fMargins[0] = (float) 0.1667;
        fMargins[1] = (float) 0.1667;
        fMargins[2] = (float) 0.1667;
        fMargins[3] = (float) 0.1667;
        return TRUE;
    }

    virtual BOOL UseCMYK (unsigned int iPrintMode) { return FALSE;}
	virtual DRIVER_ERROR Encapsulate (const RASTERDATA* InputRaster, BOOL bLastPlane);
	virtual BOOL SupportSeparateBlack (PrintMode *pCurrentMode)
    {
        return FALSE;
    }

    Compressor* CreateCompressor (unsigned int RasterSize);
    int         m_iYPos;
	BOOL		m_bStartPageNotSent;

	HeaderLJFastRaster  *phLJFastRaster;
protected:

#ifdef APDK_HP_UX
    virtual LJFastRaster & operator = (Printer& rhs)
    {
        return *this;
    }
#endif

	virtual DATA_FORMAT GetDataFormat() { return RASTER_STRIP; }

    BOOL        m_bJobStarted;

	Compressor  *m_pCompressor;
}; // LJFastRaster

class LJFastRasterNormalMode : public GrayMode
{
public:
    LJFastRasterNormalMode ();
};   // LJFastRasterNormalMode

class LJFastRasterDraftMode : public GrayMode
{
public:
    LJFastRasterDraftMode ();
};   // LJFastRasterDraftMode

/*!
\internal
*/

#include "hptypes.h"

#define HIBYTE(sVar) (BYTE) ((sVar & 0xFF00) >> 8)
#define LOBYTE(sVar) (BYTE) ((sVar & 0x00FF))

class ModeDeltaPlus : public Compressor
{
	friend class LJFastRaster;
	friend class HeaderLJFastRaster;

public:
    ModeDeltaPlus(SystemServices* pSys, Printer* pPrinter, unsigned int RasterSize);
    virtual ~ModeDeltaPlus();
    BOOL Process(RASTERDATA* input);
	BYTE* NextOutputRaster(COLORTYPE color);
	unsigned int GetOutputWidth(COLORTYPE  color);

protected:
	BOOL  Compress(HPUInt8* outmem, uint32_t* outlen, const HPUInt8* inmem, const uint32_t row_width, const uint32_t inheight, uint32_t horz_ht_dist);
private:
    Printer* thePrinter;

	HPUInt8* encode_header(HPUInt8* outptr, const HPUInt8* pastoutmem, uint32_t isrun, uint32_t location, uint32_t seedrow_count, uint32_t run_count, const HPUInt8 new_color);

	unsigned char		*pbyInputImageBuffer;

	HPLJBITMAP          m_DestBitmap;;
	long                m_lCurrCDRasterRow;			// Current  raster index. in PrintNextBand
	long                m_lCurrBlockHeight;
    long                m_lPrinterRasterRow;		// Current printer raster row.

	uint32_t	        m_compressedsize;
	BOOL                m_bCompressed;
	float               m_fRatio;
    HPUInt8             *pbySeedRow;
}; //ModeDeltaPlus


#ifdef APDK_LJFASTRASTER
//! LJFastRasterProxy
/*!
******************************************************************************/
class LJFastRasterProxy : public PrinterProxy
{
public:
    LJFastRasterProxy () : PrinterProxy(
        "LJFastRaster",                   // family name
        "hp LaserJet 1010\0"
		"hp LaserJet 1012\0"
/*
 *      The 1015 also supports a PCL path. It will be used a a LJMono printer.
 *
		"hp LaserJet 1015\0"
 */
    )
    {
        m_iPrinterType = eLJFastRaster;
    }
    inline Printer* CreatePrinter (SystemServices* pSS) const
    {
        return new LJFastRaster(pSS);
    }
	inline PRINTER_TYPE GetPrinterType () const
    {
        return eLJFastRaster;
    }
	inline unsigned int GetModelBit () const
    {
        return 0x1;
    }
};
#endif

APDK_END_NAMESPACE

#endif //APDK_LJFASTRASTER_H
