/*****************************************************************************\
  dj6xx.h : Interface for the DJ6XX class

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


#ifndef APDK_DJ6XX_H
#define APDK_DJ6XX_H

APDK_BEGIN_NAMESPACE

/*!
\internal
*/
class DJ6XX : public Printer
{
public:

    DJ6XX(SystemServices* pSS, int numfonts, BOOL proto = FALSE);
	virtual BOOL GetMargins (PAPER_SIZE ps, float *fMargins)
	{
		if (ps == A4)
			fMargins[0] = (float) 0.135;
		else
			fMargins[0] = (float) 0.25;	// Left Margin
		fMargins[1] = fMargins[0];		// Right Margin
		fMargins[2] = (float) 0.125;	// Top Margin
		fMargins[3] = (float) 0.67;		// Bottom Margin

		return TRUE;
	}

#ifdef APDK_HP_UX
protected:
    virtual DJ6XX & operator = (Printer& rhs)
    {
        return *this;
    }
#endif

}; //DJ6XX


// there are no printers directly supported by DJ6XX - so we don't need a proxy.
//! DJ6XXProxy
/*!
******************************************************************************/
/*
static class DJ6XXProxy : public PrinterProxy
{
public:
    DJ6XXProxy() : PrinterProxy(
        "DeskJet 600 series printers",
        "DESKJET 66\0"
        "DESKJET 67\0"
        "DESKJET 68\0"
        "DESKJET 6\0"
        "e-printer e20\0"
#ifdef APDK_MLC_PRINTER
        "Deskjet 78\0"
#endif
    ) {}
    inline Printer* CreatePrinter(SystemServices* pSS) const { return new DJ6XX(pSS); }
} DJ6XXProxy;
*/

APDK_END_NAMESPACE

#endif //APDK_DJ6XX_H
