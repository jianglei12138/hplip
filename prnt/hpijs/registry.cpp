/*****************************************************************************\
  registry.cpp : Implimentation for the DeviceRegistry class

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


// The purpose of this file is to facilitate addition and subtraction
// of supported devices from the system.

#include "header.h"
#include "printerfactory.h"

#include "printer.h"
#include "apollo2xxx.h"
#include "apollo21xx.h"
#include "apollo2560.h"
#include "apollo2xxx.h"
#ifdef APDK_DJ3320
#include "dj3320.h"
#ifdef APDK_DJ3600
#include "dj3600.h"
#include "dj4100.h"
#include "djd2600.h"
#endif
#endif
#include "dj400.h"
#include "dj6xx.h"
#include "dj600.h"
#include "dj630.h"
#include "dj660.h"
#include "dj690.h"
#include "dj350.h"
#include "dj540.h"
#include "dj8xx.h"
#include "dj850.h"
#include "dj8x5.h"
#include "dj890.h"
#include "dj9xx.h"
#include "dj9xxvip.h"
#include "djgenericvip.h"
#include "dj55xx.h"
#include "ojprokx50.h"
#include "ljmono.h"
#include "ljcolor.h"
#include "psp100.h"
#include "psp470.h"
#include "pscript.h"
#include "ljjetready.h"
#include "ljfastraster.h"
#if defined (APDK_LJZJS_MONO) || defined (APDK_LJZJS_COLOR) || defined (APDK_LJM1005)
#include "ljzjs.h"
#endif
#ifdef APDK_LJZJS_MONO
#include "ljzjsmono.h"
#endif
#ifdef APDK_LJZJS_COLOR
#include "ljzjscolor.h"
#endif
#ifdef APDK_LJM1005
#include "ljm1005.h"
#include "ljp1xxx.h"
#endif

#ifdef APDK_QUICKCONNECT
#include "quickconnect.h"
#endif

APDK_BEGIN_NAMESPACE

#ifdef APDK_PSCRIPT
PScriptProxy DeviceRegistry::s_PScriptProxy;
#endif

#ifdef APDK_LJMONO
LJMonoProxy DeviceRegistry::s_LJMonoProxy;
#endif

#ifdef APDK_LJCOLOR
LJColorProxy DeviceRegistry::s_LJColorProxy;
#endif

#ifdef APDK_LJJETREADY
LJJetReadyProxy DeviceRegistry::s_LJJetReadyProxy;
#endif

#ifdef APDK_LJFASTRASTER
LJFastRasterProxy DeviceRegistry::s_LJFastRasterProxy;
#endif

#ifdef APDK_LJZJS_MONO
LJZjsMonoProxy DeviceRegistry::s_LJZjsMonoProxy;
#endif

#ifdef APDK_LJZJS_COLOR
LJZjsColorProxy DeviceRegistry::s_LJZjsColorProxy;
#endif

#ifdef APDK_LJM1005
LJM1005Proxy DeviceRegistry::s_LJM1005Proxy;
LJP1XXXProxy DeviceRegistry::s_LJP1XXXProxy;
#endif

#if defined(APDK_PSP100) && defined (APDK_DJ9xxVIP)
PSP100Proxy DeviceRegistry::s_PSP100Proxy;
PSP470Proxy DeviceRegistry::s_PSP470Proxy;
#endif

#if defined(APDK_DJGENERICVIP) && defined (APDK_DJ9xxVIP)
DJGenericVIPProxy DeviceRegistry::s_DJGenericVIPProxy;
DJ55xxProxy DeviceRegistry::s_DJ55xxProxy;
#endif

#ifdef APDK_DJ9xx
DJ9xxProxy DeviceRegistry::s_DJ9xxProxy;
#endif

#ifdef APDK_DJ9xxVIP
DJ9xxVIPProxy DeviceRegistry::s_DJ9xxVIPProxy;
OJProKx50Proxy DeviceRegistry::s_OJProKx50Proxy;
#endif

#if defined(APDK_DJ8xx)|| defined(APDK_DJ9xx)
DJ8xxProxy DeviceRegistry::s_DJ8xxProxy;
#endif

#if defined(APDK_DJ8xx)|| defined(APDK_DJ9xx)
#ifdef APDK_DJ8x5
DJ8x5Proxy DeviceRegistry::s_DJ8x5Proxy;
#endif
#endif

#if defined(APDK_DJ890)
DJ890Proxy DeviceRegistry::s_DJ890Proxy;
#endif

#if defined(APDK_DJ850)
DJ850Proxy DeviceRegistry::s_DJ850Proxy;
#endif

#ifdef APDK_DJ6xxPhoto
DJ6xxPhotoProxy DeviceRegistry::s_DJ6xxPhotoProxy;
#endif

#ifdef APDK_DJ6xx
DJ660Proxy DeviceRegistry::s_DJ660Proxy;
#endif

#ifdef APDK_DJ630
DJ630Proxy DeviceRegistry::s_DJ630Proxy;
#endif

#ifdef APDK_DJ600
DJ600Proxy DeviceRegistry::s_DJ600Proxy;
#endif

#ifdef APDK_DJ540
DJ540Proxy DeviceRegistry::s_DJ540Proxy;
#endif

#ifdef APDK_DJ400
DJ400Proxy DeviceRegistry::s_DJ400Proxy;
#endif

#ifdef APDK_DJ350
DJ350Proxy DeviceRegistry::s_DJ350Proxy;
#endif

#if defined(APDK_DJ3600) && defined (APDK_DJ3320)
DJ3600Proxy DeviceRegistry::s_DJ3600Proxy;
DJ4100Proxy DeviceRegistry::s_DJ4100Proxy;
DJD2600Proxy DeviceRegistry::s_DJD2600Proxy;
#endif

#if defined (APDK_DJ3320)
DJ3320Proxy DeviceRegistry::s_DJ3320Proxy;
#endif

#ifdef APDK_APOLLO2560
Apollo2560Proxy DeviceRegistry::s_Apollo2560Proxy;
#endif

#ifdef APDK_APOLLO21XX
Apollo21xxProxy DeviceRegistry::s_Apollo21xxProxy;
#endif

#ifdef APDK_APOLLO2XXX
Apollo2xxxProxy DeviceRegistry::s_Apollo2xxxProxy;
#endif

#ifdef APDK_QUICKCONNECT
QuickConnectProxy DeviceRegistry::s_QuickConnectProxy;
#endif

DeviceRegistry::DeviceRegistry()
    : device(UNSUPPORTED)
{
}


DeviceRegistry::~DeviceRegistry()
{
    DBG1("deleting DeviceRegistry\n");
}


DRIVER_ERROR DeviceRegistry::SelectDevice(const PRINTER_TYPE Model)
{
    if (Model > MAX_PRINTER_TYPE)
        return UNSUPPORTED_PRINTER;
    device = Model;

	return NO_ERROR;
}


DRIVER_ERROR DeviceRegistry::SelectDevice(char* model, int *pVIPVersion, char* pens, SystemServices* pSS)
// used by PrintContext constructor
// based on this 'model' string, we will search for the enum'd value
// and set this enum'd value in 'device'
{

#if defined(DEBUG) && (DBG_MASK & DBG_LVL1)
    printf("DR::SelectDevice: model= '%s'\n",model);
    printf("DR::SelectDevice: VIPver= %d\n",*pVIPVersion);
    printf("DR::SelectDevice: pens= '%s'\n",pens);
#endif

	int j = 0;
    char pen1 = '\0';   // black/color(for CCM)/photo(for 690) pen
    char pen2 = '\0';   // color/non-existent(for CCM) pen

    BOOL match=FALSE;

    DRIVER_ERROR err = NO_ERROR;

    FAMILY_HANDLE familyHandle = pPFI->FindDevIdMatch(model);
    if (familyHandle != NULL)
    {
		device = pPFI->GetFamilyType(familyHandle);
		match = TRUE;
	}

    if (!match) // see if printer supports VIP, if so set compatible device
    {
        if (*pVIPVersion == 1)
        {
            match = TRUE;
            device = eDJ9xxVIP;
        }
        else if (*pVIPVersion > 1)
        {
            match = TRUE;
            device = eDJGenericVIP; // eDJ9xxVIP;
        }
    }

/*
 *  See if this is a sleek (LIDIL) printer, or PostScript printer
 */

    if (!match)
    {
        BYTE DevIDBuffer[DevIDBuffSize];

        err = pSS->GetDeviceID(DevIDBuffer, DevIDBuffSize, FALSE);
        ERRCHECK;   // should be either NO_ERROR or BAD_DEVICE_ID

		char	*cmdStr = (char *) strstr ((const char *) DevIDBuffer+2, "CMD:");
        char    *cmdStrEnd;
        if ((strstr((const char *) DevIDBuffer+2,"CMD:LDL")))
        {
            device = eDJ3320;
            match = TRUE;
        }
        if (!match && cmdStr && (cmdStrEnd = (char *) strstr (cmdStr, ";")))
        {
            *cmdStrEnd = '\0';
            if (strstr (cmdStr, "LDL"))
            {
                match = TRUE;
                device = eDJ4100;
            }
            *cmdStrEnd = ';';
        }
		if (!match && !cmdStr)
		{
			cmdStr = (char *) strstr ((const char *) DevIDBuffer+2, "COMMAND SET:");
		}
		if (!match && cmdStr && (strstr ((const char *) cmdStr+4, "POSTSCRIPT") || 
			                     strstr ((const char *) cmdStr+4, "PostScript") || 
					             strstr ((const char *) cmdStr+4, "Postscript") || 
					             strstr ((const char *) cmdStr+4, "postscript") ))
		{
			device = ePScript;
			match = TRUE;
		}
    }

    if (!match)
    {
    // The devID model string did not have a match for a known printer
    // and the printer doesn't support VIP so let's look at the pen info for clues

        // if we don't have pen info (VSTATUS) it's presumably
        //  either sleek, DJ4xx or non-HP
		device = UNSUPPORTED;
        if ( pens[0] != '\0' )
        {
            // DJ8xx (and DJ970?) printers return penID $X0$X0
            //  when powered off
            if(pens[1] == 'X')
            {
                DBG1("DR:(Unknown Model) Need to do a POWER ON to get penIDs\n");

                DWORD length=sizeof(DJ895_Power_On);
                err = pSS->ToDevice(DJ895_Power_On, &length);
                ERRCHECK;

                err = pSS->FlushIO();
                ERRCHECK;

                // give the printer some time to power up
                if (pSS->BusyWait((DWORD)1000) == JOB_CANCELED)
                return JOB_CANCELED;

                // we must re-query the devID
                err=GetPrinterModel(model,pVIPVersion,pens,pSS);
                ERRCHECK;
            }

            // Arggghh.  The pen(s) COULD be missing
            do
            {

//				Is this binary-encoded format?

				if (pens[0] != '$')
				{
					break;
				}

                // get pen1 - penID format is $HB0$FC0
                pen1=pens[1];

                // get pen2 - if it exists
                j=2;
                BOOL NO_PEN2 = FALSE;
                while(pens[j] != '$')   // handles variable length penIDs
                {
                    j++;
                    if ( pens[j] == '\0' )
                    // never found a pen2
                    {
                        pen2 = '\0';
                        NO_PEN2 = TRUE;
                        break;
                    }
                }
                if (NO_PEN2 == FALSE)
                {
                    j++;
                    pen2 = pens[j];
                }

                if(pen1 == 'A' || pen2 == 'A')
                {
                    if(pen1 == 'A')
                    {
                        // 2-pen printer with both pens missing
                        if(pen2 == 'A')
                            pSS->DisplayPrinterStatus(DISPLAY_NO_PENS);

                        // 1-pen printer with missing pen
                        else if(pen2 == '\0')
                            pSS->DisplayPrinterStatus(DISPLAY_NO_PEN_DJ600);

						// may be one-pen DJ8xx derivative
						else if (pen2 == 'F')
						{
							device = eDJ8x5;
							return NO_ERROR;
						}
                        // 2-pen printer with BLACK missing
                        else pSS->DisplayPrinterStatus(DISPLAY_NO_BLACK_PEN);
                    }
                    // 2-pen printer with COLOR missing
                    else if(pen2 == 'A')
					{

//						possibly DJ8x5 derivative

						if (pen1 == 'H' || pen1 == 'Z' || pen1 == 'L')
						{
							device = eDJ8x5;
							return NO_ERROR;
						}
                        pSS->DisplayPrinterStatus(DISPLAY_NO_COLOR_PEN);
					}

                    if (pSS->BusyWait(500) == JOB_CANCELED)
                        return  JOB_CANCELED;

                    // we must re-query the devID
                    err=GetPrinterModel(model,pVIPVersion,pens,pSS);
                    ERRCHECK;
                }

            } while(pen1 == 'A' || pen2 == 'A');

            // now that we have pens to look at, let's do the logic
            //  to instantiate the 'best-fit' driver

            if (pen1 == 'H' || pen1 == 'Z' || pen1 == 'L') // (BLACK)
            {
                // check for a 850/855/870
                if (pen2 == 'M')
					device = eDJ850;
                else if (strncmp (model,"DESKJET 890",11) == 0)
                    device=eDJ890; // 890 has same pens as DJ895!
                else if (pen2 == 'N')	// (COLOR)
					device = eDJ9xx;
                // It must be a DJ8xx derivative or will hopefully at
                // least recognize a DJ8xx print mode
                else
					device = eDJ8xx;
            }
            else if(pen1 == 'C') // (BLACK)
            {
                // check for 1-pen printer
                if (pen2 == '\0') device = eDJ600;
                // must be a 2-pen 6xx-derivative
                else device = eDJ6xx;
            }
            else if(pen1 == 'M') // Multi-dye load
            {
                // must be a 690-derivative
                device = eDJ6xxPhoto;
            }

            // check for 540-style pens?
            //  D = color, E = black

//            else device=UNSUPPORTED;
        }
    }


    // Early DJ8xx printer do not yet have full bi-di so check
    // the model to avoid a communication problem.
    if ( ( (strncmp(model,"DESKJET 81",10) == 0)
        || (strncmp(model,"DESKJET 83",10) == 0)
        || (strncmp(model,"DESKJET 88",10) == 0)
        || (strncmp(model,"DESKJET 895",11) == 0)
         )
        && (pSS->IOMode.bUSB)
       )
    {
        DBG1("This printer has limited USB status\n");
        pSS->IOMode.bStatus = FALSE;
        pSS->IOMode.bDevID = FALSE;
    }

    if ( ( (strncmp(model,"DESKJET 63",10) == 0)
        || (strncmp(model,"DESKJET 64",10) == 0)
         )
        && (pSS->IOMode.bUSB)
       )
    {
        DBG1("This printer has limited USB status, but we did get DeviceIDString\n");
        pSS->IOMode.bStatus = FALSE;
    }

    if (device == UNSUPPORTED) return UNSUPPORTED_PRINTER;
    else return NO_ERROR;
} //SelectDevice

DRIVER_ERROR DeviceRegistry::SelectDevice(const char* sDevID, SystemServices* pSS)
{
    char strModel[DevIDBuffSize]; // to contain the MODEL (MDL) from the DevID
    char strPens[64];   // to contain the VSTATUS penID from the DevID
    int  VIPVersion;    // VIP version from the DevID

	DRIVER_ERROR err = ParseDevIDString(sDevID, strModel, &VIPVersion, strPens);
	if (err != NO_ERROR)
	{
		return UNSUPPORTED_PRINTER;
	}

	return SelectDevice(strModel, &VIPVersion, strPens, pSS);
}

DRIVER_ERROR DeviceRegistry::InstantiatePrinter(Printer*& p, SystemServices* pSS)
// Instantiate a printer object and return a pointer p based on the previously
// set 'device' variable
{
    //ASSERT(p == NULL);  // if it's not then we're going to loose memory

    FAMILY_HANDLE familyHandle = pPFI->FindDevIdMatch(ModelName[device]);
    if (familyHandle == NULL)
    {
        ASSERT(familyHandle);
        DBG1("DR::InstantiatePrinter - no family match\n");
        return UNSUPPORTED_PRINTER;
    }
    p = pPFI->CreatePrinter(pSS, familyHandle);
    NEWCHECK(p);
    return p->constructor_error;
} //InstantiatePrinter



DRIVER_ERROR DeviceRegistry::GetPrinterModel(char* strModel, int *pVIPVersion, char* strPens, SystemServices* pSS)
{
    DRIVER_ERROR err;
    BYTE DevIDBuffer[DevIDBuffSize];

    err = pSS->GetDeviceID(DevIDBuffer, DevIDBuffSize, TRUE);
    ERRCHECK;   // should be either NO_ERROR or BAD_DEVICE_ID

    return ParseDevIDString((const char*)DevIDBuffer, strModel, pVIPVersion, strPens);

} //GetPrinterModel

#define HEXTOINT(x, p) if (x >= '0' && x <= '9')      *p |= x - '0'; \
                       else if (x >= 'A' && x <= 'F') *p |= 0xA + x - 'A'; \
                       else if (x >= 'a' && x <= 'f') *p |= 0xA + x - 'a'


//ParseDevIDString
//! Parse a device id string
/*!
Enter a full description of the method here. This will be the API doc.

******************************************************************************/
DRIVER_ERROR DeviceRegistry::ParseDevIDString(const char* sDevID, char* strModel, int *pVIPVersion, char* strPens)
{
    int i;  // simple counter
    char* pStr = NULL;  // string pointer used in parsing DevID

    // get the model name
    // - note: I'm setting pStr to the return of strstr
    //   so I need to increment past my search string
    if ( (pStr = (char *)strstr(sDevID+2,"MODEL:")) )
        pStr+=6;
    else
        if ( (pStr=(char *)strstr(sDevID+2,"MDL:")) )
            pStr+=4;
        else return BAD_DEVICE_ID;

    // my own version of strtok to pull out the model string here
    i = 0;
    while ( (pStr[i] != ';') && (pStr[i] != '\0') && (i < DevIDBuffSize))
	{
        strModel[i] = pStr[i];
		i++;
	}
    strModel[i] = '\0';


    // see if this printer support VIP or not
    if ( (pStr=(char *)strstr(sDevID+2,";S:00")) )   // binary encoded device ID status (version 0)
    {
        pStr += 15;     // get to the VIP support field (version of 0 == doesn't support VIP)
        if ((*pStr >= '0') && (*pStr <= '9'))
        {
            *pVIPVersion = *pStr - '0';
        }
        else if ((*pStr >= 'A') && (*pStr <= 'F'))
        {
            *pVIPVersion = 10 + (*pStr - 'A');
        }
        else
        {
            *pVIPVersion = 0;
        }
    }

/*
 *  DevID string has changed starting with Jupiter.
 *  Following ";S:", two nibbles for Version Number
 *  12 nibbles for Status Information, the last nibble
 *  is reserved for future use, the second from last
 *  indicates whether the printer has VIP support.
 *
 *  Actually, four nibbles were added.
 *  The first fourteen nibbles contain feature state info.
 *  So, starting with version 02 of device id, following ":S:" there are
 *   2 nibbles for version number
 *  14 nibbles for feature state  (the 15th nibble from ';' is the vip flag
 *   2 nibbles for printer status
 *
 *  Crystal added 4 more nibbles to the option field.
 */

    else if ((pStr = (char *)strstr (sDevID+2, ";S:")))
    {
        *pVIPVersion = 0;
        HEXTOINT (*(pStr+3), pVIPVersion);
        *pVIPVersion = *pVIPVersion << 4;
        HEXTOINT (*(pStr+4), pVIPVersion);
        if (*(pStr + 15) == '1')
        {
            (*pVIPVersion)++;
        }
        else
        {
            *pVIPVersion = 0;
        }
    }

    else
    {
        *pVIPVersion = 0;
    }

    // now get the pen info
    if( (pStr=(char *)strstr(sDevID+2,"VSTATUS:")) )
    {
        pStr+=8;
        i=0;
        while ( (pStr[i] != ',') && (pStr[i] != ';') && (pStr[i] != '\0') )
		{
            strPens[i] = pStr[i];
			i++;
		}
        strPens[i] = '\0';
    }
    else if ( (pStr = (char *)strstr(sDevID + 2, ";S:00")) ||   // binary encoded device ID status (version 0)
             (pStr = (char *)strstr (sDevID + 2, ";S:")))  // Jupiter and later style
    {

        int     iVersion = 0;
        HEXTOINT (*(pStr+3), &iVersion);
        iVersion = iVersion << 4;
        HEXTOINT (*(pStr+4), &iVersion);
        if (iVersion <= 2)
        {
            pStr += 19;     // get to the number of pens field
        }
        else if (iVersion < 4)
        {
            pStr += 21;
        }
        else
        {
            pStr += 25;
        }

        // each supported pen has a block of 8 bytes of info so copy the number of pens byte
        // plus 8 bytes for each supported ped
        if ((*pStr >= '0') && (*pStr <= '9'))
        {
            i = 1 + ((*pStr-'0')*8);
        }
        else if ((*pStr >= 'A') && (*pStr <= 'F'))
        {
            i = 1 + ((10 + (*pStr-'A')) * 8);
        }
        else
        {   // bogus number of pens field
            i = 1;
        }
        memcpy(strPens, pStr, i);
        strPens[i] = '\0';
    }
    else   // no VSTATUS for 400 and sleek printers
        strPens[0] = '\0';

    return NO_ERROR;
} //ParseDevIDString

APDK_END_NAMESPACE

