/*****************************************************************************\
  Pipeline.cpp : Implementation of Pipeline class

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

/*
 *  Pipeline.cpp
 *  hpcups
 *
 */

#include "Pipeline.h"
bool Pipeline::NextOutputRaster(RASTERDATA& next_raster)
{
    return Exec->NextOutputRaster(next_raster);
}
unsigned int Pipeline::GetMaxOutputWidth()
{
    return Exec->GetMaxOutputWidth();
}

///////////////////////////////////////////////////////////
// Pipeline management
Pipeline::Pipeline (Processor *E) :
    next(NULL),
    prev(NULL) {
    Exec = E;
    Exec->myphase = this;
} //Pipeline

void Pipeline::AddPhase (Pipeline *newp) {
    Pipeline    *p = this;
    while (p->next) {
        p = p->next;
    }
    p->next = newp;
    newp->prev = p;
} //AddPhase

Pipeline::~Pipeline ()
{
} //~Pipeline


bool Pipeline::Process (RASTERDATA *raster)
{
    return Exec->Process (raster);
} //Process


DRIVER_ERROR Pipeline::Execute (RASTERDATA *InputRaster)
{
    err = NO_ERROR;
    if (Process (InputRaster)        // true if output ready; may set err
        && (err == NO_ERROR)) {
        if (next) {
            while ( NextOutputRaster( next->Exec->raster ) ) {
                err = next->Execute(&(next->Exec->raster));
                ERRCHECK;
            }
        }
    }
    return err;
} //Execute


DRIVER_ERROR Pipeline::Flush ()
{
    err = NO_ERROR;

    Exec->Flush ();

    if (next && (err == NO_ERROR)) {
        while ( NextOutputRaster( next->Exec->raster ) ) {
            err = next->Execute(&(next->Exec->raster));
            ERRCHECK;
        }
        // one more to continue flushing downstream
        err = next->Flush ();
    }
    return err;
} //Flush

