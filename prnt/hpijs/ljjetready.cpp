/*****************************************************************************\
  ljjetready.cpp : Implimentation for the LJJetReady class

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


#ifdef APDK_LJJETREADY
/*
 *  Not sure need HAVE_PROTOTYPES here, a user reported a compiler error under Tru64. The error
 *  was for the assignment jerr.error_exit = Gjpeg_error; This seems to be due to JMETHOD
 *  definition in jmorecfg.h
 */

//#define HAVE_PROTOTYPES

#include "header.h"
#include "io_defs.h"
#include "ljjetready.h"
#include "printerproxy.h"
#include "resources.h"

#ifdef HAVE_LIBDL
#include <dlfcn.h>
#endif

extern "C"
{
int (*HPLJJRCompress) (BYTE       *pbOutBuffer, 
                       uint32_t   *outlen, 
                       BYTE       *inmem, 
                       const uint32_t iLogicalImageWidth,
                       const uint32_t iLogicalImageHeight);
}

APDK_BEGIN_NAMESPACE

#ifdef HAVE_LIBDL
extern void *LoadPlugin (const char *szPluginName);
#endif

extern MediaSize PaperToMediaSize(PAPER_SIZE psize);

#define MOJAVE_STRIP_HEIGHT	128 // Mojave strip height should always be 128 for compression sake

#define MOJAVE_RESOLUTION	600 // Mojave supports only one resolution- 600 DPI	

BYTE JrBeginSessionSeq[]				= {0xC0, 0x00, 0xF8, 0x86, 0xC0, 0x03, 0xF8, 0x8F, 0xD1, 0x58, 
										    0x02, 0x58, 0x02, 0xF8, 0x89, 0x41};
BYTE JrFeedOrientationSeq[]				= {0xC0, 0x00  , 0xF8, 0x28 };
//											   |fd ori enum|       |ori cmd|											
BYTE JrPaperSizeSeq[]					= {0xC0,  0x00      ,0xF8, 0x25};
//											   |pap siz enum|     |pap sz cmd|
BYTE JrMedSourceSeq[]					= {0xC0,  0x00      ,0xF8, 0x26        };
//											   |Med src enum|     |Med src cmd|
BYTE JrMedDestinationSeq[]				= {0xC0,  0x00        ,0xF8 , 0x24        };
//											   |Med Dest enum|      |Med src cmd|
BYTE JrBeginPageSeq[]				    = {0x43, 0xD3, 0x64, 0x00, 0x64, 0x00, 0xF8, 0x2A, 0x75, 0xD3, 
										    0x00, 0x00, 0x00, 0x00, 0xF8, 0x4C, 0x6B};
BYTE JrBeginImageSeq[]				    = {0xC2, 0x00, 0x40, 0x70, 0x68, 0xF8, 0x91, 0xC1};

BYTE JrVU_ver_TagSeq[]					= {0xC2, 0x00, 0x00, 0x04, 0x00 , 0xF8, 0x95			  };
//												|endian alignd         |  |Jr_ver_ tag|
BYTE JrDataLengthSeq[]					= {0xC2, 0x86, 0x0A, 0x00, 0x00, 0xF8, 0x92	  };
//																			  | VU data length|
BYTE JrVenUniqSeq[]						= {0x46};
BYTE JrVUExtn_3Seq[]					= {0xC2, 0x02, 0x40, 0x70, 0x68			,0xF8, 0x91	};
//												|endian alignd Jr rd img tag|	  |VU extensn|
BYTE JrEndPageSeq[]						=  {0x44};
BYTE JrEndSessionSeq[]					=  {0x42};
// PJL level commands..

//**JETLIB ENTRIES
const char ccpPJLStartJob[]				= {ESC, '%', '-','1','2','3','4','5','X' };;
const char ccpPJLExitSeq[] 				= {ESC, '%', '-','1','2','3','4','5','X', '@','P','J','L',' ','E','O','J', LF, ESC,'%', '-','1','2','3','4','5','X'};
const char ccpPJLSetRes[]				= {'@','P','J','L',' ','S','E','T',' ','R','E','S','O','L','U','T','I','O','N','=','6','0','0', LF};
const char ccpPJLUTF8[]					= {'@','P','J','L',' ','S','E','T',' ','S','T','R','I','N','G','C','O','D','E','S','E','T','=','U','T','F','8', LF};
const char ccpPJLSetPlanes[]			= {'@','P','J','L',' ','S','E','T',' ','P','L','A','N','E','S','I','N','U','S','E','=','1',LF}; //Sent only for GrayScale Printing
const char ccpPCLEnterXL[]				= {'@','P','J','L',' ','E','N','T','E','R',' ','L','A','N','G','U','A','G','E','=','P','C','L','X','L', LF};
const char ccpPJLSetTO[]				= {'@','P','J','L',' ','S','E','T',' ','T','I','M','E','O','U','T','=','9','0', LF};
const char ccpPJLSetCopyCount[]			= {'@','P','J','L',' ','S','E','T',' ','C','O','P','I','E','S','=','1',LF};
const char ccpUEL[]                     = {ESC, '%', '-','1','2','3','4','5','X' };
const char ccpPJLComment[]				= {')',' ','H','P','-','P','C','L',' ','X','L',';','3',';','0',';','C','o','m','m','e','n','t',',',' ','P','C','L','-','X','L',' ','J','e','t','R','e','a','d','y',' ','g','e','n','e','r','a','t','o','r', LF};
//**END JETLIB ENTRIES


LJJetReady::LJJetReady (SystemServices* pSS, int numfonts, BOOL proto)
    : Printer(pSS, numfonts, proto)
{

    if ((!proto) && (IOMode.bDevID))
    {
        constructor_error = VerifyPenInfo();
        CERRCHECK;
    }
    else ePen = BOTH_PENS;    // matches default mode
	
    pMode[GRAYMODE_INDEX]      = new LJJetReadyGrayMode ();
    pMode[DEFAULTMODE_INDEX]   = new LJJetReadyNormalMode ();
    ModeCount = 2;


    CMYMap = NULL;
    m_bJobStarted = FALSE;
#ifdef  APDK_AUTODUPLEX
    m_bRotateBackPage = FALSE;  // Lasers don't require back side image to be rotated
#endif
    m_pCompressor = NULL;
    m_iYPos = 0;

	m_bStartPageNotSent = TRUE;

    HPLJJRCompress = NULL;

#ifdef HAVE_LIBDL
    m_hHPLibHandle = LoadPlugin ("lj.so");
    if (m_hHPLibHandle)
    {
        dlerror ();
        *(void **) (&HPLJJRCompress) = dlsym (m_hHPLibHandle, "HPJetReadyCompress");
    }
    if (HPLJJRCompress)
    {
        pMode[ModeCount] = new LJJetReadyBestColorMode ();
        ModeCount++;
        pMode[ModeCount] = new LJJetReadyBestGrayMode ();
        ModeCount++;
    }
#endif
    m_eCompressMode = COMPRESS_MODE_JPEG;

    DBG1("LJJetReady created\n");
}

LJJetReady::~LJJetReady ()
{
    DISPLAY_STATUS  eDispStatus;

#ifdef HAVE_LIBDL
    if (m_hHPLibHandle)
    {
        dlclose (m_hHPLibHandle);
    }
#endif

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

LJJetReadyNormalMode::LJJetReadyNormalMode ()
: PrintMode (NULL)
{
    ResolutionX[0] = 600;
    ResolutionY[0] = 600;
    BaseResX = BaseResY = 600;

    bFontCapable = FALSE;

#if defined(APDK_VIP_COLORFILTERING)
    Config.bErnie = FALSE;
#endif
    Config.bColorImage = FALSE;
	theQuality = qualityNormal;

    pmQuality = QUALITY_NORMAL;
	Config.bColorImage = FALSE;
#ifdef APDK_AUTODUPLEX
    bDuplexCapable = TRUE;
#endif
}

LJJetReadyBestColorMode::LJJetReadyBestColorMode () : PrintMode (NULL)
{
    ResolutionX[0] =
    ResolutionY[0] =
    BaseResX =
    BaseResY = 600;

    bFontCapable = FALSE;

#if defined(APDK_VIP_COLORFILTERING)
    Config.bErnie = FALSE; // Raghu
#endif

    Config.bColorImage = FALSE;
    theQuality = qualityPresentation;
    pmQuality = QUALITY_BEST;
#ifdef APDK_AUTODUPLEX
    bDuplexCapable = TRUE;
#endif
}

LJJetReadyGrayMode::LJJetReadyGrayMode () : PrintMode (NULL)
{
    ResolutionX[0] =
    ResolutionY[0] =
    BaseResX =
    BaseResY = 600;

    bFontCapable = FALSE;

#if defined(APDK_VIP_COLORFILTERING)
    Config.bErnie = FALSE; // Raghu
#endif

    Config.bColorImage = FALSE;
    pmColor = GREY_K;
#ifdef APDK_AUTODUPLEX
    bDuplexCapable = TRUE;
#endif
}

LJJetReadyBestGrayMode::LJJetReadyBestGrayMode () : PrintMode (NULL)
{
    ResolutionX[0] =
    ResolutionY[0] =
    BaseResX =
    BaseResY = 600;

    bFontCapable = FALSE;

#if defined(APDK_VIP_COLORFILTERING)
    Config.bErnie = FALSE; // Raghu
#endif

    Config.bColorImage = FALSE;
    pmColor = GREY_K;
    pmQuality = QUALITY_BEST;
    theQuality = qualityPresentation;
#ifdef APDK_AUTODUPLEX
    bDuplexCapable = TRUE;
#endif
}

HeaderLJJetReady::HeaderLJJetReady (Printer* p, PrintContext* pc)
    : Header(p,pc)
{ 
	SetLastBand(FALSE);
}

DRIVER_ERROR HeaderLJJetReady::Send ()
{
    StartSend ();

    return NO_ERROR;
}

DRIVER_ERROR HeaderLJJetReady::StartSend ()
{
    DRIVER_ERROR err;
    char    szScratchStr[64];

    err = thePrinter->Send ((const BYTE*)ccpPJLStartJob,	sizeof(ccpPJLStartJob));
    ERRCHECK;

    //Send the UTF8 encoding command
    err = thePrinter->Send ((const BYTE*)ccpPJLUTF8, sizeof(ccpPJLUTF8));
    ERRCHECK;

    // If it is Grayscale printjob, send the PJL command indicating the same
    COLORMODE       eC = COLOR;
    MEDIATYPE       eM;
    QUALITY_MODE    eQ;
    BOOL            bD;

    ((LJJetReady *)thePrinter)->bGrey_K = FALSE;
    if ((thePrintContext->GetPrintModeSettings (eQ, eM, eC, bD)) == NO_ERROR &&
        eC == GREY_K)
    {
        ((LJJetReady *)thePrinter)->bGrey_K = TRUE;
        err = thePrinter->Send ((const BYTE*)ccpPJLSetPlanes,sizeof(ccpPJLSetPlanes));
        ERRCHECK;
    }
	
    //Send the Number of copies command
//    err = thePrinter->Send ((const BYTE*)ccpPJLSetCopyCount,sizeof(ccpPJLSetCopyCount));
    sprintf (szScratchStr, "@PJL SET COPIES=%d\015\012", thePrintContext->GetCopyCount ());
    err = thePrinter->Send ((const BYTE *) szScratchStr, strlen (szScratchStr));
    ERRCHECK;

    // Send the Duplex command
    strcpy (szScratchStr, "@PJL SET DUPLEX=OFF\015\012");
#ifdef APDK_AUTODUPLEX
    DUPLEXMODE  dupmode = thePrintContext->QueryDuplexMode ();
    if (dupmode != DUPLEXMODE_NONE)
    {
        strcpy (szScratchStr, "@PJL SET DUPLEX=ON\015\012@PJL SET BINDING=");
        if (dupmode == DUPLEXMODE_BOOK)
            strcat (szScratchStr, "LONGEDGE\015\012");
        else
            strcat (szScratchStr, "SHORTEDGE\015\012");
    }
#endif
    err = thePrinter->Send ((const BYTE *) szScratchStr, strlen (szScratchStr));
    ERRCHECK;

    //Set the resolution to 600
    err = thePrinter->Send ((const BYTE*)ccpPJLSetRes,sizeof(ccpPJLSetRes));
    ERRCHECK;

    //Send the time out command
    err = thePrinter->Send ((const BYTE*)ccpPJLSetTO,sizeof(ccpPJLSetTO));
    ERRCHECK;

    //send the mojave PCL_XL_ENTER_LANG command
    err = thePrinter->Send ((const BYTE*)ccpPCLEnterXL,sizeof(ccpPCLEnterXL));
    ERRCHECK;

    //send the comment string
    err = thePrinter->Send ((const BYTE*)ccpPJLComment,sizeof(ccpPJLComment));
    ERRCHECK;

    err = thePrinter->Send ((const BYTE*)JrBeginSessionSeq,sizeof(JrBeginSessionSeq));
    ERRCHECK;

    return err;
}

int HeaderLJJetReady::JRPaperToMediaSize(PAPER_SIZE psize)
{
    switch(psize)
    {
        case LETTER:            return 0;
        case LEGAL:             return 1;
        case A4:                return 2;
        case B4:                return 10;
        case B5:                return 11;
        case OUFUKU:            return 14;
        case HAGAKI:            return 14;
        case A6:                return 17;
#ifdef APDK_EXTENDED_MEDIASIZE
        case A3:                return 5;
        case A5:                return 16;
        case LEDGER:            return 4;
        case EXECUTIVE:         return 3;
        case CUSTOM_SIZE:       return 96;
        case ENVELOPE_NO_10:    return 6;
        case ENVELOPE_A2:       return 6;
        case ENVELOPE_C6:       return 8;
        case ENVELOPE_DL:       return 9;
#endif
        default:                return 0;
    }
}

DRIVER_ERROR HeaderLJJetReady::StartPage ()
{
    DRIVER_ERROR err;
    BYTE    szCustomSize[64];

	/* Orienatation: is JrFeedOrientationSeq[1]. Can take the following values:
		Portrait				: 0x00
		Landscape:				: 0x01
		Reversed    Portrait	: 0x02
		Reversed    Landscape	: 0x03
		Image		Orientataion: 0x04
		
		Mojave  supports only one feed orientation: Portrait
	*/
	err = thePrinter->Send ((const BYTE*)JrFeedOrientationSeq,sizeof(JrFeedOrientationSeq));
	ERRCHECK;

	// Paper Size
	PAPER_SIZE ps = thePrintContext->GetPaperSize ();
	int msizeCode;
    msizeCode = JRPaperToMediaSize(ps);	
	//Put the papersize into the JrPaperSizeSeq[]
	JrPaperSizeSeq[1] = (BYTE) msizeCode; 
#ifdef APDK_EXTENDED_MEDIASIZE
	if(msizeCode == 96) //Custom paper size
	{
        BYTE    szScratchStr[] = {"\xF8\x2F\xC0\x00\xF8\x30"};
        union
        {
            float       fValue;
            uint32_t    uiValue;
        } LJJRUnion;
        uint32_t    uiXsize;
        uint32_t    uiYsize;
        int         k = 0;
        LJJRUnion.fValue = (float) thePrintContext->PhysicalPageSizeX ();
        uiXsize = LJJRUnion.uiValue;
        LJJRUnion.fValue = (float) thePrintContext->PhysicalPageSizeY ();
        uiYsize = LJJRUnion.uiValue;
        szCustomSize[k++] = 0xD5;
        szCustomSize[k++] = (BYTE) (uiXsize & 0x000000FF);
        szCustomSize[k++] = (BYTE) ((uiXsize & 0x0000FF00) >> 8);
        szCustomSize[k++] = (BYTE) ((uiXsize & 0x00FF0000) >> 16);
        szCustomSize[k++] = (BYTE) ((uiXsize & 0xFF000000) >> 24);
        szCustomSize[k++] = (BYTE) (uiYsize & 0x000000FF);
        szCustomSize[k++] = (BYTE) ((uiYsize & 0x0000FF00) >> 8);
        szCustomSize[k++] = (BYTE) ((uiYsize & 0x00FF0000) >> 16);
        szCustomSize[k++] = (BYTE) ((uiYsize & 0xFF000000) >> 24);
        err = thePrinter->Send ((const BYTE *) szCustomSize, k);

        err = thePrinter->Send (szScratchStr, sizeof (szScratchStr));
		ERRCHECK;
	}	
	else
#endif
	{
		err = thePrinter->Send ((const BYTE*)JrPaperSizeSeq,sizeof(JrPaperSizeSeq));
		ERRCHECK;
	}

	// If it is Grayscale printjob, send the PJL command indicating the same
    COLORMODE       eC = COLOR;
    MEDIATYPE       eM;
    QUALITY_MODE    eQ;
    BOOL            bD;

    thePrintContext->GetPrintModeSettings (eQ, eM, eC, bD);

/*
 *  Send Printable Area. This is necessary in case source width is less than printable width.
 *
 * 
 *  width is imagewidth - multiple of 96
 *  height is physical page height - margins for 8x5 x 11 - 6600 - 200 = 6400
 */

    BYTE    szPrintableArea[] = {"\xD1\x00\x00\x00\x00\xF8\x74"};
    int     iWidth = (int) thePrintContext->InputPixelsPerRow ();
    int     iPageHeight;

/*
 *  There is a firmware bug in Mojave and Lakota that causes image replicatin across the
 *  page if width is less than the printable width in grayscale. So, set width to printable
 *  width for grayscale.
 */

    if (eC == GREY_K)
    {
        iWidth = (int) (thePrintContext->PrintableWidth () *
                        thePrintContext->EffectiveResolutionX ());
    }

/*
 *  The minimum printable width is 1600 pixels (3 inch * 600 - 200 for margins)
 */

    if (iWidth < 1600)
    {
        iWidth = 1600;
    }

/*
 *  Further, source width must be a multiple of 32.
 */


    iWidth = ((iWidth + 31) / 32) * 32;

    iPageHeight = (int) (thePrintContext->PrintableHeight () *
                         thePrintContext->EffectiveResolutionY ());
    iPageHeight = ((iPageHeight + (MOJAVE_STRIP_HEIGHT - 1)) / MOJAVE_STRIP_HEIGHT) * MOJAVE_STRIP_HEIGHT;

    szPrintableArea[1] = iWidth & 0xFF;
    szPrintableArea[2] = (iWidth >> 8) & 0xFF;
    szPrintableArea[3] = iPageHeight & 0xFF;
    szPrintableArea[4] = (iPageHeight >> 8) & 0xFF;
    err = thePrinter->Send (szPrintableArea, 7);
    ERRCHECK;

	//MapPCLMediaTypeToString (eM);  // Optional.  No need to send

	err = thePrinter->Send ((const BYTE *)JrBeginPageSeq, sizeof(JrBeginPageSeq));
	ERRCHECK;

	// The colorspace command has to be sent to intimate the printer to switch to color/monochrome 
	// depending upon the data that we are about to send.
	// This command must be sent after SetCursor and before beginimage command
	if (eC == GREY_K )
	{
		// indicates switch to monochrome mode 
		err = thePrinter->Send ((const BYTE *) "\xC0\x07\xF8\x03\x6A", 5);
		ERRCHECK;
	}
	else
	{
		// indicates switch Color mode
		err = thePrinter->Send ((const BYTE *) "\xC0\x06\xF8\x03\x6A", 5);
		ERRCHECK;
	}

	bLastBand = FALSE;

    return err;
}

DRIVER_ERROR HeaderLJJetReady::EndPage ()
{
    DRIVER_ERROR err;

	err = thePrinter->Send ((const BYTE*)JrVUExtn_3Seq,sizeof(JrVUExtn_3Seq));
	ERRCHECK;
	err = thePrinter->Send ((const BYTE*)JrVU_ver_TagSeq,sizeof(JrVU_ver_TagSeq));
	ERRCHECK;
	err = thePrinter->Send ((const BYTE*)JrVenUniqSeq,sizeof(JrVenUniqSeq));
	ERRCHECK;
	err = thePrinter->Send ((const BYTE*)JrEndPageSeq,sizeof(JrEndPageSeq));
	ERRCHECK;

    return err;
}


//-----------------------------------------------------------------------------
// Function:    CDJPcl::MapPCLMediaTypeToString()
//
// Description: Sends the command for mediatype.
// Input:       None
// Modifies:    None
//
// Precond:     None
//              
//                 
// Postcond:    The mediatype commands are sent
//
// Returns:     None
//
//-----------------------------------------------------------------------------
DRIVER_ERROR HeaderLJJetReady::MapPCLMediaTypeToString (MEDIATYPE eM)
{
    DRIVER_ERROR    err;
    BYTE            szPlain[] = {"\xC8\xC1\x05\x00Plain\xF8\x27"};
    BYTE            szPhoto[] = {"\xC8\xC1\x05\x00Gloss\xF8\x27"};

    switch (eM)
    {
        case (MEDIA_PLAIN):
            err = thePrinter->Send (szPlain, sizeof (szPlain));
            ERRCHECK;
            break;

        case (MEDIA_PHOTO):
            err = thePrinter->Send (szPhoto, sizeof (szPhoto));
            ERRCHECK;
            break;
        default:
            //** unsupported media type; return error code
            assert (0);
			err = SYSTEM_ERROR;
            break;// to be verified..
    }

    return err;	
}

DRIVER_ERROR HeaderLJJetReady::EndJob ()
{
    DRIVER_ERROR    err = NO_ERROR;

	err = thePrinter->Send((const BYTE*)JrEndSessionSeq, sizeof(JrEndSessionSeq));
	ERRCHECK;

	err = thePrinter->Send((const BYTE*)ccpPJLExitSeq, sizeof(ccpPJLExitSeq));
	ERRCHECK;

    return err;
}

DRIVER_ERROR HeaderLJJetReady::FormFeed ()
{
	DRIVER_ERROR    err = NO_ERROR;
    ModeJPEG        *pCompressor;
    LJJetReady      *thisPrinter = (LJJetReady *) thePrinter;
    int             iPageHeight;

    iPageHeight = (int) (thePrintContext->PrintableHeight () *
                         thePrintContext->EffectiveResolutionY ());
    iPageHeight = ((iPageHeight + MOJAVE_STRIP_HEIGHT - 1) / MOJAVE_STRIP_HEIGHT) * MOJAVE_STRIP_HEIGHT;

/*
 *  Send white rasters to fill up the rest of the page.
 *  This is required according to Brian Mayer, but introduces a long delay
 *  in printing. LJ3500 works fine without these white strips.
 */

    pCompressor = (ModeJPEG *) thisPrinter->GetCompressor ();
    err = pCompressor->SendWhiteStrips (iPageHeight, thisPrinter->bGrey_K);


	bLastBand = TRUE;

	err = EndPage ();

    thisPrinter->m_bStartPageNotSent = TRUE;
	return err;
}

DRIVER_ERROR HeaderLJJetReady::SendCAPy (unsigned int iAbsY)
{
    return NO_ERROR;
}

DRIVER_ERROR LJJetReady::Encapsulate (const RASTERDATA* InputRaster, BOOL bLastPlane)
{
    BYTE            res[64];
    BYTE            *pDataPtr = NULL;
    DRIVER_ERROR    err;

    //if (m_bStartPageNotSent)
    //{
    //    err = phLJJetReady->StartPage ();
    //    if (err != NO_ERROR)
    //        return err;
	//	m_bStartPageNotSent = FALSE;
    //}


    unsigned short int   wCoordinate; 

    unsigned long   ulVUDataLength = 0;

	// For color JPEG, you need to skip the header information of 623 bytes

    int iJpegHeaderSize = 623;
#ifdef HAVE_LIBDL
    if (HPLJJRCompress && m_eCompressMode == COMPRESS_MODE_LJ)
    {
        iJpegHeaderSize = 0;
    }
#endif

	if (bGrey_K)
    {
		ulVUDataLength = InputRaster->rastersize[COLORTYPE_COLOR];
        pDataPtr = (BYTE *) InputRaster->rasterdata[COLORTYPE_COLOR];
    }
	else
	{
		ulVUDataLength = InputRaster->rastersize[COLORTYPE_COLOR]    - iJpegHeaderSize;
        pDataPtr = (BYTE *) InputRaster->rasterdata[COLORTYPE_COLOR] + iJpegHeaderSize;
	}


    //VWritePrinter("\xC2\x01\x40\x70\x68\xF8\x91\xC1", 0x08);
	BYTE JrReadImageSeq[] = {0xC2, 0x01, 0x40, 0x70,0x68, 0xF8, 0x91, 0xC1};
	err = Send ((const BYTE *)JrReadImageSeq, sizeof(JrReadImageSeq));
	ERRCHECK;

	ModeJPEG     *pCompressor = NULL;
 	pCompressor = (ModeJPEG*)GetCompressor();

    wCoordinate = (unsigned short int)pCompressor->GetCoordinates() - 128;
//	err = Send ((const BYTE *)&wCoordinate, sizeof(unsigned short int));
    res[0] = wCoordinate & 0xFF;
    res[1] = ((wCoordinate & 0xFF00) >> 8);
    err = Send ((const BYTE *) res, 2);
	ERRCHECK;

	BYTE JrStripHeight[] = {0xF8,0x6D,0xC1,0x80,0x00,0xF8,0x63};
	err = Send ((const BYTE *)JrStripHeight, sizeof(JrStripHeight));
	ERRCHECK;

	BYTE JrTextObjectTypeSeq[] = {0xC0,0x00, 0xF8, 0x96};
	err = Send ((const BYTE *)JrTextObjectTypeSeq, sizeof(JrTextObjectTypeSeq));
	ERRCHECK;

	err = Send ((const BYTE *)JrVU_ver_TagSeq, sizeof(JrVU_ver_TagSeq));
	ERRCHECK;

	err = Send ((const BYTE *) "\xC2", 1);
	ERRCHECK;

    ulVUDataLength += 6;
    res[0] = (BYTE) (ulVUDataLength & 0xFF);
    res[1] = (BYTE) ((ulVUDataLength & 0x0000FF00) >> 8);
    res[2] = (BYTE) ((ulVUDataLength & 0x00FF0000) >> 16);
    res[3] = (BYTE) ((ulVUDataLength & 0xFF000000) >> 24);
    res[4] = 0xF8;
    res[5] = 0x92;
    res[6] = 0x46;
    res[7] = 0x21;
#ifdef HAVE_LIBDL
    if (HPLJJRCompress && m_eCompressMode == COMPRESS_MODE_LJ)
    {
        res[7] = 0x11;
    }
#endif
    res[8] = 0x90;

    ulVUDataLength -= 6;
    res[9] = (BYTE) (ulVUDataLength & 0xFF);
    res[10] = (BYTE) ((ulVUDataLength & 0x0000FF00) >> 8);
    res[11] = (BYTE) ((ulVUDataLength & 0x00FF0000) >> 16);
    res[12] = (BYTE) ((ulVUDataLength & 0xFF000000) >> 24);

	err = Send (res, 13);
	ERRCHECK;

	err = Send (pDataPtr, ulVUDataLength);
	ERRCHECK;

    return err;
}

Header* LJJetReady::SelectHeader (PrintContext *pc)
{

/*
 *  To take care of what seems to be a firmware bug in the printer's grayscale path,
 *  need printable width value later. So, save the PrintContext here.
 *  Raghu
 */

    COLORMODE       eC = COLOR;
    MEDIATYPE       eM;
    QUALITY_MODE    eQ;
    BOOL            bD;

    thePrintContext = pc;
    phLJJetReady = new HeaderLJJetReady (this, pc);
    pc->GetPrintModeSettings (eQ, eM, eC, bD);
    if (eQ == QUALITY_BEST)
    {
        m_eCompressMode = COMPRESS_MODE_LJ;
    }

#ifdef HAVE_LIBDL
    if (HPLJJRCompress && m_eCompressMode != COMPRESS_MODE_LJ)
    {
        dlclose (m_hHPLibHandle);
        m_hHPLibHandle = NULL;
        HPLJJRCompress = NULL;
    }
#endif
	return phLJJetReady;
}

DRIVER_ERROR LJJetReady::VerifyPenInfo()
{
    ePen = BOTH_PENS;
    return NO_ERROR;
}

DRIVER_ERROR LJJetReady::ParsePenInfo(PEN_TYPE& ePen, BOOL QueryPrinter)
{
    ePen = BOTH_PENS;

    return NO_ERROR;
}

Compressor* LJJetReady::CreateCompressor (unsigned int RasterSize)
{

/*
 *  There is some problem printing in grayscale. When the inputwidth is less than the full
 *  page width, printer replicates the image across the width. To fix this, make the width of
 *  each strip the width of the printable page size. Note that this does not create any scaling
 *  of the image.
 *  In addition, there is a minimum source width requirement, which is
 *      3 in. * resolution - 2 * margins = 3 * 600 - 2 * 100 = 1600
 *  So, minimum RasterSize must be 1600 * 3 here (width * 3 for RGB).
 *  Raghu
 */

    if (bGrey_K)
    {
        RasterSize = (int) (thePrintContext->PrintableWidth () * thePrintContext->EffectiveResolutionX ()) * 3;
    }

    if (RasterSize < 4800)
    {
        RasterSize = 4800;
    }

/*
 *  Further, source width must be a multiple of 32 (96 for RGB here).
 */

    RasterSize = ((RasterSize + 95) / 96) * 96;

    m_pCompressor = new ModeJPEG (pSS, this, RasterSize, m_eCompressMode);
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
DISPLAY_STATUS LJJetReady::ParseError(BYTE status_reg)
{
    DBG1("LJJetReady: parsing error info\n");

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

BYTE *    ModeJPEG::fpJPEGBuffer = NULL;       // image buffer
DWORD     ModeJPEG::fJPEGBufferPos;            // position of 1'st empty byte in image buffer

void ModeJPEG::jpeg_flush_output_buffer_callback(JOCTET *outbuf, BYTE* buffer, DWORD size)
{
    fJPEGBufferPos += size;
    memcpy (fpJPEGBuffer, buffer, size);
    fpJPEGBuffer += size;
}

//----------------------------------------------------------------
// These are "overrides" to the JPEG library error routines
//----------------------------------------------------------------
static void Gjpeg_error (j_common_ptr cinfo)
{
    // The standard behavior is to send a message to stderr.
}
/*
void ModeJPEG::SetBuffer(BYTE * pJPEGBuffer)
{
    fpJPEGBuffer = pJPEGBuffer;
}
*/
DWORD ModeJPEG::GetJPEGBufferSize() const
{
    return fJPEGBufferPos;
}

/*
BYTE* ModeJPEG::GetBuffer() 
{
    return fpJPEGBuffer;
}
*/

extern "C"
{
void jpeg_buffer_dest (j_compress_ptr cinfo, JOCTET* outbuff, void* flush_output_buffer_callback);
void hp_rgb_ycc_setup (int iFlag);
}

#define ConvertToGrayMacro(red, green, blue) ((unsigned char)( ( (red * 30) + (green * 59) + (blue * 11) ) / 100 ))

//--------------------------------------------------------------------
// Function:    ModeJPEG::ModeJPEG
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
ModeJPEG::ModeJPEG 
(    
	SystemServices* pSys,
    Printer* pPrinter,
    unsigned int PlaneSize,
    COMPRESS_MODE eCompressMode
) :
    Compressor(pSys, PlaneSize, TRUE),
    thePrinter(pPrinter)    // needed by Flush                        
{
    if (constructor_error != NO_ERROR)  // if error in base constructor
    {
        return;
    }
    fpJPEGBuffer = NULL;    // image buffer
    fJPEGBufferPos = 0;     // position of 1'st empty byte in image buffer
    fpJPEGStart = NULL;

	m_SourceBitmap.pvBits = (BYTE*) new BYTE[(inputsize * MOJAVE_STRIP_HEIGHT)];

	if (m_SourceBitmap.pvBits == NULL)
    {
		constructor_error = ALLOCMEM_ERROR;
    }
    else
    {
    	memset (m_SourceBitmap.pvBits, 0xFF, inputsize * MOJAVE_STRIP_HEIGHT);
    }
    m_SourceBitmap.cjBits = 0;
    m_SourceBitmap.bitmapInfo.bmiHeader.biWidth = 0;
    m_SourceBitmap.bitmapInfo.bmiHeader.biHeight = 0;


	m_DestBitmap.pvBits = NULL;
    m_DestBitmap.cjBits = 0;
    m_DestBitmap.bitmapInfo.bmiHeader.biWidth = 0;
    m_DestBitmap.bitmapInfo.bmiHeader.biHeight = 0;

	m_lCurrCDRasterRow	= 0;
	m_lPrinterRasterRow = 0;

    m_iRasterWidth = inputsize / 3;

	iRastersReady = 0;

	compressBuf = NULL;
    m_eCompressMode = eCompressMode;

} // ModeJPEG::ModeJPEG


//--------------------------------------------------------------------
// Function:    ModeJPEG::~ModeJPEG
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
ModeJPEG::~ModeJPEG ()
{   
	if (m_SourceBitmap.pvBits)
	{
		delete m_SourceBitmap.pvBits;
		m_SourceBitmap.pvBits = NULL;
	}

	if (m_DestBitmap.pvBits)
	{
		delete m_DestBitmap.pvBits;
		m_DestBitmap.pvBits = NULL;
	}

} // ModeJPEG::~ModeJPEG

/*
 *  JetReady specs require that data be sent for the whole page. When input data
 *  does not cover the whole page, send white rasters to cover the rest of the
 *  page.
 */

DRIVER_ERROR ModeJPEG::SendWhiteStrips (int iPageHeight, BOOL bGray)
{
    DRIVER_ERROR    err = NO_ERROR;
	HPLJBITMAP      hWhiteBitmap;
    QTABLEINFO      qTableInfo;
    RASTERDATA      whiteRaster;

    qTableInfo.qFactor = 6;

	hWhiteBitmap.pvBits = NULL;

	if (bGray)
	{
        m_SourceBitmap.cjBits = m_iRasterWidth * MOJAVE_STRIP_HEIGHT;
        memset (m_SourceBitmap.pvBits, 0x00, m_SourceBitmap.cjBits);
    }
    else
    {
        m_SourceBitmap.cjBits = m_iRasterWidth * MOJAVE_STRIP_HEIGHT * 3;
        memset (m_SourceBitmap.pvBits, 0xFF, m_SourceBitmap.cjBits);
    }
	memset(&m_SourceBitmap.bitmapInfo.bmiHeader, 0, sizeof(HPBITMAPINFOHEADER));
	m_SourceBitmap.bitmapInfo.bmiHeader.biWidth = m_iRasterWidth;
	m_SourceBitmap.bitmapInfo.bmiHeader.biHeight = MOJAVE_STRIP_HEIGHT;
	m_SourceBitmap.bitmapInfo.bmiHeader.biSizeImage = m_SourceBitmap.cjBits;
	m_SourceBitmap.bitmapInfo.bmiHeader.biBitCount = 8;
    Compress (&m_SourceBitmap, &m_DestBitmap, &qTableInfo, bGray);
    whiteRaster.rastersize[COLORTYPE_COLOR] = m_DestBitmap.cjBits;
    whiteRaster.rasterdata[COLORTYPE_COLOR] = m_DestBitmap.pvBits;
    while (m_lPrinterRasterRow < iPageHeight)
    {
        thePrinter->Encapsulate (&whiteRaster, TRUE);
        m_lPrinterRasterRow += MOJAVE_STRIP_HEIGHT;
    }

    return err;
}

BOOL  ModeJPEG::Compress( HPLJBITMAP *pSrcBitmap,
                          HPLJBITMAP *pTrgBitmap,
                          QTABLEINFO *pQTable,
						  BOOL		  bGrayscaleSet
                        ) 

{
#ifdef HAVE_LIBDL
    if (HPLJJRCompress && m_eCompressMode == COMPRESS_MODE_LJ)
    {
        int     iRet;
        memcpy (pTrgBitmap, pSrcBitmap, sizeof (HPLJBITMAP));
        pTrgBitmap->pvBits = new BYTE[pSrcBitmap->bitmapInfo.bmiHeader.biWidth * pSrcBitmap->bitmapInfo.bmiHeader.biHeight * 3];
        if (pTrgBitmap->pvBits == NULL)
        {
            return FALSE;
        }
        if (bGrayscaleSet)
        {
            BYTE    *p = pSrcBitmap->pvBits;
            for (int j = 0; j < pSrcBitmap->bitmapInfo.bmiHeader.biHeight; j++)
            {
                for (int i = 0; i < pSrcBitmap->bitmapInfo.bmiHeader.biWidth; i++)
                {
                    p[0] = ConvertToGrayMacro (p[0], p[1], p[2]);
                    p[1] = p[2] = 0;
                    p += 3;
                }
            }
        }
        iRet = HPLJJRCompress (pTrgBitmap->pvBits, (uint32_t *) &pTrgBitmap->cjBits, pSrcBitmap->pvBits,
                               pSrcBitmap->bitmapInfo.bmiHeader.biWidth, pSrcBitmap->bitmapInfo.bmiHeader.biHeight);
        if (iRet < 0)
            return FALSE;
        return TRUE;
    }
#endif // HAVE_LIBDL

#ifdef JPEG_FILE_OUTPUT
	static int ifilecount = 0;
    char szFileName [256];
    sprintf(szFileName,"C:\\Temp\\compressed%03d",ifilecount++);
    strcat(szFileName,".jpg");
    FILE *pFile = fopen(szFileName, "w+b");
#endif

	fJPEGBufferPos = 0;

    hp_rgb_ycc_setup (1);   // Use modified Mojave CSC table

    //----------------------------------------------------------------
    // Setup for compression 
    //----------------------------------------------------------------
    
    //----------------------------------------------------------------
    // JPEG Lib Step 1: Allocate and initialize JPEG compression object
    //----------------------------------------------------------------
    cinfo.err = jpeg_std_error( &jerr );

    //Fix the error handler to return when an error occurs,
    //the default exit()s which is nasty for a driver to do.
    jerr.error_exit = Gjpeg_error;
    //Set the return jump address. This must be done now since
    // jpeg_create_compress could cause an error.
    if (setjmp(setjmp_buffer)) {

        // If we get here, the JPEG code has signaled an error.
        //* We need to clean up the JPEG object, and return.
         
        jpeg_destroy_compress(&cinfo);
        return FALSE;
    }

    jpeg_create_compress( &cinfo );
   
    //----------------------------------------------------------------
    // JPEG Lib Step 2: Specify the destination for the compressed data
    // For our purposes we need to replace the "data destination" module
    // with one that uses the JPEG library's I/O suspension mode.
    // The buffer pointer and free space are updated each time GetNextRow
    // is called; so only the function pointers are set here.
    //----------------------------------------------------------------

    //----------------------------------------------------------------
    // JPEG Lib Step 3: Set parameters for compression, including image size & colorspace
    //----------------------------------------------------------------

    int iColorsUsed;

    if(bGrayscaleSet)
    {
        cinfo.in_color_space = JCS_GRAYSCALE;
        iColorsUsed = 1;
    }
    else
    {
        cinfo.in_color_space = JCS_RGB; // arbitrary guess
        iColorsUsed = 3;
    }
    jpeg_set_defaults( &cinfo );

    cinfo.image_width = pSrcBitmap->bitmapInfo.bmiHeader.biWidth; 
    cinfo.image_height = pSrcBitmap->bitmapInfo.bmiHeader.biHeight;
    cinfo.input_components = iColorsUsed;  //change if bit depths others than 24bpp are ever needed
    cinfo.data_precision = 8;

    //
    // Create a static quant table here. 
    //
    static unsigned int mojave_quant_table1[64] =  { 2,3,4,5,5,5,5,5,
                                                    3,6,5,8,5,8,5,8,
                                                    4,5,5,5,5,5,5,5,
                                                    5,8,5,8,5,8,5,8,
                                                    5,5,5,5,5,5,5,5,
                                                    5,8,5,8,5,8,5,8,
                                                    5,5,5,5,5,5,5,5,
                                                    5,8,5,8,5,8,5,8 };
    //
    // Use this variable for representing the scale_factor for now.
    //
    unsigned int iScaleFactor = pQTable->qFactor;
    
    //
    // JetReady specific Q-Tables will be added here. We do the following:
    //  1. Add three Q-Tables.
    //  2. Scale the Q-Table elemets with the given scale factor.
    //  3. Check to see if any of the element is in the table is greater than 255
    //     reset that elemet to 255.
    //  5. There is a specific scaling need to be done to the first 6 
    //     elements in the matrix. This required to achieve the better
    //     compression ratio.
    //  4. Check to see if any the of recently modified element is
    //     greater than 255, reset that with 255.
    //  Following for loop implements the above logic.
    //
    //  Please refer to sRGBLaserHostBasedSoftwareERS.doc v9.0 section 5.2.5.3.1.1
    //  for more details.
    //
    //  [NOTE] These loop needs to be further optimized.
    //
    for (int i = 0; i < 3; i++)
    {
        //
        // Adding Q-Table.
        //
        jpeg_add_quant_table(&cinfo, i, mojave_quant_table1,  0, FALSE );
        //
        // Scaling the Q-Table elements. 
        // Reset the element to 255, if it is greater than 255.
        //
        for(int j = 1; j < 64; j++)
        {
            cinfo.quant_tbl_ptrs[i]->quantval[j] = (UINT16)((mojave_quant_table1[j] * iScaleFactor) & 0xFF);  

        }   // for (int j = 1; j < 64; j++)
        //
        // Special scaling for first 6 elements in the table.
        // Reset the specially scaled elements 255, if it is greater than 255.
        //

        //
        // 1st component in the table. Unchanged, I need not change anything here.
        //
        cinfo.quant_tbl_ptrs[i]->quantval[0] = (UINT16)mojave_quant_table1[0];
        
        //
        // 2nd and 3rd components in the zig zag order
        //
		// The following dTemp is being used  to ceil the vales: e.g 28.5 to 29
        //
        double dTemp = mojave_quant_table1[1] * (1 + 0.25 * (iScaleFactor - 1)) + 0.5;
        cinfo.quant_tbl_ptrs[i]->quantval[1] = (UINT16)dTemp & 0xFF;
        
        dTemp = mojave_quant_table1[8] * (1 + 0.25 * (iScaleFactor - 1)) + 0.5;
        cinfo.quant_tbl_ptrs[i]->quantval[8] = (UINT16)dTemp & 0xFF;
       
        //
        // 4th, 5th and 6th components in the zig zag order
        //
        dTemp = mojave_quant_table1[16] * (1 + 0.50 * (iScaleFactor - 1)) + 0.5;
        cinfo.quant_tbl_ptrs[i]->quantval[16] = (UINT16)dTemp & 0xFF;
        
        dTemp = mojave_quant_table1[9] * (1 + 0.50 * (iScaleFactor - 1)) + 0.5;
        cinfo.quant_tbl_ptrs[i]->quantval[9]  = (UINT16)dTemp & 0xFF;
        
        dTemp = mojave_quant_table1[2] * (1 + 0.50 * (iScaleFactor - 1)) + 0.5;
        cinfo.quant_tbl_ptrs[i]->quantval[2]  = (UINT16)dTemp & 0xFF;
    }   // for (i = 0; i < 3; i++)

    //
    // Hard code to use sampling mode 4:4:4
    //
    cinfo.comp_info[0].h_samp_factor = 1;
    cinfo.comp_info[0].v_samp_factor = 1;

    // Specify data destination for compression 
    fpJPEGBuffer = new BYTE[cinfo.image_width * cinfo.image_height * iColorsUsed];  //3 bytes for 24 bpp
    if (NULL == fpJPEGBuffer)
    {
        //ERR(("JPEGCompress: Not enough memory\n"));
        return FALSE;
    }

    fpJPEGStart = fpJPEGBuffer;

    jpeg_buffer_dest(&cinfo, (JOCTET *) fpJPEGBuffer, (void*) (ModeJPEG::jpeg_flush_output_buffer_callback));
    if(bGrayscaleSet)
    {
        cinfo.write_JFIF_header  =
        cinfo.write_Adobe_marker = FALSE;
        jpeg_suppress_tables(&cinfo, TRUE);
    }

    //----------------------------------------------------------------
    // JPEG Lib Step 4: Start the compression cycle
    //  set destination to table file
    //    jpeg_write_tables(cinfo);
    //    set destination to image file
    //  jpeg_start_compress(cinfo, FALSE);
    //----------------------------------------------------------------
    
    jpeg_start_compress( &cinfo, TRUE );    
   

    //----------------------------------------------------------------
    // This completes the JPEG setup.
    //----------------------------------------------------------------
    

    // do the jpeg compression
    JSAMPROW    pRowArray[1];
    BYTE* pCurLine = (BYTE*) pSrcBitmap->pvBits;
    DWORD dwScanLine;
    
    dwScanLine = cinfo.image_width * iColorsUsed;

	for(unsigned int nrow = 0; nrow < cinfo.image_height; nrow++)
    {
        pRowArray[0] = (JSAMPROW) pCurLine;
        jpeg_write_scanlines(&cinfo, pRowArray, 1);
        pCurLine += dwScanLine;
    }

    //----------------------------------------------------------------
    // JPEG Lib Step 6: Finish compression
    //----------------------------------------------------------------
    // Tell the compressor about the extra buffer space for the trailer
    jpeg_finish_compress( &cinfo );

    //
    // Read the quantization tables used for the compression.
    //

    if (cinfo.quant_tbl_ptrs[0] != NULL)
    {
        for (int iI = 0; iI < QTABLE_SIZE; iI++)
        {
            pQTable->qtable0[iI] = (DWORD)cinfo.quant_tbl_ptrs[0]->quantval[iI];
        }

    }

    if (cinfo.quant_tbl_ptrs[1] != NULL)
    {
        for (int iI = 0; iI < QTABLE_SIZE; iI++)
        {
            pQTable->qtable1[iI] = (DWORD)cinfo.quant_tbl_ptrs[1]->quantval[iI];
        }
    }

    if (cinfo.quant_tbl_ptrs[2] != NULL)
    {
        for (int iI = 0; iI < QTABLE_SIZE; iI++)
        {
            pQTable->qtable2[iI] = (DWORD)cinfo.quant_tbl_ptrs[2]->quantval[iI];
        }
    }


    //----------------------------------------------------------------
    // JPEG Lib Step 7: Destroy the compression object
    //----------------------------------------------------------------
    jpeg_destroy_compress( &cinfo );
                  
    memcpy(pTrgBitmap, pSrcBitmap, sizeof(HPLJBITMAP));

#ifdef JPEG_FILE_OUTPUT
        fwrite(fpJPEGStart, sizeof(BYTE), GetJPEGBufferSize(), pFile);
#endif
    //
    if(fpJPEGStart && bGrayscaleSet)
    {
        long lBufferSize = GetJPEGBufferSize();
        long l = 0;
        while (l < lBufferSize)
        {
            if(fpJPEGStart[l] == 0xFF && fpJPEGStart[l+1] == 0xDA)
                break;
            l++;
        }
        if(l != lBufferSize)
        {
            memcpy (fpJPEGStart, fpJPEGStart + l + 10, lBufferSize - l - 10);
            memset (fpJPEGStart + lBufferSize - l - 10, 0xFF, l + 10);
            pTrgBitmap->cjBits = lBufferSize - l - 10;
            pTrgBitmap->pvBits = (BYTE*)fpJPEGStart;
        }
        else
        {
            pTrgBitmap->pvBits = (BYTE*)fpJPEGStart;
        }
    }
    else
    {
        pTrgBitmap->pvBits = (BYTE*) fpJPEGStart;
        pTrgBitmap->cjBits = GetJPEGBufferSize();
    }

#ifdef JPEG_FILE_OUTPUT
	fclose(pFile);
#endif

    return TRUE;

}

BOOL ModeJPEG::Process
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
		if (m_lCurrCDRasterRow < MOJAVE_STRIP_HEIGHT )
		{
			//Copy the data to m_SourceBitmap
			memcpy(m_SourceBitmap.pvBits + m_lCurrCDRasterRow * inputsize, input->rasterdata[COLORTYPE_COLOR], input->rastersize[COLORTYPE_COLOR]);
			m_lCurrCDRasterRow ++;
		}

		if (m_lCurrCDRasterRow == MOJAVE_STRIP_HEIGHT || ((LJJetReady*)thePrinter)->phLJJetReady->IsLastBand())
		{
			m_SourceBitmap.cjBits = inputsize * MOJAVE_STRIP_HEIGHT;
			memset(&m_SourceBitmap.bitmapInfo.bmiHeader, 0, sizeof(HPBITMAPINFOHEADER));
			m_SourceBitmap.bitmapInfo.bmiHeader.biWidth = inputsize / 3;
			m_SourceBitmap.bitmapInfo.bmiHeader.biHeight = MOJAVE_STRIP_HEIGHT;

			QTABLEINFO  qTableInfo;
			BOOL	bGrayScaleSet = ((LJJetReady *)thePrinter)->bGrey_K;
			BOOL	bRet		  = TRUE;

			qTableInfo.qFactor = 6;
			if (((LJJetReady*)thePrinter)->m_bStartPageNotSent)
			{
				err = (((LJJetReady*)thePrinter)->phLJJetReady)->StartPage();
				((LJJetReady*)thePrinter)->m_bStartPageNotSent = FALSE;

				char res[64];
				//
				//  0xC2       - unsigned 32 bit int   
				//  0x68704000 - begin image for the JetReady path - endin correct to 00 40 70 68
				//  0xF8       - unsigned 8 bit attribute
				//  0x91       - VUextension  (a vendor unique attribute for JetReady) 
				//  0xC1       - Unsigned 16 bit int
				err = thePrinter->Send ((const BYTE *)JrBeginImageSeq, sizeof(JrBeginImageSeq));
				ERRCHECK;

				unsigned short int sourcewidth = (unsigned short int) (inputsize / 3);
				unsigned short int sourceheight = (unsigned short int)((((LJJetReady*)thePrinter)->phLJJetReady)->thePrintContext->PhysicalPageSizeY() * MOJAVE_RESOLUTION - 200);
				if ( 0 != (sourceheight % MOJAVE_STRIP_HEIGHT) )
				{
					sourceheight = ( (sourceheight / MOJAVE_STRIP_HEIGHT) + 1) * MOJAVE_STRIP_HEIGHT;
				}	

				//Write the source width to the printer;
//				err = thePrinter->Send ((const BYTE*)&sourcewidth, sizeof(unsigned short int));
                res[0] = sourcewidth & 0x00FF;
                res[1] = ((sourcewidth & 0xFF00) >> 8);
                err = thePrinter->Send ((const BYTE *) res, 2);
				ERRCHECK;
				//  0xF8       - unsigned 8 bit attribute
				//  0x6C       - source width attribute
				//  0xC1       - Unsigned 16 bit int
				strcpy (res, "\xF8\x6C\xC1");
				err = thePrinter->Send ((const BYTE *) res, strlen (res));
				ERRCHECK;
				
				//Write the source height to the printer
//				err = thePrinter->Send ((const BYTE*)&sourceheight, sizeof(unsigned short int));
                res[0] = sourceheight & 0x00FF;
                res[1] = ((sourceheight & 0xFF00) >> 8);
                err = thePrinter->Send ((const BYTE *) res, 2);
				ERRCHECK;
				// 0xF8       - unsigned 8 bit attribute
				// 0x6B       - source height attribute
				// 0xC1       - Unsigned 16 bit int
				strcpy (res, "\xF8\x6B\xC1");
				err = thePrinter->Send ((const BYTE *) res, strlen (res));
				ERRCHECK;
				
				unsigned short int stripcount = sourceheight / MOJAVE_STRIP_HEIGHT;
				//stripcount = 4;
//				err = thePrinter->Send ((const BYTE *)&stripcount, sizeof(stripcount));
                res[0] = stripcount & 0x00FF;
                res[1] = ((stripcount & 0xFF00) >> 8);
                err = thePrinter->Send ((const BYTE *) res, 2);
				ERRCHECK;
				//0xF8       - unsigned 8 bit attribute
				//0x93       - special attribute 1 for strip count  
				//0xC1       - Unsigned 16 bit int
				strcpy (res, "\xF8\x93\xC1");
				err = thePrinter->Send ((const BYTE *) res, strlen (res));
				//BYTE JrStripCountSeq[] = {0x32,0x00,0xF8,0x93,0xC1};
				//err = thePrinter->Send ((const BYTE *)JrStripCountSeq, sizeof(JrStripCountSeq));
				ERRCHECK;

				// Write the MOJAVE_STRIP_HEIGHT value
				//0xF8       - unsigned 8 bit attribute
				//0x94       - special attribute 2 used for strip height
				BYTE JrStripHeightSeq[] = {0x80,0x00,0xF8,0x94};
				err = thePrinter->Send ((const BYTE *)JrStripHeightSeq, sizeof(JrStripHeightSeq));
				ERRCHECK;

				if  (bGrayScaleSet)
				{
					// 0x00 indicates grayscale printing
					BYTE JrGrayScaleSeq[] = {0xC0,0x00,0xF8,0x97};
					err = thePrinter->Send ((const BYTE *)JrGrayScaleSeq, sizeof(JrGrayScaleSeq));
					ERRCHECK;
				}
				else
				{
					// 0x04 indicates Color printing
					BYTE JrColorSeq[] = {0xC0,0x04,0xF8,0x97};
					err = thePrinter->Send ((const BYTE *)JrColorSeq, sizeof(JrColorSeq));
					ERRCHECK;
				}

				// Interleaved Color Enumeration for Mojave
				BYTE JrSeq[] = {0xC0,0x00,0xF8,0x98};
				err = thePrinter->Send ((const BYTE *)JrSeq, sizeof(JrSeq));
				ERRCHECK;

				//0xC2       - unsigned 32 bit int   
				//0x00030000 - JetReady version number 
				//0xF8       - unsigned 8 bit attribute
				//0x95       - JetReadyVersion attribute
				err = thePrinter->Send ((const BYTE *)JrVU_ver_TagSeq, sizeof(JrVU_ver_TagSeq));
				ERRCHECK;

				//  Send the JPEG statement for the sake of Dual Compression
				//  0xC2       - unsigned 32 bit int   
				//  VU DataLength - 824 bytes for jpeg header
				//  0xF8       - unsigned 8 bit attribute
				//  0x92       - VU data length
				//  0x46       - vendor unique  
				BYTE JrDataLengthSeq[] = {0xC2,0x38,0x03,0x00,0x00,0xF8,0x92,0x46};
#ifdef HAVE_LIBDL
                if (HPLJJRCompress && m_eCompressMode == COMPRESS_MODE_LJ)
                {
                    JrDataLengthSeq[1] = 0;
                    JrDataLengthSeq[2] = 0;
                }
#endif
				err = thePrinter->Send ((const BYTE *)JrDataLengthSeq, sizeof(JrDataLengthSeq));
				ERRCHECK;

				bRet = Compress (&m_SourceBitmap, 
								 &m_DestBitmap,
								 &qTableInfo,
								 FALSE	// We are only worried about the qTables not about the colorspace.
							    );
                if (!HPLJJRCompress || (HPLJJRCompress && m_eCompressMode != COMPRESS_MODE_LJ))
                {

				    //VWritePrinter("\x00\x80\x00\x03\x00\x00", 0x6);
				    BYTE JrQTSeq[] = {0x00,0x80,0x00,0x03,0x00,0x00};
				    err = thePrinter->Send ((const BYTE *)JrQTSeq, sizeof(JrQTSeq));
				    ERRCHECK;

#ifdef APDK_LITTLE_ENDIAN
				    //VWritePrinter((VOID*) pQTableInfo->qtable0, sizeof(DWORD) * QTABLE_SIZE);
				    err = thePrinter->Send ((const BYTE *)qTableInfo.qtable0, sizeof(DWORD) * QTABLE_SIZE);
				    ERRCHECK;


				    //VWritePrinter((VOID*) pQTableInfo->qtable1, sizeof(DWORD) * QTABLE_SIZE);
				    err = thePrinter->Send ((const BYTE *)qTableInfo.qtable1, sizeof(DWORD) * QTABLE_SIZE);
				    ERRCHECK;

				    //VWritePrinter((VOID*) pQTableInfo->qtable2, sizeof(DWORD) * QTABLE_SIZE);
				    err = thePrinter->Send ((const BYTE *)qTableInfo.qtable2, sizeof(DWORD) * QTABLE_SIZE);
				    ERRCHECK;
#else
                    BYTE    szStr[sizeof (DWORD) * QTABLE_SIZE * 3];
                    BYTE    *p;
                    p = szStr;
                    for (int i = 0; i < QTABLE_SIZE; i++)
                    {
                        *p++ = qTableInfo.qtable0[i] & 0xFF;
                        *p++ = (qTableInfo.qtable0[i] >> 8) & 0xFF;
                        *p++ = (qTableInfo.qtable0[i] >> 16) & 0xFF;
                        *p++ = (qTableInfo.qtable0[i] >> 24) & 0xFF;
                    }
                    for (int i = 0; i < QTABLE_SIZE; i++)
                    {
                        *p++ = qTableInfo.qtable1[i] & 0xFF;
                        *p++ = (qTableInfo.qtable1[i] >> 8) & 0xFF;
                        *p++ = (qTableInfo.qtable1[i] >> 16) & 0xFF;
                        *p++ = (qTableInfo.qtable1[i] >> 24) & 0xFF;
                    }
                    for (int i = 0; i < QTABLE_SIZE; i++)
                    {
                        *p++ = qTableInfo.qtable2[i] & 0xFF;
                        *p++ = (qTableInfo.qtable2[i] >> 8) & 0xFF;
                        *p++ = (qTableInfo.qtable2[i] >> 16) & 0xFF;
                        *p++ = (qTableInfo.qtable2[i] >> 24) & 0xFF;
                    }
                    err = thePrinter->Send ((const BYTE *) szStr, 3 * sizeof (DWORD) * QTABLE_SIZE);
                    ERRCHECK;

#endif
				    // Start of JPEG Control
				    // 0x8001 JPEG Control register
				    // size - unsigned 32 bit number 0x0000_002C
				    // 11 32 bit words total = 44 bytes
				    // Control  one
				    // Color 0x6614_E001
				    // Mono  0x0000_E005
				    //
				    //VWritePrinter("\x01\x80\x2C\x00\x00\x00", 0x6);
				    BYTE JrCRSeq[] = {0x01,0x80,0x2C,0x00,0x00,0x00};
				    err = thePrinter->Send ((const BYTE *)JrCRSeq, sizeof(JrCRSeq));
				    ERRCHECK;

				    if (bGrayScaleSet)
				    {
					    //VWritePrinter("\x05\xE0\x00\x00", 0x4);
					    BYTE JrCR1GSeq[] = {0x05,0xE0,0x00,0x00};
					    err = thePrinter->Send ((const BYTE *)JrCR1GSeq, sizeof(JrCR1GSeq));
					    ERRCHECK;
				    }
				    else
				    {
					    //VWritePrinter("\x01\xE0\x14\x66", 0x4);
					    BYTE JrCR1CSeq[] = {0x01,0xE0,0x14,0x66};
					    err = thePrinter->Send ((const BYTE *)JrCR1CSeq, sizeof(JrCR1CSeq));
					    ERRCHECK;
				    }
        
				    //
				    // Control three
				    // Color 0x0000_0001
				    // Mono  0x0000_0000
				    // bit 0 = convert rgb data
				    // bit 1: 0 = 13 bit precision 
				    //        1 = 14 bit precision
				    //
				    if (bGrayScaleSet)
				    {
					    //VWritePrinter("\x00\x00\x00\x00", 0x4);
					    BYTE JrCR3GSeq[] = {0x00,0x00,0x00,0x00};

					    err = thePrinter->Send ((const BYTE *)JrCR3GSeq, sizeof(JrCR3GSeq));
					    ERRCHECK;
				    }
				    else
				    {
					    //VWritePrinter("\x01\x00\x00\x00", 0x4);
					    BYTE JrCR3CSeq[] = {0x01,0x00,0x00,0x00};
					    err = thePrinter->Send ((const BYTE *)JrCR3CSeq, sizeof(JrCR3CSeq));
					    ERRCHECK;
				    }
        
				    //
				    // CSC matrix
				    // 11  12  13  2.0   0.0    0.0
				    // 21  22  23  2.0  -2.0    0.0
				    // 31  32  33  2.0   0.0   -2.0

				    // Decompression matrix
				    //VWritePrinter("\x00\x20\x00\x00", 0x4);
				    BYTE JrCSC1Seq[] = {0x00,0x20,0x00,0x00};
				    err = thePrinter->Send ((const BYTE *)JrCSC1Seq, sizeof(JrCSC1Seq));
				    ERRCHECK;

				    //VWritePrinter("\x00\x00\x00\x00", 0x4);
				    BYTE JrCSC2Seq[] = {0x00,0x00,0x00,0x00};
				    err = thePrinter->Send ((const BYTE *)JrCSC2Seq, sizeof(JrCSC2Seq));
				    ERRCHECK;

				    //VWritePrinter("\x00\x00\x00\x00", 0x4);
				    BYTE JrCSC3Seq[] = {0x00,0x00,0x00,0x00};
				    err = thePrinter->Send ((const BYTE *)JrCSC3Seq, sizeof(JrCSC3Seq));
				    ERRCHECK;

				    //VWritePrinter("\x00\x20\x00\x00", 0x4);
				    BYTE JrCSC4Seq[] = {0x00,0x20,0x00,0x00};
				    err = thePrinter->Send ((const BYTE *)JrCSC4Seq, sizeof(JrCSC4Seq));
				    ERRCHECK;

				    //VWritePrinter("\x00\xE0\x00\x00", 0x4);
				    BYTE JrCSC5Seq[] = {0x00,0xE0,0x00,0x00};
				    err = thePrinter->Send ((const BYTE *)JrCSC5Seq, sizeof(JrCSC5Seq));
				    ERRCHECK;

				    //VWritePrinter("\x00\x00\x00\x00", 0x4);
				    BYTE JrCSC6Seq[] = {0x00,0x00,0x00,0x00};
				    err = thePrinter->Send ((const BYTE *)JrCSC6Seq, sizeof(JrCSC6Seq));
				    ERRCHECK;

				    //VWritePrinter("\x00\x20\x00\x00", 0x4);
				    BYTE JrCSC7Seq[] = {0x00,0x20,0x00,0x00};
				    err = thePrinter->Send ((const BYTE *)JrCSC7Seq, sizeof(JrCSC7Seq));
				    ERRCHECK;

				    //VWritePrinter("\x00\x00\x00\x00", 0x4);
				    BYTE JrCSC8Seq[] = {0x00,0x00,0x00,0x00};
				    err = thePrinter->Send ((const BYTE *)JrCSC8Seq, sizeof(JrCSC8Seq));
				    ERRCHECK;

				    //VWritePrinter("\x00\xE0\x00\x00", 0x4);
				    BYTE JrCSC9Seq[] = {0x00,0xE0,0x00,0x00};
				    err = thePrinter->Send ((const BYTE *)JrCSC9Seq, sizeof(JrCSC9Seq));
				    ERRCHECK;
                } // if (!HPLJJRCompress)
            }     // if (....bStartPageNotSent)


			BYTE         *pbTemp;
			HPLJBITMAP   jpegGrayBitmap;

			jpegGrayBitmap.pvBits = NULL;
			//
			// Convert 24bpp Gray to 8bpp Gray.
			// JPEG takes K 8bpp gray data. We are using two different buffers for these.
			//
			if(bGrayScaleSet && m_eCompressMode == COMPRESS_MODE_JPEG)
			{
				pbTemp = (BYTE*)m_SourceBitmap.pvBits;
        
				jpegGrayBitmap.pvBits = new BYTE[m_SourceBitmap.cjBits / 3];

				for(long i = 0, j=0; i < (long)m_SourceBitmap.cjBits; i += 3, j++)
				{
					pbTemp[i] = ConvertToGrayMacro(pbTemp[i], pbTemp[i + 1], pbTemp[i + 2] );
					pbTemp[i]	  = 255 - pbTemp[i];
					pbTemp[i + 1] = pbTemp[i];
					pbTemp[i + 2] = pbTemp[i];

					jpegGrayBitmap.pvBits[j] = pbTemp[i];
				}
			}

			if (m_DestBitmap.pvBits)
            {
				delete m_DestBitmap.pvBits;
				m_DestBitmap.pvBits = NULL;
			}
			//
			// JPEG grayscale specific operations are done here.
			//
			if(bGrayScaleSet && m_eCompressMode == COMPRESS_MODE_JPEG)
			{
				jpegGrayBitmap.cjBits = m_SourceBitmap.cjBits / 3;
				jpegGrayBitmap.bitmapInfo.bmiHeader.biSizeImage = m_SourceBitmap.bitmapInfo.bmiHeader.biSizeImage / 3;
				jpegGrayBitmap.bitmapInfo.bmiHeader.biBitCount = 8;
				jpegGrayBitmap.bitmapInfo.bmiHeader.biWidth = m_SourceBitmap.bitmapInfo.bmiHeader.biWidth;
				jpegGrayBitmap.bitmapInfo.bmiHeader.biHeight = m_SourceBitmap.bitmapInfo.bmiHeader.biHeight;
			}
			if (bGrayScaleSet && m_eCompressMode == COMPRESS_MODE_JPEG)
			{
				bRet = Compress (&jpegGrayBitmap, &m_DestBitmap, &qTableInfo,bGrayScaleSet);
			}
			else
			{
				bRet = Compress (&m_SourceBitmap, &m_DestBitmap, &qTableInfo,bGrayScaleSet);
			}

			m_SourceBitmap.cjBits = 0;
			m_SourceBitmap.bitmapInfo.bmiHeader.biWidth = 0;
			m_SourceBitmap.bitmapInfo.bmiHeader.biHeight = 0;

			memset(m_SourceBitmap.pvBits, 0xFF, inputsize * MOJAVE_STRIP_HEIGHT);

			if (jpegGrayBitmap.pvBits)
			{
				delete jpegGrayBitmap.pvBits;
				jpegGrayBitmap.pvBits = NULL;
			}
			m_lPrinterRasterRow += MOJAVE_STRIP_HEIGHT;
	
			m_lCurrCDRasterRow = 0;
			iRastersReady = 1;

			((LJJetReady*)thePrinter)->phLJJetReady->SetLastBand(FALSE);
		}
		else
		{
			return FALSE;
		}
	}

	return TRUE;
} //Process

BYTE* ModeJPEG::NextOutputRaster(COLORTYPE color)
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
		return m_DestBitmap.pvBits;
	}
}

unsigned int ModeJPEG::GetOutputWidth(COLORTYPE  color)
{
	if (color == COLORTYPE_COLOR)
	{
		return m_DestBitmap.cjBits;
	}
	else
	{
		return 0;
	}
} //GetOutputWidth

APDK_END_NAMESPACE

#endif  // defined APDK_LJJetReady
