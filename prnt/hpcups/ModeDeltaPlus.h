/*****************************************************************************\
  DeltaPlus.h : Interface for the DeltaPlus class

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

#ifndef MODE_DELTAPLUS_H
#define MODE_DELTAPLUS_H

#include "CommonDefinitions.h"
#include "Processor.h"
#include "Compressor.h"

#define INDY_STRIP_HEIGHT 128

class ModeDeltaPlus : public Compressor
{
public:
    ModeDeltaPlus(unsigned int RasterSize);
    virtual ~ModeDeltaPlus();
    DRIVER_ERROR Init();
    bool Process(RASTERDATA *input);
    bool NextOutputRaster(RASTERDATA &next_raster);
    bool IsCompressed()
    {
        return m_bCompressed;
    }
	BYTE GetFRatio()
	{
	    return (BYTE) (m_fRatio + 0.5);
	}
	long GetCurrentRasterRow()
	{
	    return m_lPrinterRasterRow;
	}
	long GetCurrentBlockHeight()
	{
	    return m_lCurrBlockHeight;
	}
	void NewPage()
	{
        m_lPrinterRasterRow = 0;
	    m_bLastBand = false;
	}
	void SetLastBand()
	{
	    m_bLastBand = true;
	}

private:
    bool  compress(BYTE *outmem, uint32_t *outlen, const BYTE *inmem,
                   const uint32_t row_width, const uint32_t inheight,
                   uint32_t horz_ht_dist);

    BYTE  *encode_header(BYTE *outptr, const BYTE *pastoutmem, uint32_t isrun,
                         uint32_t location, uint32_t seedrow_count, uint32_t run_count,
                         const BYTE new_color);

    BYTE                *pbyInputImageBuffer;

    long                m_lCurrCDRasterRow;         // Current  raster index. in PrintNextBand
    long                m_lCurrBlockHeight;
    long                m_lPrinterRasterRow;        // Current printer raster row.

    uint32_t            m_compressedsize;
    bool                m_bCompressed;
    float               m_fRatio;
    BYTE                *pbySeedRow;
	bool                m_bLastBand;
}; // ModeDeltaPlus

#endif // MODE_DELTAPLUS_H

