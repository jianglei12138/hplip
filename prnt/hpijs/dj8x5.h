/*****************************************************************************\
  dj8x5.h : Interface for the DJ8x5 class

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


#ifndef APDK_DJ8X5_H
#define APDK_DJ8X5_H

APDK_BEGIN_NAMESPACE

/*!
\internal
*/
class DJ8x5 : public DJ8xx
{
public:
    DJ8x5 (SystemServices* pSS,int numfonts=0, BOOL proto=FALSE);

    virtual DRIVER_ERROR VerifyPenInfo();
    virtual PEN_TYPE DefaultPenSet();

}; //DJ8x5


class DJ8x5Mode1 : public PrintMode
{
public:
    DJ8x5Mode1();
}; //DJ8x5Mode1


class DJ8x5Mode2 : public PrintMode
{
public:
    DJ8x5Mode2();
}; //DJ8x5Mode2


class DJ8x5Mode3 : public PrintMode
{
public:
    DJ8x5Mode3();
}; //DJ8x5Mode3

/*
class DJ8x5Mode4 : public PrintMode
{
    public:
        DJ8x5Mode4 ();
}; //DJ8x5Mode4
*/

/*
class DJ8x5Mode5 : public PrintMode
{
    public:
        DJ8x5Mode5 ();
};
*/
#if defined(APDK_DJ8xx)|| defined(APDK_DJ9xx)

#ifdef APDK_DJ8x5
//! DJ8x5Proxy
/*!
******************************************************************************/
class DJ8x5Proxy : public PrinterProxy
{
public:
    DJ8x5Proxy() : PrinterProxy(
        "DJ8x5",                    // family name
        "DESKJET 825\0"                                 // DeskJet 825
        "DESKJET 845\0"                                 // DeskJet 845
#ifdef APDK_MLC_PRINTER
#endif
    ) {m_iPrinterType = eDJ8x5;}
    inline Printer* CreatePrinter(SystemServices* pSS) const { return new DJ8x5(pSS); }
	inline PRINTER_TYPE GetPrinterType() const { return eDJ8x5;}
	inline unsigned int GetModelBit() const { return 0x400;}
} ;
#endif
#endif

APDK_END_NAMESPACE

#endif //APDK_DJ8X5_H
