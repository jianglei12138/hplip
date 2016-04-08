/*****************************************************************************\
  LJJetReady.h : Interface for the LJJetReady class

  Copyright (c) 1996 - 2008, HP Co.
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


#ifndef APDK_LJJETREADY_H
#define APDK_LJJETREADY_H

#ifdef APDK_LJJETREADY

#include <setjmp.h>

extern "C"
{
#include "jpeglib.h"
}

APDK_BEGIN_NAMESPACE
/*!
\internal
*/

enum COMPRESS_MODE  {	COMPRESS_MODE0 = 0,
						COMPRESS_MODE2 = 2, 
						COMPRESS_MODE9 = 9,
						COMPRESS_MODE_AUTO = 10,
						COMPRESS_MODE_JPEG = 11,
						COMPRESS_MODE_LJ =   12,
                        COMPRESS_MODE_GRAFIT = 16
					};

class LJJetReady : public Printer
{
public:
    LJJetReady (SystemServices* pSS,int numfonts=0, BOOL proto=FALSE);
    ~LJJetReady ();

    virtual Header* SelectHeader (PrintContext* pc);
    virtual DRIVER_ERROR VerifyPenInfo ();
    virtual DRIVER_ERROR ParsePenInfo (PEN_TYPE& ePen, BOOL QueryPrinter=TRUE);
    virtual DISPLAY_STATUS ParseError (BYTE status_reg);
	inline virtual BOOL SupportSeparateBlack() {return FALSE;}
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

//  See comments in ljmono.h for an explanation.

    virtual BOOL HagakiFeedDuplexerPresent (BOOL bQueryPrinter)
    {
        return TRUE;
    }
	virtual BOOL SupportSeparateBlack (PrintMode *pCurrentMode)
    {
        return FALSE;
    }


    Compressor* CreateCompressor (unsigned int RasterSize);
	Compressor* GetCompressor() { return m_pCompressor; }
	BOOL	bFGColorSet;
    BOOL    bGrey_K;
    int     m_iYPos;
	bool    m_bStartPageNotSent;
	HeaderLJJetReady  *phLJJetReady;

protected:

#ifdef APDK_HP_UX
    virtual LJJetReady & operator = (Printer& rhs)
    {
        return *this;
    }
#endif

	virtual DATA_FORMAT GetDataFormat() { return RASTER_STRIP; }
    BOOL        m_bJobStarted;
    int         m_iYResolution;
    Compressor  *m_pCompressor;
    PrintContext    *thePrintContext;
#ifdef HAVE_LIBDL
    void    *m_hHPLibHandle;
#endif
    COMPRESS_MODE    m_eCompressMode;

}; // LJJetReady

class LJJetReadyNormalMode : public PrintMode
{
public:
    LJJetReadyNormalMode ();
};   // LJJetReadyNormalMode

class LJJetReadyBestColorMode : public PrintMode
{
public:
    LJJetReadyBestColorMode ();
};   // LJJetReadyBestColorMode

class LJJetReadyGrayMode : public PrintMode
{
public:
    LJJetReadyGrayMode ();
};   // LJJetReadyGrayMode

class LJJetReadyBestGrayMode : public PrintMode
{
public:
    LJJetReadyBestGrayMode ();
};   // LJJetReadyBestGrayMode

/*!
\internal
*/

const int JPEG_HEADER_SIZE = 600; //As recommended by JPEG Library Docs
const int JPEG_TRAILER_SIZE = 100; //As recommended by JPEG Library Docs

const int QTABLE_SIZE =  64;
struct QTABLEINFO {
    DWORD qtable0[QTABLE_SIZE];
    DWORD qtable1[QTABLE_SIZE];
    DWORD qtable2[QTABLE_SIZE];
    int   qFactor;
};

class ModeJPEG : public Compressor
{
	friend class LJJetReady;
	friend class HeaderLJJetReady;

public:
    ModeJPEG(SystemServices* pSys, Printer* pPrinter, unsigned int RasterSize, COMPRESS_MODE eCompressMode);
    virtual ~ModeJPEG();
    BOOL Process(RASTERDATA* input);
	BYTE* NextOutputRaster(COLORTYPE color);
	unsigned int GetOutputWidth(COLORTYPE  color);
	long  GetCoordinates() { return m_lPrinterRasterRow; }
    DRIVER_ERROR SendWhiteStrips (int iPageHeight, BOOL bGray);

protected:
    BOOL    Compress( HPLJBITMAP *pSrcBmap,
                      HPLJBITMAP *pTrgBitmap,
                      QTABLEINFO *pQTable,
					  BOOL bGrayscaleSet
                    );

    BYTE*  GetBuffer(void);
    void   SetBuffer(BYTE* pJPEGBuffer);
    DWORD  GetJPEGBufferSize(void) const;

    static BYTE*   fpJPEGBuffer;      // This is passed destination JPEG buffer
    static DWORD   fJPEGBufferPos;    // position of first empty Byte in return Buffer
                                      //   has been called
    JSAMPROW    fRow_array[1];        // JPEG processing routine expects an
                                      //   array of pointers. We always send
                                      //   1 row at a time.

    DWORD  fBufferSize;
    BYTE*  fpJPEGStart;
    
    //----------------------------------------------------------------
    // These are JPEG library "objects" used in the actual jpeg compression routines
    //----------------------------------------------------------------
    struct jpeg_compress_struct   cinfo;
    struct jpeg_decompress_struct dinfo;
    struct jpeg_error_mgr jerr;
    struct jpeg_destination_mgr dest;
    jmp_buf setjmp_buffer;      /* for return to caller */

    //----------------------------------------------------------------
    // IJG dest manager callback
    //----------------------------------------------------------------
    static void jpeg_flush_output_buffer_callback(JOCTET *outbuf, BYTE* buffer, DWORD size);

private:
    Printer*            thePrinter;

    HPLJBITMAP			m_SourceBitmap;
	HPLJBITMAP          m_DestBitmap;
	long                m_lCurrCDRasterRow;			// Current  raster index. in PrintNextBand
    long                m_lPrinterRasterRow;		// Current printer raster row.
    int                 m_iRasterWidth;             // Input image width
    COMPRESS_MODE       m_eCompressMode;
}; //ModeJPEG


#ifdef APDK_LJJETREADY
//! LJJetReadyProxy
/*!
******************************************************************************/
class LJJetReadyProxy : public PrinterProxy
{
public:
    LJJetReadyProxy() : PrinterProxy(
        "LJJetReady",                   // family name
        "hp color LaserJet 3500\0"
		"hp color LaserJet 3550\0"
        "hp color LaserJet 3600\0"
        "Hp Color LaserJet CP151\0"
#ifdef APDK_MLC_PRINTER
#endif
    ) {m_iPrinterType = eLJJetReady;}
    inline Printer* CreatePrinter(SystemServices* pSS) const { return new LJJetReady(pSS); }
	inline PRINTER_TYPE GetPrinterType() const { return eLJJetReady;}
	inline unsigned int GetModelBit() const { return 0x1;}
};
#endif

APDK_END_NAMESPACE

#endif // APDK_LJJETREADY
#endif //APDK_LJJETREADY_H
