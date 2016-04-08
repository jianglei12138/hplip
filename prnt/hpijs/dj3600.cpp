/*****************************************************************************\
  dj3600.cpp : Implimentation for the DJ3600 class

  Copyright (c) 2003-2003, HP Co.
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


#if defined(APDK_DJ3600) && defined (APDK_DJ3320)

#include "header.h"
#include "dj3320.h"
#include "dj3600.h"
#include "printerproxy.h"

APDK_BEGIN_NAMESPACE

DJ3600::DJ3600 (SystemServices* pSS, BOOL proto)
    : DJ3320 (pSS, proto)
{
}

/*
* byte 13 - indicates full bleed (to every edge of paper) is supported if 
* the modifier bit in the lowest position is "set".  
*

* 0000b or 0h = Unimplemented, or not specified.
* 0001b or 1h = Full bleed (4 edge) printing supported on photo quality papers; this field is a modifier upon the bits in byte 12. That is, if this bit is set, it implies that the "max print width supported" values and smaller widths, ALL support full bleed, 4 edge, printing on photo quality paper.
* 0010b or 2h = Full bleed (4 edge) printing supported on non-photo papers (e.g. plain, bond, etc.); this field is a modifier upon the bits in byte 12. If this bit is set, it implies that the "max print width supported" values and smaller widths, all support full bleed, 4 edge, printing on non-photo paper.
* nn00b  = The "nn" bits are reserved for future definitions
*/

BOOL DJ3600::FullBleedCapable (PAPER_SIZE ps, FullbleedType  *fbType, float *xOverSpray, float *yOverSpray,
                                     float *fLeftOverSpray, float *fTopOverSpray)
{
    BYTE    sDevIdStr[DevIDBuffSize];
    char    *pStr;
    if (pSS->IOMode.bDevID && (pSS->GetDeviceID (sDevIdStr, DevIDBuffSize, FALSE)) == NO_ERROR)
    {
        if ((pStr = strstr ((char *) sDevIdStr, ";S:")))
        {
			char  byte13 = pStr[18];
			short value = (byte13 >= '0' && byte13 <= '9') ? byte13 - '0' : 
					  ((byte13 >= 'A' && byte13 <= 'F') ? byte13 - 'A' + 10 :
					  ((byte13 >= 'a' && byte13 <= 'f') ? byte13 - 'a' + 10 : -1));
			switch (ps)
			{
				case PHOTO_SIZE:
				case A6:
				case CARD_4x6:
				case OUFUKU:
				case HAGAKI:
				case A6_WITH_TEAR_OFF_TAB:
				{
					*xOverSpray = (float) 0.216;
					*yOverSpray = (float) 0.174;

					if (fLeftOverSpray)
						*fLeftOverSpray = (float) 0.098;
					if (fTopOverSpray)
						*fTopOverSpray  = (float) 0.070;

					if (ps == PHOTO_SIZE || ps == A6_WITH_TEAR_OFF_TAB)
						*fbType = fullbleed4EdgeAllMedia;
					else if ((value != -1) && ((value & 0x03) == 0x03))
						*fbType = fullbleed4EdgeAllMedia;
					else if ((value != -1) && ((value & 0x01) == 0x01))
						*fbType = fullbleed4EdgePhotoMedia;
					else if ((value != -1) && ((value & 0x02) == 0x02))
						*fbType = fullbleed4EdgeNonPhotoMedia;
					else
						*fbType = fullbleed3EdgeAllMedia;

					return TRUE;
				}
				default:
					break;
			}
        }
    }
	else
	{
		switch (ps)
		{
			case PHOTO_SIZE:
			case A6:
			case CARD_4x6:
			case OUFUKU:
			case HAGAKI:
			case A6_WITH_TEAR_OFF_TAB:
			{
				*xOverSpray = (float) 0.216;
				*yOverSpray = (float) 0.174;

				if (fLeftOverSpray)
					*fLeftOverSpray = (float) 0.098;
				if (fTopOverSpray)
					*fTopOverSpray  = (float) 0.070;

				if (ps == PHOTO_SIZE || ps == A6_WITH_TEAR_OFF_TAB)
					*fbType = fullbleed4EdgeAllMedia;
				else
					*fbType = fullbleed3EdgeAllMedia;

				return TRUE;
			}
			default:
				break;
		}
	}
    *xOverSpray = (float) 0;
    *yOverSpray = (float) 0;
    if (fLeftOverSpray)
        *fLeftOverSpray = (float) 0;
    if (fTopOverSpray)
        *fTopOverSpray  = (float) 0;
	*fbType = fullbleedNotSupported;
    return FALSE;
}


APDK_END_NAMESPACE

#endif // defined(APDK_DJ3600) && defined (APDK_DJ3320)
