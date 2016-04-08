/*****************************************************************************\
  Lidil.h : Interface for Lidil class

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

#ifndef LIDIL_H
#define LIDIL_H

class LidilCompress;

#define SWATH_HEIGHT        100
#define DEVUNITS_XBOW       2400

const int CANCELSIZE        = 16;
const int SYNCSIZE          = 2304;
const int SYNC_CMD_OPT_SIZE = 245;

typedef enum
{
    eLDLStartJob          = 0,
    eLDLEndJob            = 0,
    eLDLLoadPage          = 1,
    eLDLEjectPage         = 2,
    eLDLPrintSweep        = 3,
    eLDLLoadSweepData     = 4,
    eLDLQueryPrinter      = 5,
    eLDLComments          = 7,
    eLDLHandlePen         = 8,
    eLDLControl           = 12,
    eLDLDataStreamVersion = 12
} LDLCMD;

typedef enum
{
    eLDLUnknownColor = -1,
    eLDLBlack,
    eLDLCyan,
    eLDLMagenta,
    eLDLYellow,
    eLDLLoCyan,
    eLDLLoMagenta,
    eLDLLoBlack,
    eLDLMaxColor
} COLORENUM;


#define LDLPACKET_MINSIZE   16  // define the minimum packet size defined by the protocol
#define FRAME_SYN '$'           // defined the synchronization frame
#define OPTIMIZED_DELAYLIMIT 0  // defined how many PrintSweep will be sent before the
                                // first LoadSweepData command

#define LDL_MAX_IMAGE_SIZE  2048

#define SIZEOF_LDLHDR         10
#define SIZEOF_LDLTERM        1
#define SIZEOF_LDL_JOB_CMDOPT 5

// define possible operation field
#define OPERATION_STJOB     0
#define OPERATION_ENDJOB    1
#define OPERATION_CANCJOB   2

#define SIZEOF_LDL_LDPAGE_CMDOPT 17

// defn for possible option field settings
#define MEDIATYPE_PLAIN 0           // possible types for mediatype field
#define MEDIATYPE_PHOTO 3

#define MEDIASRC_MAINTRAY 0         // possible setting for mediasrc
#define MEDIADEST_MAINBIN 0         // possible setting for mediadest

#define DRAFT_QUALITY -1       // possible setting for quality
#define NORMAL_QUALITY 0
#define BEST_QUALITY   1
#define MAXDPI_QIALITY 2

#define SPECLOAD_NONE       0       // possible setting for specload
#define SPECLOAD_ENVELOPE   1

#define MEDIALD_SPEED           0x00000001  // bitfield defn for opt_fields
#define NEED_TO_SERVICE_PERIOD  0x00000002
#define MINTIME_BTW_SWEEP       0x00000004

#define DEVUNITS_XBOW                 2400    // Crossbow device units is 2400 dots per inch
#define SIZEOF_LDL_LDPAGE_OPTFLDS     4
#define SIZEOF_LDL_COLROPT_ACTIVECOLR 2
#define NO_ACTIVE_COLORS              0
#define SIZEOF_LDL_EJPAGE_CMDOPT      4
#define MEDIA_EJSPEED                 1    // bitfield defn for opt_fields
#define SIZEOF_LDL_EJPAGE_OPTFLDS     1
#define SIZEOF_LDL_PRTSWP_CMDOPT      18
#define SWINGFMT_UNCOMPRSS            0    // define possible swing format
#define PRNDRN_LEFTTORIGHT            0    // define the possible print direction
#define PRNDRN_RIGHTTOLEFT            1

#define IPS_CARRSPEED                 0x00000001    // bitfield defn for printsweep optional field
#define IPS_INIPRNSPEED               0x00000002
#define IPS_MEDIASPEED                0x00000004
#define PAPER_ACCURACY                0x00000008
#define ACCURATEPOSN_NEEDED           0x00000010
#define DRYTIME                       0x00000020    // bit 6-31 undefined

#define SIZEOF_LDL_PRTSWP_OPTFLDS 3
#define SIZEOF_LDL_PRTSWP_COLROPT 29
#define SIZEOF_LDL_LDSWPDATA_CMDOPT 2

#define OPERATION_CONTINUE   2
#define DATASTREAMVERSION    3
#define OPERATION_SPIT_PEN   2

class Lidil: public Encapsulator
{
public:
    Lidil();
    ~Lidil();
    DRIVER_ERROR    Encapsulate (RASTERDATA *InputRaster, bool bLastPlane);
    DRIVER_ERROR    StartJob(SystemServices *pSystemServices, JobAttributes *pJA);
    DRIVER_ERROR    EndJob();
    DRIVER_ERROR    StartPage(JobAttributes *pJA);
    DRIVER_ERROR    FormFeed();
    DRIVER_ERROR    Configure(Pipeline **pipeline);
    DRIVER_ERROR    SendCAPy (int iOffset);
    void            CancelJob();
protected:
    DRIVER_ERROR    addJobSettings() {return NO_ERROR;}
    DRIVER_ERROR    flushPrinterBuffer() {return NO_ERROR;}
private:
    bool selectPrintMode();
    bool selectPrintMode(int index);
    DRIVER_ERROR allocateSwathBuffers();
    void    addInt32(Int32    iVal);
    void    addInt16(Int16    iVal);
    void    fillLidilHeader(void *pLidilHdr, int Command, UInt16 CmdLen, UInt16 DataLen = 0);
    unsigned int getSwathWidth (int iStart, int iLast, int iWidth);
    DRIVER_ERROR    processSwath();
	DRIVER_ERROR    processBlackSwath(bool bBlackPresent, bool bColorPresent, short sColorSize, int LeftEdge, BYTE mask);
	DRIVER_ERROR    processColorSwath(bool bPhotoPresent, bool bColorPresent, bool bBlackPresent, short *sColorSize, BYTE mask);
	DRIVER_ERROR    processPhotoSwath(bool bPhotoPresent, bool bColorPresent, BYTE mask);
    bool    isBlankRaster(BYTE *raster, int width);
    void    applyShingleMask(int iCPlane, BYTE *input);
    DRIVER_ERROR loadSweepData (BYTE *imagedata, int imagesize);
    DRIVER_ERROR printSweep (UInt32 SweepSize,
                             bool ColorPresent,
                             bool BlackPresent,
						     bool PhotoPresent,
                             Int32 VerticalPosition,
                             Int32 LeftEdge,
                             Int32 RightEdge,
                             char PrintDirection,
                             Int16 sFirstNozzle,
                             Int16 sLastNozzle);

    PrintMode    *m_pPM;
    bool         m_bBidirectionalPrintingOn;
    bool         m_bPrevRowWasBlank;
    bool         m_bLittleEndian;
    char         m_cPassNumber;
    char         m_cPrintDirection;
    char         m_cKtoCVertAlign;
    char         m_cPtoCVertAlign;
    char         m_cPlaneNumber;
    UInt16       m_sSwathHeight;
    UInt16       m_sRefCount;
    int          m_iBitDepth;
    int          m_iNextRaster;
    int          m_iBlankRasters;
    int          m_iVertPosn;
    int          m_iImageWidth;
    int          m_iRasterCount;
    int          m_lidil_version;
    int          m_iBytesPerSwing;
    int          m_iColorPenResolution;
    int          m_iBlackPenResolution;
    int          m_iNumBlackNozzles;
    int          m_iNextColor;
    int          m_iLeftMargin;
    BYTE         *m_ldlCompressData;
    BYTE         *m_szCompressBuf;
    BYTE         ***m_SwathData;
    LidilCompress    *m_pLidilCompress;
};

#endif // LIDIL_H

