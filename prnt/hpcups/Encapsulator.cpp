/*****************************************************************************\
  Encapsulator.cpp : Encapsulator class implementation

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
#include "PrinterCommands.h"

Encapsulator::Encapsulator()
{
    page_number = 0;
    pcl_buffer = NULL;
    cur_pcl_buffer_size = 0;
    m_pJA = NULL;
    m_pMA = NULL;
    m_pQA = NULL;
}
Encapsulator::~Encapsulator()
{
    if (pcl_buffer) {
        delete [] pcl_buffer;
    }
}

DRIVER_ERROR Encapsulator::StartJob(SystemServices *pSystemServices, JobAttributes *pJA)
{
    DRIVER_ERROR    err = NO_ERROR;

    m_pSystemServices = pSystemServices;

    m_pJA = pJA;
    m_pMA = &pJA->media_attributes;
    m_pQA = &pJA->quality_attributes;

    cur_pcl_buffer_size = PCL_BUFFER_SIZE;
    pcl_buffer = new BYTE[cur_pcl_buffer_size + 2];
    if (pcl_buffer == NULL) {
        return ALLOCMEM_ERROR;
    }
    memset(pcl_buffer, 0, cur_pcl_buffer_size);
    cur_pcl_buffer_ptr = pcl_buffer;

    err = flushPrinterBuffer();

    struct    tm    *t;
    time_t    long_time;
    time(&long_time);
    t = localtime(&long_time);

    addToHeader(Reset, sizeof(Reset));
    addToHeader(UEL, sizeof(UEL));

//  Now add other header info


#ifndef UNITTESTING
    if (jobAttrPJLAllowed())
    {
        addToHeader("@PJL SET STRINGCODESET=UTF8\012");
        addToHeader("@PJL COMMENT=\"Job Start Time: %s\"\012", m_pJA->job_start_time);
        addToHeader("@PJL JOBNAME=\"%s\"\012", m_pJA->job_title);
        addToHeader("@PJL SET JOBNAME=\"%s\"\012", m_pJA->job_title);
        addToHeader("@PJL COMMENT=\"%s; %s; %s; %s\"\012",
                    m_pJA->printer_name, m_pJA->os_name, m_pJA->driver_name, m_pJA->driver_version);
        addToHeader("@PJL COMMENT=\"Username: %s; App Filename: %s; %d-%d-%d\"\012",
                    m_pJA->user_name, m_pJA->job_title, t->tm_mon + 1, t->tm_mday, t->tm_year + 1900);
        addToHeader("@PJL SET JOBATTR=\"JobAcct1=%s\"\012", m_pJA->user_name);
        addToHeader("@PJL SET JOBATTR=\"JobAcct2=%s\"\012", m_pJA->host_name);
        addToHeader("@PJL SET JOBATTR=\"JobAcct3=%s\"\012", m_pJA->domain_name);
        addToHeader("@PJL SET JOBATTR=\"JobAcct4=%4d%02d%02d%02d%02d%02d\"\012",
                    t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
        addToHeader("@PJL SET JOBATTR=\"JobAcct5=%s\"\012", m_pJA->uuid);

    //  Add start of job time stamp to the header
        addToHeader("@PJL SET TIMESTAMP=%4d%02d%02d%02d%02d%02d\012",
                    t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
        addToHeader("@PJL SET JOBATTR=\"JobAcct6=Spooler Subsystem App\"\012");
        addToHeader("@PJL SET JOBATTR=\"JobAcct7=\"%s\"\012", m_pJA->job_title);
        addToHeader("@PJL SET JOBATTR=\"JobAcct8=\"%s\"\012", m_pJA->user_name);
        addToHeader("@PJL SET JOBATTR=\"JobAcct9=(null)\"\012");

        addToHeader("@PJL SET USERNAME=\"%s\"\012", m_pJA->user_name);
    }
#endif

//  Add platform specific PJL here
    addJobSettings();

    return err;
}

DRIVER_ERROR Encapsulator::Send(const BYTE *pBuffer, int length)
{
    DRIVER_ERROR    err;
//  Dont' have any buffer allocated, just send the incoming data to the device
    if (pcl_buffer == NULL) {
        return sendBuffer(pBuffer, length);
    }

//  Add the incoming data to the printer data buffer if there is enough room, otherwise, flush the buffer

    if (cur_pcl_buffer_ptr + length > (pcl_buffer + cur_pcl_buffer_size)) {
        err = sendBuffer(static_cast<const BYTE *>(pcl_buffer), (cur_pcl_buffer_ptr - pcl_buffer));
        if (err != NO_ERROR)
            return err;
        cur_pcl_buffer_ptr = pcl_buffer;
    }

    if (length < cur_pcl_buffer_size) {
        memcpy(cur_pcl_buffer_ptr, pBuffer, length);
        cur_pcl_buffer_ptr += length;
    }
    else {
        err = sendBuffer(pBuffer, length);
    }
    return NO_ERROR;
} // Send

void Encapsulator::addToHeader(const BYTE *command_string, int length)
{
    memcpy(cur_pcl_buffer_ptr, command_string, length);
    cur_pcl_buffer_ptr += length;
}

void Encapsulator::addToHeader(const char *fmt, ...)
{
    va_list args;
    int     n;
    va_start(args, fmt);
    int     size = cur_pcl_buffer_size - (cur_pcl_buffer_ptr - pcl_buffer);

    n = vsnprintf((char *) cur_pcl_buffer_ptr, size - 1, fmt, args);
    cur_pcl_buffer_ptr += n;
    va_end(args);
}

DRIVER_ERROR Encapsulator::sendBuffer(const BYTE *pBuffer, int length)
{
    DRIVER_ERROR    err;
    if (length == 0) {
        return NO_ERROR;
    }
    err = m_pSystemServices->Send(pBuffer, length);
    return err;
}

DRIVER_ERROR Encapsulator::Cleanup()
{
    DRIVER_ERROR    err = NO_ERROR;
    if (pcl_buffer && cur_pcl_buffer_ptr - pcl_buffer > 0)
    {
        err = sendBuffer(static_cast<const BYTE *>(pcl_buffer), (cur_pcl_buffer_ptr - pcl_buffer));
    }
    cur_pcl_buffer_ptr = pcl_buffer;
    return err;
}

DRIVER_ERROR Encapsulator::FormFeed()
{
    DRIVER_ERROR    err;
    err = this->Send((const BYTE *) "\x1B*rC\x0C", 5);
    if (err == NO_ERROR) {
        return Cleanup();
    }
    return err;
}

DRIVER_ERROR Encapsulator::SendCAPy(int iOffset)
{
    DRIVER_ERROR    err = NO_ERROR;
    char            str[12];
    sprintf(str, "\x1b*b%dY", iOffset);
    err = this->Send ((const BYTE *) str, strlen(str));
    return err;
}

DRIVER_ERROR Encapsulator::EndJob()
{
    DRIVER_ERROR    err = NO_ERROR;
    err = m_pSystemServices->Send(Reset, sizeof(Reset));
    if (err == NO_ERROR)
        err = m_pSystemServices->Send(UEL, sizeof(UEL));
    return err;
}

void Encapsulator::CancelJob()
{
    BYTE    buffer[4098];
    memset(buffer, 0x0, sizeof(buffer));
    memcpy(buffer+4087, "\x1B%-12345X", 9);

//  This sequence is good for PCL3 printers. Update this for non-PCL printers.

    m_pSystemServices->Send((const BYTE *) buffer, 4096);
}

// Used only by LJMono and LJColor

void Encapsulator::sendJobHeader()
{
    char            szStr[256];
    int             top_margin = 0;

//  Set media source, type, size and quality modes.
//  Duplex and portrait mode - <ESC>&l<n>s, <ESC>&l0o

    addToHeader("\033&l%dh%dm%da%ds8c0o0E\033*o%dM", m_pJA->media_source, m_pQA->media_type,
                m_pMA->pcl_id, m_pJA->e_duplex_mode, m_pQA->print_quality);

    addToHeader("\033&u%dD\033*t%dR\033*r%dS", m_pQA->horizontal_resolution, m_pQA->vertical_resolution, m_pMA->printable_width);

/*
 *  Custom papersize command
 */

    if (m_pMA->pcl_id == CUSTOM_MEDIA_SIZE) {
        short   sWidth, sHeight;
        BYTE    b1, b2;
        sWidth  = static_cast<short>(m_pMA->physical_width);
        sHeight = static_cast<short>(m_pMA->physical_height);
        memcpy (szStr, "\x1B*o5W\x0E\x05\x00\x00\x00\x1B*o5W\x0E\x06\x00\x00\x00", 20);
        b1 = (BYTE) ((sWidth & 0xFF00) >> 8);
        b2 = (BYTE) (sWidth & 0xFF);
        szStr[8] = b1;
        szStr[9] = b2;
        b1 = (BYTE) ((sHeight & 0xFF00) >> 8);
        b2 = (BYTE) (sHeight & 0xFF);
        szStr[18] = b1;
        szStr[19] = b2;
        addToHeader((const BYTE *) szStr, 20);
    }

    const BYTE *pgrafMode = grafMode2;
    if (m_pJA->color_mode == 0)
    {
        configureRasterData();
        pgrafMode = grafMode3;
    }

    addToHeader((const BYTE *) grafStart, sizeof(grafStart));
    addToHeader((const BYTE *) pgrafMode, 5);
    addToHeader((const BYTE *) seedSame, sizeof(seedSame));

    top_margin = m_pMA->printable_start_y - ((m_pJA->mech_offset * m_pQA->actual_vertical_resolution)/1000);
    int left_margin = 0;
    if (m_pJA->integer_values[1] > 0)
    {
        left_margin = ((m_pJA->integer_values[1] * m_pQA->horizontal_resolution) / 100 - m_pMA->printable_width) / 2;
    }
    addToHeader("\x1b*p%dx%dY", left_margin, top_margin);

    return;
}


DRIVER_ERROR Encapsulator::preProcessRasterData(cups_raster_t **cups_raster, cups_page_header2_t* firstpage_cups_header, char* pSwapedPagesFileName)
{
	dbglog ("DEBUG: Encapsulator::preProcessRasterData.............. \n");
	return NO_ERROR;
}

