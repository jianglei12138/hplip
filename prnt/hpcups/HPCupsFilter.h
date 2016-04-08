/*****************************************************************************\
  HPCupsFilter.h : Interface for HPCupsFilter class

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

#ifndef HP_CUPSFILTER_H
#define HP_CUPSFILTER_H

#include "CommonDefinitions.h"
#include "SystemServices.h"
#include "Compressor.h"
#include "Job.h"

#include "dbuscomm.h"

#define		DBITMAPFILEHEADER		14
#define		DBITMAPINFOHEADER		40

enum eRasterType
{
    BLACK_RASTER,
    COLOR_RASTER
};

typedef struct
{
    short    bfType;
    int      bfSize;
    short    bfReserved1;
    short    bfReserved2;
    int      bfOffBits;
} BITMAPFILEHEADER;

typedef struct
{
    int     biSize;
    int     biWidth;
    int     biHeight;
    short   biPlanes;
    short   biBitCount;
    int     biCompression;
    int     biSizeImage;
    int     biXPelsPerMeter;
    int     biYPelsPerMeter;
    int     biClrUsed;
    int     biClrImportant;
} BITMAPINFOHEADER;

class HPCupsFilter
{
public:

    HPCupsFilter();
    ~HPCupsFilter();    
    int     StartPrintJob(int  argc, char *argv[]);
    void    CancelJob();
    void    RemoveStatus();

    void WriteBMPHeader (FILE *fp, int width, int height, eRasterType raster_type);
    void WriteBMPRaster (FILE *fp, BYTE *raster, int width, eRasterType raster_type);

private:

    BYTE            *m_pPrinterBuffer;        
    Job            m_Job;
    SystemServices         *m_pSys;
    int             child_pid;
    char            **m_argv;
    JobAttributes   m_JA;
    ppd_file_t      *m_ppd;
    DBusCommunicator m_DBusComm;
	
private:
    void            closeFilter();
    void            cleanup();
    //void            getLogLevel();
    DRIVER_ERROR    startPage (cups_page_header2_t *header);
    int             processRasterData(cups_raster_t *cups_raster);
    void            extractBlackPixels(cups_page_header2_t *cups_header,
                                       BYTE *kRaster, BYTE *rgbRaster);
    void            printCupsHeaderInfo(cups_page_header2_t *header);
    bool            isBlankRaster(BYTE *input_raster, cups_page_header2_t *header);
	int             m_iLogLevel;

    void CreateBMPHeader(int width, int height, int planes, int bpp);
    void WriteCBMPHeader (FILE *fp, int width, int height);
    void WriteKBMPHeader (FILE *fp, int width, int height);
    void WriteCBMPRaster (FILE *fp, BYTE *rgb_raster, int width);
    void WriteKBMPRaster (FILE *fp, BYTE *k_raster, int width);
    int    adj_c_width;
    int    adj_k_width;
    BYTE   *black_raster;
    BYTE   *color_raster;
    BITMAPFILEHEADER    bmfh;
    BITMAPINFOHEADER    bmih;

#ifdef UNITTESTING
    friend class TestHPCupsFilter;
#endif

};

#endif  // HP_CUPSFILTER_H

