/*****************************************************************************\
  dj9xx.cpp : Implimentation for the DJ9xx class

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


#if APDK_DJ9xx

#include "header.h"
#include "io_defs.h"
#include "dj8xx.h"
#include "dj9xx.h"
#include "resources.h"
#include "printerproxy.h"

APDK_BEGIN_NAMESPACE
extern BYTE* GetHT3x3_4();
extern BYTE* GetHT6x6_4_970();
APDK_END_NAMESPACE

APDK_BEGIN_NAMESPACE

extern uint32_t ulMapDJ970_KCMY[ 9 * 9 * 9 ];
extern uint32_t ulMapDJ970_KCMY_3x3x2[ 9 * 9 * 9 ];
extern uint32_t ulMapDJ970_Gossimer_Normal_KCMY[ 9 * 9 * 9 ];
extern uint32_t ulMapDJ600_CCM_K[ 9 * 9 * 9 ];
extern uint32_t ulMapGRAY_K_6x6x1[9 * 9 * 9];
extern uint32_t ulMapDJ970_Draft_KCMY[9 * 9 * 9];

extern void AsciiHexToBinary(BYTE* dest, char* src, int count);

DJ9xx::DJ9xx(SystemServices* pSS, BOOL proto)
: Printer(pSS,NUM_DJ6XX_FONTS,proto)
{
    if (IOMode.bDevID)
    {
        bCheckForCancelButton = TRUE;
        constructor_error = VerifyPenInfo();
        CERRCHECK;
    }
    else ePen=BOTH_PENS;    // matches default mode


    pMode[DEFAULTMODE_INDEX] = new DJ970Mode1();   // Normal Color
    pMode[SPECIALMODE_INDEX] = new DJ970Mode2();   // Photo

#ifdef APDK_AUTODUPLEX
/*
 *  When bidi is available, query printer for duplexer
 *  For now, this is available only on Linux which is unidi only.
 */

    bDuplexCapable = TRUE;
#endif

#ifdef APDK_EXTENDED_MEDIASIZE
    pMode[GRAYMODE_INDEX]      = new DJ970Mode3 ();   // Draft Grayscale K
    pMode[SPECIALMODE_INDEX+1] = new DJ970Mode4 ();   // Normal Grayscale K
    pMode[SPECIALMODE_INDEX+2] = new DJ970Mode5 ();   // Draft Color
    pMode[SPECIALMODE_INDEX+3] = new DJ970ModePres();       // Best Color
    pMode[SPECIALMODE_INDEX+4] = new DJ970ModePhotoPres();  // HiRes
    ModeCount=7;
#else
    pMode[GRAYMODE_INDEX]    = new GrayMode (ulMapDJ600_CCM_K);
    ModeCount=3;
#endif

    CMYMap = ulMapDJ970_KCMY;
}

DJ9xx::~DJ9xx()
{ }

DJ970Mode1::DJ970Mode1()   // Normal Color
: PrintMode(ulMapDJ970_KCMY_3x3x2)
// 600x600x1 K
// 300x300x2 CMY
{

    ColorDepth[K]=1;  // 600x600x1 K

    for (int i=1; i < 4; i++)
        ColorDepth[i]=2;    // 300x300x2 CMY

    ResolutionX[K]=ResolutionY[K]=600;

    MixedRes = TRUE;
#ifdef APDK_AUTODUPLEX
    bDuplexCapable = TRUE;
#endif

    ColorFEDTable = (BYTE*) HT300x3004level970_open;
}

DJ970Mode2::DJ970Mode2()    // Photo
: PrintMode(ulMapDJ970_Gossimer_Normal_KCMY)
// 600x600x1 K
// 600x600x2 CMY
{
    int i;
    ColorDepth[K]=1;  // 600x600x1 K

    for (i=1; i < 4; i++)
        ColorDepth[i]=2;    // 300x300x2 CMY

    for (i=0; i < 4; i++)
        ResolutionX[i]=ResolutionY[i]=600;

    BaseResX = BaseResY = 600;
    MixedRes = FALSE;

    medium = mediaGlossy;

    ColorFEDTable = GetHT6x6_4_970();

//    strcpy(ModeName, "Photo");
    bFontCapable=FALSE;
#ifdef APDK_AUTODUPLEX
    bDuplexCapable = FALSE;
#endif

    pmQuality   = QUALITY_BEST;
    pmMediaType = MEDIA_PHOTO;
}

#ifdef APDK_EXTENDED_MEDIASIZE
DJ970Mode3::DJ970Mode3 () : GrayMode (ulMapDJ600_CCM_K)   // Draft Grayscale K
{
#ifdef APDK_AUTODUPLEX
    bDuplexCapable = TRUE;
#endif
    pmQuality = QUALITY_DRAFT;
    theQuality = qualityDraft;
}

DJ970Mode4::DJ970Mode4 () : PrintMode (ulMapGRAY_K_6x6x1)    // Normal Grayscale K
{
    bFontCapable = FALSE;
#ifdef APDK_AUTODUPLEX
    bDuplexCapable = TRUE;
#endif
    ResolutionX[0] =
    ResolutionY[0] = 600;
    BaseResX = 600;
    BaseResY = 600;
    CompatiblePens[1] = BLACK_PEN;
    pmQuality = QUALITY_NORMAL;
    theQuality = qualityNormal;
    dyeCount = 1;
    pmColor = GREY_K;
}

DJ970Mode5::DJ970Mode5()    // Draft Color
: PrintMode(ulMapDJ970_Draft_KCMY)
// 300x300x1 K
// 300x300x1 CMY
{
#ifdef APDK_AUTODUPLEX
    bDuplexCapable = TRUE;
#endif
    pmQuality = QUALITY_DRAFT;
    theQuality = qualityDraft;
}

// 2001.06.14 mrb: Added Presentation Mode: 600x600x2 for color,
//                                          600x600x1 for b/w
DJ970ModePres::DJ970ModePres() : PrintMode(ulMapDJ970_KCMY)
// 600x600x1 K
// 600x600x2 CMY
{
    ColorDepth[K]=1;
    
    ColorDepth[C]=2;
    ColorDepth[M]=2;
    ColorDepth[Y]=2;

    ResolutionX[K]=ResolutionY[K]=600;
    ResolutionX[C]=ResolutionY[C]=600;
    ResolutionX[M]=ResolutionY[M]=600;
    ResolutionX[Y]=ResolutionY[Y]=600;

    BaseResX = BaseResY = 600;
    theQuality = qualityPresentation;

    // 2001.07.09 mrb: Added for presentation mode of DJ970
    ColorFEDTable = (BYTE*) HT600x600x4_Pres970_open;

#ifdef APDK_AUTODUPLEX
    bDuplexCapable = TRUE;
#endif
    pmQuality   = QUALITY_BEST;
}

// 2001.06.14 mrb: Added Presentation Photo Mode: 1200x1200x1 for color.
DJ970ModePhotoPres::DJ970ModePhotoPres() 
: PrintMode(ulMapDJ970_Gossimer_Normal_KCMY) 
// 1200x1200x1 CMY
{
    ColorDepth[K]=1;
    ColorDepth[C]=1;
    ColorDepth[M]=1;
    ColorDepth[Y]=1;

    ResolutionX[K]=ResolutionY[K]=1200;
    ResolutionX[C]=ResolutionY[C]=1200;
    ResolutionX[M]=ResolutionY[M]=1200;
    ResolutionX[Y]=ResolutionY[Y]=1200;

    BaseResX = BaseResY = 1200;

    medium = mediaGlossy;
    theQuality = qualityPresentation;

    // 2001.07.09 mrb: Added for presentation mode of DJ970
    ColorFEDTable = (BYTE*) HT1200x1200x1_PhotoPres970_open;

    bFontCapable=FALSE;
    pmQuality   = QUALITY_HIGHRES_PHOTO;
    pmMediaType = MEDIA_PHOTO;
}
#endif  // APDK_EXTENDED_MEDIASIZE

BOOL DJ9xx::UseGUIMode(PrintMode* pPrintMode)
{

    if ((!pPrintMode->bFontCapable)
#ifdef APDK_AUTODUPLEX
        || pPrintMode->QueryDuplexMode ()
#endif
        )
        return TRUE;
    return FALSE;
}

Compressor* DJ9xx::CreateCompressor(unsigned int RasterSize)
{
    return new Mode2(pSS,RasterSize);
}

Header900::Header900(Printer* p,PrintContext* pc)
    : Header895(p,pc)
{ }

Header* DJ9xx::SelectHeader(PrintContext* pc)
{
    return new Header900(this,pc);
}

DRIVER_ERROR Header900::Send()
{
    DRIVER_ERROR err;
    //BOOL bDuplex = FALSE;

    StartSend();

    // this code will look for the duplexer enabled in the device ID and send the right
    // escape to the printer to enable duplexing.  At this time, however, we are not
    // going to support duplexing.  One, it is not supported with PCL3, which we need
    // for device font support.  Second, we don't have the resources to reformat the page
    // for book duplexing and can only do tablet.

    /*BYTE bDevIDBuff[DevIDBuffSize];
    err = theTranslator->pSS->GetDeviceID(bDevIDBuff, DevIDBuffSize, TRUE);
    ERRCHECK;

    // look for duplex code in bDevIDBuff
    Duplex = DuplexEnabled(bDevIDBuff);

    if(bDuplex)
    {
        err = thePrinter->Send((const BYTE*)EnableDuplex,sizeof(EnableDuplex));
        ERRCHECK;
    }*/

#ifdef APDK_AUTODUPLEX
    if (thePrintContext->QueryDuplexMode () != DUPLEXMODE_NONE)
        err = thePrinter->Send ((const BYTE *) EnableDuplex, sizeof (EnableDuplex));
#endif

    err = ConfigureRasterData();
    ERRCHECK;

    err=Graphics();     // start raster graphics and set compression mode

    return err;
}

BOOL Header900::DuplexEnabled(BYTE* bDevIDBuff)
{
    char* pStrVstatus = NULL;
    char* pStrDuplex = NULL;
    char* pStrSemicolon = NULL;

    if((pStrVstatus = strstr((char*)bDevIDBuff + 2,"VSTATUS:")))
        pStrVstatus += 8;
    else
        return FALSE;

    pStrDuplex = pStrVstatus;
    pStrSemicolon = pStrVstatus;

    // now parse VSTATUS parameters to find if we are in simplex or duplex
    if (!(pStrSemicolon = strstr((char*)pStrVstatus,";")))
        return FALSE;

    if ( (pStrDuplex = strstr((char*)pStrVstatus,"DP")) )
        if(pStrDuplex < pStrSemicolon)
            return TRUE;
    if ( (pStrDuplex = strstr((char*)pStrVstatus,"SM")) )
        if(pStrDuplex < pStrSemicolon)
            return FALSE;

    DBG1("didn't find SM or DP!!\n");
    return FALSE;
}


BYTE DJ9xx::PhotoTrayStatus
(
    BOOL bQueryPrinter
)
{
    DRIVER_ERROR err;
    char* pStrVstatus = NULL;
    char* pStrPhotoTray = NULL;
    char* pStrSemicolon = NULL;

    BYTE bDevIDBuff[DevIDBuffSize];

    if (!IOMode.bDevID)
    {
        bQueryPrinter = FALSE;
    }

    err=pSS->GetDeviceID(bDevIDBuff, DevIDBuffSize, bQueryPrinter);
    if (err!=NO_ERROR)
    {
        return 0;
    }

    if((pStrVstatus = strstr((char*)bDevIDBuff + 2,"VSTATUS:")))
    {
        pStrVstatus += 8;
    }
    else
    {
        return 0;
    }

    pStrPhotoTray = pStrVstatus;
    pStrSemicolon = pStrVstatus;

    // now parse VSTATUS parameters to find if we are in simplex or duplex
    if (!(pStrSemicolon = strstr((char*)pStrVstatus,";")))
    {
        return 0;
    }

    if ( (pStrPhotoTray = strstr((char*)pStrVstatus,"PH")) )
    {
        if(pStrPhotoTray < pStrSemicolon)
        {
            return '9';  // return same as VIP installed and engaged status
        }
    }
    if ( (pStrPhotoTray = strstr((char*)pStrVstatus,"NR")) )
    {
        if(pStrPhotoTray < pStrSemicolon)
        {
            return 0;
        }
    }

    DBG1("didn't find PH or NR!!\n");
    return 0;
} //PhotoTrayStatus

BOOL DJ9xx::PhotoTrayPresent
(
    BOOL bQueryPrinter
)
{
    // present (and not engaged) == 8
    return ((PhotoTrayStatus(bQueryPrinter) & 8) == 8);
} //PhotoTrayInstalled


PHOTOTRAY_STATE DJ9xx::PhotoTrayEngaged
(
    BOOL bQueryPrinter
)
{
    // present and engaged == 9
    return ((PHOTOTRAY_STATE) ((PhotoTrayStatus(bQueryPrinter) & 9) == 9));
} //PhotoTrayEngaged


PAPER_SIZE DJ9xx::MandatoryPaperSize()
{
    if (PhotoTrayEngaged (TRUE))
    {
        return PHOTO_SIZE;
    }
    else
    {
        return UNSUPPORTED_SIZE;   // code for "nothing mandatory"
    }
} //MandatoryPaperSize


DISPLAY_STATUS DJ9xx::ParseError(BYTE status_reg)
{
    DBG1("DJ9XX, parsing error info\n");

    DRIVER_ERROR err = NO_ERROR;
    BYTE DevIDBuffer[DevIDBuffSize];

    char *pStr;
    if(IOMode.bDevID)
    {
        // If a bi-di cable was plugged in and everything was OK, let's see if it's still
        // plugged in and everything is OK
        err = pSS->GetDeviceID (DevIDBuffer, DevIDBuffSize, TRUE);
        if(err != NO_ERROR)
        {
            // job was bi-di but now something's messed up, probably cable unplugged
            return DISPLAY_COMM_PROBLEM;
        }

        if ( (pStr=(char *)strstr((const char*)DevIDBuffer+2,"VSTATUS:")) == NULL )
        {
            return DISPLAY_COMM_PROBLEM;
        }

        pStr+=8;   // skip "VSTATUS:"
        
		// Paper Jam or Paper Stall
        if (strstr((char*)pStr,"PAJM") || strstr((char*)pStr,"PAPS"))
        {
            return DISPLAY_PAPER_JAMMED;
        }

		// Carriage Stall
        if (strstr((char*)pStr,"CARS"))
        {
            return DISPLAY_ERROR_TRAP;
        }

        if (strstr((char*)pStr,"OOPA"))     // OOP state
        {
            DBG1("Out of Paper [from Encoded DevID]\n");
            return DISPLAY_OUT_OF_PAPER;
        }

		//  Job Cancelled (AIO printer turn idle after job canceled)
        if (strstr((char*)pStr,"CNCL"))     // CNCL state
        {
            DBG1("Printing Canceled [from Encoded DevID]\n");
            return DISPLAY_PRINTING_CANCELED;
        }

        if ( TopCoverOpen(status_reg) )
        {
            DBG1("Top Cover Open\n");
            return DISPLAY_TOP_COVER_OPEN;
        }

        // VerifyPenInfo will handle prompting the user
        // if this is a problem
        err = VerifyPenInfo();

        if(err != NO_ERROR)
            // VerifyPenInfo returned an error, which can only happen when ToDevice
            // or GetDeviceID returns an error. Either way, it's BAD_DEVICE_ID or
            // IO_ERROR, both unrecoverable.  This is probably due to the printer
            // being turned off during printing, resulting in us not being able to
            // power it back on in VerifyPenInfo, since the buffer still has a
            // partial raster in it and we can't send the power-on command.
            return DISPLAY_COMM_PROBLEM;
    }

    // check for errors we can detect from the status reg
    if (IOMode.bStatus)
    {
        if ( DEVICE_IS_OOP(status_reg) )
        {
            DBG1("Out Of Paper\n");
            return DISPLAY_OUT_OF_PAPER;
        }

        if (DEVICE_PAPER_JAMMED(status_reg))
        {
            DBG1("Paper Jammed\n");
            return DISPLAY_PAPER_JAMMED;
        }
        if (DEVICE_IO_TRAP(status_reg))
        {
            DBG1("IO trap\n");
            return DISPLAY_ERROR_TRAP;
        }
    }

    // don't know what the problem is-
    //  Is the PrinterAlive?
    if (pSS->PrinterIsAlive())
    {
        iTotal_SLOW_POLL_Count += iMax_SLOW_POLL_Count;
#if defined(DEBUG) && (DBG_MASK & DBG_LVL1)
        printf("iTotal_SLOW_POLL_Count = %d\n",iTotal_SLOW_POLL_Count);
#endif
        // -Note that iTotal_SLOW_POLL_Count is a multiple of
        //  iMax_SLOW_POLL_Count allowing us to check this
        //  on an absolute time limit - not relative to the number
        //  of times we happen to have entered ParseError.
        // -Also note that we have different thresholds for uni-di & bi-di.
        if(
            ((IOMode.bDevID == FALSE) && (iTotal_SLOW_POLL_Count >= 60)) ||
            ((IOMode.bDevID == TRUE)  && (iTotal_SLOW_POLL_Count >= 120))
          )
            return DISPLAY_BUSY;
        else return DISPLAY_PRINTING;
    }
    else
        return DISPLAY_COMM_PROBLEM;
}

DRIVER_ERROR DJ9xx::VerifyPenInfo()
{

    DRIVER_ERROR err=NO_ERROR;

    if(IOMode.bDevID == FALSE)
        return err;

    err = ParsePenInfo(ePen);

    if(err == UNSUPPORTED_PEN) // probably Power Off - pens couldn't be read
    {
        DBG1("DJ9xx::Need to do a POWER ON to get penIDs\n");

        // have to delay for DJ9xx or the POWER ON will be ignored
        if (pSS->BusyWait((DWORD)2000) == JOB_CANCELED)
            return JOB_CANCELED;

        DWORD length=sizeof(DJ895_Power_On);
        err = pSS->ToDevice(DJ895_Power_On,&length);
        ERRCHECK;

        err = pSS->FlushIO();
        ERRCHECK;

        // give the printer some time to power up
        if (pSS->BusyWait ((DWORD) 2500) == JOB_CANCELED)
            return JOB_CANCELED;

        err = ParsePenInfo(ePen);
    }

    ERRCHECK;

    // check for the normal case
    if (ePen == BOTH_PENS)
        return NO_ERROR;

    while ( ePen != BOTH_PENS   )
    {

        switch (ePen)
        {
            case BLACK_PEN:
                // black pen installed, need to install color pen
                pSS->DisplayPrinterStatus(DISPLAY_NO_COLOR_PEN);
                break;
            case COLOR_PEN:
                // color pen installed, need to install black pen
                pSS->DisplayPrinterStatus(DISPLAY_NO_BLACK_PEN);
                break;
            case NO_PEN:
                // neither pen installed
            default:
                pSS->DisplayPrinterStatus(DISPLAY_NO_PENS);
                break;
        }

        if (pSS->BusyWait(500) == JOB_CANCELED)
            return JOB_CANCELED;

        err =  ParsePenInfo(ePen);
        ERRCHECK;
    }

    pSS->DisplayPrinterStatus(DISPLAY_PRINTING);

    return NO_ERROR;

}

DRIVER_ERROR DJ9xx::ParsePenInfo(PEN_TYPE& ePen, BOOL QueryPrinter)
{
    char    *str;
    DRIVER_ERROR err = SetPenInfo (str, QueryPrinter);
    ERRCHECK;

    if (*str != '$')
    {
		// DeskJet 9300 has DJ990 style devid string.
		int num_pens = 0;
		PEN_TYPE temp_pen1 = NO_PEN;
		BYTE penInfoBits[4];
		int iNumMissingPens = 0;

		// the first byte indicates how many pens are supported
		if ((str[0] >= '0') && (str[0] <= '9'))
		{
			num_pens = str[0] - '0';
		}
		else if ((str[0] >= 'A') && (str[0] <= 'F'))
		{
			num_pens = 10 + (str[0] - 'A');
		}
		else
		{
			return BAD_DEVICE_ID;
		}

		if ((int) strlen (str) < (num_pens * 8))
		{
			return BAD_DEVICE_ID;
		}

		//  DJ990 style DevID

		if (pSS->GetVIPVersion () == 1)
		{
			if (num_pens < 2)
			{
				return UNSUPPORTED_PEN;
			}

			// parse pen1 (should be black)
			AsciiHexToBinary(penInfoBits, str+1, 4);
			penInfoBits[1] &= 0xf8; // mask off ink level trigger bits

			if ((penInfoBits[0] == 0xc1) && (penInfoBits[1] == 0x10))
			{   // black
				temp_pen1 = BLACK_PEN;
			}
			else if (penInfoBits[0] == 0xc0)
			{   // missing pen
				temp_pen1 = NO_PEN;
				iNumMissingPens = 1;
			}
			else
			{
				return UNSUPPORTED_PEN;
			}

			// now check pen2 (should be color)
			AsciiHexToBinary(penInfoBits, str+9, 4);
			penInfoBits[1] &= 0xf8; // mask off ink level trigger bits

			if ((penInfoBits[0] == 0xc2) && (penInfoBits[1] == 0x08))
			{   // Chinook
				if (temp_pen1 == BLACK_PEN)
				{
					ePen = BOTH_PENS;
				}
				else
				{
					ePen = COLOR_PEN;
				}
			}
			else if (penInfoBits[0] == 0xc0)
			{   // missing pen
				ePen = temp_pen1;
				iNumMissingPens = 1;
			}
			else
			{
				return UNSUPPORTED_PEN;
			}

			return NO_ERROR;
		}

	//  Check for missing pens

		if (*(str - 1) == '1' && *(str - 2) == '1')
		{
			return UNSUPPORTED_PEN;
		}

		char    *p = str + 1;
		BYTE    penColor;

		ePen = NO_PEN;

		for (int i = 0; i < num_pens; i++, p += 8)
		{
			AsciiHexToBinary (penInfoBits, p, 8);

			if ((penInfoBits[1] & 0xf8) == 0xf8)
			{

	//          The high 5 bits in the 3rd and 4th nibble (second byte) identify the
	//          installed pen. If all 5 bits are on, user has installed an incompatible pen.

				return UNSUPPORTED_PEN;
			}

			if ((penInfoBits[0] & 0x80) != 0x80)        // if Bit 31 is 0, this is not a pen
			{
				continue;
			}
			penColor = penInfoBits[0] & 0x3F;
			switch (penColor)
			{
				case 0:
				{
					iNumMissingPens++;
					break;
				}
				case 1:
					ePen = BLACK_PEN;
					break;
				case 2:
				{
					if (ePen == BLACK_PEN)
					{
						ePen = BOTH_PENS;
					}
					else
					{
						ePen = COLOR_PEN;
					}
					break;
				}
				case 4:             // cyan pen
				case 5:             // magenta pen
				case 6:             // yellow pen
				case 7:             // low dye load cyan pen
				case 8:             // low dye load magenta pen
				case 9:             // low dye load yellow pen
					if (ePen == BLACK_PEN || ePen == BOTH_PENS)
					{
						ePen = BOTH_PENS;
					}
					else
					{
						ePen = COLOR_PEN;
					}
					break;
				default:
					ePen = UNKNOWN_PEN;
			}
		}
		return NO_ERROR;
    }

	// parse penID
	PEN_TYPE temp_pen1;
	// check pen1, assume it is black, pen2 is color
	switch (str[1])
	{
		case 'H':
		case 'L':
		case 'Z':
			temp_pen1 = BLACK_PEN;
			break;
		case 'X': return UNSUPPORTED_PEN;
		default:  temp_pen1 = NO_PEN; break;
	}

	// now check pen2

	int i = 2;
	while ((i < DevIDBuffSize) && str[i]!='$') i++; // handles variable length penIDs
	if (i == DevIDBuffSize)
	{
		return BAD_DEVICE_ID;
	}

	i++;

	// need to be more forgiving of the color pen type because of
	// the unknown chinookID for DJ970
	// we can't guarantee the (F)lash color pen, but we can make sure
	// the pen is not (X)Undefined, (A)Missing or (M)onet
	if(str[i]!='X' && str[i]!='A' && str[i]!='M')
	// check what pen1 was
	{
		if (temp_pen1 == BLACK_PEN)
				ePen = BOTH_PENS;
		else
		{
				ePen = COLOR_PEN;
		}
	}
	else // no color pen, just set what pen1 was
		ePen = temp_pen1;

	return NO_ERROR;
}

#if defined(APDK_FONTS_NEEDED)
Font* DJ9xx::RealizeFont(const int index,const BYTE bSize,
                           const TEXTCOLOR eColor,
                           const BOOL bBold,const BOOL bItalic,
                           const BOOL bUnderline)

{

    return Printer::RealizeFont(index,bSize,eColor,bBold,bItalic,bUnderline);
}
#endif

DRIVER_ERROR DJ9xx::CleanPen()
{
    const BYTE DJ970_User_Output_Page[] = {ESC, '%','P','u','i','f','p','.',
        'm','u','l','t','i','_','b','u','t','t','o','n','_','p','u','s','h',' ','3',';',
        'u','d','w','.','q','u','i','t',';',ESC,'%','-','1','2','3','4','5','X' };

    DWORD length = sizeof(PEN_CLEAN_PML);
    DRIVER_ERROR err = pSS->ToDevice(PEN_CLEAN_PML, &length);
    ERRCHECK;

    // send this page so that the user sees some output.  If you don't send this, the
    // pens get serviced but nothing prints out.
    length = sizeof(DJ970_User_Output_Page);
    return pSS->ToDevice(DJ970_User_Output_Page, &length);
}

APDK_END_NAMESPACE

#endif  // APDK_DJ9xx
