/*****************************************************************************\
  ljfastraster.cpp : Implimentation for the LJFastRaster class

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


#ifdef APDK_LJFASTRASTER

#include "header.h"
#include "ljfastraster.h"
#include "printerproxy.h"

APDK_BEGIN_NAMESPACE

#define INDY_STRIP_HEIGHT	128			// Indy strips can't cross 128 boundary

extern uint32_t ulMapDJ600_CCM_K[ 9 * 9 * 9 ];

BYTE FrBeginSessionSeq[]				= {0xC0, 0x00, 0xF8, 0x86, 0xC0, 0x03, 0xF8, 0x8F, 0xD1, 0x58, 
										    0x02, 0x58, 0x02, 0xF8, 0x89, 0x41};
BYTE FrFeedOrientationSeq[]				= {0xC0, 0x00  , 0xF8, 0x28 };
//											   |fd ori enum|       |ori cmd|											
BYTE FrPaperSizeSeq[]					= {0xC0,  0x00      ,0xF8, 0x25};
//											   |pap siz enum|     |pap sz cmd|
BYTE FrMedSourceSeq[]					= {0xC0,  0x00      ,0xF8, 0x26        };
//											   |Med src enum|     |Med src cmd|
BYTE FrMedDestinationSeq[]				= {0xC0,  0x00        ,0xF8 , 0x24      };
//											   |Med Dest enum|      |Med src cmd|
BYTE FrBeginPageSeq[]				    = {0x43, 0xD3, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x2A, 0x75, 0xC0, 0x07,0xF8, 0x03, 0x6A,
											0xC0, 0xCC, 0xF8, 0x2C, 0x7B,
											0xD3, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x4C, 0x6B};
 
BYTE FrBeginImageSeq[]				    = {0xC2, 0x00, 0x40, 0x70, 0x68, 0xF8, 0x91, 0xC1};

BYTE FrVU_ver_TagSeq[]					= {0xC2, 0x00, 0x00, 0x04, 0x00 , 0xF8, 0x95			  };
//												|endian alignd         |  |Fr_ver_ tag|
BYTE FrDataLengthSeq[]					= {0xC2, 0x86, 0x0A, 0x00, 0x00, 0xF8, 0x92	  };
//																			  | VU data length|
BYTE FrVenUniqSeq[]						= {0x46};
BYTE FrVUExtn_3Seq[]					= {0xC2, 0x11, 0x20, 0x70, 0x68			,0xF8, 0x91	};
//												|endian alignd Fr rd img tag|	  |VU extensn|
BYTE FrOpenDataSourceSeq[]              = {0xC0, 0x00, 0xF8, 0x88, 0xC0, 0x01, 0xF8, 0x82, 0x48};

BYTE FrEndPageSeq[]						=  {0x44};
BYTE FrEndSessionSeq[]					=  {0x42};
BYTE FrCloseDataSourceSeq[]				=  {0x49};
// PJL level commands..

//**JETLIB ENTRIES

const char *ccpPJLStartJob				= "\033%-12345X";
const char *ccpPJLExitSeq 				= "\033%-12345X@PJL EOJ\012\033%-12345X";
const char *ccpPJLSetRes				= "@PJL SET RESOLUTION=600\012";
const char *ccpPCLEnterXL				= "@PJL ENTER LANGUAGE=PCLXL\012";
const char *ccpPJLSetTO				    = "@PJL SET TIMEOUT=900\012";
const char *ccpUEL                      = "\033%-12345X";
const char *ccpPJLComment				= ") HP-PCL XL;2;0;Comment\012";
const char *ccpPJLSet1BPP				= "@PJL SET BITSPERPIXEL=1\012";
const char *ccpPJLSetECONOMODE			= "@PJL SET ECONOMODE=OFF\012";
const char *ccpPJLSetTimeStamp			= "@PJL SET JOBATTR=";

//**END JETLIB ENTRIES


LJFastRaster::LJFastRaster (SystemServices* pSS, int numfonts, BOOL proto)
    : Printer(pSS, numfonts, proto)
{

    if ((!proto) && (IOMode.bDevID))
    {
        constructor_error = VerifyPenInfo();
        CERRCHECK;
    }
    else ePen = BLACK_PEN;    // matches default mode

    pMode[DEFAULTMODE_INDEX] = new LJFastRasterNormalMode ();
    pMode[GRAYMODE_INDEX]    = new LJFastRasterDraftMode ();
    //pMode[DEFAULTMODE_INDEX]   = pMode[GRAYMODE_INDEX];
    ModeCount = 2;

    CMYMap = NULL;
    m_bJobStarted = FALSE;
#ifdef  APDK_AUTODUPLEX
    m_bRotateBackPage = FALSE;  // Lasers don't require back side image to be rotated
#endif

    m_iYPos = 0;
    m_bStartPageNotSent = TRUE;

    DBG1("LJFastRaster created\n");
}

LJFastRaster::~LJFastRaster ()
{
    DISPLAY_STATUS  eDispStatus;
    if (IOMode.bStatus && m_bJobStarted)
    {
        for (int i = 0; i < 5; i++)
        {
            pSS->BusyWait (2000);
            eDispStatus = ParseError (0);
            if (eDispStatus == DISPLAY_PRINTING_COMPLETE)
            {
                pSS->DisplayPrinterStatus (eDispStatus);
                break;
            }
        }
    }
}

LJFastRasterNormalMode::LJFastRasterNormalMode ()
: GrayMode(ulMapDJ600_CCM_K)
{
    ResolutionX[0] = 600;
    ResolutionY[0] = 600;
    BaseResX = BaseResY = 600;

    MixedRes = FALSE;
   
	theQuality = qualityNormal;

    pmQuality = QUALITY_NORMAL;

#if defined(APDK_VIP_COLORFILTERING)
    Config.bErnie = FALSE; // Raghu
#endif

#ifdef APDK_AUTODUPLEX
    bDuplexCapable = TRUE;
#endif

    bFontCapable = FALSE;
}

LJFastRasterDraftMode::LJFastRasterDraftMode ()
: GrayMode(ulMapDJ600_CCM_K)
{
    ResolutionX[0] = 600;
    ResolutionY[0] = 600;
    BaseResX = BaseResY = 600;

    MixedRes = FALSE;
   
	theQuality = qualityDraft;

    pmQuality = QUALITY_DRAFT;

#if defined(APDK_VIP_COLORFILTERING)
    Config.bErnie = FALSE; // Raghu
#endif

#ifdef APDK_AUTODUPLEX
    bDuplexCapable = TRUE;
#endif

    bFontCapable = FALSE;
}

HeaderLJFastRaster::HeaderLJFastRaster (Printer* p, PrintContext* pc)
    : Header(p,pc)
{ 
	SetLastBand(FALSE);
}

DRIVER_ERROR HeaderLJFastRaster::Send ()
{
    StartSend ();

    return NO_ERROR;
}

DRIVER_ERROR HeaderLJFastRaster::StartSend ()
{
    DRIVER_ERROR    err = NO_ERROR;
    char            res[64];

	err = thePrinter->Send ((const BYTE*)ccpPJLStartJob, strlen (ccpPJLStartJob));
	ERRCHECK;

	//Set the resolution to 600
	err = thePrinter->Send ((const BYTE*)ccpPJLSetRes, strlen (ccpPJLSetRes));
	ERRCHECK;

	err = thePrinter->Send ((const BYTE*)ccpPJLSet1BPP, strlen (ccpPJLSet1BPP));
	ERRCHECK;

    QUALITY_MODE    eQ = QUALITY_NORMAL;
    COLORMODE       eC;
    MEDIATYPE       eM;
    BOOL            bD;

    thePrintContext->GetPrintModeSettings (eQ, eM, eC, bD);

    strcpy (res, "@PJL SET ECONOMODE=");

    if (eQ == QUALITY_DRAFT)
    {
        strcat (res, "ON\015\012");
    }
    else
    {
        strcat (res, "OFF\015\012");
    }
    err = thePrinter->Send ((const BYTE *) res, strlen (res));
    ERRCHECK;


	//Send the time out command
	err = thePrinter->Send ((const BYTE*)ccpPJLSetTO, strlen (ccpPJLSetTO));
	ERRCHECK;

	//send the mojave PCL_XL_ENTER_LANG command
	err = thePrinter->Send ((const BYTE*)ccpPCLEnterXL, strlen (ccpPCLEnterXL));
	ERRCHECK;

	//send the comment string
	err = thePrinter->Send ((const BYTE*)ccpPJLComment, strlen (ccpPJLComment));
	ERRCHECK;

	err = thePrinter->Send ((const BYTE*)FrBeginSessionSeq,sizeof(FrBeginSessionSeq));
	ERRCHECK;

	//** VU command to enable PCL-XL
	err = thePrinter->Send ((const BYTE*)FrVUExtn_3Seq,sizeof(FrVUExtn_3Seq));
	ERRCHECK;
	err = thePrinter->Send ((const BYTE*)FrVenUniqSeq,sizeof(FrVenUniqSeq));
	ERRCHECK;

    //----------------------------------------------------------------
    // Open a data source, which will be kept open for the life
    // of the session.  Any operators that need embedded data will
    // use this data source.
    //----------------------------------------------------------------
 	err = thePrinter->Send ((const BYTE*)FrOpenDataSourceSeq,sizeof(FrOpenDataSourceSeq));
	ERRCHECK;

    return err;
}

int HeaderLJFastRaster::FrPaperToMediaSize(PAPER_SIZE psize)
{
    switch(psize)
    {
    case LETTER:        return 0;    break;
    case LEGAL:         return 1;     break;
    case A4:            return 2;          break;
    case B4:            return 10;          break;
    case B5:            return 11;          break;
    case OUFUKU:        return 14;      break;
    case HAGAKI:        return 14;      break;
	case A6:            return 17;      break;
#ifdef APDK_EXTENDED_MEDIASIZE
    case A3:        return 5;      break;
    case A5:        return 16;      break;
    case LEDGER:        return 4;      break;
    case EXECUTIVE:     return 3;   break;
    case CUSTOM_SIZE:   return 96;      break;
	case ENVELOPE_NO_10: return 6;    break;
	case ENVELOPE_A2:    return 6;       break;
	case ENVELOPE_C6:    return 8;       break;
	case ENVELOPE_DL:    return 9;       break;
#endif
    default:            return 0;    break;
    }
}

DRIVER_ERROR HeaderLJFastRaster::StartPage ()
{
    DRIVER_ERROR err;
    BYTE    szCustomSize[16];

	/* Orienatation: is FrFeedOrientationSeq[1]. Can take the following values:
		Portrait				: 0x00
		Landscape:				: 0x01
		Reversed    Portrait	: 0x02
		Reversed    Landscape	: 0x03
		Image		Orientataion: 0x04
	*/
	err = thePrinter->Send ((const BYTE*)FrFeedOrientationSeq,sizeof(FrFeedOrientationSeq));
	ERRCHECK;
	
	//Put the papersize into the FrPaperSizeSeq[]
	PAPER_SIZE ps = thePrintContext->GetPaperSize ();
	int msizeCode = FrPaperToMediaSize(ps);
	FrPaperSizeSeq[1] = (BYTE) msizeCode; 
#ifdef APDK_EXTENDED_MEDIASIZE
	if(msizeCode == 96 || msizeCode == 17) //Custom paper size or A6
	{
        union
        {
            float       fValue;
            uint32_t    uiValue;
        } LJFUnion;
        uint32_t    uiXsize;
        uint32_t    uiYsize;
        int         k = 0;
        LJFUnion.fValue = (float) thePrintContext->PhysicalPageSizeX ();
        uiXsize = LJFUnion.uiValue;
        LJFUnion.fValue = (float) thePrintContext->PhysicalPageSizeY ();
        uiYsize = LJFUnion.uiValue;
        szCustomSize[k++] = 0xD5;
        szCustomSize[k++] = (BYTE) (uiXsize & 0x000000FF);
        szCustomSize[k++] = (BYTE) ((uiXsize & 0x0000FF00) >> 8);
        szCustomSize[k++] = (BYTE) ((uiXsize & 0x00FF0000) >> 16);
        szCustomSize[k++] = (BYTE) ((uiXsize & 0xFF000000) >> 24);
        szCustomSize[k++] = (BYTE) (uiYsize & 0x000000FF);
        szCustomSize[k++] = (BYTE) ((uiYsize & 0x0000FF00) >> 8);
        szCustomSize[k++] = (BYTE) ((uiYsize & 0x00FF0000) >> 16);
        szCustomSize[k++] = (BYTE) ((uiYsize & 0xFF000000) >> 24);
        szCustomSize[k++] = 0xF8;
        szCustomSize[k++] = 0x2F;
        err = thePrinter->Send ((const BYTE *) szCustomSize, k);
		ERRCHECK;


		BYTE FrCustomMediaSeq[] = {0xC0,0x00, 0xF8, 0x30};
		err = thePrinter->Send ((const BYTE *)FrCustomMediaSeq, sizeof(FrCustomMediaSeq));
		ERRCHECK;
	}	
	else
#endif
	{
		err = thePrinter->Send ((const BYTE*)FrPaperSizeSeq,sizeof(FrPaperSizeSeq));
		ERRCHECK;
	}

	err = thePrinter->Send ((const BYTE *)FrBeginPageSeq, sizeof(FrBeginPageSeq));
	ERRCHECK;

	bLastBand = FALSE;

    return err;
}

DRIVER_ERROR HeaderLJFastRaster::EndPage ()
{
    DRIVER_ERROR err;

    // 0x44 - end page
	err = thePrinter->Send ((const BYTE*)FrEndPageSeq,sizeof(FrEndPageSeq));
	ERRCHECK;

    return err;
}

DRIVER_ERROR HeaderLJFastRaster::EndJob ()
{
    DRIVER_ERROR    err = NO_ERROR;

	err = thePrinter->Send((const BYTE*)FrCloseDataSourceSeq, sizeof(FrCloseDataSourceSeq));
	ERRCHECK;

	err = thePrinter->Send((const BYTE*)FrEndSessionSeq, sizeof(FrEndSessionSeq));
	ERRCHECK;

	err = thePrinter->Send((const BYTE*)ccpPJLExitSeq, strlen (ccpPJLExitSeq));
	ERRCHECK;

    return err;
}

DRIVER_ERROR HeaderLJFastRaster::FormFeed ()
{
	DRIVER_ERROR    err = NO_ERROR;
    LJFastRaster    *pFRPrinter = (LJFastRaster *) thePrinter;

	bLastBand = TRUE;

	err = EndPage ();

	pFRPrinter->m_bStartPageNotSent = TRUE;
	return err;
}

DRIVER_ERROR HeaderLJFastRaster::SendCAPy (unsigned int iAbsY)
{
    return NO_ERROR;
}

#define		FAST_RASTER_HEADERSIZE	25

//** Faster Raster Path Header address values

#define		BASE_ADDRESS					0
#define		PAGE_NUM_ADDRESS				1
#define		RESOLUTION_ADDRESS_HI			2
#define		RESOLUTION_ADDRESS_LO			3
#define		COMPRESSION_ADDRESS_HI			4
#define		COMPRESSION_ADDRESS_LO			5
#define		COLOR_PLANE_SPECIFIER_ADDRESS	6
#define		COMPRESSION_RATIO				7
#define		PRODUCT_ID						8
#define		IMAGE_SIZE_ADDRESS_HIWORD_HI	12
#define		IMAGE_SIZE_ADDRESS_HIWORD_LO	13
#define		IMAGE_SIZE_ADDRESS_LOWORD_HI	14
#define		IMAGE_SIZE_ADDRESS_LOWORD_LO	15
#define		IMAGE_WIDTH_ADDRESS_HI			16
#define		IMAGE_WIDTH_ADDRESS_LO			17
#define		IMAGE_HEIGTH_ADDRESS_HI			18
#define		IMAGE_HEIGTH_ADDRESS_LO			19
#define		ABS_X_ADDRESS_HI				20
#define		ABS_X_ADDRESS_LO				21
#define		ABS_Y_ADDRESS_HI				22
#define		ABS_Y_ADDRESS_LO				23
#define		BIT_DEPTH_ADDRESS				24

#define eK 3
typedef enum
{
	eDelta32,
	eDeltaPlus = 24,
	eFX = 18,
	eRAW = 2
} CompressionMethod;

#define		KILOBYTE 1024
#define		MAX_IMAGE 200*KILOBYTE

DRIVER_ERROR LJFastRaster::Encapsulate (const RASTERDATA* InputRaster, BOOL bLastPlane)
{
    BYTE    res[64];
    DRIVER_ERROR    err = NO_ERROR;

	//** form FR header
	unsigned char	pucHeader[FAST_RASTER_HEADERSIZE];
	long lImageWidth = ((ModeDeltaPlus*)m_pCompressor)->inputsize;
	long lResolution = 600;
	long lBlockOffset = ((((ModeDeltaPlus*)m_pCompressor)->m_lPrinterRasterRow + 127) / 128) * 128 - 128;
	long lBitDepth = 1;
	long lBlockHeight = ((ModeDeltaPlus*)m_pCompressor)->m_lCurrBlockHeight;

	WORD wTemp = LOWORD (lBlockOffset);
	BYTE byHIByte = 0;
	BYTE byLOByte = 0;

    memset (pucHeader, 0, FAST_RASTER_HEADERSIZE);

	pucHeader[ABS_X_ADDRESS_HI] = 0;
	pucHeader[ABS_X_ADDRESS_LO] = 0;
	pucHeader[ABS_Y_ADDRESS_HI] = HIBYTE (wTemp);
	pucHeader[ABS_Y_ADDRESS_LO] = LOBYTE (wTemp);

	pucHeader[BASE_ADDRESS] = 0;
	pucHeader[PAGE_NUM_ADDRESS] = 1;

	wTemp = (WORD) (lResolution );
	byHIByte = HIBYTE (wTemp);
	byLOByte = LOBYTE (wTemp);
	pucHeader[RESOLUTION_ADDRESS_HI] = byHIByte;
	pucHeader[RESOLUTION_ADDRESS_LO] = byLOByte;
	
	wTemp = ((ModeDeltaPlus*)m_pCompressor)->m_bCompressed ? (WORD)eDeltaPlus : (WORD)eRAW;
	byHIByte = HIBYTE (wTemp);
	byLOByte = LOBYTE (wTemp);
	pucHeader[COMPRESSION_ADDRESS_HI] = byHIByte;
	pucHeader[COMPRESSION_ADDRESS_LO] = byLOByte;

	pucHeader[COLOR_PLANE_SPECIFIER_ADDRESS]	= (BYTE)eK;
	pucHeader[COMPRESSION_RATIO]				= (BYTE)ceil (((ModeDeltaPlus*)m_pCompressor)->m_fRatio);
	wTemp = HIWORD (InputRaster->rastersize[COLORTYPE_COLOR]);
	byHIByte = HIBYTE (wTemp);
	byLOByte = LOBYTE (wTemp);
	pucHeader[IMAGE_SIZE_ADDRESS_HIWORD_HI] = byHIByte;
	pucHeader[IMAGE_SIZE_ADDRESS_HIWORD_LO] = byLOByte;
	
	wTemp = LOWORD (InputRaster->rastersize[COLORTYPE_COLOR]);
	byHIByte = HIBYTE (wTemp);
	byLOByte = LOBYTE (wTemp);
	pucHeader[IMAGE_SIZE_ADDRESS_LOWORD_HI] = byHIByte;
	pucHeader[IMAGE_SIZE_ADDRESS_LOWORD_LO] = byLOByte;

	wTemp = LOWORD (lImageWidth*8);
	byHIByte = HIBYTE (wTemp);
	byLOByte = LOBYTE (wTemp);

	pucHeader[IMAGE_WIDTH_ADDRESS_HI] = byHIByte;
	pucHeader[IMAGE_WIDTH_ADDRESS_LO] = byLOByte;
	
	wTemp = LOWORD (lBlockHeight);
	byHIByte = HIBYTE (wTemp);
	byLOByte = LOBYTE (wTemp);
	pucHeader[IMAGE_HEIGTH_ADDRESS_HI] = byHIByte;
	pucHeader[IMAGE_HEIGTH_ADDRESS_LO] = byLOByte;
				
	wTemp = LOWORD (lBitDepth);
	pucHeader[BIT_DEPTH_ADDRESS] = LOBYTE (wTemp);

    unsigned int   ulVUDataLength = (int)(InputRaster->rastersize[COLORTYPE_COLOR] + FAST_RASTER_HEADERSIZE);

	BYTE FrEnterFRModeSeq[] = {0xC2, 0x06, 0x20, 0x70,0x68, 0xF8, 0x91, 0xC2};
	err = Send ((const BYTE *)FrEnterFRModeSeq, sizeof(FrEnterFRModeSeq));
	ERRCHECK;
    res[0] = (BYTE) (ulVUDataLength & 0xFF);
    res[1] = (BYTE) ((ulVUDataLength & 0x0000FF00) >> 8);
    res[2] = (BYTE) ((ulVUDataLength & 0x00FF0000) >> 16);
    res[3] = (BYTE) ((ulVUDataLength & 0xFF000000) >> 24);
    res[4] = 0xF8;
    res[5] = 0x92;
    res[6] = 0x46;
    err = Send (res, 7);
	ERRCHECK;

	//** now embed raster data, header and all 	
    err = Send (pucHeader, FAST_RASTER_HEADERSIZE);
	ERRCHECK;
    err = Send (InputRaster->rasterdata[COLORTYPE_COLOR],
			    InputRaster->rastersize[COLORTYPE_COLOR]);
    return err;
}

Header* LJFastRaster::SelectHeader (PrintContext *pc)
{
    phLJFastRaster = new HeaderLJFastRaster (this, pc);
	return phLJFastRaster;
}

DRIVER_ERROR LJFastRaster::VerifyPenInfo()
{
    ePen = BLACK_PEN;
    return NO_ERROR;
}

DRIVER_ERROR LJFastRaster::ParsePenInfo(PEN_TYPE& ePen, BOOL QueryPrinter)
{
    ePen = BOTH_PENS;

    return NO_ERROR;
}

Compressor* LJFastRaster::CreateCompressor (unsigned int RasterSize)
{
    m_pCompressor = new ModeDeltaPlus (pSS, this, RasterSize);
	return m_pCompressor;
}

/*
 *  Function name: ParseError
 *
 *  Owner: Darrell Walker
 *
 *  Purpose:  To determine what error state the printer is in.
 *
 *  Called by: Send()
 *
 *  Parameters on entry: status_reg is the contents of the centronics
 *                      status register (at the time the error was
 *                      detected)
 *
 *  Parameters on exit: unchanged
 *
 *  Return Values: The proper DISPLAY_STATUS to reflect the printer
 *              error state.
 *
 */
DISPLAY_STATUS LJFastRaster::ParseError(BYTE status_reg)
{
    DBG1("LJFastRaster: parsing error info\n");

    DRIVER_ERROR err = NO_ERROR;
    BYTE    szReadBuff[256];
    DWORD   iReadCount = 256;
    DISPLAY_STATUS  eStatus = (DISPLAY_STATUS) status_reg;
    char    *tmpStr;
    int     iErrorCode;

    if (!IOMode.bDevID)
        return eStatus;

    memset (szReadBuff, 0, 256);
    err = pSS->FromDevice (szReadBuff, &iReadCount);
    if (err == NO_ERROR && iReadCount == 0)
        return eStatus;

    if (strstr ((char *) szReadBuff, "JOB"))
    {
        if (!(tmpStr = strstr ((char *) szReadBuff, "NAME")))
            return DISPLAY_PRINTING;
        tmpStr += 6;
        while (*tmpStr < '0' || *tmpStr > '9')
            tmpStr++;
        sscanf (tmpStr, "%d", &iErrorCode);
        if (iErrorCode != (long) (this))
            return DISPLAY_PRINTING;
    }

    if (strstr ((char *) szReadBuff, "END"))
    {
        return DISPLAY_PRINTING_COMPLETE;
    }


    if (strstr ((char *) szReadBuff, "CANCEL"))
        return DISPLAY_PRINTING_CANCELED;

    if (!(tmpStr = strstr ((char *) szReadBuff, "CODE")))
        return eStatus;

    tmpStr += 4;
    while (*tmpStr < '0' || *tmpStr > '9')
        tmpStr++;
    sscanf (tmpStr, "%d", &iErrorCode);

    if (iErrorCode < 32000)
        return DISPLAY_PRINTING;

    if (iErrorCode == 40010 || iErrorCode == 40020)
        return DISPLAY_NO_PENS;     // Actually, out of toner

    if (iErrorCode == 40021)
        return DISPLAY_TOP_COVER_OPEN;

    if ((iErrorCode / 100) == 419)
        return DISPLAY_OUT_OF_PAPER;

    if ((iErrorCode / 1000) == 42 || iErrorCode == 40022)
    {
        DBG1("Paper Jammed\n");
        return DISPLAY_PAPER_JAMMED;
    }

    if (iErrorCode > 40049 && iErrorCode < 41000)
    {
        DBG1("IO trap\n");
        return DISPLAY_ERROR_TRAP;
    }

    if (iErrorCode == 40079)
        return DISPLAY_OFFLINE;

    return DISPLAY_ERROR_TRAP;
}


//--------------------------------------------------------------------
// Function:    ModeDeltaPlus::ModeDeltaPlus
//
// Release:     [PROTO4_1]
//
// Description: Preferred ctor
//
// Input:        padMultiple - the fBufferDataLength returned from
//                             GetRow must be divisible by this value.
//
// Modifies:     fpBuffer
//
// Precond:      None
//
// Postcond:     None
//
// Returns:      None
//
// Created:            11/07/96 cal
// Last Modified:      5/020/01 DG
//--------------------------------------------------------------------
ModeDeltaPlus::ModeDeltaPlus 
(    
	SystemServices* pSys,
    Printer* pPrinter,
    unsigned int PlaneSize
) :
    Compressor(pSys, PlaneSize, TRUE),
    thePrinter(pPrinter),    // needed by Flush
    pbyInputImageBuffer (NULL),
    pbySeedRow (NULL)
{
    if (constructor_error != NO_ERROR)  // if error in base constructor
    {
        return;
    }

	inputsize = PlaneSize / (MAXCOLORPLANES * MAXCOLORDEPTH * MAXCOLORROWS);
    inputsize = ((inputsize + 7) / 8) * 8;

	// allocate a 2X compression buffer..
	compressBuf = (BYTE*)pSS->AllocMem(2 * INDY_STRIP_HEIGHT * inputsize);
	if (compressBuf == NULL)
    {
		constructor_error = ALLOCMEM_ERROR;
    }
	memset (compressBuf, 0x00, 2 * INDY_STRIP_HEIGHT * inputsize);

	pbyInputImageBuffer = (BYTE*)pSS->AllocMem(INDY_STRIP_HEIGHT * inputsize);
	if (pbyInputImageBuffer == NULL)
		constructor_error=ALLOCMEM_ERROR;
	memset(pbyInputImageBuffer, 0x00, INDY_STRIP_HEIGHT * inputsize);

    pbySeedRow = (HPUInt8*) pSS->AllocMem (inputsize * sizeof (HPUInt8));
    if (pbySeedRow == NULL)
    {
        constructor_error = ALLOCMEM_ERROR;
    }
    memset (pbySeedRow, 0, inputsize * sizeof (HPUInt8));

	m_lCurrCDRasterRow	= 0;
	//m_lPrinterRasterRow = (((LJFastRaster*)thePrinter)->phLJFastRaster)->thePrintContext->PrintableStartY() * 600;
		
	m_lPrinterRasterRow = 0;

	iRastersReady = 0;

	m_bCompressed = FALSE;
	m_compressedsize = 0;
	m_fRatio = 0;
} // ModeDeltaPlus::ModeDeltaPlus


//--------------------------------------------------------------------
// Function:    ModeDeltaPlus::~ModeDeltaPlus
//
// Release:     [PROTO4_1]
//
// Description: Destructor.
//
// Input:       None
//
// Modifies:    fpBuffer - deletes the buffer
//              ?? - deletes other buffer(s)??
//
// Precond:     None
//
// Postcond:    None
//
// Returns:     None
//
// Created:            11/07/96 cal
// Last Modified:      5/020/01 DG
//--------------------------------------------------------------------
ModeDeltaPlus::~ModeDeltaPlus ()
{ 
	if (pbyInputImageBuffer)
	{
		pSS->FreeMem (pbyInputImageBuffer);
		pbyInputImageBuffer = NULL;
	}
    if (pbySeedRow)
    {
        pSS->FreeMem (pbySeedRow);
        pbySeedRow = NULL;
    }

} // ModeDeltaPlus::~ModeDeltaPlus

/*
 * The maximum width of a line, which is limited by the amount of hardware
 * buffer space allocated to storing the seedrow.
 */
#define ROW_LIMIT 7040
/*
 * The maximum number of literals in a single command, not counting the first
 * pixel.  This is limited by the hardware buffer used to store a literal
 * string.  For real images, I expect a value of 64 would be a suitable
 * minimum.  The minimum compression ratio will be bounded by this.  Note also
 * that the software does not need any buffer for this, so there need be no
 * limit at all on a purely software implementation.  For the sake of enabling
 * a hardware implementation, I would strongly recommend leaving it in and set
 * to some reasonable value (say 1023 or 255).
 */
#define LITERAL_LIMIT 511

/* These are set up this way to make it easy to change the predictions. */
#define LTEST_W            col > 0
#define LVAL_W(col)        cur_row[col-1]
#define LTEST_NW           col > 0
#define LVAL_NW(col)       seedrow[col-1]
#define LTEST_WW           col > 1
#define LVAL_WW(col)       cur_row[col-2]
#define LTEST_NWW          col > 1
#define LVAL_NWW(col)      seedrow[col-2]
#define LTEST_NE           (col+1) < row_width
#define LVAL_NE(col)       seedrow[col+1]
#define LTEST_NEWCOL       1
#define LVAL_NEWCOL(col)   new_color
#define LTEST_CACHE        1
#define LVAL_CACHE(col)    cache

#define LOC1TEST      LTEST_NE
#define LOC1VAL(col)  LVAL_NE(col)
#define LOC2TEST      LTEST_NW
#define LOC2VAL(col)  LVAL_NW(col)
#define LOC3TEST      LTEST_NEWCOL
#define LOC3VAL(col)  LVAL_NEWCOL(col)


#define check(condition) if (!(condition)) return 0

#define write_comp_byte(val)           \
   check(outptr < pastoutmem);         \
   *outptr++ = (HPUInt8) val;

#define read_byte(val)                 \
   check(inmem < pastinmem);           \
   val = *inmem++;

#define encode_count(count, over, mem)         \
   if (count >= over)                          \
   {                                           \
      count -= over;                           \
      if (count <= (uint32_t) 253)               \
      {                                        \
         check(mem < pastoutmem);              \
         *mem++ = (HPUInt8) count;               \
      }                                        \
      else if (count <= (uint32_t) (254 + 255) ) \
      {                                        \
         check((mem+1) < pastoutmem);          \
         check( count >= 254 );                \
         check( (count - 254) <= 255 );        \
         *mem++ = (HPUInt8) 0xFE;                \
         *mem++ = (HPUInt8) (count - 254);       \
      }                                        \
      else                                     \
      {                                        \
         check((mem+2) < pastoutmem);          \
         check( count >= 255 );                \
         check( (count - 255) <= 65535 );      \
         count -= 255;                         \
         *mem++ = (HPUInt8) 0xFF;                \
         *mem++ = (HPUInt8) (count >> 8);        \
         *mem++ = (HPUInt8) (count & 0xFF);      \
      }                                        \
   }
#define decode_count(count, over)              \
   if (count >= over)                          \
   {                                           \
      read_byte(inval);                        \
      count += (uint32_t) inval;                 \
      if (inval == (HPUInt8) 0xFE)               \
      {                                        \
         read_byte(inval);                     \
         count += (uint32_t) inval;              \
      }                                        \
      else if (inval == (HPUInt8) 0xFF)          \
      {                                        \
         read_byte(inval);                     \
         count += (((uint32_t) inval) << 8);     \
         read_byte(inval);                     \
         count += (uint32_t) inval;              \
      }                                        \
   }

#define bytes_for_count(count, over) \
  ( (count >= 255) ? 3 : (count >= over) ? 1 : 0 )


/* The number of bytes we should be greater than to call memset/memcpy */
#define memutil_thresh 15

HPUInt8* ModeDeltaPlus::encode_header(HPUInt8* outptr, const HPUInt8* pastoutmem, uint32_t isrun, uint32_t location, uint32_t seedrow_count, uint32_t run_count, const HPUInt8 new_color)
{
    uint32_t byte;

    check (location < 4);
    check( (isrun == 0) || (isrun == 1) );

    /* encode "literal" in command byte */
    byte = (isrun << 7) | (location << 5);

    /* write out number of seedrow bytes to copy */
    if (seedrow_count > 2)
        byte |= (0x03 << 3);
    else
        byte |= (seedrow_count << 3);

    if (run_count > 6)
        byte |= 0x07;
    else
        byte |= run_count;

    /* write out command byte */
    write_comp_byte(byte);

    /* macro to write count if it's 3 or more */
    encode_count( seedrow_count, 3, outptr );

    /* if required, write out color of first pixel */
    if (location == 0)
    {
        write_comp_byte( new_color );
    }

    /* macro to write count if it's 7 or more */
    encode_count( run_count, 7, outptr );

    return outptr;
}

/******************************************************************************/
/*                                COMPRESSION                                 */
/******************************************************************************/
BOOL ModeDeltaPlus::Compress (HPUInt8   *outmem,
                              uint32_t  *outlen,
                              const     HPUInt8     *inmem,
                              const     uint32_t    row_width,
                              const     uint32_t    inheight,
                              uint32_t  horz_ht_dist)
{
	register    HPUInt8     *outptr = outmem;
	register    uint32_t    col;
    const       HPUInt8     *seedrow;
    uint32_t                seedrow_count = 0;
    uint32_t                location = 0;
    HPUInt8                 new_color = (HPUInt8) 0xFF;
    const       HPUInt8     *cur_row;
    uint32_t				row;
    const       HPUInt8     *pastoutmem = outmem + *outlen;
    uint32_t                do_word_copies;
    /* Halftone distance must be 1-32 (but allow 0 == 1) */
    if (horz_ht_dist > 32)
    {
        return FALSE;
    }
    if (horz_ht_dist < 1)
    {
       horz_ht_dist = 1;
    }

    seedrow = pbySeedRow;
    do_word_copies = ((row_width % 4) == 0);

    for (row = 0; row < inheight; row++)
    {
        cur_row = inmem + (row * row_width);

        col = 0;
        while (col < row_width)
        {
            /* First look for seedrow copy */
            seedrow_count = 0;
            if (do_word_copies)
            {
                /* Try a fast word-based search */
                while ( ((col & 3) != 0) &&
                        (col < row_width) &&
                        (cur_row[col] == seedrow[col]) )
                {
                    seedrow_count++;
                    col++;
                }
                if ( (col & 3) == 0)
                {
                    while ( ((col+3) < row_width) &&
                            *((const uint32_t*) (cur_row + col)) == *((const uint32_t*) (seedrow + col)) )
                    {
                        seedrow_count += 4;
                        col += 4;
                    }
                }
            }
            while ( (col < row_width) && (cur_row[col] == seedrow[col]) )
            {
               seedrow_count++;
               col++;
            }

            /* It is possible that we have hit the end of the line already. */
            if (col == row_width)
            {
                /* encode pure seed run as fake run */
                outptr = encode_header(outptr, pastoutmem, 1 /*run*/, 1 /*location*/, seedrow_count, 0 /*runcount*/, (HPUInt8) 0 /*color*/);
                /* exit the while loop for this row */
                break;
            }
            check(col < row_width);


            /* determine the prediction for the current pixel */
            if ( (LOC1TEST) && (cur_row[col] == LOC1VAL(col)) )
                location = 1;
            else if ( (LOC2TEST) && (cur_row[col] == LOC2VAL(col)) )
                location = 2;
            else if ( (LOC3TEST) && (cur_row[col] == LOC3VAL(col)) )
                location = 3;
            else
            {
                location = 0;
                new_color = cur_row[col];
            }


            /* Look for a run */
            if (
                 ((col+1) < row_width)
                 &&
                 ((col+1) >= horz_ht_dist)
                 &&
                 (cur_row[col+1-horz_ht_dist] == cur_row[col+1])
               )
            {
                /* We found a run.  Determine the length. */
                uint32_t run_count = 0;   /* Actually 2 */
                col++;
                while ( ((col+1) < row_width) && (cur_row[col+1-horz_ht_dist] == cur_row[col+1]) )
                {
                   run_count++;
                   col++;
                }
                col++;
                outptr = encode_header(outptr, pastoutmem, 1 /*run*/, location, seedrow_count, run_count, new_color);
            }

            else

            /* We didn't find a run.  Encode literal(s). */
            {
                uint32_t replacement_count = 0;   /* Actually 1 */
                const HPUInt8* byte_array = cur_row + col + 1;
                uint32_t i;
                col++;
                /*
                 * The (col+1) in this test is used because there is no need to
                 * check for literal breaks if this is the last byte of the row.
                 * Instead we just tack it on to our literal count at the end.
                 */
                while ( (col+1) < row_width )
                {
                    /*
                     * All cases that will break with 1 unit saved.  This
                     * should be the best breaking spots, since we will always
                     * gain with the break, but never break for no gain.  This
                     * leads to longer strings which is good for decomp speed.
                     */
                    if (
                         /* Seedrow ... */
                         (
                           (cur_row[col] == seedrow[col])
                           &&
                           (
                             /* 2 seedrows */
                             (
                               (cur_row[col+1] == seedrow[col+1])
                             )
                             ||
                             /* seedrow and predict */
                             (
                               (cur_row[col+1] == LVAL_NW(col+1))
                               ||
                               (cur_row[col+1] == LVAL_NEWCOL(col+1))
                             )
                             ||
                             (
                               ((col+2) < row_width)
                               &&
                               (
                                 /* seedrow and run */
                                 (
                                   ((col + 2) >= horz_ht_dist) &&
                                   (cur_row[col+2-horz_ht_dist] == cur_row[col+2])
                                 )
                                 ||
                                 /* seedrow and northeast predict */
                                 (cur_row[col+1] == LVAL_NE(col+1))
                               )
                             )
                           )
                         )
                         ||
                         /* Run ... */
                         (
                           (cur_row[col] != seedrow[col])
                           &&
                           ((col + 1) >= horz_ht_dist)
                           &&
                           (cur_row[col+1-horz_ht_dist] == cur_row[col+1])
                           &&
                           (
                             /* Run of 3 or more */
                             (
                               ((col+2) < row_width)
                               &&
                               ((col + 2) >= horz_ht_dist)
                               &&
                               (cur_row[col+2-horz_ht_dist] == cur_row[col+2])
                             )
                             ||
                             /* Predict first unit of run */
                             (cur_row[col] == LVAL_NE(col))
                             ||
                             (cur_row[col] == LVAL_NW(col))
                             ||
                             (cur_row[col] == LVAL_NEWCOL(col))
                           )
                         )
                       )
                        break;

                    /* limited hardware buffer */
                    if (replacement_count >= LITERAL_LIMIT)
                        break;

                    /* add another literal to the list */
                    replacement_count++;
                    col++;
                }

                /* If almost at end of block, just extend the literal by one */
                if ( (col+1) == row_width ) {
                   replacement_count++;
                   col++;
                }

                outptr = encode_header(outptr, pastoutmem, 0 /*not run*/, location, seedrow_count, replacement_count, new_color);

                /* Copy bytes from the byte array.  If rc was 1, then we will
                 * have encoded a zero in the command byte, so nothing will be
                 * copied here (the 1 indicates the first pixel, which was
                 * written above or was predicted.  If rc is between 2 and 7,
                 * then a value between 1 and 6 will have been written in the
                 * command byte, and we will copy it directly.  If 8 or more,
                 * then we encode more counts, then finally copy all the values
                 * from byte_array.
                 */

                if (replacement_count > 0)
                {
                    /* Now insert rc bytes of data from byte_array */
                    if (replacement_count > memutil_thresh)
                    {
                        check( (outptr + replacement_count) <= pastoutmem );
                        memcpy(outptr, byte_array, (size_t) replacement_count);
                        outptr += replacement_count;
                    }
                    else
                    {
                        for (i = 0; i < replacement_count; i++)
                        {
                            write_comp_byte( byte_array[i] );
                        }
                    }
                }
            }

        } /* end of column */

        /* save current row as next row's seed row */
        seedrow = cur_row;

    } /* end of row */

    check( outptr <= pastoutmem );
    if (outptr > pastoutmem)
    {
       /* We're in big trouble -- we wrote PAST the end of their memory! */
       *outlen = 0;
       return 0;
    }

    *outlen = (uint32_t) (outptr - outmem);

    return 1;
} /* end of deltaplus_compress2 */


BOOL ModeDeltaPlus::Process
(
    RASTERDATA* input
)
{
	DRIVER_ERROR		err = NO_ERROR;

    if (input==NULL || 
		(input->rasterdata[COLORTYPE_COLOR]==NULL && input->rasterdata[COLORTYPE_BLACK]==NULL))    // flushing pipeline
    {
        return FALSE;
    }
	if (input->rasterdata[COLORTYPE_COLOR])
	{
		if (m_lCurrCDRasterRow < INDY_STRIP_HEIGHT )
		{
			//Copy the data to m_SourceBitmap
			memcpy(pbyInputImageBuffer + m_lCurrCDRasterRow * inputsize, input->rasterdata[COLORTYPE_COLOR], input->rastersize[COLORTYPE_COLOR]);
			m_lCurrCDRasterRow ++;
		}
		if (m_lCurrCDRasterRow == INDY_STRIP_HEIGHT || ((LJFastRaster*)thePrinter)->phLJFastRaster->IsLastBand())
		{
			if (((LJFastRaster*)thePrinter)->m_bStartPageNotSent)
			{
				err = (((LJFastRaster*)thePrinter)->phLJFastRaster)->StartPage();
				((LJFastRaster*)thePrinter)->m_bStartPageNotSent = FALSE;
						m_lPrinterRasterRow = 0;
			}

			m_compressedsize = 2 * inputsize * INDY_STRIP_HEIGHT;
            BOOL bRet = Compress (compressBuf, 
                                  &m_compressedsize,
                                  pbyInputImageBuffer,
                                  inputsize,
                                  m_lCurrCDRasterRow,
                                  16
                                  );
			if (!bRet)
			{
				memcpy (compressBuf, pbyInputImageBuffer, inputsize * INDY_STRIP_HEIGHT);
				m_compressedsize = inputsize * INDY_STRIP_HEIGHT;
			}
			else
			{
				m_bCompressed = TRUE;
				//m_fRatio = (float)m_compressedsize / (float)(inputsize * INDY_STRIP_HEIGHT);
			}

			memset(pbyInputImageBuffer, 0x00, inputsize * INDY_STRIP_HEIGHT);

			m_lPrinterRasterRow += m_lCurrCDRasterRow;
			m_lCurrBlockHeight = m_lCurrCDRasterRow;
			m_lCurrCDRasterRow = 0;
			iRastersReady = 1;

			((LJFastRaster*)thePrinter)->phLJFastRaster->SetLastBand(FALSE);
		}
		else
		{
			return FALSE;
		}
	}
	return TRUE;
} //Process

BYTE* ModeDeltaPlus::NextOutputRaster(COLORTYPE color)
// since we return 1-for-1, just return result first call
{
	if (iRastersReady==0)
		return (BYTE*)NULL;

	if (color == COLORTYPE_BLACK)
	{
		return (BYTE*)NULL;
	}
	else
	{
		iRastersReady=0;
		return compressBuf;
	}
}

unsigned int ModeDeltaPlus::GetOutputWidth(COLORTYPE  color)
{
	if (color == COLORTYPE_COLOR)
	{
		return m_compressedsize;
	}
	else
	{
		return 0;
	}
} //GetOutputWidth

APDK_END_NAMESPACE

#endif  // defined APDK_LJFASTRASTER
