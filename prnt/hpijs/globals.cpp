/*****************************************************************************\
  globals.cpp : Global functions for APDK

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


#include "header.h"
// Functions used in Translator

APDK_BEGIN_NAMESPACE

/////////////////////////////////////////////////////////////////////////
int stringlen(const char*s)
// may not be in some systems
{ int c=0;
  while (s[c++]) ;
  return c-1;
}
// utilities to save overhead for copying escape sequences

// notice that the input is assumed to be zero-terminated, but the results
// generated are not, in keeping with PCL

BYTE EscCopy(BYTE *dest, const char *s,int num, char end)
{
  dest[0]=ESC; strcpy((char*)&dest[1],s);
  BYTE k = stringlen(s)+1;
  BYTE i = sprintf((char *)&dest[k],"%d",num);
  dest[k+i] = end;
  return (k+i+1);
}
BYTE EscAmplCopy(BYTE *dest, int num, char end)
{
  dest[0]=ESC; dest[1]='&';dest[2]='l';
  BYTE i = sprintf((char *)&dest[3],"%d",num);
  dest[3+i] = end;
  return (4+i);
}

MediaSize PaperToMediaSize(PAPER_SIZE psize)
{
    switch(psize)
    {
    case LETTER:        return sizeUSLetter;    break;
    case LEGAL:         return sizeUSLegal;     break;
    case A4:            return sizeA4;          break;
    case PHOTO_SIZE:    return sizePhoto;       break;
    case CARD_4x6:      return sizePhoto;
    case A6:            return sizeA6;          break;
    case A6_WITH_TEAR_OFF_TAB: return sizeA6;
    case B4:            return sizeB4;          break;
    case B5:            return sizeB5;          break;
    case OUFUKU:        return sizeOUFUKU;      break;
    case HAGAKI:        return sizeHAGAKI;      break;
#ifdef APDK_EXTENDED_MEDIASIZE
    case A3:        return sizeA3;      break;
    case A5:        return sizeA5;      break;
    case LEDGER:        return sizeLedger;      break;
    case SUPERB_SIZE:        return sizeSuperB;      break;
    case EXECUTIVE:     return sizeExecutive;   break;
    case FLSA:          return sizeFLSA;        break;
    case CUSTOM_SIZE:   return sizeCustom;      break;
	case ENVELOPE_NO_10: return sizeNum10Env;    break;
	case ENVELOPE_A2:    return sizeA2Env;       break;
	case ENVELOPE_C6:    return sizeC6Env;       break;
	case ENVELOPE_DL:    return sizeDLEnv;       break;
	case ENVELOPE_JPN3:  return size3JPNEnv;     break;
	case ENVELOPE_JPN4:  return size4JPNEnv;     break;
#endif
    case PHOTO_5x7:     return size5x7;         break;
    case CDDVD_80:      return sizeCDDVD80;     break;
    case CDDVD_120:     return sizeCDDVD120;    break;
    default:            return sizeUSLetter;    break;
    }
}

PAPER_SIZE MediaSizeToPaper(MediaSize msize)
{
    switch(msize)
    {
    case sizeUSLetter:  return LETTER;      break;
    case sizeUSLegal:   return LEGAL;       break;
    case sizeA4:        return A4;          break;
    case sizePhoto:     return PHOTO_SIZE;  break;
    case sizeA6:        return A6;          break;
    case sizeB4:        return B4;          break;
    case sizeB5:        return B5;          break;
    case sizeOUFUKU:    return OUFUKU;      break;
    case sizeHAGAKI:    return HAGAKI;      break;
#ifdef APDK_EXTENDED_MEDIASIZE
    case sizeA3:    return A3;      break;
    case sizeA5:    return A5;      break;
    case sizeLedger:    return LEDGER;      break;
    case sizeSuperB:    return SUPERB_SIZE;      break;
    case sizeExecutive: return EXECUTIVE;   break;
    case sizeFLSA:      return FLSA;        break;
    case sizeCustom:    return CUSTOM_SIZE; break;
	case sizeNum10Env:  return ENVELOPE_NO_10;    break;
	case sizeA2Env:     return ENVELOPE_A2;       break;
	case sizeC6Env:     return ENVELOPE_C6;       break;
	case sizeDLEnv:     return ENVELOPE_DL;       break;
	case size3JPNEnv:   return ENVELOPE_JPN3;     break;
	case size4JPNEnv:   return ENVELOPE_JPN4;     break;
#endif
    case size5x7:       return PHOTO_5x7;        break;
    case sizeCDDVD80:   return CDDVD_80;         break;
    case sizeCDDVD120:  return CDDVD_120;        break;
    default:            return UNSUPPORTED_SIZE; break;
    }
}

MediaType MediaTypeToPcl (MEDIATYPE eMediaType)
{
    switch (eMediaType)
    {
        case MEDIA_PLAIN:
	        return mediaPlain;
	    case MEDIA_PREMIUM:
	        return mediaSpecial;
            case MEDIA_PHOTO:
	    case MEDIA_ADVANCED_PHOTO:
	        return mediaGlossy;
	    case MEDIA_TRANSPARENCY:
	        return mediaTransparency;
	    case MEDIA_HIGHRES_PHOTO:
	        return mediaHighresPhoto;
	    case MEDIA_AUTO:
	        return mediaAuto;
	    case MEDIA_CDDVD:
	        return mediaCDDVD;
        case MEDIA_BROCHURE:
            return mediaBrochure;
	    default:
	        return mediaPlain;
    }
}

void AsciiHexToBinary(BYTE* dest, char* src, int count)
{
    int i;
    BYTE bitPattern;
    for (i=0; i<count ; i++)
    {
        switch (src[i])
        {
            case '0':
                bitPattern = 0x00;
                break;
            case '1':
                bitPattern = 0x01;
                break;
            case '2':
                bitPattern = 0x02;
                break;
            case '3':
                bitPattern = 0x03;
                break;
            case '4':
                bitPattern = 0x04;
                break;
            case '5':
                bitPattern = 0x05;
                break;
            case '6':
                bitPattern = 0x06;
                break;
            case '7':
                bitPattern = 0x07;
                break;
            case '8':
                bitPattern = 0x08;
                break;
            case '9':
                bitPattern = 0x09;
                break;
            case 'a':
            case 'A':
                bitPattern = 0x0a;
                break;
            case 'b':
            case 'B':
                bitPattern = 0x0b;
                break;
            case 'c':
            case 'C':
                bitPattern = 0x0c;
                break;
            case 'd':
            case 'D':
                bitPattern = 0x0d;
                break;
            case 'e':
            case 'E':
                bitPattern = 0x0e;
                break;
            case 'f':
            case 'F':
                bitPattern = 0x0f;
                break;
            default:
                // default case should never happen so abort during debugging
                ASSERT(0);
                bitPattern = 0;
                break;
        }

        if ((i%2) == 0)
        {
            dest[i/2] = bitPattern << 4;
        }
        else
        {
            dest[i/2] |= bitPattern;
        }
    }
} //AsciiHexToBinary

#ifdef HAVE_LIBDL
#include <dlfcn.h>

void *LoadPlugin (const char *szPluginName)
{
    FILE    *fp;
    char    szLine[256];
    int     i;
    void    *ptemp = NULL;
    char    *p = NULL;
    int     bFound = 0;
    if ((fp = fopen ("/etc/hp/hplip.conf", "r")) == NULL)
    {
        return NULL;
    }
    while (!feof (fp))
    {
        if (!fgets (szLine, 256, fp))
        {
            break;
        }
        if (!bFound && strncmp (szLine, "[dirs]", 6))
            continue;
        bFound = 1;
        if (szLine[0] < ' ')
            break;
        if (!strncmp (szLine, "home", 4))
        {
            i = strlen (szLine);
            while (i > 0 && szLine[i] < ' ')
                szLine[i--] = '\0';
            p = szLine + 4;
            while (*p && *p != '/')
                p++;
            i = sizeof(szLine) - (strlen (p) + (p - szLine));
            snprintf (p+strlen (p), i, "/prnt/plugins/%s", szPluginName);
            ptemp = dlopen (p, RTLD_LAZY);
        }
    }
    fclose (fp);
    return ptemp;
}
#endif // HAVE_LIBDL

APDK_END_NAMESPACE

