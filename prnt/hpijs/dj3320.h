/*****************************************************************************\
  DJ3320.h : Interface for the DJ3320 class

  Copyright (c) 2001 - 2015, HP Co.
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
  \***************************************************************************/

/*
 *    Author: Raghothama Cauligi
 */

#ifndef APDK_DJ3320_H
#define APDK_DJ3320_H

#include "config.h"

APDK_BEGIN_NAMESPACE

#include "ldlencap.h"

class   LDLEncap;

class DJ3320 : public Printer
{
public:
    DJ3320 (SystemServices* pSS, BOOL proto = FALSE);

    virtual ~DJ3320 ();

    Header* SelectHeader(PrintContext* pc);
    virtual DRIVER_ERROR Send (const BYTE *pWriteBuff, DWORD dwWriteLen);

#ifdef APDK_HP_UX
    virtual DRIVER_ERROR Send (const BYTE *pWriteBuff)
    {
        return Send (pWriteBuff, strlen ((const char *) pWriteBuff));
    }
#endif

    virtual DRIVER_ERROR Encapsulate (const RASTERDATA *pRasterData, BOOL bLastPlane);
    DRIVER_ERROR VerifyPenInfo ();
    virtual DRIVER_ERROR ParsePenInfo (PEN_TYPE& ePen, BOOL QueryPrinter = TRUE);
    virtual DRIVER_ERROR SkipRasters (int nBlankRasters);
    virtual DRIVER_ERROR CleanPen ();
    virtual DRIVER_ERROR Flush (int FlushSize);
    DRIVER_ERROR SetPens (PEN_TYPE eNewPen);
	virtual DRIVER_ERROR CheckInkLevel();

    DISPLAY_STATUS ParseError (BYTE byStatusReg);
    
	inline virtual BOOL SupportSeparateBlack (PrintMode *pCurrentMode)
    {
        if (pCurrentMode->ColorDepth[1] == 2)
        {
            return FALSE;
        }
        return TRUE;
    }
    

    LDLEncap    *pLDLEncap;
    int         m_iBytesPerSwing;
    int         m_iLdlVersion;
    int         m_iColorPenResolution;
    int         m_iBlackPenResolution;
    int         m_iNumBlackNozzles;

protected:
    DISPLAY_STATUS m_dsCurrentStatus;
    virtual void InitPrintModes ();
    virtual void AdjustResolution ()
    {
        return;
    }

#ifdef APDK_HP_UX
protected:
    virtual DJ3320& operator = (Printer& rhs)
    {
        return *this;
    }
#endif

};

class DJ3320GrayMode : public PrintMode
{
public:
    DJ3320GrayMode (PEN_TYPE ePen);
};

class DJ3320KDraftMode : public GrayMode
{
public:
    DJ3320KDraftMode ();
};

class DJ3320DraftMode : public PrintMode
{
public:
    DJ3320DraftMode (PEN_TYPE ePen);

};

class DJ3320NormalMode : public PrintMode
{
public:
    DJ3320NormalMode (PEN_TYPE ePen);

};

class DJ3320PhotoMode : public PrintMode
{
public:
    DJ3320PhotoMode ();
};

class DJ3600MDLNormalMode : public PrintMode
{
public:
    DJ3600MDLNormalMode ();
};


class DJ3600MDLDraftMode : public PrintMode
{
public:
    DJ3600MDLDraftMode ();

};

class DJ3600MDLPhotoMode : public PrintMode
{
public:
    DJ3600MDLPhotoMode ();

};

#if defined (APDK_DJ3320)

//! DJ3320Proxy
/*!
******************************************************************************/
class DJ3320Proxy : public PrinterProxy
{
public:
    DJ3320Proxy() : PrinterProxy(
        "DJ3320",                   // family name
        "deskjet 3320\0"
        "deskjet 3325\0"
		"deskjet 3420\0"
        "deskjet 3425\0"
		"deskjet 3500\0"
        "deskjet 3528\0"
        "deskjet 3535\0"
		"Deskjet 3740\0"
		"Deskjet 3920\0"
		"Deskjet 3940\0"
		"Deskjet 3900\0"
        "Deskjet D1360\0"
        "Deskjet d14\0"
        "Deskjet D15\0"
        "Deskjet F22\0"
#ifdef APDK_MLC_PRINTER
		"psc 1100\0"
		"psc 1200\0"
		"officejet 4100\0"
		"officejet 4105\0"
#endif
        "Officejet 43\0"
        "officejet 4115\0"
        "officejet 5500\0"
        "psc 11\0"
        "psc 12\0"
        "psc 13\0"
        "psc 16\0"
    ) {m_iPrinterType = eDJ3320;}
    inline Printer* CreatePrinter(SystemServices* pSS) const { return new DJ3320(pSS); }
	inline PRINTER_TYPE GetPrinterType() const { return eDJ3320;}
	inline unsigned int GetModelBit() const { return 0x100;}
};

#endif

#define SWATH_HEIGHT    100     // Number of color nozzles to be used

#ifdef  APDK_LDL_COMPRESS

/************************************************************/
/*  Compression Classes                                     */
/*                                                          */
/************************************************************/

#define ALLOW_FILL_NEXT_CMD 1

#define FILL_0000_CMD  0x1000
#define FILL_FFFF_CMD  0x2000
#define FILL_NEXT_CMD  0x3000
#define FILL_IMAGE_CMD 0x4000
#define FILL_ODD_CMD   0x5000
#define FILL_EVEN_CMD  0x6000

#define MAX_HEADER_FRAME_SIZE 512
#define MAX_DATA_FRAME_SIZE 6048
#define MAX_RUNLENGTH 5

typedef enum
{
  IN_NOT,
  IN_FIRST,
  IN_IMAGE,
  IN_COPY
} LDLCOMPMODE;

class comp_ptrs_t
{
public:
	comp_ptrs_t (UInt16 *data,UInt16 datasize):image_ptr(data),raw_data(data),
												data_length(datasize){};
	comp_ptrs_t () {};
	~comp_ptrs_t () {};

public:
    BOOL    Init(UInt16 *data, UInt16 datasize);
    UInt16  *out_ptr, *image_ptr; /* image ptr points into the raw data */
    UInt16  out_cnt,  image_cnt,  copy_cnt;
    UInt16  out_array[2048+16];
    UInt16  *raw_data;
    UInt16  input_size;

    UInt16  compress;
    UInt16  display;
    UInt16  run_length; /* minimun run length */

    UInt16  data_length;  /* actual data size in record */
    UInt16  image_bytes;  /* uncompressed size */
    UInt16  command_number;
    UInt16  data_type;
};
#endif  // APDK_LDL_COMPRESS

class LDLEncap //: public Compressor
{
public:
    LDLEncap (DJ3320 *pPrinter, SystemServices *pSys, PrintContext *pc);
    DRIVER_ERROR Encapsulate (const BYTE* input, DWORD size, BOOL bLastPlane);
    ~LDLEncap ();

    DRIVER_ERROR StartJob ();
    DRIVER_ERROR EndPage ();
    DRIVER_ERROR EndJob ();
    DRIVER_ERROR Continue ();
    DRIVER_ERROR CleanPen ();
    void    Cancel ();
    void    Flush ();
    DRIVER_ERROR    SetVerticalSkip (int nBlankRasters);
    void AllocateSwathBuffer (unsigned int RasterSize);

    BOOL UpdateState (BOOL bInitialize);

    DJ3320  *pPrinterXBow;
    DRIVER_ERROR    constructor_error;

    short int   *piCreditCount;
    BYTE        byNumberOfCommands;
    BYTE        byStatusBuff[DevIDBuffSize];
    BOOL        bNewStatus;
//  BYTE        byQuery[QUERYSIZE];
    BYTE        pbyCancel[CANCELSIZE];
    BYTE        *pbySync;

private:
    DRIVER_ERROR    StartPage ();

    SystemServices  *m_pSys;
    int     m_iRasterCount;
    int     m_iImageWidth;
    int     m_iXResolution;
    int     m_iYResolution;
    UInt16  m_sRefCount;
    int     m_iNumColors;
    char    m_cPrintDirection;
    BYTE    *m_szCmdBuf;
    BOOL    m_bLittleEndian;
    PrintContext    *m_pthisPC;
    BYTE    ***m_SwathData;
    short   m_sSwathHeight;
    int     m_iBlankRasters;
    int     m_iVertPosn;
    int     m_iLeftMargin;
    BOOL    m_bBidirectionalPrintingOn;
    Int16   m_iBitDepth;
    BYTE    m_cPassNumber;
    BYTE    m_cPlaneNumber;
    BYTE    m_cPrintQuality;
    BYTE    m_cMediaType;
    BYTE    *m_szCompressBuf;
    int     m_iNextRaster;
    int     m_iNextColor;

	BOOL	m_bStartPageNotSent;

    UInt16  m_uVal16;
    UInt32  m_uVal32;
    BYTE    *m_cByte;
    BYTE    *m_pbyPacketBuff;
	DWORD	m_dwPacketBuffSize;

/*
 *  Pen Alignment Values.
 */

    BYTE    m_cKtoCVertAlign;
	BYTE    m_cPtoCVertAlign;

    DRIVER_ERROR    PrintSweep (UInt32 SweepSize,
                        BOOL ColorPresent,
                        BOOL BlackPresent,
						BOOL PhotoPresent,
                        Int32 VerticalPosition,
                        Int32 LeftEdge,
                        Int32 RightEdge,
                        char PrintDirection,
                        Int16 sFirstNozzle,
                        Int16 sLastNozzle = 0);
    DRIVER_ERROR    LoadSweepData (BYTE *dataPtr, int imagesize);
    void            FillLidilHeader (void *pLidilHdr, int Command,
                             UInt16 CmdLen, UInt16 DataLen);
    unsigned int    GetSwathWidth (int iFirst, int iLast, int iWidth);
    BOOL            IsBlankRaster (BYTE *input, int iWidth);
    DRIVER_ERROR    ProcessSwath (int iCurRasterWidth);

    BOOL            GetPackets (DWORD &dwBytesRead);

#ifdef  APDK_LDL_COMPRESS
/*
 *  Compression Related
 */

	BOOL   GetFrameInfo (BYTE **outdata, UInt16 *datasize);
	void   CompressData (Int16 compressmode = 1);

	UInt16 FlushCopy (UInt16 value);
	UInt16 FlushImage ();

    comp_ptrs_t     *m_ldlCompressData;
#endif  // APDK_LDL_COMPRESS

};

#define WRITE16(value)  \
        m_uVal16 = value; \
        m_cByte = (BYTE *) &m_uVal16; \
        if (m_bLittleEndian)       \
        { \
            m_szCmdBuf[index++] = m_cByte[1];  \
            m_szCmdBuf[index++] = m_cByte[0]; \
        } \
        else \
        { \
            memcpy (m_szCmdBuf+index, m_cByte, 2); \
            index += 2; \
        }

#define WRITE32(value) \
        m_uVal32 = value; \
        m_cByte = (BYTE *) &m_uVal32; \
        if (m_bLittleEndian)       \
        { \
            m_szCmdBuf[index++] = m_cByte[3];  \
            m_szCmdBuf[index++] = m_cByte[2]; \
            m_szCmdBuf[index++] = m_cByte[1]; \
            m_szCmdBuf[index++] = m_cByte[0]; \
        } \
        else \
        { \
            memcpy (m_szCmdBuf+index, m_cByte, 4); \
            index += 4; \
        }

APDK_END_NAMESPACE

#endif //APDK_DJ3320_H
