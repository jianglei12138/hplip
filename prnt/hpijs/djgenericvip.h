/*****************************************************************************\
  djgenericvip.h : Interface for the generic VIP printer class

  Copyright (c) 2001 - 2015, HP Co.
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


#ifndef APDK_DJ_GENERIC_VIP_H
#define APDK_DJ_GENERIC_VIP_H

APDK_BEGIN_NAMESPACE

//DJGenericVIP
//!
/*!
\internal
******************************************************************************/
class DJGenericVIP : public DJ9xxVIP
{
public:
    DJGenericVIP (SystemServices* pSS, BOOL proto = FALSE);
    Header *SelectHeader (PrintContext *pc);
    DRIVER_ERROR VerifyPenInfo ();
    virtual BOOL FullBleedCapable (PAPER_SIZE ps, FullbleedType  *fbType, float *xOverSpray, float *yOverSpray,
                                   float *fLeftOverSpray, float *fTopOverSpray);
	virtual BOOL UseGUIMode (PrintMode *pPrintMode);
	virtual PAPER_SIZE MandatoryPaperSize ();
    virtual PHOTOTRAY_STATE PhotoTrayEngaged (BOOL bQueryPrinter);
    //! Returns TRUE if a hagaki feed is present in printer.
    virtual BOOL HagakiFeedPresent(BOOL bQueryPrinter);
	virtual DATA_FORMAT GetDataFormat () { return RASTER_STRIP; }

#ifdef APDK_AUTODUPLEX
    //!Returns TRUE if duplexer and hagaki feed (combined) unit is present in printer.

    virtual BOOL HagakiFeedDuplexerPresent(BOOL bQueryPrinter);
#endif
    virtual BOOL GetMargins (PAPER_SIZE ps, float *fMargins);
#ifdef APDK_LINUX
    virtual DRIVER_ERROR    SendSpeedMechCmd (BOOL bLastPage);
#endif // APDK_LINUX

private:
	virtual void AdjustModeSettings (BOOL bDoFullBleed, MEDIATYPE ReqMedia,
									 MediaType *medium, Quality *quality);

}; //DJGenericVIP

class VIPFastDraftMode : public PrintMode
{
public:
    VIPFastDraftMode ();
}; // VIPFastDraftMode

class VIPGrayFastDraftMode : public GrayMode
{
public:
    VIPGrayFastDraftMode ();
}; // VIPGrayFastDraftMode

class VIPFastPhotoMode : public PrintMode
{
public:
    VIPFastPhotoMode ();
}; // VIPFastPhotoMode

class VIPAutoPQMode : public PrintMode
{
public:
    VIPAutoPQMode ();
}; // VIPAutoPQMode

class VIPCDDVDMode : public PrintMode
{
public:
    VIPCDDVDMode ();
}; // VIPCDDVDMode

class VIPBrochureNormalMode : public PrintMode
{
public:
    VIPBrochureNormalMode ();
}; // VIPBrochureNormalMode

class VIPPremiumNormalMode : public PrintMode
{
public:
    VIPPremiumNormalMode ();
}; // VIPPremiumNormalMode

class VIPPlainBestMode : public PrintMode
{
public:
    VIPPlainBestMode ();
}; // VIPPlainBestMode

class VIPBrochureBestMode : public PrintMode
{
public:
    VIPBrochureBestMode ();
}; // VIPBrochureBestMode

class VIPPremiumBestMode : public PrintMode
{
public:
    VIPPremiumBestMode ();
}; // VIPPremiumBestMode

#if defined(APDK_DJGENERICVIP) && defined (APDK_DJ9xxVIP)
//! DJGenericVIPProxy
/*!
******************************************************************************/
class DJGenericVIPProxy : public PrinterProxy
{
public:
    DJGenericVIPProxy() : PrinterProxy(
        "GenericVIP",                       // family name
        "deskjet 5100\0"
        "Deskjet 5400\0"
        "deskjet 5600\0"
        "Deskjet 5700\0"
        "deskjet 5800\0"
        "Deskjet 5900\0"
        "Deskjet 6500\0"
        "Deskjet 6600\0"
        "Deskjet 6800\0"
        "Deskjet 6940\0"
        "Deskjet 6980\0"
        "Deskjet 6988\0"
        "deskjet 9600\0"
        "Deskjet 9800\0"
        "Deskjet D25\0"
        "Deskjet D55\0"
        "Deskjet F24\0"
        "Deskjet F42\0"
        "Deskjet F44\0"
        "Deskjet F45\0"
        "Deskjet 2050 J510\0"
        "Deskjet 1050 J410\0"
        "Business Inkjet 1000\0"
        "hp business inkjet 1100\0"
        "HP Business Inkjet 1200\0"
        "Officejet Pro 8500\0"
        "photosmart 7150\0"
        "photosmart 7350\0"
        "photosmart 7345\0"
        "photosmart 7550\0"
        "photosmart 7960\0"
        "photosmart 7760\0"
        "photosmart 7660\0"
        "photosmart 7260\0"
        "photosmart 7268\0"
        "Photosmart 7400\0"
        "Photosmart 7800\0"
        "Photosmart 8000\0"
        "Photosmart 8100\0"
        "Photosmart 8400\0"
        "Photosmart 8700\0"
        "Photosmart 8200\0"
        "Photosmart 320\0"
        "Photosmart 370\0"
        "Photosmart 380\0"
        "Photosmart 330\0"
        "Photosmart 420\0"
        "Photosmart A430\0"
        "Photosmart A510\0"
        "Photosmart A520\0"
        "PSC 1500\0"
        "PSC 1600\0"
        "psc 2300\0"
        "PSC 2350\0"
        "psc 2400\0"
        "psc 2500\0"
        "Officejet 4400 K410\0"
        "Officejet 4500 All-in-One Printer - K710\0"
        "Officejet 4500 G510\0"
        "Officejet 6000 E609\0"
        "Officejet 6500 E709\0"
        "Officejet 7000 E809\0"
        "Officejet 74\0"
        "Officejet 73\0"
        "Officejet 72\0"
        "Officejet 60\0"
        "Officejet 62\0"
        "Officejet 63\0"
        "Officejet J35\0"
        "Officejet J36\0"
        "Officejet J46\0"
        "Officejet J55\0"
        "Officejet J57\0"
        "Officejet J64\0"
        "Officejet K71\0"
        "Officejet Pro K53\0"
        "Officejet Pro K54\0"
        "Officejet Pro K56\0"
        "Officejet Pro K86\0"
        "Officejet Pro L73\0"
        "Officejet Pro L75\0"
        "Officejet Pro L76\0"
        "Officejet Pro L77\0"
        "Officejet Pro L82\0"
        "Officejet Pro L83\0"
        "Officejet Pro L84\0"
        "Photosmart 2570\0"
        "Photosmart 2600\0"
        "Photosmart 2700\0"
        "Photosmart 3100\0"
        "Photosmart 3200\0"
        "Photosmart 3300\0"
        "Photosmart A310\0"
        "Photosmart A320\0"
        "Photosmart A440\0"
        "Photosmart B85\0"
        "Photosmart B109\0"
        "Photosmart Plus B209\0"
        "Photosmart Prem-Web C309\0"
        "Photosmart Pro B8300\0"
        "Photosmart Pro B88\0"
        "Photosmart C309\0"
        "Photosmart C31\0"
        "Photosmart C41\0"
        "Photosmart C42\0"
        "Photosmart C43\0"
        "Photosmart C44\0"
        "Photosmart C45\0"
        "Photosmart C46\0"
        "Photosmart C47\0"
        "Photosmart C51\0"
        "Photosmart C52\0"
        "Photosmart C53\0"
        "Photosmart C61\0"
        "Photosmart C62\0"
        "Photosmart C63\0"
        "Photosmart C71\0"
        "Photosmart C72\0"
        "Photosmart C81\0"
        "Photosmart D5060\0"
        "Photosmart D51\0"
        "Photosmart D53\0"
        "Photosmart D54\0"
        "Photosmart D61\0"
        "Photosmart D71\0"
        "Photosmart D72\0"
        "Photosmart D73\0"
        "Photosmart D74\0"
        "Photosmart D73\0"
        "Photosmart D110\0"
        "Photosmart Wireless B109\0"
        "Photosmart B010\0"
        "Photosmart B110\0"
    ) {m_iPrinterType = eDJGenericVIP;}
    inline Printer* CreatePrinter(SystemServices* pSS) const { return new DJGenericVIP(pSS); }
	inline PRINTER_TYPE GetPrinterType() const { return eDJGenericVIP;}
	inline unsigned int GetModelBit() const { return 0x80;}
};
#endif

APDK_END_NAMESPACE

#endif  // APDK_DJ_GENERIC_VIP_H
