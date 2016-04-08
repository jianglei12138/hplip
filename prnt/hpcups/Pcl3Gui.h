/*****************************************************************************\
  Pcl3Gui.h : Interface for Pcl3Gui class

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

#ifndef PCL3GUI_H
#define PCL3GUI_H

#include "CommonDefinitions.h"
#include "Pipeline.h"
#include "Encapsulator.h"

class Halftoner;

class Pcl3Gui: public Encapsulator
{
public:
    Pcl3Gui();
    ~Pcl3Gui();
    DRIVER_ERROR    Encapsulate (RASTERDATA *InputRaster, bool bLastPlane);
    DRIVER_ERROR    StartPage(JobAttributes *pJA);
    DRIVER_ERROR    Configure(Pipeline **pipeline);
protected:
    bool needPJLHeaders(JobAttributes *pJA)
    {
        if (!strcmp(m_pJA->printer_platform, "dj970"))
        {
            return false;
        }
        return true;
    }
    void configureRasterData ();
private:
    bool selectPrintMode();
    bool selectPrintMode(int index);
    int  colorLevels(int color_plane);

    PrintMode    *m_pPM;
    Halftoner    *m_pHalftoner;
};

#endif // PCL3GUI_H

