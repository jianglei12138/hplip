/*****************************************************************************\
  Encapsulator.h : Interface for the Encapsulator class

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

#ifndef ENCAPSULATOR_H
#define ENCAPSULATOR_H

#include "CommonDefinitions.h"
#include "SystemServices.h"
#include "Pipeline.h"

class Encapsulator
{
public:
    Encapsulator();
    virtual ~Encapsulator();
    virtual DRIVER_ERROR    StartJob (SystemServices *pSystemServices, JobAttributes *pJA);
    virtual DRIVER_ERROR    EndJob ();
    virtual DRIVER_ERROR    FormFeed ();
    virtual DRIVER_ERROR    SendCAPy (int iOffset);
    virtual DRIVER_ERROR    Encapsulate (RASTERDATA *InputRaster, bool bLastPlane) = 0;
    virtual DRIVER_ERROR    Send(const BYTE *pBuffer, int length);
    virtual DRIVER_ERROR    StartPage(JobAttributes *pJA) = 0;
    virtual DRIVER_ERROR    Cleanup();
    virtual DRIVER_ERROR    Configure(Pipeline **pipeline) = 0;
    virtual bool            CanSkipRasters() {return true;}
    virtual void            SetLastBand() {}
    virtual bool            UnpackBits() {return true;}
    virtual void            CancelJob();
    virtual DRIVER_ERROR    preProcessRasterData(cups_raster_t **cups_raster, cups_page_header2_t* firstpage_cups_header, char* pSwapedPagesFileName);

protected:
    void         addToHeader(const BYTE *command_string, int length);
    void         addToHeader(const char *fmt, ...);
    DRIVER_ERROR sendBuffer(const BYTE *pBuffer, int length);
    virtual DRIVER_ERROR addJobSettings()
    {
        cur_pcl_buffer_ptr += sprintf((char *) cur_pcl_buffer_ptr, "@PJL ENTER LANGUAGE=%s\012", m_szLanguage);
        return NO_ERROR;
    }
    virtual DRIVER_ERROR    flushPrinterBuffer()
    {
        return m_pSystemServices->Send((const BYTE *) pcl_buffer, 10000);
    }
    virtual bool jobAttrPJLAllowed() {return false;}
    virtual bool needPJLHeaders(JobAttributes *pJA)
    {
        return true;
    }
    virtual void configureRasterData() {}
    void sendJobHeader();

    SystemServices      *m_pSystemServices;
    int          page_number;
    int          cur_pcl_buffer_size;
    BYTE         *pcl_buffer;
    BYTE         *cur_pcl_buffer_ptr;
    char         m_szLanguage[16];

    JobAttributes      *m_pJA;
    MediaAttributes    *m_pMA;
    QualityAttributes  *m_pQA;
};
#endif // ENCAPSULATOR_H

