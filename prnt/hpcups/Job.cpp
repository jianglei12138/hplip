/*****************************************************************************\
  Job.cpp : Implementation of Job class

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

#include "Job.h"

Job::Job() :
    m_pSys(NULL),
    m_pPipeline(NULL),
    m_pEncap(NULL),
    skipcount(0),
    m_pBlankRaster(NULL),
    m_pBlackRaster(NULL),
    m_bDataSent(false),
    m_iRaster(0),
    m_iBlanks(0)
{
}

DRIVER_ERROR Job::Init(SystemServices *pSystemServices, JobAttributes *job_attrs, Encapsulator *encap_intf)
{
    DRIVER_ERROR err = NO_ERROR;

    if (encap_intf == NULL) {
        return NO_PRINTER_SELECTED;
    }

    m_pEncap = encap_intf;
    m_pSys = pSystemServices;

    //m_job_attributes = job_attrs;
    memcpy(&m_job_attributes, job_attrs, sizeof(m_job_attributes));

    err = m_pEncap->StartJob(m_pSys, &m_job_attributes);

    if (err == NO_ERROR)
        err = Configure();

//  Setup the blank raster used by sendrasters
    if (err == NO_ERROR)
        err = setBlankRaster();
    return err;
} //Job

/*! Allows the encapsulator to prepare for a new page
 *
 */

DRIVER_ERROR Job::StartPage(JobAttributes *job_attrs)
{

    if (job_attrs) {
        memcpy(&m_job_attributes, job_attrs, sizeof(m_job_attributes));
    }
    return m_pEncap->StartPage(&m_job_attributes);
}

DRIVER_ERROR Job::Cleanup()
{
    // Client isn't required to call NewPage at end of last page, so
    // we may need to eject a page now.
    DRIVER_ERROR    err = NO_ERROR;

/*
 *  Let the encapsulator cleanup, such as sending previous page if speed mech
 *  is enabled.
 */

    if (m_pEncap) {
        err = m_pEncap->Cleanup();
    }

    if (err != NO_ERROR) {
        return err;
    }

    if (m_bDataSent || err != NO_ERROR) {
        NewPage();
    }

    // Tell printer that job is over.
    if (m_pEncap) {
        m_pEncap->EndJob();
    }//end if
    return NO_ERROR;
}

void Job::CancelJob()
{
    if (m_pEncap)
        m_pEncap->CancelJob();
}

Job::~Job()
{
    if (m_pBlackRaster) {
        delete [] m_pBlackRaster;
    }

    if (m_pBlankRaster) {
        delete [] m_pBlankRaster;
    }

    Pipeline    *p;
    Pipeline    *p2;
    p = m_pPipeline;
    while (p)
    {
        p2 = p;
        p = p->next;
        delete p2->Exec;
        delete p2;
    }

    if (m_pEncap) {
        delete m_pEncap;
    }

} //~Job

void Job::unpackBits(BYTE *BlackImageData)
{
// Convert K from 1-bit raster to 8-bit raster.
    unsigned char  bitflag[] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};
    int    width = (m_job_attributes.media_attributes.printable_width + 7) / 8;
    for (int i = 0; i < width; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            if (BlackImageData[i] & bitflag[j])
                m_pBlackRaster[i*8+j] = 1;
        }
    }
}

DRIVER_ERROR Job::SendRasters(BYTE* BlackImageData, BYTE* ColorImageData)
{
    DRIVER_ERROR err = NO_ERROR;
    if (m_pPipeline == NULL) {
        return SYSTEM_ERROR;
    }

    if (BlackImageData == NULL && ColorImageData == NULL) {
        if (m_pEncap->CanSkipRasters()) {
            skipcount++;
            return NO_ERROR;
        }
        ColorImageData = m_pBlankRaster;
    }

    if (skipcount > 0) {
        m_pPipeline->Flush();
        err = m_pEncap->SendCAPy(skipcount);
        skipcount = 0;
    }
    m_bDataSent = true;

    if (BlackImageData || ColorImageData)
    {
        if (BlackImageData)
        {
            if (m_pEncap->UnpackBits())
            {
                err = setBlackRaster();
                ERRCHECK;
                unpackBits(BlackImageData);
                BlackImageData = m_pBlackRaster;
            }
            m_pPipeline->Exec->raster.rastersize[COLORTYPE_BLACK] = m_job_attributes.media_attributes.printable_width;
            m_pPipeline->Exec->raster.rasterdata[COLORTYPE_BLACK] = BlackImageData;
        }
        else
        {
            m_pPipeline->Exec->raster.rastersize[COLORTYPE_BLACK] = 0;
            m_pPipeline->Exec->raster.rasterdata[COLORTYPE_BLACK] = NULL;
        }
        if (ColorImageData)
        {
            m_pPipeline->Exec->raster.rastersize[COLORTYPE_COLOR] = m_job_attributes.media_attributes.printable_width * 3;
            m_pPipeline->Exec->raster.rasterdata[COLORTYPE_COLOR] = ColorImageData;
        }
        else
        {
            m_pPipeline->Exec->raster.rastersize[COLORTYPE_COLOR] = 0;
            m_pPipeline->Exec->raster.rasterdata[COLORTYPE_COLOR] = NULL;
        }
        err = m_pPipeline->Execute(&(m_pPipeline->Exec->raster));
    }

    return err;
} // Sendrasters

DRIVER_ERROR Job::NewPage()
{
    DRIVER_ERROR err;
    m_pEncap->SetLastBand();

    if (!m_bDataSent && skipcount > 0) {
        skipcount = 0;
        SendRasters(NULL, m_pBlankRaster);
    }
    m_pPipeline->Flush();
    err = m_pEncap->FormFeed();
    ERRCHECK;

//  reset flag used to see if formfeed needed
    m_bDataSent = false;
    skipcount = 0;

    return NO_ERROR;
} // Newpage

DRIVER_ERROR Job::Configure()
{
// mode has been set -- now set up rasterwidths and pipeline
    DRIVER_ERROR    err = NO_ERROR;
    Pipeline        *p;

    p = NULL;
    // Ask Encapsulator to configure itself

    err = m_pEncap->Configure(&m_pPipeline);
    if (err != NO_ERROR)
    {
        return err;
    }

    // always end pipeline with RasterSender
    // create RasterSender object
    RasterSender    *pSender;
    pSender = new RasterSender(m_pEncap);

    p = new Pipeline(pSender);

    if (m_pPipeline) {
        m_pPipeline->AddPhase(p);
    }
    else {
        m_pPipeline = p;
    }
   return NO_ERROR;
} //Configure

DRIVER_ERROR Job::setBlackRaster()
{
    if (!m_pBlackRaster) {
        m_pBlackRaster = new BYTE[m_job_attributes.media_attributes.printable_width];
        NEWCHECK(m_pBlackRaster);
    }
    memset(m_pBlackRaster, 0, m_job_attributes.media_attributes.printable_width);

    return NO_ERROR;
} //setBlackRaster

DRIVER_ERROR Job::setBlankRaster()
{
    if (m_pBlankRaster) {
        delete m_pBlankRaster;
    }
    size_t    size = m_job_attributes.media_attributes.printable_width * 3;
    m_pBlankRaster = new BYTE[size];
    NEWCHECK(m_pBlankRaster);
    memset(m_pBlankRaster, 0xFF, size);
    return NO_ERROR;
}

DRIVER_ERROR Job::preProcessRasterData(cups_raster_t **ppcups_raster, cups_page_header2_t* firstpage_cups_header, char* pPreProcessedRasterFile)
{
	dbglog ("DEBUG: Job::preProcessRasterData.............. \n");
 	return m_pEncap->preProcessRasterData(ppcups_raster, firstpage_cups_header, pPreProcessedRasterFile);
}
   

