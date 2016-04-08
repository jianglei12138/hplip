/*****************************************************************************\
  Lidil.cpp : Implementation of Lidil class

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

#include "CommonDefinitions.h"
#include "Encapsulator.h"
#include "LidilCompress.h"
#include "Lidil.h"
#include "ColorMatcher.h"
#include "Halftoner.h"
#include "resources.h"
#include "ColorMaps.h"
#include "LidilPrintModes.h"
#include "PrinterCommands.h"

typedef union
{
    Int32    int_value;
    char     char_val[4];
} Int32Bytes;

typedef union
{
    Int16    int_value;
    char     char_val[2];
} Int16Bytes;

Lidil::Lidil() : Encapsulator()
{
    m_pPM = NULL;
    m_lidil_version = 1;
    m_iBytesPerSwing = 2;
    m_iColorPenResolution = 300;
    m_iBlackPenResolution = 1200;
    m_iNumBlackNozzles    = 400;
    m_cPrintDirection = PRNDRN_LEFTTORIGHT;
    m_SwathData = NULL;
    m_sRefCount = 6;
    m_iBlankRasters = 0;
    m_iRasterCount = 0;
    m_iNextRaster = 0; 
    m_iNextColor  = 0;
    m_iBitDepth = 1;
    m_bBidirectionalPrintingOn = true;
    m_cKtoCVertAlign = 12;
    m_cPtoCVertAlign = 6;
    m_pLidilCompress = NULL;
    m_szCompressBuf  = NULL;
    Int16Bytes    val;
    val.int_value = 0x0102;
    if (val.char_val[0] == 0x01)
        m_bLittleEndian = false;
    else
        m_bLittleEndian = true;
}

Lidil::~Lidil()
{
    if (m_pLidilCompress)
        delete m_pLidilCompress;
    if (m_SwathData)
        delete [] m_SwathData;
    if (m_szCompressBuf)
        delete [] m_szCompressBuf;
}

DRIVER_ERROR Lidil::Configure(Pipeline **pipeline)
{
    Pipeline    *p = NULL;
    Pipeline    *head;
    unsigned int width;
    ColorMatcher *pColorMatcher;
    int          iRows[MAXCOLORPLANES];
    unsigned int uiResBoost;
    head = *pipeline;

    if (m_pPM->BaseResX != m_pQA->horizontal_resolution ||
        m_pPM->BaseResY != m_pQA->vertical_resolution)
    {
        dbglog("Requested resolution not supported with requested printmode\n");
        return UNSUPPORTED_PRINTMODE;
    }

    for (int i = 0; i < MAXCOLORPLANES; i++)
    {
        iRows[i] = m_pPM->ResolutionX[i] / m_pPM->BaseResX;
    }
    uiResBoost = m_pPM->BaseResX / m_pPM->BaseResY;
    if (uiResBoost == 0)
        uiResBoost = 1;

    width = m_pMA->printable_width;

    pColorMatcher = new ColorMatcher(m_pPM->cmap, m_pPM->dyeCount, width);
    head = new Pipeline(pColorMatcher);
    Halftoner    *pHalftoner;
    pHalftoner = new Halftoner (m_pPM, width, iRows, uiResBoost, m_pPM->eHT == MATRIX);
    p = new Pipeline(pHalftoner);
    head->AddPhase(p);

    *pipeline = head;
    return NO_ERROR;
}

DRIVER_ERROR Lidil::StartJob(SystemServices *pSystemServices, JobAttributes *pJA)
{
    DRIVER_ERROR    err = NO_ERROR;
    m_pSystemServices = pSystemServices;

    m_pJA = pJA;
    m_pMA = &pJA->media_attributes;
    m_pQA = &pJA->quality_attributes;

    if (!strcmp(m_pJA->printer_platform, "dj4100"))
    {
        m_lidil_version = 2;
        m_iBytesPerSwing = 4;
        m_iColorPenResolution = 600;
    }
    else if (!strcmp(m_pJA->printer_platform, "dj2600"))
    {
        m_lidil_version = 2;
        m_iBytesPerSwing = 4;
        m_iColorPenResolution = 600;
        m_iBlackPenResolution = 600;
        m_iNumBlackNozzles = 336;
    }
    if (m_pQA->print_quality == BEST_QUALITY && m_pQA->media_type == MEDIATYPE_PHOTO)
    {
        m_iBitDepth = 2;
    }
    cur_pcl_buffer_size = PCL_BUFFER_SIZE;
    pcl_buffer = new BYTE[cur_pcl_buffer_size + 2];
    if (pcl_buffer == NULL)
    {
        return ALLOCMEM_ERROR;
    }
    memset(pcl_buffer, 0, cur_pcl_buffer_size);
    cur_pcl_buffer_ptr = pcl_buffer;

    if (!selectPrintMode())
    {
        dbglog("selectPrintMode failed, PrintMode name = %s\n", m_pQA->print_mode_name);
        return UNSUPPORTED_PRINTMODE;
    }
    
    
    if (m_pPM->BaseResX != m_pQA->horizontal_resolution ||
        m_pPM->BaseResY != m_pQA->vertical_resolution)
    {
        dbglog("Requested resolution not supported with requested printmode\n");        
		dbglog(" m_pPM->BaseResX = %d\n",m_pPM->BaseResX);
		dbglog(" m_pPM->BaseResY = %d\n",m_pPM->BaseResY);
		dbglog(" m_pQA->horizontal_resolution = %d\n",m_pQA->horizontal_resolution);
		dbglog(" m_pQA->vertical_resolution = %d\n",m_pQA->vertical_resolution);    
        return UNSUPPORTED_PRINTMODE;
    }

    m_iVertPosn = (m_pMA->printable_start_y * DEVUNITS_XBOW) / m_pQA->vertical_resolution;
    m_iLeftMargin = (m_pMA->printable_start_x * DEVUNITS_XBOW) / m_pQA->vertical_resolution;
    if (m_pJA->print_borderless)
    {
        m_iVertPosn = (-m_pMA->vertical_overspray * DEVUNITS_XBOW) / (2 * m_pQA->vertical_resolution);
        m_iLeftMargin = (-m_pMA->horizontal_overspray * DEVUNITS_XBOW) / (2 * m_pQA->horizontal_resolution);
    }

    err = allocateSwathBuffers();
    if (err != NO_ERROR)
    {
        dbglog("allocateSwathBuffers failed, err = %d\n", err);
        return err;
    }

    addToHeader(LdlSync, sizeof(LdlSync));
    cur_pcl_buffer_ptr[SYNC_CMD_OPT_SIZE] = FRAME_SYN;
    err = m_pSystemServices->Send(pcl_buffer, SYNCSIZE);
    cur_pcl_buffer_ptr = pcl_buffer;
    addToHeader(LdlSyncComplete, sizeof(LdlSyncComplete));
    addToHeader(LdlReset, sizeof(LdlReset));
    UInt16    mem_needed = SIZEOF_LDLHDR + SIZEOF_LDL_JOB_CMDOPT + SIZEOF_LDLTERM;
    if (m_lidil_version == 2)
    {
        mem_needed += 4;
    }
    fillLidilHeader(NULL, eLDLStartJob, mem_needed);
    *cur_pcl_buffer_ptr++ = OPERATION_STJOB;
    addInt32(m_pJA->job_id);
    if (m_lidil_version == 2)
        addInt32(0);
    *cur_pcl_buffer_ptr++ = FRAME_SYN;
    err = Cleanup();

//    m_pLidilCompress = new LidilCompress(m_bLittleEndian);
    return err;
}

DRIVER_ERROR Lidil::EndJob()
{
    DRIVER_ERROR    err = NO_ERROR;
    UInt16    mem_needed = SIZEOF_LDLHDR + SIZEOF_LDL_JOB_CMDOPT + SIZEOF_LDLTERM;
    Cleanup();
    memset(pcl_buffer, 0, cur_pcl_buffer_size);
    fillLidilHeader(NULL, eLDLEndJob, mem_needed);
    *cur_pcl_buffer_ptr++ = OPERATION_ENDJOB;
    addInt32(m_pJA->job_id);
    *cur_pcl_buffer_ptr++ = FRAME_SYN;
    addToHeader(LdlSync, sizeof(LdlSync));
    cur_pcl_buffer_ptr += SYNC_CMD_OPT_SIZE;
    *cur_pcl_buffer_ptr++ = FRAME_SYN;
    cur_pcl_buffer_ptr = pcl_buffer + SYNCSIZE + mem_needed;
    addToHeader(LdlSyncComplete, sizeof(LdlSyncComplete));
    addToHeader(LdlReset, sizeof(LdlReset));
    err = Cleanup();
    return err;
}

void Lidil::CancelJob()
{
    UInt16 mem_needed = SIZEOF_LDLHDR + SIZEOF_LDL_JOB_CMDOPT
                        + SIZEOF_LDLTERM;
    fillLidilHeader (NULL, eLDLControl, mem_needed);
    addInt32 (m_pJA->job_id);
    *cur_pcl_buffer_ptr++ = OPERATION_CANCJOB;
    *cur_pcl_buffer_ptr++ = FRAME_SYN;
    Cleanup();
}

DRIVER_ERROR Lidil::StartPage(JobAttributes *pJA)
{
    DRIVER_ERROR    err = NO_ERROR;
    m_pJA = pJA;
    m_pMA = &pJA->media_attributes;
    m_pQA = &pJA->quality_attributes;
    UInt32 mem_needed = SIZEOF_LDLHDR  + SIZEOF_LDL_LDPAGE_CMDOPT
                                       + SIZEOF_LDL_LDPAGE_OPTFLDS
                                       + SIZEOF_LDLTERM;

    memset (cur_pcl_buffer_ptr, 0, mem_needed);

    fillLidilHeader (NULL, eLDLLoadPage, (UInt16) mem_needed);
    *cur_pcl_buffer_ptr++ = m_pQA->media_type;;
    *cur_pcl_buffer_ptr++ = MEDIASRC_MAINTRAY;
    *cur_pcl_buffer_ptr++ = MEDIADEST_MAINBIN;
    *cur_pcl_buffer_ptr++ = m_pQA->print_quality;
    *cur_pcl_buffer_ptr++ = SPECLOAD_NONE;

    addInt32((Int32) (m_pMA->physical_width * DEVUNITS_XBOW / m_pQA->horizontal_resolution));
    addInt32((Int32) (m_pMA->physical_height * DEVUNITS_XBOW / m_pQA->vertical_resolution));

    addInt32(MEDIALD_SPEED|NEED_TO_SERVICE_PERIOD|MINTIME_BTW_SWEEP);

    // set up the option fields
    *cur_pcl_buffer_ptr++ = 4; // MediaLoadSpeed;
    *cur_pcl_buffer_ptr++ = 0; // NeedToServicePeriod;
    addInt16 (200);           // MinTimeBetweenSweeps

    *cur_pcl_buffer_ptr++ = FRAME_SYN;
    err = Cleanup ();
    return err;
}

DRIVER_ERROR Lidil::SendCAPy(int iOffset)
{
    DRIVER_ERROR    err = NO_ERROR;
#if 0
    if (m_iRasterCount == 0)
    {
        m_iBlankRasters += iOffset;
        return err;
    }
#endif
    int     iCount = m_pPM->dyeCount * m_iBitDepth;
    if (m_iBitDepth == 2 && m_pPM->dyeCount != 6)
        iCount++;

    RASTERDATA    rasterdata;
    memset(&rasterdata, 0, sizeof(rasterdata));
    while (iOffset > 0)
    {
        for (int i = 0; i < iCount; i++)
        {
            err = Encapsulate (&rasterdata, 0);
            ERRCHECK;
        }
        iOffset--;
    }
    return err;
}

DRIVER_ERROR Lidil::FormFeed()
{
    DRIVER_ERROR    err = NO_ERROR;
    int icount = 0;
    int iCurNumRasters = m_iRasterCount;
    if ((m_pQA->print_quality == DRAFT_QUALITY || m_pQA->vertical_resolution == 300) && m_iRasterCount)
        icount = 1;
    else if (m_pQA->print_quality != DRAFT_QUALITY)
    {
        icount = 4 * m_iBitDepth;
        iCurNumRasters = m_sSwathHeight * m_pPM->dyeCount;
    }

    int i, j, n;
    n = m_sSwathHeight / (4 * m_iBitDepth);

    n = n * (m_cPassNumber + 1) - m_iNextRaster;
    for (i = 0; i < (int) m_pPM->dyeCount; i++)
    {
        for (j = 0; j < n; j++)
            memset (m_SwathData[i][m_iNextRaster+j], 0, m_iImageWidth * m_iBitDepth);
    }
    m_iNextRaster += n;
    n = m_sSwathHeight / (4 * m_iBitDepth);

    while (icount)
    {
        m_iRasterCount = iCurNumRasters;
        err = processSwath ();
        if (err != NO_ERROR)
            break;
        icount--;
        if (m_iNextRaster >= m_sSwathHeight)
            m_iNextRaster = 0;
        for (i = 0; i < (int) m_pPM->dyeCount; i++)
        {
            for (j = 0; j < n; j++)
                memset (m_SwathData[i][m_iNextRaster+j], 0, m_iImageWidth * m_iBitDepth);
        }
        m_iNextRaster += n;
    }

    UInt16 mem_needed = SIZEOF_LDLHDR
                        + SIZEOF_LDL_EJPAGE_CMDOPT
                        + SIZEOF_LDL_EJPAGE_OPTFLDS
                        + SIZEOF_LDLTERM;

    fillLidilHeader(NULL, eLDLEjectPage, mem_needed);

    addInt32 (MEDIA_EJSPEED);

    *cur_pcl_buffer_ptr++ = 15;
    *cur_pcl_buffer_ptr++ = FRAME_SYN;
    if(err == NO_ERROR)
    {
        err = Cleanup();
    }

    m_sRefCount = 6;
    m_iBlankRasters = 0;
    m_iVertPosn = (m_pMA->printable_start_y * DEVUNITS_XBOW) / m_pQA->vertical_resolution;
    m_iRasterCount = 0;
    m_iNextRaster = 0;
    m_iNextColor  = 0;

    if (m_pJA->print_borderless)
    {
        m_iVertPosn = (int) (-m_pMA->vertical_overspray * DEVUNITS_XBOW) / (2 * m_pQA->vertical_resolution);
    }

    if (m_pQA->print_quality != DRAFT_QUALITY)
        m_iRasterCount = (m_sSwathHeight - m_sSwathHeight / 4) * m_pPM->dyeCount;

    if (m_pQA->print_quality == DRAFT_QUALITY && m_pQA->vertical_resolution != 300)
    {
        m_iRasterCount = (m_sSwathHeight - m_sSwathHeight / (4*m_iBitDepth)) * m_pPM->dyeCount;
        m_iVertPosn -= (((m_sSwathHeight - m_sSwathHeight / (4*m_iBitDepth)) * 600 / m_pQA->vertical_resolution) * DEVUNITS_XBOW / 600);
    }
    m_cPassNumber = 0;

    if (m_pJA->print_borderless)
    {
        if (m_iVertPosn < -850) m_iVertPosn = -850;
    }
    else
    {
        if (m_iVertPosn < -600) m_iVertPosn = -600;
    }
    if (m_iBitDepth == 2)
        m_iVertPosn += 6;

    for (i = 0; i < (int) m_pPM->dyeCount; i++)
    {
        for (int j = 0; j < m_sSwathHeight; j++)
        {
            memset (m_SwathData[i][j], 0, m_iImageWidth);
        }
    }

    return err;
}

DRIVER_ERROR Lidil::Encapsulate(RASTERDATA *InputRaster, bool bLastPlane)
{
    DRIVER_ERROR    err = NO_ERROR;
    int iPlaneNum = 0;
    if (m_iBitDepth == 2)
    {
        if (m_pPM->dyeCount != 6)
        {
            if (m_cPlaneNumber == 0)
            {
                m_cPlaneNumber++;
                return NO_ERROR;
            }
        }
        int iCPlane;
        if (m_pPM->dyeCount == 6)
        {
            iPlaneNum = m_cPlaneNumber % 2;
        }
        else
        {
            iPlaneNum = (m_cPlaneNumber + 1) % 2;
        }
        int iRowNum = (m_iRasterCount / 6) * 2 + iPlaneNum;
        iRowNum = m_iNextRaster;
        if (m_pPM->dyeCount == 6)
        {
            iCPlane = m_cPlaneNumber / 2;
        }
        else
        {
            iCPlane = (m_cPlaneNumber - 1) / 2;
        }
        if (iPlaneNum == 0)
        {
            if (!InputRaster->rasterdata[COLORTYPE_COLOR])
            {
                m_bPrevRowWasBlank = true;
                memset (m_SwathData[iCPlane][iRowNum], 0, m_iImageWidth * 2);
            }
            else
            {
                m_bPrevRowWasBlank = false;
                memcpy (m_SwathData[iCPlane][iRowNum], InputRaster->rasterdata[COLORTYPE_COLOR], InputRaster->rastersize[COLORTYPE_COLOR]);
            }
        }
        if (m_pPM->dyeCount == 6)
        {
            m_cPlaneNumber = (m_cPlaneNumber + 1) % 12;
        }
        else
        {
            m_cPlaneNumber = (m_cPlaneNumber + 1) % 8;
        }

        if (iPlaneNum == 1)
        {
            if (InputRaster->rasterdata[COLORTYPE_COLOR] || !m_bPrevRowWasBlank)
            {
                applyShingleMask(iCPlane, InputRaster->rasterdata[COLORTYPE_COLOR]);
            }
            else
            {
                memset (m_SwathData[iCPlane][m_iNextRaster+1], 0, m_iImageWidth * 2);
            }

            if (m_pPM->dyeCount == 6)
            {
                m_cPlaneNumber = m_cPlaneNumber % 12;
            }
            else
            {
                m_cPlaneNumber = m_cPlaneNumber % 7;
            }
        }
    }
    else
    {
        if (InputRaster->rasterdata[COLORTYPE_COLOR] == NULL)
            memset (m_SwathData[m_iNextColor][m_iNextRaster], 0, m_iImageWidth);
        else
            memcpy (m_SwathData[m_iNextColor][m_iNextRaster], InputRaster->rasterdata[COLORTYPE_COLOR], m_iImageWidth);
    }
    m_iRasterCount++;
    if (m_iBitDepth == 1 || (m_iBitDepth == 2 && iPlaneNum == 1))
        m_iNextColor++;
    if (m_iNextColor == (int) m_pPM->dyeCount)
    {
        m_iNextColor = 0;
        if (m_iBitDepth == 2)
            m_iNextRaster += 2;
        else
            m_iNextRaster++;
    }
    if (m_iRasterCount < (int) (m_sSwathHeight * m_pPM->dyeCount))
        return NO_ERROR;

    err = processSwath ();

    if (m_iNextRaster >= m_sSwathHeight)
    {
        m_iNextRaster = 0;
    }
    return err;
}

void    Lidil::fillLidilHeader(void *pLidilHdr, int Command, UInt16 CmdLen, UInt16 DataLen)
{
    memset(cur_pcl_buffer_ptr, 0, SIZEOF_LDLHDR);
    *cur_pcl_buffer_ptr++ = FRAME_SYN;
    addInt16(CmdLen);
    cur_pcl_buffer_ptr += 2;
    *cur_pcl_buffer_ptr++ = (BYTE) Command;
    addInt16(m_sRefCount++);
    addInt16(DataLen);
}

void    Lidil::addInt32(Int32    iVal)
{
    Int32Bytes    val;
    val.int_value = iVal;
    if (m_bLittleEndian)
    {
        for (int i = 3; i > -1; i--)
        *cur_pcl_buffer_ptr++ = val.char_val[i];
    }
    else
    {
        memcpy(cur_pcl_buffer_ptr, val.char_val, 4);
        cur_pcl_buffer_ptr += 4;
    }
}

void    Lidil::addInt16(Int16    iVal)
{
    Int16Bytes    val;
    val.int_value = iVal;
    if (m_bLittleEndian)
    {
        *cur_pcl_buffer_ptr++ = val.char_val[1];
    *cur_pcl_buffer_ptr++ = val.char_val[0];
    }
    else
    {
        memcpy(cur_pcl_buffer_ptr, val.char_val, 2);
        cur_pcl_buffer_ptr += 2;
    }
}


bool Lidil::selectPrintMode(int index)
{
    PrintMode    *p = lidil_print_modes_table[index].print_modes;
    int iPMIndex = 0; 
    
    if (!strcmp(m_pJA->printer_platform, "dj4100") || (!strcmp(m_pJA->printer_platform, "dj2600")))
    { 
    	//Encapsulator for Viper Trim class products is not written properly, hence mapping the Index to
    	//old values.   
    	iPMIndex = PQ_Cartridge_Map_ViperTrim[m_pJA->integer_values[2]][m_pJA->integer_values[1]];      
    }
    else
    {
    	iPMIndex = PQ_Cartridge_Map[m_pJA->integer_values[2]][m_pJA->integer_values[1]];
    									//m_pJA->integer_values[1] is basically cupsInteger1 value given in PPD.
    									//m_pJA->integer_values[2] is basically cupsInteger2 value given in PPD.    
    }

    dbglog("CupeInteger1 = [%d]\n",m_pJA->integer_values[1]); 
    dbglog("CupeInteger2 = [%d]\n",m_pJA->integer_values[2]); 
    dbglog("PrintMode Index = [%d]\n",iPMIndex); 
    
    if( -1 == iPMIndex)
    {
    	dbglog("Unsupported Cartridge and Print Quality combination..\n");
    	return false;
    }
            
    for (int i = 0; i < lidil_print_modes_table[index].count; i++, p++)
    {
        if (i == iPMIndex)
        {
            dbglog("Print Mode = [%s]\n",p->name); 
            m_pPM = p;
            return true;
        }
    }
    return false;
}

/*
bool Lidil::selectPrintMode(int index)
{
    PrintMode    *p = lidil_print_modes_table[index].print_modes;    
    for (int i = 0; i < lidil_print_modes_table[index].count; i++, p++)
    {
        if (!strcmp(m_pJA->quality_attributes.print_mode_name, p->name))
        {        
            m_pPM = p;
            return true;
        }
    }
    return false;
}*/


bool Lidil::selectPrintMode()
{
    if (m_pJA->printer_platform[0] == 0)
    {
        dbglog("printer_platform is undefined\n");
        return false;
    }
    for (unsigned int i = 0; i < sizeof(lidil_print_modes_table) / sizeof(lidil_print_modes_table[0]); i++)
    {
        if (!strcmp(m_pJA->printer_platform, lidil_print_modes_table[i].printer_platform_name))
        {
            return selectPrintMode(i);
        }
    }
    dbglog("Unsupported printer_platform: %s\n", m_pJA->printer_platform);
    return false;
}

DRIVER_ERROR Lidil::loadSweepData (BYTE *imagedata, int imagesize)
{
    UInt16 mem_needed = SIZEOF_LDLHDR + SIZEOF_LDL_LDSWPDATA_CMDOPT
                                      + SIZEOF_LDLTERM;
    cur_pcl_buffer_ptr = pcl_buffer;
    memset (pcl_buffer, 0, mem_needed);
    if (mem_needed < LDLPACKET_MINSIZE)
    {
        memset(pcl_buffer + mem_needed-1, 0xFF, LDLPACKET_MINSIZE - mem_needed);
    mem_needed = LDLPACKET_MINSIZE;
    }

    BYTE    *compressed_dataptr = imagedata;
    UInt16  compressed_size     = imagesize;

    if (m_pLidilCompress)
    {
        m_pLidilCompress->Init ((UInt16 *) (imagedata+16), imagesize);
        m_pLidilCompress->CompressData ();
        m_pLidilCompress->GetFrameInfo (&compressed_dataptr, &compressed_size);
    }

    fillLidilHeader (NULL, eLDLLoadSweepData, mem_needed, compressed_size);
    addInt16 (imagesize);
    pcl_buffer[15] = FRAME_SYN;

    memcpy (compressed_dataptr, pcl_buffer, 16);
    cur_pcl_buffer_ptr = pcl_buffer;
    return (sendBuffer ((const BYTE *) compressed_dataptr, compressed_size+16));
}

DRIVER_ERROR Lidil::printSweep (UInt32 SweepSize,
                                bool ColorPresent,
                                bool BlackPresent,
                    bool PhotoPresent,
                                Int32 VerticalPosition,
                                Int32 LeftEdge,
                                Int32 RightEdge,
                                char PrintDirection,
                                Int16 sFirstNozzle,
                                Int16 sLastNozzle)
{
    DRIVER_ERROR    err;
    // determine how many colors will be generated
    UInt16 colorcount = 0;
    UInt32  uiAffectedColors = 0;
    if (ColorPresent == true) colorcount += 3;
    if (BlackPresent == true) colorcount++;
    if (PhotoPresent == true) 
    {
        if (ColorPresent == false)
            colorcount++;
        else
            colorcount+=3;
    }

    UInt16  mem_needed;
    if (m_lidil_version == 1)
    {
        mem_needed =   SIZEOF_LDLHDR
                         + SIZEOF_LDL_PRTSWP_CMDOPT
                         + SIZEOF_LDL_PRTSWP_OPTFLDS
                         + SIZEOF_LDL_PRTSWP_COLROPT * colorcount
                         + SIZEOF_LDLTERM;
    
        if (colorcount != 0)
            mem_needed += SIZEOF_LDL_COLROPT_ACTIVECOLR;
    }
    else
    {
        mem_needed =   SIZEOF_LDLHDR
                         + SIZEOF_LDL_PRTSWP_CMDOPT + 7
                         + SIZEOF_LDL_PRTSWP_OPTFLDS
                         + SIZEOF_LDL_PRTSWP_COLROPT + 4
                         + SIZEOF_LDLTERM;
    }

    memset (pcl_buffer, 0, mem_needed);
    cur_pcl_buffer_ptr = pcl_buffer;

    fillLidilHeader (NULL, eLDLPrintSweep, mem_needed);
    if (m_lidil_version == 2)
    {
        *cur_pcl_buffer_ptr++ = 1;    // Version number
    }
    addInt32 (SweepSize);
    addInt32 (VerticalPosition);
    addInt32 (m_iLeftMargin);
    if (m_lidil_version == 1)
    {
        // LIDIL First Version
        *cur_pcl_buffer_ptr++ = SWINGFMT_UNCOMPRSS;
    }
    else
    {
        // LIDIL Second Version
        *cur_pcl_buffer_ptr++ = 1;
    }
    *cur_pcl_buffer_ptr++ = PrintDirection;
    if (m_lidil_version == 2)
    {
        addInt32 (0); // Shingle mask
    }
    addInt32 (IPS_CARRSPEED|IPS_INIPRNSPEED|ACCURATEPOSN_NEEDED);
 // Carriage Speed - 25 for plain, 12 for photo
    if (m_pQA->print_quality == BEST_QUALITY && m_pQA->media_type == MEDIATYPE_PHOTO)
        *cur_pcl_buffer_ptr++ = 12;
    else
        *cur_pcl_buffer_ptr++ = 25;
    *cur_pcl_buffer_ptr++ = 4; // Initial Print Speed
    *cur_pcl_buffer_ptr++ = 1; // Need Accurate Position
    if (m_lidil_version == 2)
    {
        *cur_pcl_buffer_ptr++ = 1; // Number of entries in the sweep
    }

    // fill in the color information
    if (colorcount == 0)
    {
        *cur_pcl_buffer_ptr++ = NO_ACTIVE_COLORS;
        *cur_pcl_buffer_ptr++ = FRAME_SYN;
        // write out the data
        err = Cleanup();
        return err;
    }

    // figure out what are the active colors and fill in the optional color fields.

    UInt16 colrpresent = 0;
    UInt16 colr_found=0;
    UInt16 colormask = 0x01;
    UInt16 offset = eLDLBlack;
    UInt16 iDataRes;
    UInt16 iPrintRes;
    uiAffectedColors = offset;
    if (BlackPresent == true)
    {
    uiAffectedColors = 0x1;
    }
    if(BlackPresent == false && PhotoPresent == false)
    {
    offset = eLDLCyan;
    colormask=0x02;
    uiAffectedColors |= 0x0000000e;
    }
    else if (BlackPresent == false && PhotoPresent == true)
    {
        if (ColorPresent == true)
        {
            offset = eLDLCyan;
            colormask=0x02;
            uiAffectedColors |= 0x0000007e;
        }
        else
        {
            offset = eLDLLoBlack;
            colormask=0x40;
            uiAffectedColors |= 0x00000070;
        }
    }

    int actv_colr_index = cur_pcl_buffer_ptr - pcl_buffer;
    int iColorRes = 300;
    if (m_lidil_version == 1)
    {
        cur_pcl_buffer_ptr += 2;
    }
    else
    {
        iColorRes = 600;
    }
    for(UInt16 i = offset; colr_found < colorcount && i < eLDLMaxColor; i++)
    {
        colr_found++;
        colrpresent = colrpresent | colormask;

        if (m_lidil_version == 2)
        {
            addInt32 (uiAffectedColors);
        }
        addInt32 (LeftEdge);
        addInt32 (RightEdge);
        addInt32 (LeftEdge);
        addInt32 (RightEdge);

        if ((i == 0 && m_lidil_version == 1) || (BlackPresent && m_lidil_version == 2))
        {
            iDataRes = 600;
            iPrintRes = m_iBlackPenResolution;
        }
        else
        {
            iDataRes  = iColorRes; // 300;
            iPrintRes = iColorRes; // 300;
        }
        addInt16 (iDataRes);         // Vertical Data Resolution
        addInt16 (iPrintRes);        // Vertical Print Resolution

        if (m_lidil_version == 2)
        {
            addInt16 (m_pQA->horizontal_resolution * m_iBitDepth);   // Horizontal Data Resolution // Collie
        }
        else
        {
            addInt16 (m_pQA->horizontal_resolution);
        }

        if (m_pQA->horizontal_resolution == 300)
        {
            addInt16 (600);   // Force 2 drop for draft mode.
        }
        else
        {
            if (m_lidil_version == 2)
            {
                addInt16 (m_pQA->horizontal_resolution * m_iBitDepth);   // Horizontal Print Resolution // Collie
            }
            else
            {
            addInt16 (m_pQA->horizontal_resolution);
            }
        }
        addInt16 (sFirstNozzle);
        if (sLastNozzle == 0)
        {
            int     iTmp = m_iRasterCount / m_pPM->dyeCount;;
            if (m_lidil_version == 2)
            {
                addInt16 (sFirstNozzle - 1 + ((iTmp * iPrintRes) / (m_pQA->vertical_resolution * m_iBitDepth))); // Collie
            }
            else
            {
                addInt16 (sFirstNozzle - 1 + ((iTmp * iPrintRes) / (m_pQA->vertical_resolution)));
            }
        }
        else
        {
            addInt16 (sLastNozzle);
        }

        *cur_pcl_buffer_ptr++ = 0;    // Vertical Alignment
        colormask = colormask << 1;
        if (m_lidil_version == 2)
        {
            break;
        }
    }
    // write the active color field
    if (m_lidil_version == 1)
    {
        BYTE    *tmp = cur_pcl_buffer_ptr;
        cur_pcl_buffer_ptr = pcl_buffer + actv_colr_index;
        addInt16 (colrpresent);
        cur_pcl_buffer_ptr = tmp;
    }

    if (m_lidil_version == 2)
    {
        *cur_pcl_buffer_ptr++ = 0; // # of entries in the shingle array
    }
    *cur_pcl_buffer_ptr++ = FRAME_SYN;

    // write out the data
    err = Cleanup();
    return err;
}

DRIVER_ERROR Lidil::allocateSwathBuffers()
{
    int size = m_pMA->printable_width;
    size = (size + 7) / 8;
    m_iImageWidth = size;
    m_ldlCompressData = NULL;
    if (m_lidil_version == 1)
    {
//        m_ldlCompressData = new comp_ptrs_t;
    }

    if (m_iBitDepth == 2)
    {
        size *= 2;
    }

    int     iCompressBufSize = (m_iBytesPerSwing / 2) * LDL_MAX_IMAGE_SIZE+20;    // additional space for load sweep command
    m_szCompressBuf = new BYTE[iCompressBufSize];
    if (m_szCompressBuf == NULL)
        return ALLOCMEM_ERROR;
    memset (m_szCompressBuf, 0, iCompressBufSize);

    BYTE    *p = NULL;
    int     iSwathBuffSize;

    m_sSwathHeight = SWATH_HEIGHT;

/*
 *  This swath buffer cannot be greater than the number of nozzles - 400 for black
 *  and 100 for color - we can use.
 */

    int    iAdjHeight = (m_iNumBlackNozzles / 32) * 8;
    if (m_pPM->dyeCount == 1)
    {
        m_sSwathHeight = m_sSwathHeight * 4;
        if ((int) (m_sSwathHeight * 1200 / m_pQA->vertical_resolution) > m_iNumBlackNozzles)
            m_sSwathHeight = m_pQA->vertical_resolution / 3;
    }
    else if (m_pQA->print_quality != DRAFT_QUALITY && m_pQA->vertical_resolution > 300 && m_pPM->dyeCount > 1 && m_iBitDepth == 1) // Collie change
    {
        m_sSwathHeight = (m_sSwathHeight / 4) * 4 * 2;
        if (m_sSwathHeight > 200)
            m_sSwathHeight = 200;
    }
    else if (m_iBitDepth == 2)
        m_sSwathHeight = iAdjHeight * 4;

    if (m_pQA->print_quality == NORMAL_QUALITY)
        m_sSwathHeight = iAdjHeight * 2;

    if (m_pQA->print_quality == DRAFT_QUALITY && m_pPM->dyeCount != 1)
    {
        m_sSwathHeight *= m_iBytesPerSwing / 2;
    }

    iSwathBuffSize = m_pPM->dyeCount * sizeof (BYTE *) +
                     m_pPM->dyeCount * m_sSwathHeight * sizeof (BYTE *) +
                     size * m_pPM->dyeCount * m_sSwathHeight;
    if ((p = new BYTE[iSwathBuffSize]) == NULL)
    {
        return ALLOCMEM_ERROR;
    }

    int     i;
    m_SwathData = (BYTE ***) p;
    for (i = 0; i < (int) m_pPM->dyeCount; i++)
        m_SwathData[i] = (BYTE **) (p + sizeof (BYTE *) * m_pPM->dyeCount + i * m_sSwathHeight * sizeof (BYTE *));

    for (i = 0; i < (int) m_pPM->dyeCount; i++)
    {
        p = (BYTE *) m_SwathData + sizeof (BYTE *) * m_pPM->dyeCount +
                m_pPM->dyeCount * m_sSwathHeight * sizeof (BYTE *) +
                size * m_sSwathHeight * i;
        for (int j = 0; j < m_sSwathHeight; j++)
        {
            memset (p, 0, size);
            m_SwathData[i][j] = p;
            p = p + size;
        }
    }

    if (m_pQA->print_quality != DRAFT_QUALITY && m_pQA->vertical_resolution != 300)
    {
        m_iRasterCount = (m_sSwathHeight - m_sSwathHeight / (4*m_iBitDepth)) * m_pPM->dyeCount;
        m_iVertPosn -= (((m_sSwathHeight - m_sSwathHeight / (4*m_iBitDepth)) * 600 / m_pQA->vertical_resolution) * DEVUNITS_XBOW / 600);
    }
    m_cPassNumber = 0;

    if (m_pJA->print_borderless)
    {
        if (m_iVertPosn < -850) m_iVertPosn = -850;
    }
    else
    {
        if (m_iVertPosn < -600) m_iVertPosn = -600;
    }
    if (m_iBitDepth == 2)
        m_iVertPosn += 6;
    return NO_ERROR;
}

unsigned int Lidil::getSwathWidth (int iStart, int iLast, int iWidth)
{
    int k;
    int i, j;
    for (i = iWidth - 1; i > -1; i--)
    {
        for (j = iStart; j < iLast; j++)
        {
            for (k = m_iRasterCount / m_pPM->dyeCount-1; k >= 0; k--)
            {

                if (m_SwathData[j][k][i])
                {
                    return (i+1);
                }
            }
        }
    }

    return 0;
}

DRIVER_ERROR Lidil::processSwath()
{
    DRIVER_ERROR    err = NO_ERROR;
    Int16           sCurSwathHeight = m_iRasterCount / m_pPM->dyeCount;
    m_iVertPosn += ((m_iBlankRasters) * 600 / m_pQA->vertical_resolution) * DEVUNITS_XBOW / 600;
    m_iBlankRasters = 0;

    bool    bColorPresent = true;
    bool    bBlackPresent = true;
    bool    bPhotoPresent = true;
    short   sColorSize = 0;
    int     LeftEdge = 0;
    BYTE    csavMask;

    if (m_pPM->dyeCount == 1)
    {
        bColorPresent = false;
        bPhotoPresent = false;
    }
    if (m_pPM->dyeCount == 3)
    {
        bBlackPresent = false;
        bPhotoPresent = false;
    }
    if (m_pPM->dyeCount == 6)
    {
        bBlackPresent = false;
    }
    if (m_pPM->dyeCount == 4)
    {
        bPhotoPresent = false;
    }

    if (!m_bBidirectionalPrintingOn)
        m_cPrintDirection = PRNDRN_LEFTTORIGHT;

    int     iStartRaster = m_cPassNumber % (2 * m_iBitDepth);
    BYTE    mask = 0xFF;

    if (m_lidil_version == 2)
    {
        iStartRaster = 0;   // Version 2 - REVISIT
    }

    if (m_pQA->print_quality != DRAFT_QUALITY && m_pQA->vertical_resolution != 300)
    {
        if ((m_cPassNumber % (4 * m_iBitDepth)) < (2 * m_iBitDepth))
            mask = 0xAA;
        else
            mask = 0x55;
    }
    csavMask = mask;

/*
 *  Check if RefCount is close to overflow of 65k.
 */

    if (m_sRefCount > 64000)
        m_sRefCount = 6;

/*
 *  Photo Swath
 */

    err = processPhotoSwath(bPhotoPresent, bColorPresent, mask);
    if (err != NO_ERROR)
        return err;

/*
 *  Color Swath
 */

    err = processColorSwath(bPhotoPresent, bColorPresent, bBlackPresent, &sColorSize,  mask);
    if (err != NO_ERROR)
        return err;
/*
 *  Black Swath
 */

    err = processBlackSwath(bBlackPresent, bColorPresent, sColorSize, LeftEdge, csavMask);

    m_iRasterCount = 0;

    if (m_pQA->print_quality != DRAFT_QUALITY && m_pQA->vertical_resolution != 300)
    {
        m_cPassNumber = (m_cPassNumber + 1) % (4 * m_iBitDepth);
        m_iVertPosn += ((((sCurSwathHeight/(4 * m_iBitDepth))) * 600 / m_pQA->vertical_resolution) * DEVUNITS_XBOW / 600) / m_iBitDepth;
        if (m_iBitDepth == 1)
        {
            if (m_cPassNumber % 2)
                m_iVertPosn += 4;
            else
                m_iVertPosn -= 4;
        }
        else
        {
            m_iVertPosn -= 2;
            if ((m_cPassNumber % 4) == 0)
                  m_iVertPosn += (DEVUNITS_XBOW / m_iColorPenResolution);
        }
        m_iRasterCount = (sCurSwathHeight - sCurSwathHeight / (4 * m_iBitDepth)) * m_pPM->dyeCount;
    }
    else
    {
        m_iVertPosn += ((sCurSwathHeight * 4 * 600) / m_pQA->vertical_resolution);

    }

    return err;
}

DRIVER_ERROR Lidil::processPhotoSwath(bool    bPhotoPresent,
                                      bool    bColorPresent,
                                      BYTE    mask)
{
    if (!bPhotoPresent)
    {
        return NO_ERROR;
    }

    BYTE            csavMask = mask;
    int             iOffset = 0;
    int             i;
    int             j;
    int             n;
    int             count;
    int             size;
    int             start;
    DRIVER_ERROR    err = NO_ERROR;
    int             sCurSwathHeight = m_iRasterCount / m_pPM->dyeCount;
    unsigned int    uiSwathSize;
    int             LeftEdge = 0;
    int             RightEdge;
    int             iStartRaster = 0;
    int             delta;
    int    iColors    = 1;
    int    LastColor  = 1;
    int    StartColor = 0;
    int    width = m_iImageWidth;
    if (bColorPresent)
    {
        iColors = 6;
        LastColor = 6;
        StartColor = 0;
        if (m_lidil_version != 1)
        {
            width *= m_iBitDepth;
        }
    }
    size = getSwathWidth (StartColor, LastColor, width);
    if (size == 0)
        return NO_ERROR;

    if (size % m_iBytesPerSwing)
        size = ((size/m_iBytesPerSwing) + 1) * m_iBytesPerSwing;
    if (m_lidil_version == 1)
    {
        RightEdge = LeftEdge + (size * 8 * 600 / m_pQA->horizontal_resolution - 1 * (600 / m_pQA->vertical_resolution)) *
                                (DEVUNITS_XBOW / 600);
    }
    else
    {
        RightEdge = LeftEdge + (size * 8 * 600 / m_pQA->horizontal_resolution - 1 * (600 / m_pQA->vertical_resolution)) *
                                (DEVUNITS_XBOW / (600 * m_iBitDepth));
    }
    Int16   sLastNozzle;
    Int16   sFirstNozzle = 1;
    unsigned int    uSweepSize;
    int     jDelta = m_pQA->vertical_resolution / m_iColorPenResolution;
    jDelta *= m_iBitDepth;

    uiSwathSize = size * iColors * sCurSwathHeight / jDelta;

    uSweepSize = sCurSwathHeight * m_iBytesPerSwing / jDelta;
    n = LDL_MAX_IMAGE_SIZE / (uSweepSize);
    count = 0;

    if (m_iBitDepth == 2)
        iStartRaster = (4 - (iStartRaster+1)) % 4;

    if (m_lidil_version == 2)
    {
        iStartRaster = 0;   // Collie - REVISIT
    }

    sLastNozzle = sFirstNozzle - 1 + sCurSwathHeight / jDelta;

    BYTE *cb = m_szCompressBuf + 16;    // load sweep command
    memset (m_szCompressBuf, 0x0, LDL_MAX_IMAGE_SIZE * (m_iBytesPerSwing / 2));

    // 1200 dpi split into two
    int    ib = 0;

    if (m_pQA->vertical_resolution > 300 && m_pQA->print_quality != DRAFT_QUALITY)
    {
        iOffset = (sCurSwathHeight / (4 * m_iBitDepth));
        iOffset = iOffset + iOffset * ((m_cPassNumber) % (4 * m_iBitDepth));
    }

    BYTE    cVertAlign = 0;

    if (bColorPresent)
    {
        cVertAlign = m_cPtoCVertAlign;
    }

    for (ib = 0; ib < (int) m_iBitDepth; ib++)
    {
        if (m_cPrintDirection == PRNDRN_RIGHTTOLEFT)
        {
            start = size - m_iBytesPerSwing;
            delta = -m_iBytesPerSwing;
        }
        else
        {
            start = 0;
            delta = m_iBytesPerSwing;
        }

        err = printSweep (uiSwathSize, bColorPresent, false, bPhotoPresent,
                                  m_iVertPosn+cVertAlign, LeftEdge, RightEdge, m_cPrintDirection,
                                  sFirstNozzle, sLastNozzle);
                ERRCHECK;

        i = start + ib * m_iImageWidth;     // 1200 dpi split into two
        for (int l = 0; l < size; l += m_iBytesPerSwing)   // Collie
        {
            for (int k = StartColor+1; k < LastColor; k++)
            {
                mask = csavMask;
                for (j = iOffset + iStartRaster; j < sCurSwathHeight; j += jDelta)
                {
                    for (int is = 0; is < m_iBytesPerSwing; is++)
                    {
                        *cb++ = m_SwathData[k][j][i+is]   & mask;
                    }
                    mask = ~mask;
                }
                for (j = iStartRaster; j < iOffset; j += jDelta)
                {
                    for (int is = 0; is < m_iBytesPerSwing; is++)
                    {
                        *cb++ = m_SwathData[k][j][i+is]   & mask;
                    }
                    mask = ~mask;
                }

                count++;
                if (count == n)
                {
                    err = loadSweepData (m_szCompressBuf, (unsigned int) (cb - m_szCompressBuf-16));
                    memset (m_szCompressBuf, 0, LDL_MAX_IMAGE_SIZE * (m_iBytesPerSwing / 2));
                    cb = m_szCompressBuf+16;
                    count = 0;
                    ERRCHECK;
                }
            }
            mask = csavMask;
            for (j = iOffset + iStartRaster; j < sCurSwathHeight; j += jDelta)
            {
                for (int is = 0; is < m_iBytesPerSwing; is++)
                {
                    *cb++ = m_SwathData[0][j][i + is]   & mask;
                }
                mask = ~mask;
            }
            for (j = iStartRaster; j < iOffset; j += jDelta)
            {
                for (int is = 0; is < m_iBytesPerSwing; is++)
                {
                    *cb++ = m_SwathData[0][j][i + is]   & mask;
                }
                mask = ~mask;
            }

            count++;
            if (count == n)
            {
                err = loadSweepData (m_szCompressBuf, (unsigned int) (cb - m_szCompressBuf-16));
                memset (m_szCompressBuf, 0, LDL_MAX_IMAGE_SIZE * (m_iBytesPerSwing / 2));
                cb = m_szCompressBuf+16;
                count = 0;
                ERRCHECK;
            }
            i = i + delta;
        }
        if (count != 0)
        {
            err = loadSweepData (m_szCompressBuf, (unsigned int) (cb - m_szCompressBuf-16));
            memset (m_szCompressBuf, 0, LDL_MAX_IMAGE_SIZE * (m_iBytesPerSwing / 2));
            cb = m_szCompressBuf+16;
            count = 0;
            ERRCHECK;
        }

        if (m_bBidirectionalPrintingOn)
            m_cPrintDirection = (m_cPrintDirection + 1) % 2;

        if (m_lidil_version == 2) // Collie
        {
            break;
        }
        LeftEdge += 2;
        RightEdge += 2;

    }   // 1200 dpi split into two - end of for ib = 0 loop
    return err;
}

DRIVER_ERROR Lidil::processColorSwath(bool    bPhotoPresent,
                                      bool    bColorPresent,
                                      bool    bBlackPresent,
                      short   *sColorSize,
                                      BYTE    mask)
{
    BYTE            csavMask = mask;
    int             iStartRaster = 0;
    int             LeftEdge = 0;
    int             RightEdge;
    unsigned int    start;
    int             size;
    unsigned int    delta;
    int             i;
    int             j;
    int             n;
    int             count;
    int             iOffset = 0;
    DRIVER_ERROR    err = NO_ERROR;
    int             sCurSwathHeight = m_iRasterCount / m_pPM->dyeCount;
    unsigned int    uiSwathSize;
    if (bPhotoPresent || !bColorPresent)
    {
        return NO_ERROR;
    }

    int    iColors = 3;
    int    LastColor = 4;
    int    StartColor = 1;
    if (!bBlackPresent)
    {
        StartColor = 0;
        LastColor  = 3;
    }
    if (m_lidil_version == 1)
    {
        // 1200 dpi split into two
        size = getSwathWidth (StartColor, LastColor, m_iImageWidth/* * m_iBitDepth*/);
    }
    else
    {
        size = getSwathWidth (StartColor, LastColor, m_iImageWidth * m_iBitDepth);
    }
    *sColorSize = size;
    if (size == 0)
    {
        return NO_ERROR;
    }

    if (size % m_iBytesPerSwing)
    size = ((size / m_iBytesPerSwing) + 1) * m_iBytesPerSwing;

    if (m_lidil_version == 1)
    {
    RightEdge = LeftEdge + (size * 8 * 600 / m_pQA->horizontal_resolution - 1 * (600 / m_pQA->vertical_resolution)) *
                (DEVUNITS_XBOW / 600);
    }
    else
    {
    RightEdge = LeftEdge + (size * 8 * 600 / m_pQA->horizontal_resolution - 1 * (600 / m_pQA->vertical_resolution)) *
                (DEVUNITS_XBOW / (600 * m_iBitDepth));
    }
    Int16   sLastNozzle;
    Int16   sFirstNozzle = 1;
    unsigned int    uSweepSize;
    int     jDelta = m_pQA->vertical_resolution / m_iColorPenResolution;
    jDelta *= m_iBitDepth;

    uiSwathSize = size * iColors * sCurSwathHeight / jDelta;

    uSweepSize = sCurSwathHeight * m_iBytesPerSwing / jDelta;
    n = LDL_MAX_IMAGE_SIZE / (uSweepSize);
    count = 0;

    if (m_iBitDepth == 2)
    {
    iStartRaster = (4 - (iStartRaster+1)) % 4;
    if (m_lidil_version == 2)
    {
        iStartRaster = m_cPassNumber % (m_iBitDepth);
    }
    }

    sLastNozzle = sFirstNozzle - 1 + sCurSwathHeight / jDelta;

    BYTE *cb = m_szCompressBuf + 16;    // load sweep command
    memset (m_szCompressBuf, 0x0, LDL_MAX_IMAGE_SIZE * (m_iBytesPerSwing / 2));

    // 1200 dpi split into two
    int    ib = 0;

    if (m_pQA->vertical_resolution > 300 && m_pQA->print_quality != DRAFT_QUALITY)
    {
    iOffset = (sCurSwathHeight / (4 * m_iBitDepth));
    iOffset = iOffset + iOffset * ((m_cPassNumber) % (4 * m_iBitDepth));
    }

    for (ib = 0; ib < (int) m_iBitDepth; ib++)
    {
    if (m_cPrintDirection == PRNDRN_RIGHTTOLEFT)
    {
        start = size - m_iBytesPerSwing;
        delta = -m_iBytesPerSwing;
    }
    else
    {
        start = 0;
        delta = m_iBytesPerSwing;
    }
    err = printSweep (uiSwathSize, bColorPresent, false, false,
              m_iVertPosn, LeftEdge, RightEdge, m_cPrintDirection,
              sFirstNozzle, sLastNozzle);
    ERRCHECK;

    i = start + ib * m_iImageWidth;     // 1200 dpi split into two
    for (int l = 0; l < size; l += m_iBytesPerSwing)   // Collie
    {
        for (int k = StartColor; k < LastColor; k++)
        {
        mask = csavMask;
        for (j = iOffset + iStartRaster; j < sCurSwathHeight; j += jDelta)
        {
            for (int is = 0; is < m_iBytesPerSwing; is++)
            {
            *cb++ = m_SwathData[k][j][i + is]   & mask;
            }
            mask = ~mask;
        }
        for (j = iStartRaster; j < iOffset; j += jDelta)
        {
            for (int is = 0; is < m_iBytesPerSwing; is++)
            {
            *cb++ = m_SwathData[k][j][i + is]   & mask;
            }
            mask = ~mask;
        }

        count++;
        if (count == n)
        {
            err = loadSweepData (m_szCompressBuf, (unsigned int) (cb - m_szCompressBuf-16));
            memset (m_szCompressBuf, 0, LDL_MAX_IMAGE_SIZE * (m_iBytesPerSwing / 2));

            cb = m_szCompressBuf+16;
            count = 0;
            ERRCHECK;
        }

        }
        i = i + delta;

    }
    if (count != 0)
    {
        err = loadSweepData (m_szCompressBuf, (unsigned int) (cb - m_szCompressBuf-16));
        memset (m_szCompressBuf, 0, LDL_MAX_IMAGE_SIZE * (m_iBytesPerSwing / 2));

        cb = m_szCompressBuf+16;
        count = 0;
        ERRCHECK;
    }

    LeftEdge += 2;
    RightEdge += 2;

    if (m_bBidirectionalPrintingOn)
        m_cPrintDirection = (m_cPrintDirection + 1) % 2;
    if (m_lidil_version == 2) // Collie
    {
        break;
    }

    }   // 1200 dpi split into two - end of for ib = 0 loop
    return err;
}

DRIVER_ERROR Lidil::processBlackSwath(bool     bBlackPresent,
                                      bool     bColorPresent,
                                      short    sColorSize,
                                      int      LeftEdge,
                                      BYTE     mask)
{
    Int32           RightEdge;
    unsigned int    start;
    unsigned int    delta;
    int             i;
    int             j;
    int             n;
    int             count;
    int             iOffset = 0;
    DRIVER_ERROR    err = NO_ERROR;
    int             sCurSwathHeight = m_iRasterCount / m_pPM->dyeCount;
    unsigned int    uiSwathSize;

    if (!bBlackPresent)
    {
        return NO_ERROR;
    }
    int size = getSwathWidth (0, 1, m_iImageWidth);
    if (size == 0)
        return NO_ERROR;

    if (size % m_iBytesPerSwing)
size = ((size/m_iBytesPerSwing) + 1) * m_iBytesPerSwing;

    RightEdge = LeftEdge + (size * 8 * 600 / m_pQA->horizontal_resolution - 1 * (600 / m_pQA->vertical_resolution)) * DEVUNITS_XBOW/600;
    if (m_iBitDepth != 2 && ((m_cPassNumber % 2) == 0 || m_pQA->print_quality == DRAFT_QUALITY))
    {
        Int16   sLastNozzle = 0;
        Int16   sFirstNozzle = 1;

        int     xDelta = 0;
        BYTE    cVertAlign = 0;

        if (bColorPresent)
        {
            cVertAlign = m_cKtoCVertAlign;
        }

        if (bColorPresent && sColorSize && m_bBidirectionalPrintingOn)
            m_cPrintDirection = PRNDRN_RIGHTTOLEFT;
        if (m_cPrintDirection == PRNDRN_RIGHTTOLEFT)
        {
            start = size - m_iBytesPerSwing;
            delta = -m_iBytesPerSwing;
        }
        else
        {
            start = 0;
            delta = m_iBytesPerSwing;
        }
        if (m_pQA->vertical_resolution == 300)
            xDelta = m_iBytesPerSwing;
        uiSwathSize = ((size/m_iBytesPerSwing) * sCurSwathHeight * m_iBytesPerSwing * (600 * m_iBitDepth)/ m_pQA->vertical_resolution);

        if (m_lidil_version == 2 && m_pPM->dyeCount != 1)
        {
            sFirstNozzle = 9;
        }

        err = printSweep (uiSwathSize, false, bBlackPresent, false,
            (m_iVertPosn + cVertAlign), LeftEdge, RightEdge, m_cPrintDirection, sFirstNozzle, sLastNozzle);
        ERRCHECK;

        i = start;
        BYTE *cb = m_szCompressBuf+16;
        memset (m_szCompressBuf, 0x0, LDL_MAX_IMAGE_SIZE * (m_iBytesPerSwing / 2));

        n = LDL_MAX_IMAGE_SIZE / (sCurSwathHeight * m_iBytesPerSwing * 600 / m_pQA->vertical_resolution);
        count = 0;
        iOffset = 0;
        if (m_pQA->vertical_resolution > 300 && m_pQA->print_quality != DRAFT_QUALITY)
        {
            iOffset = sCurSwathHeight / 4;
            iOffset = iOffset + iOffset * (m_cPassNumber % 4);
        }

        for (int l = 0; l < size; l += m_iBytesPerSwing) // Collie
        {
            for (j = iOffset; j < sCurSwathHeight; j++)
            {
                for (int is = 0; is < m_iBytesPerSwing; is++)
                {
                    *cb++ = m_SwathData[0][j][i + is]   & mask;
                }
                cb += xDelta;
            }
            for (j = 0; j < iOffset; j++)
            {
                for (int is = 0; is < m_iBytesPerSwing; is++)
                {
                    *cb++ = m_SwathData[0][j][i + is]   & mask;
                }
                cb += xDelta;
            }

            count++;
            if (count == n)
            {
                err = loadSweepData (m_szCompressBuf, (unsigned int) (cb - m_szCompressBuf-16));
                memset (m_szCompressBuf, 0, LDL_MAX_IMAGE_SIZE * (m_iBytesPerSwing / 2));

                cb = m_szCompressBuf+16;
                count = 0;
                ERRCHECK;
            }
            i = i + delta;
        }
        if (count != 0)
        {
            err = loadSweepData (m_szCompressBuf, (unsigned int) (cb - m_szCompressBuf-16));
            memset (m_szCompressBuf, 0, LDL_MAX_IMAGE_SIZE * (m_iBytesPerSwing / 2));

            cb = m_szCompressBuf+16;
            count = 0;
            ERRCHECK;
        }

        if (m_bBidirectionalPrintingOn)
            m_cPrintDirection = (m_cPrintDirection + 1) % 2;
    }
    return err;
}

bool Lidil::isBlankRaster(BYTE *raster, int width)
{
    if (*raster == 0 && !memcmp(raster+1, raster, width-1))
        return true;
    return false;
}

void Lidil::applyShingleMask(int iCPlane, BYTE *input)
{
    // do the dotmapping here
    BYTE    cbyte1, cbyte2;
    BYTE    c1, c2;
    int     j = 0;
    BYTE    r1b1 = 0;
    BYTE    r1b2 = 0;
    BYTE    r2b1 = 0;
    BYTE    r2b2 = 0;
    BYTE    bitmask[8] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};

    // Collie changes
    int     iNextBitPos = m_iImageWidth;
    int     iJIncrement = 1;
    if (m_lidil_version == 2)
    {
        iNextBitPos = 1;
        iJIncrement = 2;
    }

    // Previous row
    memcpy (m_szCompressBuf, m_SwathData[iCPlane][m_iNextRaster], m_iImageWidth);

    static  BYTE    rand_table[4][4] = {{0, 3, 1, 2},
                                        {3, 1, 2, 0},
                                        {1, 2, 0, 3},
                                        {2, 0, 3, 1}};

    BYTE    rt1, rt2;
    rt1 = m_iNextRaster % 4;

    for (int i = 0; i < m_iImageWidth; i++)
    {
        cbyte2 = m_szCompressBuf[i];
        cbyte1 = (input == NULL) ? 0 : input[i];

//      1200 dpi raster split into 2 600 dpi rasters

        r1b1 = 0;
        r1b2 = 0;
        r2b1 = 0;
        r2b2 = 0;

        for (int ibit = 0; ibit < 8; ibit++)
        {
            c1 = (cbyte1 & bitmask[ibit]) ? 1 : 0;
            c2 = (cbyte2 & bitmask[ibit]) ? 1 : 0;
            c1 = 2 * c1 + c2;

            rt2 = (i + ibit) % 4;
            rt2 = rand_table[rt1][rt2];

            if (c1 == 2)
            {
                if (rt2 == 0)
                {
                    r1b1 = r1b1 | (0xff & (cbyte1 & bitmask[ibit]));
                    r2b2 = r2b2 | (0xff & bitmask[ibit]);
                }
                else if (rt2 == 1)
                {
                    r1b2 = r1b2 | (0xff & bitmask[ibit]);
                    r2b1 = r2b1 | (0xff & bitmask[ibit]);
                }

                else if (rt2 == 2)
                {
                    r1b1 = r1b1 | (0xff & bitmask[ibit]);
                    r2b1 = r2b1 | (0xff & bitmask[ibit]);
                }
                else if (rt2 == 3)
                {
                    r1b2 = r1b2 | (0xff & bitmask[ibit]);
                    r2b2 = r2b2 | (0xff & bitmask[ibit]);
                }

            }
            else if (c1 == 1)
            {
                if (rt2 == 0)
                    r1b1 = r1b1 | (0xff & bitmask[ibit]);
                else if (rt2 == 1)
                    r1b2 = r1b2 | (0xff & bitmask[ibit]);
                else if (rt2 == 2)
                    r2b1 = r2b1 | (0xff & bitmask[ibit]);
                else
                    r2b2 = r2b2 | (0xff & bitmask[ibit]);

            }
            else if (c1 == 3)
            {
                r1b1 = r1b1 | (0xff & bitmask[ibit]);
                r1b2 = r1b2 | (0xff & bitmask[ibit]);
                r2b1 = r2b1 | (0xff & bitmask[ibit]);
                r2b2 = r2b2 | (0xff & bitmask[ibit]);
            }
        }
        m_SwathData[iCPlane][m_iNextRaster][j] = r1b1;
        m_SwathData[iCPlane][m_iNextRaster][j+iNextBitPos] = r1b2;
        m_SwathData[iCPlane][m_iNextRaster+1][j]   = r2b1;
        m_SwathData[iCPlane][m_iNextRaster+1][j+iNextBitPos] = r2b2;

        j += iJIncrement;
    }
}

