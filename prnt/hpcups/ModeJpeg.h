/*****************************************************************************\
  ModeJpeg.h : Jpeg compressor definitions

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

#ifndef MODE_JPEG_H
#define MODE_JPEG_H

#define HAVE_UNSIGNED_SHORT
#include "Processor.h"
#include "Compressor.h"
extern "C"
{
#include "jpeglib.h"
}

class ModeJpeg : public Compressor
{
public:
    ModeJpeg (unsigned int RasterSize);
    ~ModeJpeg();
    bool Process (RASTERDATA *input);
    bool NextOutputRaster (RASTERDATA& next_raster);
    void Flush ();
    DRIVER_ERROR    Init(int max_file_size, int page_height);
    DRIVER_ERROR    Init(int color_mode, int page_height, COMPRESS_MODE *eCompressMode, QTableInfo *qTableInfo);
    void            StoreJpegData(BYTE *buffer, int size);
private:
    void    compress();
    void    jpegCompressForQuickConnect();
    void    jpegCompressForJetReady();
    void    taosCompressForJetReady();
    void    rgbToGray(BYTE *rgbData, int iNumBytes);
    int     m_iRowWidth;
    int     m_iRowNumber;
    int     m_iBandHeight;
    int     m_iColorMode;
    int     m_uiGrayscaleOffset;
    COMPRESSOR_TYPE    m_eCompressor;
    unsigned int       m_max_file_size;
    BYTE               *m_pbyInputBuffer;
    QTableInfo         *m_pQTableInfo;
    void               *m_hHPLibHandle;
};

#endif // MODE_JPEG_H

