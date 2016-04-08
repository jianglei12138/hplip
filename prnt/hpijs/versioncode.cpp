/*****************************************************************************\
  versioncode.cpp : version information routines

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
#include "printerfactory.h"

APDK_BEGIN_NAMESPACE
extern BOOL ProprietaryImaging();
extern BOOL ProprietaryScaling();
APDK_END_NAMESPACE

APDK_BEGIN_NAMESPACE

extern char DeveloperString[32];

/*! \addtogroup globals
@{
*/

const char VersionStringID[21] = "04.09.00A_11-06-2009";   //!< Version information

char result[500];

void HP_strcat(char* str1, const char* str2)
{
    while(*str1)
    {
        str1++;
    }
    while( (*str1++ = *str2++) )
    {
        // nothing.  Just copying str2 to str1
        ;
    }
}


/*!
Returns a string identifying components and features of the driver build.
If bCompressed is TRUE then the string returned is a bit representation string
of the build options (ie. speed, scaling, capture, etc...).  If bCompressed is
FALSE then the string is a textual concatenation of the developer build string,
the APDK version string, and string representation of the build options.

This function returns a string identifying components and features of the driver
build, such as which printer-models are supported, whether font and scaling
support is included, and other information intended to help solve customer problems.

\param int bCompressed If true, the information is packed into a decimal number
that may be decoded by Hewlett-Packard. Otherwise, the string consists of
human-readable indicators.
\return char* string representing version information.
\note If bCompressed is TRUE then the return string has only build options and
no version information.
*/
char* Version(BOOL bCompressed)
{
    if (bCompressed)
    {
        unsigned int bits=0;

        if (ProprietaryScaling())
            bits = bits | 0x80000000;

#ifdef APDK_CAPTURE
        bits = bits | 0x40000000;
#endif

#ifdef APDK_LITTLE_ENDIAN
        bits = bits | 0x20000000;
#endif

        if (ProprietaryImaging())
            bits = bits | 0x10000000;

#ifdef APDK_COURIER
        bits = bits | 0x08000000;
#endif
#ifdef APDK_CGTIMES
        bits = bits | 0x04000000;
#endif
#ifdef APDK_LTRGOTHIC
        bits = bits | 0x02000000;
#endif
#ifdef APDK_UNIVERS
        bits = bits | 0x01000000;
#endif

#ifdef APDK_OPTIMIZE_FOR_SPEED
        bits = bits | 0x00800000;
#endif

    bits = bits | pPFI->GetModelBits();

  // room left for 23 more here
        sprintf(result,"%0x", bits);
    }
    else
    {
        strcpy(result,DeveloperString);
        HP_strcat(result,"!!");
        HP_strcat(result,VersionStringID);
        HP_strcat(result," ");

        if (ProprietaryScaling())
            HP_strcat(result,"propscale ");
        else 
			HP_strcat(result,"openscale ");

        if (ProprietaryImaging())
            HP_strcat(result,"propimg ");
        else 
			HP_strcat(result,"openimg ");

#ifdef APDK_CAPTURE
        HP_strcat(result,"debug ");
#else
        HP_strcat(result,"normal ");
#endif

#ifdef APDK_LITTLE_ENDIAN
        HP_strcat(result,"little_endian ");
#else
        HP_strcat(result,"big_endian ");
#endif

#if defined(APDK_FONTS_NEEDED)
        HP_strcat(result,"fonts:");
#else
        HP_strcat(result,"no_fonts");
#endif
#ifdef APDK_COURIER
        HP_strcat(result,"C");
#endif
#ifdef APDK_CGTIMES
        HP_strcat(result,"T");
#endif
#ifdef APDK_LTRGOTHIC
        HP_strcat(result,"L");
#endif
#ifdef APDK_UNIVERS
        HP_strcat(result,"U");
#endif
        HP_strcat(result," ");

#ifdef APDK_OPTIMIZE_FOR_SPEED
        HP_strcat(result,"speed ");
#else
        HP_strcat(result,"memory ");
#endif

		char modelstring[300];
		pPFI->GetModelString(modelstring, sizeof(modelstring));
        HP_strcat(result, modelstring);

    }

    return result;
} //Version

/*! @} */

APDK_END_NAMESPACE
