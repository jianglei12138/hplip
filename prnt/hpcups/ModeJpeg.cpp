/*****************************************************************************\
  ModeJpeg.cpp : Jpeg compressor implementation

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

#include "CommonDefinitions.h"
#include <setjmp.h>
#include "ModeJpeg.h"
#include <dlfcn.h>
#include "Utils.h"
#include "utils.h"

#define MAX_JPEG_FILE_SIZE 2097152    // 2 Mgabytes

extern "C"
{
    int (*HPLJJRCompress) (BYTE        *pCompressedData,
                           uint32_t    *pCompressedDataLen,
                           BYTE        *InputData,
                           const uint32_t    uiLogicalImageWidth,
                           const uint32_t    uiLogicalImageHeight);
}

ModeJpeg::ModeJpeg(unsigned int raster_width) : Compressor (raster_width, false)
{
    m_eCompressor       = COMPRESSOR_JPEG_QUICKCONNECT;
    m_iColorMode        = 0;
    m_max_file_size     = MAX_JPEG_FILE_SIZE;;
    compressedsize      = 0;
    m_iRowWidth         = (int) raster_width * 3;
    m_iRowNumber        = 0;
    m_iBandHeight       = 0;
    m_uiGrayscaleOffset = 0;
    m_pbyInputBuffer    = NULL;
    m_hHPLibHandle      = NULL;
    compressBuf         = NULL;

//  Don't need originalKData buffer allocate by Compressor, delete it
    if (originalKData)
    {
        delete [] originalKData;
        originalKData = NULL;
    }
}

DRIVER_ERROR ModeJpeg::Init(int max_file_size, int page_height)
{
    int    buffer_size = m_iRowWidth * (page_height + 2);
    m_pbyInputBuffer = new BYTE[buffer_size];
    if (m_pbyInputBuffer == NULL)
    {
        return ALLOCMEM_ERROR;
    }
    if (max_file_size > 0)
    {
        m_max_file_size = (unsigned int) max_file_size;
    }
    compressBuf = new BYTE[m_max_file_size];
    if (compressBuf == NULL)
    {
        return ALLOCMEM_ERROR;
    }
    m_iBandHeight = page_height;
    return NO_ERROR;
}

DRIVER_ERROR ModeJpeg::Init(int color_mode, int band_height, COMPRESS_MODE *eCompressMode, QTableInfo *qtable_info)
{
    DRIVER_ERROR    err;
    m_iColorMode = color_mode;
    m_pQTableInfo = qtable_info;
    int    buf_size = band_height * m_iRowWidth;
    err = Init(buf_size, band_height);
    if (err != NO_ERROR)
    {
        return err;
    }

//  If plugin is not available or the plugin does not contain Taos compressor, stay with JPEG compression
    m_eCompressor = COMPRESSOR_JPEG_JETREADY;
    if (*eCompressMode == COMPRESS_MODE_LJ)
    {
        m_hHPLibHandle = load_plugin_library(UTILS_PRINT_PLUGIN_LIBRARY, PRNT_PLUGIN_LJ);
        if (m_hHPLibHandle)
        {
            dlerror ();
            *(void **) (&HPLJJRCompress) = get_library_symbol(m_hHPLibHandle, "HPJetReadyCompress");
            if (HPLJJRCompress == NULL)
            {
                unload_library(m_hHPLibHandle);
                m_hHPLibHandle = NULL;
                *eCompressMode = COMPRESS_MODE_JPEG;
            }
            else
            {
                m_eCompressor = COMPRESSOR_TAOS;
            }
        }
    }
    return err;
}

ModeJpeg::~ModeJpeg()
{
    unload_library(m_hHPLibHandle);
    if (m_pbyInputBuffer)
    {
        delete [] m_pbyInputBuffer;
    }
}

void ModeJpeg::Flush()
{
    if (m_iRowNumber > 0)
    {
        compress();
    }
}

#define RGBTOGRAY(rgb) (BYTE) ((rgb[0] * 30 + rgb[1] * 59 + rgb[2] * 11) / 100)
void ModeJpeg::rgbToGray(BYTE *rgbData, int iNumBytes)
{
    if (m_eCompressor == COMPRESSOR_TAOS)
    {
        BYTE    *p = m_pbyInputBuffer + (m_iRowNumber * m_iRowWidth);
        for (int i = 0; i < iNumBytes; i += 3, rgbData += 3)
        {
            *p++ = RGBTOGRAY(rgbData);
            *p++ = 0;
            *p++ - 0;
        }
        return;
    }
    BYTE    *p = m_pbyInputBuffer + (m_iRowNumber * m_iRowWidth / 3);
    for (int i = 0; i < iNumBytes; i += 3, rgbData += 3)
    {
        *p++ = 255 - RGBTOGRAY(rgbData);
    }
}

bool ModeJpeg::Process(RASTERDATA *input)
{
    if (input == NULL)
    {
        return false;
    }
    if (input->rasterdata[COLORTYPE_COLOR])
    {
        if (m_iColorMode == 0)
        {
            memcpy(m_pbyInputBuffer + (m_iRowNumber * m_iRowWidth), input->rasterdata[COLORTYPE_COLOR],
                   input->rastersize[COLORTYPE_COLOR]);
        }
        else
        {
            rgbToGray(input->rasterdata[COLORTYPE_COLOR], input->rastersize[COLORTYPE_COLOR]);
        }
    }
    m_iRowNumber++;
    if (m_iRowNumber == m_iBandHeight)
    {
        compress();
        return true;
    }
    return false;
}

bool ModeJpeg::NextOutputRaster (RASTERDATA &next_raster)
{
    if (iRastersReady == 0)
    {
        return false;
    }
    if (myplane == COLORTYPE_COLOR && compressedsize != 0)
    {
        next_raster.rastersize[COLORTYPE_COLOR] = compressedsize - m_uiGrayscaleOffset;
        next_raster.rasterdata[COLORTYPE_COLOR] = compressBuf + m_uiGrayscaleOffset;
    }
    else
    {
        next_raster.rastersize[COLORTYPE_COLOR] = 0;
        next_raster.rasterdata[COLORTYPE_COLOR] = NULL;
    }
    next_raster.rastersize[COLORTYPE_BLACK] = 0;
    next_raster.rasterdata[COLORTYPE_BLACK] = NULL;

    iRastersReady = 0;
    m_uiGrayscaleOffset = 0;
    return true;
}

static void output_buffer_callback (JOCTET *outbuf, BYTE *buffer, int size)
{
    ModeJpeg    *pModeJpeg = (ModeJpeg *) outbuf;
    pModeJpeg->StoreJpegData (buffer, size);
}

void ModeJpeg::StoreJpegData (BYTE *buffer, int iSize)
{
    compressedsize += iSize;
    if (compressedsize < m_max_file_size)
    {
        memcpy (compressBuf + compressedsize - iSize, buffer, iSize);
    }
    else
    {
        compressedsize = m_max_file_size + 1;
    }
}

//----------------------------------------------------------------
// These are "overrides" to the JPEG library error routines
//----------------------------------------------------------------

static void HPJpeg_error (j_common_ptr cinfo)
{

}

extern "C"
{
    void jpeg_buffer_dest (j_compress_ptr cinfo, JOCTET* outbuff, void* flush_output_buffer_callback);
    void hp_rgb_ycc_setup (int iFlag);
}

void ModeJpeg::compress () 
{
    switch(m_eCompressor)
    {
        case COMPRESSOR_JPEG_QUICKCONNECT:
        {
            jpegCompressForQuickConnect();
            break;
        }
        case COMPRESSOR_JPEG_JETREADY:
        {
            jpegCompressForJetReady();
            break;
        }
        case COMPRESSOR_TAOS:
        {
            taosCompressForJetReady();
            break;
        }
    }
    m_iRowNumber = 0;
    iRastersReady = 1;
    memset(m_pbyInputBuffer, 0xFF, m_iRowWidth * m_iBandHeight);
}

void ModeJpeg::jpegCompressForQuickConnect()
{
    BYTE    *p;
    int     iQuality = 100;

/*
 *  Convert the byte buffer to jpg, if converted size is greater than 2MB, delete it and
 *  convert with a higher compression factor.
 */

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr       jerr;
    jmp_buf                     setjmp_buffer;

    bool    bRedo;

//  Use the standard RGB to YCC table rather than the modified one for JetReady

    hp_rgb_ycc_setup (0);
    do
    {
        bRedo = 0;
        compressedsize = 0;
        memset (compressBuf, 0xFF, m_max_file_size);
        p = m_pbyInputBuffer;

        cinfo.err = jpeg_std_error (&jerr);
        jerr.error_exit = HPJpeg_error;
        if (setjmp (setjmp_buffer))
        {
            jpeg_destroy_compress (&cinfo);
            return;
        }

        jpeg_create_compress (&cinfo);
        cinfo.in_color_space = JCS_RGB;
        jpeg_set_defaults (&cinfo);
        cinfo.image_width = m_iRowWidth / 3;
        cinfo.image_height = m_iRowNumber;
        cinfo.input_components = 3;
        cinfo.data_precision = 8;
        jpeg_set_quality (&cinfo, iQuality, TRUE);
        jpeg_buffer_dest (&cinfo, (JOCTET *) this, (void *) (output_buffer_callback));
        jpeg_start_compress (&cinfo, TRUE);
        JSAMPROW    pRowArray[1];
        for (int i = 0; i < m_iRowNumber; i++)
        {
            pRowArray[0] = (JSAMPROW) p;
            jpeg_write_scanlines (&cinfo, pRowArray, 1);
            p += (m_iRowWidth);
            if (compressedsize > m_max_file_size)
            {
                compressedsize = 0;
                bRedo = 1;
            }
        }
        jpeg_finish_compress (&cinfo);
        jpeg_destroy_compress (&cinfo);
        iQuality -= 10;
        if (iQuality == 0)
        {
            compressedsize = 0;
            return;
        }
    } while (bRedo);
    return;
}

void ModeJpeg::jpegCompressForJetReady()
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr       jerr;
    jmp_buf                     setjmp_buffer;


//  Use the modified Mojave CSC table
    hp_rgb_ycc_setup (1);

    compressedsize = 0;
    memset (compressBuf, 0xFF, m_max_file_size);

    cinfo.err = jpeg_std_error (&jerr);
    jerr.error_exit = HPJpeg_error;
    if (setjmp (setjmp_buffer))
    {
        jpeg_destroy_compress (&cinfo);
        return;
    }

    jpeg_create_compress (&cinfo);
    cinfo.in_color_space = (m_iColorMode == 0) ? JCS_RGB : JCS_GRAYSCALE;
    jpeg_set_defaults (&cinfo);
    cinfo.image_width = m_iRowWidth / 3;
    cinfo.image_height = m_iBandHeight;
    cinfo.input_components = (m_iColorMode == 0) ? 3 : 1;
    cinfo.data_precision = 8;

    // Create a static quant table here. 

    static unsigned int mojave_quant_table1[64] =  {
                                                    2,3,4,5,5,5,5,5,
                                                    3,6,5,8,5,8,5,8,
                                                    4,5,5,5,5,5,5,5,
                                                    5,8,5,8,5,8,5,8,
                                                    5,5,5,5,5,5,5,5,
                                                    5,8,5,8,5,8,5,8,
                                                    5,5,5,5,5,5,5,5,
                                                    5,8,5,8,5,8,5,8
                                                    };
    
    //
    // JetReady specific Q-Tables will be added here. We do the following:
    //  1. Add three Q-Tables.
    //  2. Scale the Q-Table elemets with the given scale factor.
    //  3. Check to see if any of the element in the table is greater than 255
    //     reset that elemet to 255.
    //  5. There is a specific scaling needed to be done to the first 6 
    //     elements in the matrix. This is required to achieve better
    //     compression ratio.
    //  4. Check to see if any the of the recently modified element is
    //     greater than 255, reset that to 255.
    //
    //  Please refer to sRGBLaserHostBasedSoftwareERS.doc v9.0 section 5.2.5.3.1.1
    //  for more details.
    //
    //  [NOTE] These loop needs to be further optimized.
    //
    for (int i = 0; i < 3; i++)
    {
        // Adding Q-Table.

        jpeg_add_quant_table(&cinfo, i, mojave_quant_table1,  0, FALSE );
        //
        // Scaling the Q-Table elements. 
        // Reset the element to 255, if it is greater than 255.
        //

        for(int j = 1; j < 64; j++)
        {
            cinfo.quant_tbl_ptrs[i]->quantval[j] = (UINT16)((mojave_quant_table1[j] * m_pQTableInfo->qFactor) & 0xFF);  
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
        double dTemp = mojave_quant_table1[1] * (1 + 0.25 * (m_pQTableInfo->qFactor - 1)) + 0.5;
        cinfo.quant_tbl_ptrs[i]->quantval[1] = (UINT16)dTemp & 0xFF;
        
        dTemp = mojave_quant_table1[8] * (1 + 0.25 * (m_pQTableInfo->qFactor - 1)) + 0.5;
        cinfo.quant_tbl_ptrs[i]->quantval[8] = (UINT16)dTemp & 0xFF;
       
        //
        // 4th, 5th and 6th components in the zig zag order
        //
        dTemp = mojave_quant_table1[16] * (1 + 0.50 * (m_pQTableInfo->qFactor - 1)) + 0.5;
        cinfo.quant_tbl_ptrs[i]->quantval[16] = (UINT16)dTemp & 0xFF;
        
        dTemp = mojave_quant_table1[9] * (1 + 0.50 * (m_pQTableInfo->qFactor - 1)) + 0.5;
        cinfo.quant_tbl_ptrs[i]->quantval[9]  = (UINT16)dTemp & 0xFF;
        
        dTemp = mojave_quant_table1[2] * (1 + 0.50 * (m_pQTableInfo->qFactor - 1)) + 0.5;
        cinfo.quant_tbl_ptrs[i]->quantval[2]  = (UINT16)dTemp & 0xFF;
    }   // for (i = 0; i < 3; i++)

    // Hard code to use sampling mode 4:4:4

    cinfo.comp_info[0].h_samp_factor = 1;
    cinfo.comp_info[0].v_samp_factor = 1;

    jpeg_buffer_dest (&cinfo, (JOCTET *) this, (void *) (output_buffer_callback));

    int    row_width = m_iRowWidth;
    if (m_iColorMode != 0)
    {
        row_width = m_iRowWidth / 3;
        cinfo.write_JFIF_header = FALSE;
        cinfo.write_Adobe_marker = FALSE;
        jpeg_suppress_tables(&cinfo, TRUE);
    }

    jpeg_start_compress (&cinfo, TRUE);
    JSAMPROW    pRowArray[1];
    BYTE        *pScanLine = m_pbyInputBuffer;
    int         i;
    for (i = 0; i < m_iBandHeight; i++)
    {
        pRowArray[0] = (JSAMPROW) pScanLine;
        jpeg_write_scanlines (&cinfo, pRowArray, 1);
        pScanLine += (row_width);
    }
    jpeg_finish_compress (&cinfo);

//  Read the quantization table used for this compression

    if (cinfo.quant_tbl_ptrs[0] != NULL)
    {
//        memcpy(m_pQTableInfo->qtable0, cinfo.quant_tbl_ptrs[0]->quantval, QTABLE_SIZE);
        for (i = 0; i < QTABLE_SIZE; i++)
        {
            m_pQTableInfo->qtable0[i] = cinfo.quant_tbl_ptrs[0]->quantval[i];
        }
    }
    if (cinfo.quant_tbl_ptrs[1] != NULL)
    {
//        memcpy(m_pQTableInfo->qtable1, cinfo.quant_tbl_ptrs[1]->quantval, QTABLE_SIZE);
        for (i = 0; i < QTABLE_SIZE; i++)
        {
            m_pQTableInfo->qtable1[i] = cinfo.quant_tbl_ptrs[1]->quantval[i];
        }
    }
    if (cinfo.quant_tbl_ptrs[2] != NULL)
    {
//        memcpy(m_pQTableInfo->qtable2, cinfo.quant_tbl_ptrs[2]->quantval, QTABLE_SIZE);
        for (i = 0; i < QTABLE_SIZE; i++)
        {
            m_pQTableInfo->qtable2[i] = cinfo.quant_tbl_ptrs[2]->quantval[i];
        }
    }
    jpeg_destroy_compress (&cinfo);
    if (m_iColorMode != 0)
    {
        unsigned int    l = 0;
        while (l < compressedsize)
        {
            if (compressBuf[l] == 0xFF && compressBuf[l+1] == 0xDA)
                break;
            l++;
        }
        if (l != compressedsize)
        {
            m_uiGrayscaleOffset = l + 10;
        }
    }
}

void ModeJpeg::taosCompressForJetReady()
{
    int    iRet;
    int    bufSize = m_max_file_size;
    iRet = HPLJJRCompress(compressBuf, (uint32_t *) &bufSize, m_pbyInputBuffer, m_iRowWidth / 3, m_iBandHeight);
    compressedsize = (iRet < 0) ? 0 : bufSize;
}

