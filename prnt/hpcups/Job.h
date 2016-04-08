/*****************************************************************************\
  Job.h : Interface for the Job class

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

#ifndef JOB_H
#define JOB_H

#include "Encapsulator.h"
#include "Pipeline.h"
#include "Processor.h"
#include "EncapsulatorFactory.h"
#include "Mode9.h"
#include "Mode10.h"
#include "RasterSender.h"
class Job
{
public:
    Job();

    DRIVER_ERROR Init(SystemServices *pSystemServices, JobAttributes *job_attrs, Encapsulator *encap_intf);
    DRIVER_ERROR Cleanup();
    virtual ~Job();
    DRIVER_ERROR SendRasters(BYTE* BlackImageData=(BYTE*)NULL, BYTE* ColorImageData=(BYTE*)NULL);
    DRIVER_ERROR NewPage();
    DRIVER_ERROR StartPage(JobAttributes *job_attrs);
    DRIVER_ERROR preProcessRasterData(cups_raster_t **cups_raster, cups_page_header2_t* firstpage_cups_header, char* pPreProcessedRasterFile);   
    void         CancelJob();
private:

    void             unpackBits(BYTE *BlackImageData);
    JobAttributes    m_job_attributes;
    SystemServices *m_pSys;
    Pipeline       *m_pPipeline;
    Encapsulator    *m_pEncap;
    unsigned int skipcount;
    BYTE* m_pBlankRaster;
    BYTE* m_pBlackRaster;
    bool m_bDataSent;
    DRIVER_ERROR Configure();
    DRIVER_ERROR setBlankRaster();
    DRIVER_ERROR setBlackRaster();
    int     m_iRaster;
    int     m_iBandNum;
    int     m_iBlanks;
    int     m_resolution_ratio;
    int     m_row_number;
}; // Job

#endif // JOB_H

