/*****************************************************************************\
    hpcupsfax.cpp : HP CUPS fax filter

    Copyright (c) 2001 - 2010, HP Co.
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:
    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
    3. Neither the name of the Hewlett-Packard nor the names of its
       contributors may be used to endorse or promote products derived
       from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
    IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
    OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
    IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
    NOT LIMITED TO, PATENT INFRINGEMENT; PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
    STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
    IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.
\*****************************************************************************/

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <math.h>
#include <cups/cups.h>
#include <cups/raster.h>
#include <string>
#ifdef FALSE
#undef FALSE
#endif
#ifdef TRUE
#undef TRUE
#endif
#include "hpip.h"
#include "hpcupsfax.h"
#include "bug.h"
#include "utils.h"
using namespace std;

int    fax_encoding = RASTER_MMR;
char   device_name[16];
BYTE   szFileHeader[68];
BYTE   szPageHeader[64];

uint32_t (*convert_endian_l)(uint32_t);
uint16_t (*convert_endian_s)(uint16_t);

static int iLogLevel = 1;
char hpFileName[MAX_FILE_PATH_LEN] ;

#define TIFF_HDR_SIZE 8
#define LITTLE_ENDIAN_MODE I
#define BIG_ENDIAN_MODE M

#define DBG(args...) syslog(LOG_INFO, __FILE__ " " STRINGIZE(__LINE__) ": " args)

// GrayLevel = (5/16)R + (9/16)G + (2/16)B
#define RGB2BW(r, g, b) (BYTE) (((r << 2) + r + (g << 3) + g + (b << 1)) >> 4)


void RGB2Gray (BYTE *pRGBData, int iNumPixels, BYTE *pGData)
{
    int     i;
    BYTE    *pIn = pRGBData;
    BYTE    *pOut = pGData;
    for (i = 0; i < iNumPixels; i++, pIn += 3)
    {
        *pOut++ = RGB2BW ((unsigned short) *pIn, (unsigned short) pIn[1], (unsigned short) pIn[2]);
    }
}

static void GetLogLevel ()
{
    FILE    *fp;
    char    str[258];
    char    *p;
    fp = fopen ("/etc/cups/cupsd.conf", "r");
    if (fp == NULL)
        return;
    while (!feof (fp))
    {
        if (!fgets (str, 256, fp))
	{
	    break;
	}
	if ((p = strstr (str, "hpLogLevel")))
	{
	    p += strlen ("hpLogLevel") + 1;
	    iLogLevel = atoi (p);
	    break;
	}
    }
    fclose (fp);
}

void PrintCupsHeader (cups_page_header2_t m_cupsHeader)
{
    if (iLogLevel == 0)
    {
        return;
    }
    BUG ("DEBUG: HPFAX - startPage...\n");
    BUG ("DEBUG: HPFAX - MediaClass = \"%s\"\n", m_cupsHeader.MediaClass);
    BUG ("DEBUG: HPFAX - MediaColor = \"%s\"\n", m_cupsHeader.MediaColor);
    BUG ("DEBUG: HPFAX - MediaType = \"%s\"\n", m_cupsHeader.MediaType);
    BUG ("DEBUG: HPFAX - OutputType = \"%s\"\n", m_cupsHeader.OutputType);
    BUG ("DEBUG: HPFAX - AdvanceDistance = %d\n", m_cupsHeader.AdvanceDistance);
    BUG ("DEBUG: HPFAX - AdvanceMedia = %d\n", m_cupsHeader.AdvanceMedia);
    BUG ("DEBUG: HPFAX - Collate = %d\n", m_cupsHeader.Collate);
    BUG ("DEBUG: HPFAX - CutMedia = %d\n", m_cupsHeader.CutMedia);
    BUG ("DEBUG: HPFAX - Duplex = %d\n", m_cupsHeader.Duplex);
    BUG ("DEBUG: HPFAX - HWResolution = [ %d %d ]\n", m_cupsHeader.HWResolution[0], m_cupsHeader.HWResolution[1]);
    BUG ("DEBUG: HPFAX - ImagingBoundingBox = [ %d %d %d %d ]\n",
               m_cupsHeader.ImagingBoundingBox[0], m_cupsHeader.ImagingBoundingBox[1],
               m_cupsHeader.ImagingBoundingBox[2], m_cupsHeader.ImagingBoundingBox[3]);
    BUG ("DEBUG: HPFAX - InsertSheet = %d\n", m_cupsHeader.InsertSheet);
    BUG ("DEBUG: HPFAX - Jog = %d\n", m_cupsHeader.Jog);
    BUG ("DEBUG: HPFAX - LeadingEdge = %d\n", m_cupsHeader.LeadingEdge);
    BUG ("DEBUG: HPFAX - Margins = [ %d %d ]\n", m_cupsHeader.Margins[0], m_cupsHeader.Margins[1]);
    BUG ("DEBUG: HPFAX - ManualFeed = %d\n", m_cupsHeader.ManualFeed);
    BUG ("DEBUG: HPFAX - MediaPosition = %d\n", m_cupsHeader.MediaPosition);
    BUG ("DEBUG: HPFAX - MediaWeight = %d\n", m_cupsHeader.MediaWeight);
    BUG ("DEBUG: HPFAX - MirrorPrint = %d\n", m_cupsHeader.MirrorPrint);
    BUG ("DEBUG: HPFAX - NegativePrint = %d\n", m_cupsHeader.NegativePrint);
    BUG ("DEBUG: HPFAX - NumCopies = %d\n", m_cupsHeader.NumCopies);
    BUG ("DEBUG: HPFAX - Orientation = %d\n", m_cupsHeader.Orientation);
    BUG ("DEBUG: HPFAX - OutputFaceUp = %d\n", m_cupsHeader.OutputFaceUp);
    BUG ("DEBUG: HPFAX - PageSize = [ %d %d ]\n", m_cupsHeader.PageSize[0], m_cupsHeader.PageSize[1]);
    BUG ("DEBUG: HPFAX - Separations = %d\n", m_cupsHeader.Separations);
    BUG ("DEBUG: HPFAX - TraySwitch = %d\n", m_cupsHeader.TraySwitch);
    BUG ("DEBUG: HPFAX - Tumble = %d\n", m_cupsHeader.Tumble);
    BUG ("DEBUG: HPFAX - cupsWidth = %d\n", m_cupsHeader.cupsWidth);
    BUG ("DEBUG: HPFAX - cupsHeight = %d\n", m_cupsHeader.cupsHeight);
    BUG ("DEBUG: HPFAX - cupsMediaType = %d\n", m_cupsHeader.cupsMediaType);
    BUG ("DEBUG: HPFAX - cupsRowStep = %d\n", m_cupsHeader.cupsRowStep);
    BUG ("DEBUG: HPFAX - cupsBitsPerColor = %d\n", m_cupsHeader.cupsBitsPerColor);
    BUG ("DEBUG: HPFAX - cupsBitsPerPixel = %d\n", m_cupsHeader.cupsBitsPerPixel);
    BUG ("DEBUG: HPFAX - cupsBytesPerLine = %d\n", m_cupsHeader.cupsBytesPerLine);
    BUG ("DEBUG: HPFAX - cupsColorOrder = %d\n", m_cupsHeader.cupsColorOrder);
    BUG ("DEBUG: HPFAX - cupsColorSpace = %d\n", m_cupsHeader.cupsColorSpace);
    BUG ("DEBUG: HPFAX - cupsCompression = %d\n", m_cupsHeader.cupsCompression);
    BUG ("DEBUG: HPFAX - cupsPageSizeName = %s\n", m_cupsHeader.cupsPageSizeName);
}

int GetPageSizeFromString(char *string)
{
   int iPageSize = atoi(string);
   if(iPageSize == 0)
   {
      if(strcmp(string,"Letter") ==0){
         iPageSize = 1;
      }
      else if(strcmp(string,"A4") ==0){
         iPageSize = 2;
      }
      else if(strcmp(string,"Legal") ==0){
         iPageSize = 3;
      }
      else{
         DBG("hpcupsfax: GetPageSizeFromString:Default Page Size is taken,ensure it is ok.\n");
         iPageSize = 1;
      }
   }
   DBG("hpcupsfax: GetPageSizeFromString: PageSize = %d\n",iPageSize);
   return iPageSize;
}     

int  ProcessRasterData (cups_raster_t *cups_raster, int fdFax)
{
    int                status = 0;
    unsigned int                i;
    int                widthMMR;
    int                iInputBufSize;
    BYTE               *pThisScanLine;
    LPBYTE             pbOutputBuf = NULL;
    LPBYTE             pInputBuf = NULL;
    IP_XFORM_SPEC      xForm[3];
    IP_IMAGE_TRAITS    traits;
    IP_HANDLE          hJob;

    BYTE                *p;
    cups_page_header2_t    cups_header;
    int                    iPageNum = 0;
    int                    color_mode;
    char                   *pDev;
    unsigned    int     uiPageNum = 0;

    pDev = getenv ("DEVICE_URI");

    while (cupsRasterReadHeader2 (cups_raster, &cups_header))
    {
        iPageNum++;
        if (iPageNum == 1)
        {
            PrintCupsHeader (cups_header);
            color_mode = (cups_header.cupsRowStep == 0) ? HPLIPFAX_MONO : HPLIPFAX_COLOR;
            if (color_mode == HPLIPFAX_COLOR)
            {
                fax_encoding = RASTER_JPEG;
            }
            memset (szFileHeader, 0, sizeof (szFileHeader));
            memcpy (szFileHeader, "hplip_g3", 8);
            p = szFileHeader + 8;
            *p++ = 1;                                    // Version Number
            HPLIPPUTINT32 (p, 0); p += 4;                // Total number of pages in this job
            HPLIPPUTINT16 (p, cups_header.HWResolution[0]); p += 2;
            HPLIPPUTINT16 (p, cups_header.HWResolution[1]); p += 2;
            BUG("ATOI Value  = %d",atoi (cups_header.cupsPageSizeName));
            *p++ = GetPageSizeFromString(cups_header.cupsPageSizeName);  // Output paper size
            *p++ = atoi (cups_header.OutputType);        // Output quality
            *p++ = fax_encoding;                         // MH, MMR or JPEG
            p += 4;                                      // Reserved 1
            p += 4;                                      // Reserved 2
            write (fdFax, szFileHeader, (p - szFileHeader));
        }

        widthMMR = (((cups_header.cupsWidth + 7) >> 3)) << 3;
        
/*
 *      Devices in the HPFax2 category require fixed width of 2528 pixels.
 *      Example: LaserJet 2727 MFP
 */
       
        if (strcmp (device_name, "HPFax4") ==0)
        {
            widthMMR = 1728;                      
        }
        else if (!strcmp (device_name, "HPFax2"))
        {
            widthMMR = 2528;
        }              
        iInputBufSize = widthMMR * cups_header.cupsHeight;

        pInputBuf = (LPBYTE) malloc (iInputBufSize);
        if (pInputBuf == NULL)
        {
            BUG ("Unable to allocate pInputBuf, size = %d\n", iInputBufSize);
            goto BUGOUT;
        }
        memset (pInputBuf, 0xFF, iInputBufSize);

        pThisScanLine = new BYTE[cups_header.cupsBytesPerLine];
        if (pThisScanLine == NULL)
        {
            BUG ("Unable to allocate pThisScanLine, size = %d\n", cups_header.cupsBytesPerLine);
            goto BUGOUT;
        }

        for (i = 0; i < cups_header.cupsHeight; i++)
        {
            cupsRasterReadPixels (cups_raster, pThisScanLine, cups_header.cupsBytesPerLine);
            RGB2Gray (pThisScanLine, cups_header.cupsWidth, pInputBuf + i * widthMMR);
        }

        WORD         wResult;
        DWORD        dwInputAvail;
        DWORD        dwInputUsed;
        DWORD        dwInputNextPos;
        DWORD        dwOutputAvail;
        DWORD        dwOutputUsed;
        DWORD        dwOutputThisPos;
        pbOutputBuf = (LPBYTE) malloc (iInputBufSize);
        if (pbOutputBuf == NULL)
        {
            BUG ("unable to allocate pbOutputBuf,  buffer size = %d\n", iInputBufSize);
            goto BUGOUT;
        }
        memset (pbOutputBuf, 0xFF, iInputBufSize);

        memset (xForm, 0, sizeof (xForm));

        if (color_mode == HPLIPFAX_MONO)
        {
            i = 0;
            xForm[i].eXform = X_GRAY_2_BI;

            // 0   - Error diffusion
            // >0  - Threshold value

            xForm[i].aXformInfo[IP_GRAY_2_BI_THRESHOLD].dword = 127;
            i++;

            xForm[i].eXform = X_FAX_ENCODE;
            if (fax_encoding== RASTER_MMR)
            {
                xForm[i].aXformInfo[IP_FAX_FORMAT].dword = IP_FAX_MMR;
            }
            else
            {
                xForm[i].aXformInfo[IP_FAX_FORMAT].dword = IP_FAX_MH;
            }
 /*         0 = EOLs are in data as usual; */
 /*         1 = no EOLs in data. */
            xForm[i].aXformInfo[IP_FAX_NO_EOLS].dword = 0;
            xForm[i].pXform = NULL;
            xForm[i].pfReadPeek = NULL;
            xForm[i].pfWritePeek = NULL;
            i++;

            wResult = ipOpen (i, xForm, 0, &hJob);
            traits.iComponentsPerPixel = 1;
            traits.iBitsPerPixel = 8;
        }
        else
        {
            xForm[0].eXform = X_CNV_COLOR_SPACE;
            xForm[0].aXformInfo[IP_CNV_COLOR_SPACE_WHICH_CNV].dword = IP_CNV_SRGB_TO_YCC;
            xForm[1].eXform = X_CNV_COLOR_SPACE;
            xForm[1].aXformInfo[IP_CNV_COLOR_SPACE_WHICH_CNV].dword = IP_CNV_YCC_TO_CIELAB;
            xForm[0].eXform = X_JPG_ENCODE;
            xForm[0].aXformInfo[IP_JPG_ENCODE_FOR_COLOR_FAX].dword = 1;
            wResult = ipOpen (1, xForm, 0, &hJob);
            traits.iComponentsPerPixel = 3;
            traits.iBitsPerPixel = 8;
        }

        if (wResult != IP_DONE)
        {
            BUG ("ipOpen failed: wResult = %x\n", wResult);
            goto BUGOUT;
        }
        traits.iPixelsPerRow = widthMMR;
        traits.lHorizDPI     = cups_header.HWResolution[0];
        traits.lVertDPI      = cups_header.HWResolution[1];
        traits.lNumRows      = cups_header.cupsHeight;
        traits.iNumPages     = 1;
        traits.iPageNum      = 1;

        wResult = ipSetDefaultInputTraits (hJob, &traits);
        if (wResult != IP_DONE)
        {
            BUG ("ipSetDefaultInputTraits failed: wResult = %x\n", wResult);
            wResult = ipClose (hJob);
            goto BUGOUT;
        }
        dwInputAvail = iInputBufSize;
        dwOutputAvail = dwInputAvail;

        wResult = ipConvert (hJob, dwInputAvail, pInputBuf, &dwInputUsed,
                             &dwInputNextPos, dwOutputAvail, pbOutputBuf,
                             &dwOutputUsed, &dwOutputThisPos);

        if (wResult == IP_FATAL_ERROR)
        {
            BUG ("ipConvert failed, wResult = %d\n", wResult);
            goto BUGOUT;
        }
        if (iLogLevel > 0)
        {
            BUG ("dwInputAvail = %d dwInputUsed = %d dwOutputUsed = %d\n",
                 dwInputAvail, dwInputUsed, dwOutputUsed);
        }
        wResult = ipClose (hJob);
        hJob = 0;

        uiPageNum++;

        p = szPageHeader;
        HPLIPPUTINT32 (p, uiPageNum); p += 4;                // Current page number
        HPLIPPUTINT32 (p, widthMMR); p += 4;                // Num of pixels per row
        HPLIPPUTINT32 (p, cups_header.cupsHeight); p += 4;    // Num of rows in this page
        HPLIPPUTINT32 (p, dwOutputUsed); p += 4;            // Size in bytes of encoded data
        HPLIPPUTINT32 (p, 0); p += 4;                        // Thumbnail data size
        HPLIPPUTINT32 (p, 0); p += 4;                        // Reserved for future use
        write (fdFax, szPageHeader, (p - szPageHeader));
        write (fdFax, pbOutputBuf, dwOutputUsed);
/*
 *      Write the thumbnail data here
 */

        // Send this to fax handler

        free (pbOutputBuf);
        free (pInputBuf);
        free (pThisScanLine);
        pbOutputBuf = NULL;
        pInputBuf = NULL;
        pThisScanLine = NULL;

    } /* end while (1) */

    lseek (fdFax, 9, SEEK_SET);
    HPLIPPUTINT32 ((szFileHeader + 9), uiPageNum);
    write (fdFax, szFileHeader + 9, 4);

BUGOUT:
    if (pbOutputBuf)
    {
        free (pbOutputBuf);
    }

    if (pInputBuf)
    {
        free (pInputBuf);
    }

    return status;
}

/* 
 * Return Zero if all the below are successful
 * Reading from stdin into a temp file
 * Getting the final file with HPLIP file and page headers
 */
int ProcessTiffData(int fromFD, int toFD, char* user_name)
{
    BYTE      *p;
    int       fdTiff;
    BYTE      pTmp[4096];
    struct timeval tv;
    fd_set fds;
    int page_length;
    unsigned int page_counter;
    unsigned int current_ifd_start;
    unsigned int *ifd_offset;
    unsigned int next_ifd_offset;
    unsigned short ifd_count;
    bool big_endian = false;
    int i, ret, len;
    BYTE   szTiffHeader[TIFF_HDR_SIZE];
    int bytes_written = 0;
    int ret_status = 0;
    int bytes_read = 0;
    char hpTiffFileName[MAX_FILE_PATH_LEN];
    long input_file_size = 0;
    FILE* pFilePtrFax;
    snprintf(hpTiffFileName,sizeof(hpTiffFileName), "%s/hp_%s_fax_tiffXXXXXX",CUPS_TMP_DIR,user_name);


//    fdTiff = mkstemp (hpTiffFileName);
    fdTiff = createTempFile(hpTiffFileName, &pFilePtrFax);
    if (fdTiff < 0)
    {
        BUG("ERROR: Unable to open Fax output file - %s for writing\n", hpTiffFileName);
        return 1;
    }

    memset (szFileHeader, 0, sizeof (szFileHeader));
    memcpy (szFileHeader, "hplip_g3", 8);
    p = szFileHeader + 8;
    *p++ = 1;                                    // Version Number
    HPLIPPUTINT32 (p, 0); p += 4;                // Total number of pages in this job
    HPLIPPUTINT16 (p, 0); p += 2;  //HWResolution[0]
    HPLIPPUTINT16 (p, 0); p += 2;  //HWResolution[0]
    *p++ = atoi ("0");  // Output paper size (cupsPageSizeName)
    *p++ = atoi ("0");        // Output quality (cups OutputType)
    *p++ = fax_encoding;                         // MH, MMR or JPEG, TIFF
    p += 4;                                      // Reserved 1
    p += 4;                                      // Reserved 2
    write (toFD, szFileHeader, (p - szFileHeader));

    FD_ZERO(&fds);
    FD_SET(fromFD, &fds);
    tv.tv_sec = 0;
    tv.tv_usec = 100 * 1000; //100 ms
    i = 0;

    
    len = read (fromFD, pTmp, 4096);
    if (len > 0) {
       DBG("hpcupsfax: read %d bytes from stdin", len);
       write (fdTiff, pTmp, len);
       bytes_written += len;
    } else {
           DBG("hpcupsfax: No data was available...");
    }

    while (i++ < 10)
    {
        memset (pTmp, 0, 4096);
        ret = select(fromFD+1, &fds, NULL, NULL, &tv);
        if (ret < 0) {
            DBG("hpcupsfax: Timed out, Continue...");
            continue;
        }

        if (FD_ISSET(fromFD, &fds)) {
           DBG("hpcupsfax: Data is available");
           while(1) {
              memset (pTmp, 0, 4096);
              len = read (fromFD, pTmp, 4096);
              DBG("hpcupsfax: read %d bytes from stdin", len);
              if (len <= 0) {
                  DBG("hpcupsfax: No data was available, Continue...");
                  break; //break from inner while()
              }
              write (fdTiff, pTmp, len);
              bytes_written += len;
           }
        }
    }
    DBG("hpcupsfax: total bytes written to fdTiff is  %d ", bytes_written);
    input_file_size = bytes_written;

    ret = lseek (fdTiff, 0, SEEK_SET);
    memset (szTiffHeader, 0, sizeof (TIFF_HDR_SIZE));
    ret = read (fdTiff, szTiffHeader, 8);
    DBG("hpcupsfax: read %d bytes from fdTiff", ret);
    ifd_offset = (unsigned int *) &(szTiffHeader[4]);
    if (szTiffHeader[0] == 'M') {
       DBG("hpcupsfax: it is big endian");
       big_endian = true;
       *ifd_offset = ntohl(*ifd_offset); 
    }
    DBG("hpcupsfax: ifd_offset is %d", *ifd_offset);
 
    current_ifd_start = 0;
    page_counter = 0;
    bytes_written = 0;
//WHILE
    while(1) {
        // Note down the number of tags 
        ret = lseek (fdTiff, *ifd_offset, SEEK_SET);
        ret = read (fdTiff, &ifd_count, 2);
        if (big_endian) {
            ifd_count = ntohs(ifd_count);
        }
        DBG("hpcupsfax: read %d bytes from fdTiff; ifd count is %d", ret, ifd_count);

        // Read the end of IFD to check if there is another IFD following (for e.g., next page or image)
        ret = lseek (fdTiff, (*ifd_offset+2+((ifd_count)*12)), SEEK_SET);
        ret = read (fdTiff, &next_ifd_offset, 4);
        if (big_endian) {
            next_ifd_offset = ntohs(next_ifd_offset);
        }
        DBG("hpcupsfax: read %d bytes from fdTiff at %d; next ifd offset is %d", 
                                  ret, (*ifd_offset+2+((ifd_count)*12)), next_ifd_offset);

        // Increment the page counter
        page_counter = page_counter + 1;
        DBG("hpcupsfax: Current page_counter is %d", page_counter);

       // Write Tiff data for the current page (IFD)
       page_length = next_ifd_offset-current_ifd_start;
       DBG("hpcupsfax: page_length is %d ", page_length);
       if (page_length <= 0) {
           len = lseek (fdTiff, 0, SEEK_END);
           page_length = len - current_ifd_start;
       }
       DBG("hpcupsfax: current_ifd_start=%d next_ifd_offset=%d total bytes are %d", current_ifd_start, next_ifd_offset, page_length);

       // Write HPLIP page header
       p = szPageHeader;
       HPLIPPUTINT32 (p, page_counter); p += 4;     // Current page number
       HPLIPPUTINT32 (p, 0); p += 4;                // Num of pixels per row - It is ImageWidth for Tiff
       HPLIPPUTINT32 (p, 0); p += 4;                // Num of rows in this page - It is ImageLength for Tiff
       HPLIPPUTINT32 (p, page_length); p += 4;      // Size in bytes of encoded data
       HPLIPPUTINT32 (p, 0); p += 4;                // Thumbnail data size
       HPLIPPUTINT32 (p, 0); p += 4;                // Reserved for future use
       ret = write (toFD, szPageHeader, (p - szPageHeader));

       ret = lseek (fdTiff, current_ifd_start, SEEK_SET);
       while (page_length > 0) {
            if (page_length < 4096) {
                len = page_length;
            } else {
                len = 4096;
            } 
            bytes_read = read (fdTiff, pTmp, len);
            ret = write (toFD, pTmp, bytes_read);
            page_length = page_length - ret;
            bytes_written += ret;
       }

       // If there is no next IFD, break from the loop. Else, continue...
       if (bytes_written > input_file_size) {
            BUG("Error!! Bytes written to toFD is becoming more than input file size.");
            ret_status = -1;
            break; // while(1) for page counting
       }

       if (next_ifd_offset == 0) {
            break; // while(1) for page counting
       }
       current_ifd_start = *ifd_offset = next_ifd_offset;
    } // while(1) for page counting

    lseek (toFD, 9, SEEK_SET);
    HPLIPPUTINT32 ((szFileHeader + 9), page_counter);
    write (toFD, szFileHeader + 9, 4);

	if (!(iLogLevel & SAVE_PCL_FILE))
	{
         unlink(hpTiffFileName);
	}
    return ret_status;
}


int send_data_to_stdout(int fromFD)
{
    int     iSize, i;
    int     len;
    BYTE    *pTmp = NULL;

    iSize = lseek (fromFD, 0, SEEK_END);
    lseek (fromFD, 0, SEEK_SET);

    DBG("hpcupsfax: lseek(fromFD) returned %d", iSize);
    if (iSize > 0)
    {
        pTmp = (BYTE *) malloc (iSize);
    }
    if (pTmp == NULL)
    {
        iSize = 1024;
        pTmp = (BYTE *) malloc  (iSize);
        if (pTmp == NULL)
        {
          return 1;
        }
    }

    while ((len = read (fromFD, pTmp, iSize)) > 0)
    {
        write (STDOUT_FILENO, pTmp, len);
    }
    free (pTmp);

    return 0;
}

int main (int argc, char **argv)
{
    int                 status = 0;
    int                 fd = 0;
    int                 fdFax = -1;
    int i = 0;
    FILE                *pFilePtrFax;
    cups_raster_t       *cups_raster;
    ppd_file_t          *ppd;
    ppd_attr_t          *attr;
    ppd_attr_t          *compression_attr;

    /*********** PROLOGUE ***********/

    GetLogLevel();
    openlog("hpcupsfax", LOG_PID,  LOG_DAEMON);

    if (argc < 6 || argc > 7)
    {
        BUG ("ERROR: %s job-id user title copies options [file]\n", *argv);
        return 1;
    }

    if (argc == 7)
    {
        if ((fd = open (argv[6], O_RDONLY)) == -1)
        {
            BUG ("ERROR: Unable to open raster file %s\n", argv[6]);
            return 1;
        }
    }

    while (argv[i] != NULL) {
         DBG("hpcupsfax: argv[%d] = %s\n", i, argv[i]);
         i++;
    }

    snprintf(hpFileName,sizeof(hpFileName),"%s/hp_%s_fax_Log_XXXXXX",CUPS_TMP_DIR, argv[2]);

//    fdFax = mkstemp (hpFileName);
    fdFax = createTempFile(hpFileName, &pFilePtrFax);
    if (fdFax < 0)
    {
         BUG ("ERROR: Unable to open Fax output file - %s for writing\n", hpFileName);
         return 1;
    }
    else
        chmod(hpFileName, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP );

    /*********** MAIN ***********/

    ppd = ppdOpenFile (getenv ("PPD"));
    if (ppd == NULL)
    {
        BUG ("ERROR: Unable to open ppd file %s\n", getenv ("PPD"));
        return 1;
    }
    if ((attr = ppdFindAttr (ppd, "cupsModelName", NULL)) == NULL ||
        (attr && attr->value == NULL))
    {
        ppdClose (ppd);
        BUG ("ERROR: Required cupsModelName is missing in ppd file\n");
        return 1;
    }

    memset (device_name, 0, sizeof (device_name));
    strncpy (device_name, attr->value, 15);

    if ((attr = ppdFindAttr (ppd, "DefaultEncoding", NULL)) == NULL ||
        (attr && attr->value == NULL))
    {
        ppdClose (ppd);
        BUG ("ERROR: Required DefaultEncoding is missing in ppd file\n");
        return 1;
    }     
    if (strstr(argv[5],"Encoding=MMR"))
    {
       fax_encoding = RASTER_MMR;
    }   
    else if(strstr(argv[5],"Encoding=MH"))
    {
       fax_encoding = RASTER_MH; 
    }       
    else if (strstr(argv[5],"Encoding=Auto"))
    { 
       BUG ("WARNING: AUTO is selected for Fax Encoding! Ensure this type is correct for the device");                 
       fax_encoding = RASTER_AUTO;
    }    
    else if (strstr(argv[5],"Encoding=TIFF"))
    {
       fax_encoding = RASTER_TIFF;
    }
    else
    {
       if(strcmp(attr->value,"MH") == 0) {
          fax_encoding = RASTER_MH;
       } else if(strcmp(attr->value,"MMR") == 0){
          fax_encoding = RASTER_MMR;
       }else if(strcmp(attr->value,"TIFF") ==0){
          fax_encoding = RASTER_TIFF;               
       }else if(strcmp(attr->value,"Auto") ==0){
          BUG ("WARNING: AUTO is selected for Fax Encoding! Ensure this type is correct for the device");                 
          fax_encoding = RASTER_AUTO;
       }
    }
    if (fax_encoding < 0) {
        BUG ("ERROR: Required DefaultEncoding is invalid in ppd file\n");
        return 1;
    }
    DBG("hpcupsfax: main: fax_encoding = %d \n", fax_encoding);
    ppdClose (ppd);

    if (fax_encoding == RASTER_TIFF)
    {
        status = ProcessTiffData(fd, fdFax, argv[2]);
    } else {
       cups_raster = cupsRasterOpen (fd, CUPS_RASTER_READ);
       if (cups_raster == NULL)
       {
           status = 1;
           BUG ("cupsRasterOpen failed, fd = %d\n", fd);
           goto EPILOGUE;
       }

       status = ProcessRasterData (cups_raster, fdFax);

       cupsRasterClose (cups_raster);
    }

    DBG("hpcupsfax: Send data to stdout \n");
    status = send_data_to_stdout(fdFax);

    /*********** EPILOGUE ***********/
EPILOGUE:
    if (fd != 0)
    {
        close (fd);
    }

    if (fdFax > 0)
    {
        close (fdFax);
	if (!(iLogLevel & SAVE_PCL_FILE))
	{
             //Retain the intermediate file only if it is needed for debugging purpose.
             unlink(hpFileName);
	}
    }

    return status;
}

