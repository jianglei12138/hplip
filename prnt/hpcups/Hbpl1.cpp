/*****************************************************************************\
  Hbpl1.cpp : Implementation for the Hbpl1 class

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
  Contributor(s): Sanjay Kumar, Goutam Kodu

\*****************************************************************************/


#include <dlfcn.h>
#include "Hbpl1.h"
#include "utils.h"
#define ZJC_BAND_HEIGHT    100
#define ON 1
#define OFF 0
#define COLOR 0
#define BLACKONLY 1
#define GRAYSCALE 2

Hbpl1Wrapper* (*fptr_create)(Hbpl1* const m_Hbpl1);
void (*fptr_destroy)(Hbpl1Wrapper*);
Hbpl1::Hbpl1 () : Encapsulator ()
{
    memset(&m_PM, 0, sizeof(m_PM));
    strcpy(m_szLanguage, "HBPL1");
    dbglog("Hbpl1 constructor : m_szLanguage = %s", m_szLanguage);
    m_numScanLines = -1; // value checked for allocating buffer
    m_nScanLineCount = 0;
    m_nStripSize = 0;
    m_nStripHeight = HBPL1_DEFAULT_STRIP_HEIGHT;
    m_nBandCount = 0;
    m_pbyStripData = NULL;
    m_init = false;
    m_ColorMode = COLORTYPE_BOTH; //Grayscale
    m_ColorTheme = NONE;

    m_hHPLibHandle = load_plugin_library(UTILS_PRINT_PLUGIN_LIBRARY, PRNT_PLUGIN_HBPL1);
    if (m_hHPLibHandle)
    {
        dlerror ();
        fptr_create = (Hbpl1Wrapper* (*)(Hbpl1*))get_library_symbol(m_hHPLibHandle, "create_object");
        fptr_destroy = (void (*)(Hbpl1Wrapper*))get_library_symbol(m_hHPLibHandle, "destroy_object");
        if (fptr_create != NULL && fptr_destroy!= NULL )
            m_init = true;

        if ((m_pHbpl1Wrapper = fptr_create(this)) == NULL)
             m_init = false;
    }

}

Hbpl1::~Hbpl1()
{
    if (m_pbyStripData)
    {
	delete [] m_pbyStripData;
	m_pbyStripData = NULL;
    }

    // unloading the library and delete Hbpl1Wrapper
    if(m_init)
    {
        if(fptr_destroy)
            fptr_destroy( m_pHbpl1Wrapper );

        unload_library(m_hHPLibHandle);
        m_hHPLibHandle = NULL;
    }
}


DRIVER_ERROR Hbpl1::addJobSettings()
{
    DRIVER_ERROR err = NO_ERROR;
    return err;
}


DRIVER_ERROR Hbpl1::Configure(Pipeline **pipeline)
{
    DRIVER_ERROR err = NO_ERROR;
    return err;
}

DRIVER_ERROR Hbpl1::StartJob(SystemServices *pSystemServices, JobAttributes *pJA)
{
    DRIVER_ERROR        err = NO_ERROR;

    if (!m_init)
    {
        return PLUGIN_LIBRARY_MISSING;
    }

    memcpy(&m_JA, pJA,sizeof(m_JA));
    m_nScanLineSize = m_JA.media_attributes.printable_width;
    m_nStripSize = m_nScanLineSize * 3 * m_nStripHeight;
    m_numStrips = m_JA.media_attributes.printable_height/m_nStripHeight;
   	m_Economode = (m_JA.integer_values[2] == ON) ? true : false;  //Economode
    m_ColorTheme = (COLORTHEME)m_JA.integer_values[5]; // cupsInteger5 value
    m_PrintinGrayscale = m_JA.integer_values[3]; // cupsInterger3 value
    m_pSystemServices = pSystemServices; //Reset and UEL not required
    err = m_pHbpl1Wrapper->StartJob((void**)&m_pOutBuffer, &m_OutBuffSize);
    err = sendBuffer(static_cast<const BYTE *>(m_pOutBuffer), m_OutBuffSize);
    m_pHbpl1Wrapper->FreeBuffer(m_pOutBuffer, m_OutBuffSize);

    if (m_PrintinGrayscale ==  ON){  //Grayscale = ON
        m_ColorMode = COLORTYPE_BOTH;
    } else if(m_JA.color_mode == COLOR){ // if color cartridge && Grayscale = OFF
        m_ColorMode = COLORTYPE_COLOR;
    } else if(m_JA.color_mode == BLACKONLY){ // if black cartridge && Grayscale = OFF
        m_ColorMode = COLORTYPE_BLACK;
    } else
        m_ColorMode = COLORTYPE_BOTH; //Grayscale

    return err;
}


DRIVER_ERROR Hbpl1::EndJob()
{
    DRIVER_ERROR        err = NO_ERROR;

     if (!m_init)
    {
        return PLUGIN_LIBRARY_MISSING;
    }

    err = m_pHbpl1Wrapper->EndJob((void**)&m_pOutBuffer, &m_OutBuffSize);
    err = sendBuffer(static_cast<const BYTE *>(m_pOutBuffer), m_OutBuffSize);
    m_pHbpl1Wrapper->FreeBuffer(m_pOutBuffer, m_OutBuffSize);
    return err;
}


DRIVER_ERROR Hbpl1::StartPage (JobAttributes *pJA)
{
    DRIVER_ERROR        err = NO_ERROR;

    err = m_pHbpl1Wrapper->StartPage((void**)&m_pOutBuffer, &m_OutBuffSize);
    err = sendBuffer(static_cast<const BYTE *>(m_pOutBuffer), m_OutBuffSize);
    m_pHbpl1Wrapper->FreeBuffer(m_pOutBuffer, m_OutBuffSize);
    return err;
}

// EndPage() will be called in FormFeed()


DRIVER_ERROR Hbpl1::sendBlankBands()
{
    DRIVER_ERROR err = NO_ERROR;
    return err;
}

DRIVER_ERROR Hbpl1::FormFeed ()
{

    if (0 != m_numScanLines && m_pbyStripData && 0 != m_nStripSize)
	{
		++m_nBandCount;
		m_pHbpl1Wrapper->Encapsulate(m_pbyStripData, m_nStripSize, m_nStripHeight, (void**)&m_pOutBuffer, &m_OutBuffSize);
		sendBuffer(m_pOutBuffer, m_OutBuffSize);
		memset(m_pbyStripData,0xFF,m_nStripSize);
	}

	while(m_nBandCount < m_numStrips)
	{
		++m_nBandCount;
		m_pHbpl1Wrapper->Encapsulate(m_pbyStripData, m_nStripSize, m_nStripHeight, (void**)&m_pOutBuffer, &m_OutBuffSize);
		sendBuffer(m_pOutBuffer, m_OutBuffSize);
	}

	m_pHbpl1Wrapper->EndPage((void**)&m_pOutBuffer, &m_OutBuffSize);
	sendBuffer(m_pOutBuffer, m_OutBuffSize);
    m_pHbpl1Wrapper->FreeBuffer(m_pOutBuffer,m_OutBuffSize);
	m_nBandCount = 0;

	return NO_ERROR;

}


DRIVER_ERROR    Hbpl1::Encapsulate (RASTERDATA *InputRaster, bool bLastPlane)
{
    DRIVER_ERROR    err = NO_ERROR;

    ++m_nScanLineCount;

    if ( m_numScanLines == -1)
    {
        if((int)m_JA.media_attributes.printable_height % m_nStripHeight)
        m_numStrips++;
        err = m_pHbpl1Wrapper->InitStripBuffer(m_nScanLineSize * 3 * m_nStripHeight);
    }

    if (m_numScanLines < m_nStripHeight && m_pbyStripData)
    {
        memcpy((BYTE*)&(m_pbyStripData[m_numScanLines * m_nScanLineSize * 3]),InputRaster->rasterdata[COLORTYPE_COLOR],InputRaster->rastersize[COLORTYPE_COLOR]);
        m_numScanLines++;
    }

    if (m_numScanLines >= m_nStripHeight && m_pbyStripData)
    {
        ++m_nBandCount;
        m_pHbpl1Wrapper->Encapsulate(m_pbyStripData, m_nStripSize, m_nStripHeight, (void**)&m_pOutBuffer, &m_OutBuffSize);
        memset(m_pbyStripData,0xFF,m_nStripSize);
        m_numScanLines = 0;
        err = sendBuffer(static_cast<const BYTE *>(m_pOutBuffer), m_OutBuffSize);
        m_pHbpl1Wrapper->FreeBuffer(m_pOutBuffer, m_OutBuffSize);
    }

    return err;
}


