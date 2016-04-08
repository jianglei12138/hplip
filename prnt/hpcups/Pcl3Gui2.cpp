/*****************************************************************************\
  Pcl3Gui2.cpp : Implementation of Pcl3Gui2 class

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
#include "Pcl3Gui2.h"
#include "ErnieFilter.h"
#include "Mode10.h"
#include "Mode9.h"
#include "PrinterCommands.h"

Pcl3Gui2::Pcl3Gui2() : Encapsulator()
{
    speed_mech_enabled = true;
    m_run_ernie_filter = true;
    crd_type = eCrd_both;
    strcpy(m_szLanguage, "PCL3GUI");
}

Pcl3Gui2::~Pcl3Gui2()
{
}

DRIVER_ERROR Pcl3Gui2::Configure(Pipeline **pipeline)
{
    Pipeline    *p = NULL;
    Pipeline    *head;
    unsigned int width;
    head = *pipeline;
    if (m_pJA->e_duplex_mode != DUPLEXMODE_NONE ||
        m_pJA->total_pages < 3) {
	    speed_mech_enabled = false;
    }

    width = m_pMA->printable_width;;
    if (m_run_ernie_filter) {
	    ErnieFilter    *pErnie;

       // Normal: threshold = (resolution) * (0.0876) - 2
       int threshold = ((m_pQA->horizontal_resolution * 876) / 10000) - 2;

       pErnie = new ErnieFilter (width, eBGRPixelData, threshold);
       p = new Pipeline (pErnie);
       if (head) {
          head->AddPhase (p);
       }
       else {
           head = p;
       }
    }

    if (crd_type != eCrd_black_only) {
        Mode10    *pMode10;
        pMode10 = new Mode10 (width * 3);
        pMode10->myplane = COLORTYPE_COLOR;

        p = new Pipeline (pMode10);
        if (head) {
            head->AddPhase (p);
        }
        else {
            head = p;
        }
    }

    width = (width + 7) / 8;

    if (width > 0 && crd_type != eCrd_color_only) {
        Mode9    *pMode9;
        // VIP black data is 1 bit here
        pMode9 = new Mode9 (width);
        pMode9->myplane = COLORTYPE_BLACK;
        p = new Pipeline (pMode9);
        if (head) {
            head->AddPhase (p);
        }
	else {
            head = p;
        }
    }
    *pipeline = head;
    return NO_ERROR;
}

DRIVER_ERROR Pcl3Gui2::StartPage(JobAttributes *pJA)
{
    DRIVER_ERROR    err = NO_ERROR;
    char            szStr[256];
    int             top_margin = 0;
    unsigned int unit_of_measure = 300;
    page_number++;

//  Under windows, pJA address may have changed, re-init here.

    m_pJA = pJA;
    m_pMA = &pJA->media_attributes;
    m_pQA = &pJA->quality_attributes;

//  Set media source, type, size and quality modes.

    sprintf(szStr, "\033&l%dH\033&l%dM\033&l%dA\033*o%dM", m_pJA->media_source, m_pQA->media_type,
            m_pMA->pcl_id, m_pQA->print_quality);

//  Now add media subtype

    int    i = strlen(szStr);
    memcpy(szStr+i, MediaSubtypeSeq, sizeof(MediaSubtypeSeq));
    i += sizeof(MediaSubtypeSeq);
    szStr[i++] = (char) (m_pQA->media_subtype & 0xFFFF) >> 8;
    szStr[i++] = (char) m_pQA->media_subtype & 0xFF;
    addToHeader((const BYTE *) szStr, i);

    if (m_pJA->e_duplex_mode != DUPLEXMODE_NONE) {
        addToHeader((const BYTE *) EnableDuplex, sizeof(EnableDuplex));
        speed_mech_enabled = false;
    }

    configureRasterData();

    if (m_pJA->color_mode != 0) {
//        GrayscaleSeq[9] = m_pJA->color_mode;
        addToHeader((const BYTE *) GrayscaleSeq, 9);
	*cur_pcl_buffer_ptr++ = (BYTE) m_pJA->color_mode;
    }

    unit_of_measure = (m_pQA->horizontal_resolution < m_pQA->actual_vertical_resolution) ? m_pQA->horizontal_resolution : m_pQA->actual_vertical_resolution;
    sprintf(szStr,"\033&u%dD", unit_of_measure);
    addToHeader((const BYTE *) szStr, strlen(szStr));

    
    sprintf(szStr,"\033*t%dR", m_pQA->actual_vertical_resolution);
    addToHeader((const BYTE *) szStr, strlen(szStr));

    // another command that helps PCLviewer
    unsigned int width = m_pMA->printable_width;
    sprintf(szStr, "\033*r%dS", width);
    addToHeader((const BYTE *) szStr, strlen(szStr));

/*
 *  Custom papersize command
 */

    if (m_pMA->pcl_id == CUSTOM_MEDIA_SIZE) {
        short   sWidth, sHeight;
        BYTE    b1, b2;
        sWidth  = static_cast<short>(m_pMA->physical_width);
        sHeight = static_cast<short>(m_pMA->physical_height);
        memcpy (szStr, "\x1B*o5W\x0E\x05\x00\x00\x00\x1B*o5W\x0E\x06\x00\x00\x00", 20);
        b1 = (BYTE) ((sWidth & 0xFF00) >> 8);
        b2 = (BYTE) (sWidth & 0xFF);
        szStr[8] = b1;
        szStr[9] = b2;
        b1 = (BYTE) ((sHeight & 0xFF00) >> 8);
        b2 = (BYTE) (sHeight & 0xFF);
        szStr[18] = b1;
        szStr[19] = b2;
        addToHeader((const BYTE *) szStr, 20);
    }

    if (m_pJA->print_borderless) {
        BYTE cBuf[4];
        
        // For SPD products
        if (m_pJA->HPSPDClass == 1)
        {
            BYTE TopOverSpraySeq[]  = {0x1b, 0x2A, 0x6F, 0x35, 0x57, 0x0E, 0x0D, 0x00, 0x00, 0x01};
                                    // "Esc*o5W 0E 0D 00 00 01" Top edge overspray for full-bleed printing

            addToHeader((const BYTE *) TopOverSpraySeq, sizeof(TopOverSpraySeq));
        }
        else
        {
            BYTE TopOverSpraySeq[]  = {0x1b, 0x2A, 0x6F, 0x35, 0x57, 0x0E, 0x02, 0x00};
                                    // "Esc*o5W 0E 02 00 00 00" Top edge overspray for full-bleed printing

            BYTE LeftOverSpraySeq[] = {0x1b, 0x2A, 0x6F, 0x35, 0x57, 0x0E, 0x01, 0x00};
                                    // "Esc*o5W 0E 01 00 00 00" Left edge overspray for full-bleed printing

            cBuf[1] = (m_pMA->top_overspray) & 0xFF;
            cBuf[0] = (m_pMA->top_overspray) >> 8;

            addToHeader((const BYTE *) TopOverSpraySeq, sizeof(TopOverSpraySeq));
            addToHeader((const BYTE *) cBuf, 2);

            cBuf[1] = (m_pMA->left_overspray) & 0xFF;
            cBuf[0] = (m_pMA->left_overspray) >> 8;

            addToHeader((const BYTE *) LeftOverSpraySeq, sizeof(LeftOverSpraySeq));
            addToHeader((const BYTE *) cBuf, 2);
        }
    }

//  Now send media pre-load command
    addToHeader((const BYTE *) "\033&l-2H", 6);

//  Before sending speed mech commands, send the current buffer to the printer
    err = sendBuffer(static_cast<const BYTE *>(pcl_buffer), (cur_pcl_buffer_ptr - pcl_buffer));
    cur_pcl_buffer_ptr = pcl_buffer;

//  send speed mech commands

    if (speed_mech_enabled) {
        addToHeader(speed_mech_cmd, sizeof (speed_mech_cmd));
        *cur_pcl_buffer_ptr++ = (BYTE) ((m_pJA->total_pages & 0x0000FF00) >> 8);
        *cur_pcl_buffer_ptr++ = (BYTE) ((m_pJA->total_pages & 0x000000FF));
        if (page_number < m_pJA->total_pages) {
            addToHeader(speed_mech_continue, sizeof (speed_mech_continue));
        }
        else {
            addToHeader(speed_mech_end, sizeof (speed_mech_end));
        }
    }

    addToHeader((const BYTE *) grafStart, sizeof(grafStart));

    if (!m_pJA->print_borderless) {
        top_margin = m_pMA->printable_start_y - ((m_pJA->mech_offset * m_pQA->actual_vertical_resolution)/1000);
    }
    addToHeader("\x1b*p%dY", top_margin);
    return err;
}

DRIVER_ERROR Pcl3Gui2::Encapsulate(RASTERDATA *InputRaster, bool bLastPlane)
{
    DRIVER_ERROR    err;
    if (crd_type != eCrd_color_only) {
        err = encapsulateRaster(InputRaster->rasterdata[COLORTYPE_BLACK], InputRaster->rastersize[COLORTYPE_BLACK], COLORTYPE_BLACK);
    }
    if (crd_type != eCrd_black_only) {
        err = encapsulateRaster(InputRaster->rasterdata[COLORTYPE_COLOR], InputRaster->rastersize[COLORTYPE_COLOR], COLORTYPE_COLOR);
    }
    return err;
}

DRIVER_ERROR Pcl3Gui2::encapsulateRaster(BYTE *raster, unsigned int length, COLORTYPE c_type)
{
    DRIVER_ERROR     err;
    char    scratch[20];
    int     scratchLen;
    char    c = 'W';
    if (crd_type == eCrd_color_only) {
        return NO_ERROR;
    }
    if (c_type == COLORTYPE_BLACK && crd_type == eCrd_both) {
        c = 'V';
    }
    scratchLen = sprintf(scratch, "\033*b%u%c", length, c);
    err = this->Send((const BYTE*) scratch, scratchLen);
    if (err == NO_ERROR && length > 0)
    {
        err = this->Send(raster, length);
    }
    return err;
}

void Pcl3Gui2::configureRasterData()
{
    int    i;
    char   *p;
    char   sequences[3][64];
    int    seq_sizes[] = {sizeof(crd_sequence_k), sizeof(crd_sequence_color),
                          sizeof(crd_sequence_both)};
    memcpy(sequences[0], crd_sequence_k, seq_sizes[0]);
    memcpy(sequences[1], crd_sequence_color, seq_sizes[1]);
    memcpy(sequences[2], crd_sequence_both, seq_sizes[2]);
    // First, update the resolution entries. Currently, this assumes K & RGB
    // resolutions are the same.
    for (i = 0; i < 3; i++) {
	p = sequences[i] + 10;
        *p++ = m_pQA->horizontal_resolution / 256;
        *p++ = m_pQA->horizontal_resolution % 256;
        *p++ = m_pQA->vertical_resolution / 256;
        *p++ = m_pQA->vertical_resolution % 256;
    }
    p = &sequences[2][18];
    *p++ = m_pQA->horizontal_resolution / 256;
    *p++ = m_pQA->horizontal_resolution % 256;
    *p++ = m_pQA->vertical_resolution / 256;
    *p++ = m_pQA->vertical_resolution % 256;

    memcpy(cur_pcl_buffer_ptr, sequences[crd_type], seq_sizes[crd_type]);
    cur_pcl_buffer_ptr += seq_sizes[crd_type];

} //configureRasterData

