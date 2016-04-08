/*****************************************************************************\
  Pclm_wrapper.h : Interface for the PCLm class

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

  Author(s): Suma Byrappa
  Contributor(s) : Goutam Kodu , Sanjay Kumar
\*****************************************************************************/


#ifndef HBPL1_WRAPPER_H
#define HBPL1_WRAPPER_H
#include "CommonDefinitions.h"
#include "Hbpl1.h"


class Hbpl1;

class Hbpl1Wrapper
{
    friend class Hbpl1;
public:
    Hbpl1Wrapper(Hbpl1* const m_Hbpl1);
    ~Hbpl1Wrapper ();
     /* use virtual otherwise linker will try to perform static linkage */
    virtual DRIVER_ERROR StartJob(void **pOutBuffer, int *iOutBufferSize);
    virtual DRIVER_ERROR EndJob(void **pOutBUffer, int *iOutBifferSize);
    virtual DRIVER_ERROR StartPage(void **pOutBuffer, int *iOutBufferSize);
    virtual DRIVER_ERROR EndPage(void **pOutBuffer, int *iOutBufferSize);
    virtual DRIVER_ERROR Encapsulate(void *pInBuffer, int inBufferSize, int numLines, void **pOutBuffer, int *iOutBufferSize);
    virtual DRIVER_ERROR SkipLines(int iSkipLines);
    virtual void FreeBuffer(void*, int);
    virtual DRIVER_ERROR FormFeed();

private:
    virtual DRIVER_ERROR InitStripBuffer(long long NoOfBytes);
    virtual void FreeStripBuffer(void);
    Hbpl1 *o_Hbpl1;
}; // Hbpl1Wrapper

#endif // HBPL1_WRAPPER_H

