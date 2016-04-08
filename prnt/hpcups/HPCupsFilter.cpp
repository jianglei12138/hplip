/*****************************************************************************\
  HPCupsFilter.cpp : Interface for HPCupsFilter class

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

  Author: Naga Samrat Chowdary Narla, Sanjay Kumar, Amarnath Chitumalla,Goutam Kodu
\*****************************************************************************/

#include "HPCupsFilter.h"
#include <signal.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <time.h>
#include "utils.h"

#define HP_FILE_VERSION_STR    "03.09.08.0"

static HPCupsFilter    filter;


#ifndef UNITTESTING 
int main (int  argc, char *argv[])
{
    openlog("hpcups", LOG_PID,  LOG_DAEMON);

    if (argc < 6 || argc > 7) {
        dbglog("ERROR: %s job-id user title copies options [file]\n", *argv);
        return JOB_CANCELED;
    }

    return filter.StartPrintJob(argc, argv);
}
#endif

void HPCancelJob(int sig)
{
    filter.CancelJob();
    exit(0);
}

void HPCupsFilter::CreateBMPHeader (int width, int height, int planes, int bpp)
{
    memset (&this->bmfh, 0, 14);
    memset (&this->bmih, 0, 40);
    bmfh.bfOffBits = 54;
    bmfh.bfType = 0x4d42;
    bmfh.bfReserved1 = 0;
    bmfh.bfReserved2 = 0;
    bmih.biSize = DBITMAPINFOHEADER;
    bmih.biWidth = width;
    bmih.biHeight = -height;
    bmih.biPlanes = 1;
    bmih.biBitCount = planes * bpp;
    bmih.biCompression = 0;
    bmih.biSizeImage = width * height * planes * bpp / 8;
    bmih.biClrImportant = 0;
    bmih.biClrUsed = (planes == 3) ? 0 : 2;
    bmih.biXPelsPerMeter = 0;
    bmih.biYPelsPerMeter = 0;

    bmfh.bfOffBits += bmih.biClrUsed * 4;
    bmfh.bfSize = bmih.biSizeImage + bmfh.bfOffBits;
}

void HPCupsFilter::WriteBMPHeader (FILE *fp, int width, int height, eRasterType raster_type)
{
    if (fp == NULL) 
        return;

    if (raster_type == BLACK_RASTER) {
        WriteKBMPHeader (fp, width, height);
    } else {
        WriteCBMPHeader (fp, width, height);
    }
}

void HPCupsFilter::WriteCBMPHeader (FILE *fp, int width, int height)
{
    if (fp == NULL) 
        return;

    adj_c_width = width;
    if (width % 4)
    {
        adj_c_width = (width / 4 + 1) * 4;
    }
    color_raster = new BYTE[adj_c_width * 3];
    memset (color_raster, 0xFF, adj_c_width * 3);
    CreateBMPHeader(adj_c_width, height, 3, 8);
    fwrite (&this->bmfh.bfType, 1, sizeof (short), fp);
    fwrite (&this->bmfh.bfSize, 1, sizeof (int), fp);
    fwrite (&this->bmfh.bfReserved1, 1, sizeof (short), fp);
    fwrite (&this->bmfh.bfReserved2, 1, sizeof (short), fp);
    fwrite (&this->bmfh.bfOffBits, 1, sizeof (int), fp);
    fwrite (&this->bmih, 1, DBITMAPINFOHEADER, fp);
}

void HPCupsFilter::WriteKBMPHeader(FILE *fp, int width, int height)
{
    BYTE    cmap[8];

    if (fp == NULL)
        return;

    adj_k_width = width;
    if (width % 32)
    {
        adj_k_width = (width / 32 + 1) * 32;
    }

    CreateBMPHeader(adj_k_width, height, 1, 1);
    adj_k_width /= 8;
    black_raster = new BYTE[adj_k_width];
    memset (black_raster, 0, adj_k_width);

    fwrite (&this->bmfh.bfType, 1, sizeof (short), fp);
    fwrite (&this->bmfh.bfSize, 1, sizeof (int), fp);
    fwrite (&this->bmfh.bfReserved1, 1, sizeof (short), fp);
    fwrite (&this->bmfh.bfReserved2, 1, sizeof (short), fp);
    fwrite (&this->bmfh.bfOffBits, 1, sizeof (int), fp);
    fwrite (&this->bmih, 1, DBITMAPINFOHEADER, fp);
    memset(cmap, 0, sizeof(cmap));
    cmap[0] = cmap[1] = cmap[2] = cmap[3] = 255;
    fwrite(cmap, 1, sizeof(cmap), fp);
}

void HPCupsFilter::WriteBMPRaster (FILE *fp, BYTE *raster, int width, eRasterType raster_type)
{
    if (fp == NULL)
        return;

    if (raster_type == BLACK_RASTER) {
        return WriteKBMPRaster (fp, raster, width);
    } else {
        return WriteCBMPRaster (fp, raster, width);
    }
}

void HPCupsFilter::WriteCBMPRaster (FILE *fp, BYTE *pbyrgb, int width)
{
    if (fp == NULL)
        return;

    //BYTE    c[3];
    int     i;
    BYTE    *p = pbyrgb;
    BYTE    *q = color_raster;
    if (pbyrgb == NULL) {
        memset (color_raster, 0xFF, adj_c_width * 3);
    } else {
        for (i = 0; i < width; i++) {
            q[0] = p[2];
            q[1] = p[1];
            q[2] = p[0];
            p += 3;
            q += 3;
        }
    }

    fwrite (color_raster, 1, adj_c_width * 3, fp);
}

void HPCupsFilter::WriteKBMPRaster (FILE *fp, BYTE *pbyk, int width)
{
    if (fp == NULL)
        return;

    if (pbyk == NULL) {
        memset (black_raster, 0, adj_k_width);
    } else {
        memcpy (black_raster, pbyk, width);
    }

    fwrite (black_raster, 1, adj_k_width, fp);
}

HPCupsFilter::HPCupsFilter() : m_pPrinterBuffer(NULL)
{
    setbuf (stderr, NULL);

    adj_c_width = 0;
    adj_k_width = 0;
    black_raster = NULL;
    color_raster = NULL;
}

HPCupsFilter::~HPCupsFilter()
{
}

void HPCupsFilter::closeFilter ()
{
    //! If we printed any pages, end the current job instance.
    m_Job.Cleanup();
    cleanup();
}

void HPCupsFilter::cleanup()
{
    if (m_pPrinterBuffer) {
        delete [] m_pPrinterBuffer;
        m_pPrinterBuffer = NULL;
    }

    if(m_ppd){
       ppdClose(m_ppd);
       m_ppd = NULL;
    }
    if (m_pSys)
    {
    	delete m_pSys;
    	m_pSys = NULL;
    }
}

void HPCupsFilter::CancelJob()
{
    m_Job.CancelJob();
    cleanup();
}

DRIVER_ERROR HPCupsFilter::startPage (cups_page_header2_t *cups_header)
{
    DRIVER_ERROR        err = NO_ERROR;
    ppd_attr_t          *attr;
    int                 xoverspray = 120;
    int                 yoverspray = 60;

/*
 *  Check for invalid data
 */
    if (cups_header->HWResolution[0] == 100 && cups_header->HWResolution[1] == 100)
    {

/*
 *      Something went wrong, cups is defaulting to 100 dpi.
 *      Some inkjet printers do not support 100 dpi. Return error.
 */

        dbglog("ERROR: Unsupported resolution\n");
        return JOB_CANCELED;
    }

//  XOverSpray and YOverSpray are entered as fractional value * 1000

    if (((attr = ppdFindAttr(m_ppd, "HPXOverSpray", NULL)) != NULL) &&
        (attr && attr->value != NULL)) {
           xoverspray = atoi(attr->value);
    }
    if (((attr = ppdFindAttr(m_ppd, "HPYOverSpray", NULL)) != NULL) &&
        (attr && attr->value != NULL)) {
           yoverspray = atoi(attr->value);
    }

    if (m_iLogLevel & BASIC_LOG) {
        printCupsHeaderInfo(cups_header);
    }

    m_JA.quality_attributes.media_type = cups_header->cupsMediaType;
    m_JA.quality_attributes.print_quality = atoi(cups_header->OutputType);
    m_JA.quality_attributes.horizontal_resolution = cups_header->HWResolution[0];
    m_JA.quality_attributes.vertical_resolution   = cups_header->HWResolution[1];
    m_JA.quality_attributes.actual_vertical_resolution   = cups_header->HWResolution[1];

//  Get the printer's actual resolution, may be different than what is reported
    char   *p;
    if ((p = strstr (cups_header->OutputType, "_"))) {
        int    x = 0, y = 0;
        p++;
        x = atoi(p);
        while (*p && *p != 'x')
            p++;
        if (*p && *p == 'x') {
            p++;
            y = atoi(p);
        }
//      Currently, there is one printer with one printmode that supports lower y-resolution
        if (y != 0) {
            m_JA.quality_attributes.actual_vertical_resolution = y;
        }
    }
    m_JA.color_mode = cups_header->cupsRowStep;
    m_JA.media_source = cups_header->MediaPosition;

    m_JA.print_borderless = (cups_header->ImagingBoundingBox[0] == 0) ? true : false;
    if (cups_header->Duplex) {
        m_JA.e_duplex_mode  = (cups_header->Tumble == 0) ? DUPLEXMODE_BOOK : DUPLEXMODE_TABLET;
    }
    else {
        m_JA.e_duplex_mode = DUPLEXMODE_NONE;
    }
    m_JA.krgb_mode = (cups_header->cupsColorSpace == CUPS_CSPACE_RGBW) ? true : false;

    /*
     *  Cups PageSize dimensions are in PostScript units, which are 72 units to an inch
     *  and is stored as <width, height>
     *  The ImagingBoundingBox is in PostScript units and are stored as <lower_left> <upper_right>
     *  and <0, 0> is at the bottom left
     *      lower_left_x = ImagingBoundingBox[0]
     *      lower_left_y = ImagingBoundingBox[1]
     *      upper_right_x = ImagingBoundingBox[2]
     *      upper_right_y = ImagingBoundingBox[3]
     *  We require <top_left> <bottom_right> values and <0, 0> is top left
     *  So,
     *      PrintableStartX = lower_left_x
     *      PrintableStartY = PhysicalPageHeight - upper_right_y
     */

    int    horz_res = cups_header->HWResolution[0];
    int    vert_res = cups_header->HWResolution[1];
    m_JA.media_attributes.pcl_id = cups_header->cupsInteger[0];
    m_JA.media_attributes.printable_start_x = (cups_header->Margins[0] * horz_res) / 72;
    m_JA.media_attributes.printable_start_y = ((cups_header->PageSize[1] - cups_header->ImagingBoundingBox[3]) * vert_res) / 72;
    m_JA.media_attributes.horizontal_overspray = (xoverspray * horz_res) / 1000;
    m_JA.media_attributes.vertical_overspray   = (yoverspray * vert_res) / 1000;

    /*
     * Left and top overspray in dots. We haven't defined ovespray for all classes in the drv.
     * Hence using default values in the case of older classes.
     */
    m_JA.media_attributes.left_overspray = cups_header->cupsReal[0] ? (cups_header->cupsReal[0] * horz_res) : m_JA.media_attributes.horizontal_overspray / 2;
    m_JA.media_attributes.top_overspray  = cups_header->cupsReal[1] ? (cups_header->cupsReal[1] * vert_res) : m_JA.media_attributes.vertical_overspray / 2;

    if (((attr = ppdFindAttr(m_ppd, "HPMechOffset", NULL)) != NULL) &&
        (attr && attr->value != NULL)) {
        m_JA.mech_offset = atoi(attr->value);
    }

    if(cups_header->PageSize[0] > 612) // Check for B size paper. B Size paper has width > 8.5 inch (612 points)
    {
        //Check if HPMechOffsetBSize attribute is defined. If it is defined then use this offset value.
        if (((attr = ppdFindAttr(m_ppd, "HPMechOffsetBSize", NULL)) != NULL) && (attr && attr->value != NULL)) 
        {
            m_JA.mech_offset = atoi(attr->value);
        }

    }

//  Get printer platform name
    if (((attr = ppdFindAttr(m_ppd, "hpPrinterPlatform", NULL)) != NULL) && (attr->value != NULL)) {

        strncpy(m_JA.printer_platform, attr->value, sizeof(m_JA.printer_platform)-1);

        if (m_iLogLevel & BASIC_LOG) {
            dbglog("HPCUPS: found Printer Platform, it is - %s\n", attr->value);
        }	

        if (strcmp(m_JA.printer_platform, "ljzjscolor") == 0) {
            if (((attr = ppdFindAttr(m_ppd, "hpLJZjsColorVersion", NULL)) != NULL) && 
                 (attr->value != NULL)) 
            {
                 m_JA.printer_platform_version = atoi(attr->value);
            }
        }
    }

//Get Raster Preprocessing status
    if (((attr = ppdFindAttr(m_ppd, "hpReverseRasterPages", NULL)) != NULL) && 
         (attr->value != NULL)) 
    {
          m_JA.pre_process_raster = atoi(attr->value);
    }

    if (((attr = ppdFindAttr(m_ppd, "HPSPDClass", NULL)) == NULL) ||
         (attr && attr->value == NULL)) 
    {
        m_JA.HPSPDClass = 0;
    }
    else
    {
        m_JA.HPSPDClass = atoi(attr->value);
    }
    

// Get the encapsulation technology from ppd

    if (((attr = ppdFindAttr(m_ppd, "hpPrinterLanguage", NULL)) == NULL) ||
         (attr && attr->value == NULL)) 
    {
            dbglog("DEBUG: Bad PPD - hpPrinterLanguage not found\n");
            ppdClose(m_ppd);
            m_ppd = NULL;
            return SYSTEM_ERROR;
    }
    strncpy(m_JA.printer_language, attr->value, sizeof(m_JA.printer_language)-1);

    if (strcmp(m_JA.printer_language, "hbpl1") == 0) {
        m_JA.media_attributes.physical_width = cups_header->PageSize[0];
        m_JA.media_attributes.physical_height = cups_header->PageSize[1];
        m_JA.media_attributes.printable_width = ((cups_header->ImagingBoundingBox[2]-cups_header->ImagingBoundingBox[0]) * horz_res) / 72;
        m_JA.media_attributes.printable_height = ((cups_header->ImagingBoundingBox[3]-cups_header->ImagingBoundingBox[1]) * vert_res) / 72;
        strncpy(m_JA.media_attributes.PageSizeName, &cups_header->cupsString[0][0], sizeof(m_JA.media_attributes.PageSizeName));
        strncpy(m_JA.media_attributes.MediaTypeName, cups_header->MediaType, sizeof(m_JA.media_attributes.MediaTypeName));
        strncpy(m_JA.quality_attributes.hbpl1_print_quality, cups_header->OutputType, sizeof(m_JA.quality_attributes.hbpl1_print_quality));
        m_JA.color_mode = cups_header->cupsRowStep;
    }
    else {
        m_JA.media_attributes.physical_width   = (cups_header->PageSize[0] * horz_res) / 72;
        m_JA.media_attributes.physical_height  = (cups_header->PageSize[1] * vert_res) / 72;
        m_JA.media_attributes.printable_width  = cups_header->cupsWidth;
        m_JA.media_attributes.printable_height = cups_header->cupsHeight;
    }

    if (m_iLogLevel & BASIC_LOG) {
        dbglog("HPCUPS: found Printer Language, it is - %s\n", attr->value);
    }

//  Fill in the other PCL header info

    struct utsname  uts_name;
    uname(&uts_name);
    strncpy(m_JA.job_title, m_argv[3], sizeof(m_JA.job_title)-1);
    strncpy(m_JA.user_name, m_argv[2], sizeof(m_JA.user_name)-1);
    strncpy(m_JA.host_name, uts_name.nodename, sizeof(m_JA.host_name)-1);
    strncpy(m_JA.os_name, uts_name.sysname, sizeof(m_JA.os_name)-1);
    getdomainname(m_JA.domain_name, sizeof(m_JA.domain_name) - 1);
    int i = strlen(m_argv[0]) - 1;
    while (i >= 0 && m_argv[0][i] != '/') {
        i--;
    }
    snprintf(m_JA.driver_name, sizeof(m_JA.driver_name), "%s; %s", &m_argv[0][i+1], HP_FILE_VERSION_STR);
    char    *ptr = getenv("DEVICE_URI");
    i = 0;
    if (ptr) {
        while (*ptr) {
            if (*ptr == '%') {
                ptr += 3;
                m_JA.printer_name[i++] = ' ';
            }
            m_JA.printer_name[i++] = *ptr++;
        }
    }

    string strPrinterURI="" , strPrinterName= "";
    if (getenv("DEVICE_URI"))
        m_DBusComm.initDBusComm(DBUS_PATH,DBUS_INTERFACE, getenv("DEVICE_URI"), m_JA.printer_name);

    ptr = strstr(m_argv[5], "job-uuid");
    if (ptr) {
        strncpy(m_JA.uuid, ptr + strlen("job-uuid=urn:uuid:"), sizeof(m_JA.uuid)-1);
    }

    for (i = 0; i < 16; i++)
        m_JA.integer_values[i] = cups_header->cupsInteger[i];

    if (cups_header->cupsString[0]) {
        strncpy(m_JA.quality_attributes.print_mode_name, &cups_header->cupsString[0][0],
                sizeof(m_JA.quality_attributes.print_mode_name)-1);
    }

    Encapsulator *encap_interface = EncapsulatorFactory::GetEncapsulator(m_JA.printer_language);

    if ((err = m_Job.Init(m_pSys, &m_JA, encap_interface)) != NO_ERROR)
    {
        if (err == PLUGIN_LIBRARY_MISSING)
        {
            fputs ("STATE: +hplip.plugin-error\n", stderr);

            m_DBusComm.sendEvent(EVENT_PRINT_FAILED_MISSING_PLUGIN, "Plugin missing", m_JA.job_id, m_JA.user_name);

        }
        dbglog ("m_Job initialization failed with error = %d\n", err);
        ppdClose(m_ppd);
        m_ppd = NULL;
        return err;
    }

    if (m_iLogLevel & BASIC_LOG) {
        dbglog("HPCUPS: returning NO_ERROR from startPage\n");
    }

    m_pPrinterBuffer = new BYTE[cups_header->cupsWidth * 4 + 32];

    return NO_ERROR;
}

int HPCupsFilter::StartPrintJob(int  argc, char *argv[])
{
    int              fd = 0;
    cups_raster_t    *cups_raster;
    int              err = 0;

    memset(&m_JA, 0, sizeof(JobAttributes));
    struct    tm       *t;
    struct timeval	 tv;
    time_t             long_time;
    time(&long_time);
    t = localtime(&long_time);
    gettimeofday(&tv, NULL);
    strncpy(m_JA.job_start_time, asctime(t), sizeof(m_JA.job_start_time)-1);    // returns Fri Jun  5 08:12:16 2009
    snprintf(m_JA.job_start_time+19, sizeof(m_JA.job_start_time) - 20, ":%ld %d", tv.tv_usec/1000, t->tm_year + 1900); // add milliseconds

#ifdef UNITTESTING
    memset(m_JA.job_start_time,0,sizeof(m_JA.job_start_time));
    snprintf(m_JA.job_start_time, sizeof(m_JA.job_start_time),"Mon Dec  9 17:48:58:586 2013" );
#endif

    m_iLogLevel = getHPLogLevel();
    m_JA.job_id = atoi(argv[1]);
    strncpy(m_JA.user_name,argv[2],sizeof(m_JA.user_name)-1);

    m_ppd = ppdOpenFile(getenv("PPD"));
    if (m_ppd == NULL) {
        dbglog("DEBUG: ppdOpenFile failed for %s\n", getenv("PPD"));
        return SYSTEM_ERROR;
    }

    m_argv = argv;
    if (m_iLogLevel & BASIC_LOG) {
        for (int i = 0; i < argc; i++) {
            dbglog("argv[%d] = %s\n", i, argv[i]);
        }
    }

    if (argc == 7)
    {
        if (m_iLogLevel & BASIC_LOG)
        {
            dbglog("Page Stream Data Name: %s\n", argv[6] );
        }
        if ((fd = open (argv[6], O_RDONLY)) == -1)
        {
            perror("ERROR: Unable to open raster file - ");
            return 1;
        }
    }

	m_pSys = new SystemServices(m_iLogLevel, m_JA.job_id, m_JA.user_name);

/*
 *  When user cancels a print job, the spooler sends SIGTERM signal
 *  to the filter. Must catch this signal to send end job sequence
 *  to the printer.
 */

    signal(SIGTERM, HPCancelJob);

    cups_raster = cupsRasterOpen(fd, CUPS_RASTER_READ);

    if (cups_raster == NULL) {
        dbglog("cupsRasterOpen failed, fd = %d\n", fd);
        if (fd != 0) {
            close(fd);
        }
        closeFilter();
        return 1;
    }

    if ((err = processRasterData(cups_raster))) {
        if (fd != 0) {
            close(fd);
        }

        if (m_iLogLevel & BASIC_LOG)
            dbglog("HPCUPS: processRasterData returned %d, calling closeFilter()\n", err);

        closeFilter();
        cupsRasterClose(cups_raster);
        return 1;
    }

    if (fd != 0) {
        close(fd);
    }

    if (m_iLogLevel & BASIC_LOG)
        dbglog("HPCUPS: StartPrintJob end of job, calling closeFilter()\n");

    closeFilter();
    cupsRasterClose(cups_raster);

    return 0;
}

bool HPCupsFilter::isBlankRaster(BYTE *input_raster, cups_page_header2_t *header)
{
    int length_in_bytes = (int)header->cupsBytesPerLine;
    if (input_raster == NULL) {
        return true;
    }

    if(header->cupsColorSpace == CUPS_CSPACE_K){
	if (*input_raster == 0x00 &&
            !(memcmp(input_raster + 1, input_raster, length_in_bytes - 1))) {
            return true;
        }
    }
    else if (*input_raster == 0xFF &&
             !(memcmp(input_raster + 1, input_raster, length_in_bytes - 1))) {
            return true;
    }
    
    return false;
}


int HPCupsFilter::processRasterData(cups_raster_t *cups_raster)
{
    FILE                   *kfp = NULL;
    FILE                   *cfp = NULL;
    BYTE                   *kRaster = NULL;
    BYTE                   *rgbRaster = NULL;
    int                    current_page_number = 0;
    cups_page_header2_t    cups_header;
    DRIVER_ERROR           err;
    int                    ret_status = 0;

    char hpPreProcessedRasterFile[MAX_FILE_PATH_LEN]; //temp file needed to store raster data with swaped pages.


    sprintf(hpPreProcessedRasterFile, "%s/hp_%s_cups_SwapedPagesXXXXXX",CUPS_TMP_DIR, m_JA.user_name);


    while (cupsRasterReadHeader2(cups_raster, &cups_header))
    {
        current_page_number++;

        if (current_page_number == 1) {

            if (startPage(&cups_header) != NO_ERROR) {
                return JOB_CANCELED;
            }

            if (m_JA.pre_process_raster) {
		    	// CC ToDo: Why pSwapedPagesFileName should be sent as a parameter? 
                  	// Remove if not required to send it as parameter
                err = m_Job.preProcessRasterData(&cups_raster, &cups_header, hpPreProcessedRasterFile);
                if (err != NO_ERROR) {
                    if (m_iLogLevel & BASIC_LOG) {
                        dbglog ("DEBUG: Job::StartPage failed with err = %d\n", err);
                    }
                    ret_status = JOB_CANCELED;
                    break;
                }
            }

            if (cups_header.cupsColorSpace == CUPS_CSPACE_RGBW) {
                rgbRaster = new BYTE[cups_header.cupsWidth * 3];
                if (rgbRaster == NULL) {
                    return ALLOCMEM_ERROR;
                }
                kRaster = new BYTE[cups_header.cupsWidth];
                if (kRaster == NULL) {
                    delete [] rgbRaster;
                    return ALLOCMEM_ERROR;
                }
                memset (kRaster, 0, cups_header.cupsWidth);
                memset (rgbRaster, 0xFF, cups_header.cupsWidth * 3);
            }
        } // end of if(current_page_number == 1)

        if (cups_header.cupsColorSpace == CUPS_CSPACE_K) {
            kRaster = m_pPrinterBuffer;
            rgbRaster = NULL;
        }
        else if (cups_header.cupsColorSpace != CUPS_CSPACE_RGBW) {
            rgbRaster = m_pPrinterBuffer;
            kRaster = NULL;
        }

        BYTE    *color_raster = NULL;
        BYTE    *black_raster = NULL;

        err = m_Job.StartPage(&m_JA);
        if (err != NO_ERROR) {
            if (m_iLogLevel & BASIC_LOG) {
                dbglog ("DEBUG: Job::StartPage failed with err = %d\n", err);
            }
            ret_status = JOB_CANCELED;
            break;
        }

        // Save Raster file for Debugging
        if (m_iLogLevel & SAVE_INPUT_RASTERS)
        {
            char    szFileName[MAX_FILE_PATH_LEN];
            memset(szFileName, 0, sizeof(szFileName));

            if (cups_header.cupsColorSpace == CUPS_CSPACE_RGBW ||
                cups_header.cupsColorSpace == CUPS_CSPACE_RGB)
            {

                snprintf (szFileName, sizeof(szFileName), "%s/hpcups_%s_c_bmp_%d_XXXXXX", CUPS_TMP_DIR, m_JA.user_name, current_page_number);
                createTempFile(szFileName, &cfp);
                if (cfp)
                {
                    chmod (szFileName, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
                }
            }

            if (cups_header.cupsColorSpace == CUPS_CSPACE_RGBW ||
                cups_header.cupsColorSpace == CUPS_CSPACE_K)
            {
                snprintf (szFileName, sizeof(szFileName), "%s/hpcups_%s_k_bmp_%d_XXXXXX", CUPS_TMP_DIR, m_JA.user_name, current_page_number);
                createTempFile(szFileName, &kfp);
                if (kfp)
                {
                    chmod (szFileName, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
                }
            }

            WriteBMPHeader (cfp, cups_header.cupsWidth, cups_header.cupsHeight, COLOR_RASTER);
            WriteBMPHeader (kfp, cups_header.cupsWidth, cups_header.cupsHeight, BLACK_RASTER);
        }

        fprintf(stderr, "PAGE: %d %s\r\n", current_page_number, m_argv[4]);
        // Iterating through the raster per page
        bool is_ljmono = strcmp(m_JA.printer_language, "ljmono");
        for (int y = 0; y < (int) cups_header.cupsHeight; y++) {
            cupsRasterReadPixels (cups_raster, m_pPrinterBuffer, cups_header.cupsBytesPerLine);
            color_raster = rgbRaster;
            black_raster = kRaster;

            if ((y == 0) && !is_ljmono) {
                //For ljmono, make sure that first line is not a blankRaster line.Otherwise printer
                //may not skip blank lines before actual data
                //Need to revisit to cross check if it is a firmware issue.

                *m_pPrinterBuffer = 0x01;
                dbglog("First raster data plane..\n" );
            }

            if (this->isBlankRaster((BYTE *) m_pPrinterBuffer, &cups_header)) {
                color_raster = NULL;
                black_raster = NULL;
            }

            extractBlackPixels(&cups_header, black_raster, color_raster);

            //! Sending Raster bits off to encapsulation
            err = m_Job.SendRasters (black_raster, color_raster);
            if (err != NO_ERROR) {
                break;
            }

            if (m_iLogLevel & SAVE_INPUT_RASTERS)
            {
                WriteBMPRaster (cfp, color_raster, cups_header.cupsWidth, COLOR_RASTER);
                WriteBMPRaster (kfp, black_raster, cups_header.cupsWidth/8, BLACK_RASTER);
            }
        }  // for() loop end

        m_Job.NewPage();
        if (err != NO_ERROR) {
            break;
        }
    }  // while() loop end

    //! Remove the old processing band data...
    if (cups_header.cupsColorSpace == CUPS_CSPACE_RGBW) {
        delete [] kRaster;
        delete [] rgbRaster;
        kRaster = NULL;
        rgbRaster = NULL;
    }

    unlink(hpPreProcessedRasterFile);
    return ret_status;
}

void HPCupsFilter::extractBlackPixels(cups_page_header2_t *cups_header, BYTE *kRaster, BYTE *rgbRaster)
{
/*
 *  DON'T DO BITPACKING HERE, DO IT IN HALFTONER FOR CMYK PRINTES
 *  AND IN MODE9 FOR RGB PRINTERS
 */

    static BYTE pixel_value[8] = {
            0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01
    };

    if (rgbRaster == NULL) {
        return;
    }

    if (cups_header->cupsColorSpace == CUPS_CSPACE_RGBW) {
        int    k = 0;
        BYTE   *pIn = m_pPrinterBuffer;
        BYTE   kVal = 0;
        BYTE   white=0;
        BYTE   *rgb = rgbRaster;
        BYTE   *black   = kRaster;
        memset (kRaster, 0, cups_header->cupsWidth);


        for (unsigned int i = 0; i < cups_header->cupsWidth; i++) {
            rgb[0] = *pIn++;
            rgb[1] = *pIn++;
            rgb[2] = *pIn++;
            white = *pIn++;

            if(white == 0)
            {
            	//If W component is 0 (means black is 1) then no need of having RGB for that pixel.
            	//ghostscript >= 8.71 sends both W and RGB for black pixel(i.e RGBW=(0,0,0,0)).
				kVal |= pixel_value[k];
				rgb[0] = 0xFF;
				rgb[1] = 0xFF;
				rgb[2] = 0xFF;
            }
            else if(white == 0xFF)
            {
            	kVal |= 0;
            }
            else
            {
              int cr,cg,cb;
              cr = rgb[0] - (int)(255 - white);
              rgb[0] = cr >= 0 ? cr : 0;

              cg = rgb[1] - (int)(255 - white);
              rgb[1] = cg >= 0 ? cg : 0;

              cb = rgb[2] - (int)(255 - white);
              rgb[2] = cb >= 0 ? cb : 0;
            }

            rgb += 3;
            if (k == 7) {
                *black++ = kVal;
                kVal = 0;
                k = 0;
            }
            else {
                k++;
            }
        }  // end of for loop
    *black = kVal;

    } // end of if condition
}

void HPCupsFilter::printCupsHeaderInfo(cups_page_header2_t *header)
{

    dbglog ("DEBUG: startPage...\n");
    dbglog ("DEBUG: MediaClass = \"%s\"\n", header->MediaClass);
    dbglog ("DEBUG: MediaColor = \"%s\"\n", header->MediaColor);
    dbglog ("DEBUG: MediaType = \"%s\"\n", header->MediaType);
    dbglog ("DEBUG: OutputType = \"%s\"\n", header->OutputType);
    dbglog ("DEBUG: AdvanceDistance = %d\n", header->AdvanceDistance);
    dbglog ("DEBUG: AdvanceMedia = %d\n", header->AdvanceMedia);
    dbglog ("DEBUG: Collate = %d\n", header->Collate);
    dbglog ("DEBUG: CutMedia = %d\n", header->CutMedia);
    dbglog ("DEBUG: Duplex = %d\n", header->Duplex);
    dbglog ("DEBUG: HWResolution = [ %d %d ]\n", header->HWResolution[0], header->HWResolution[1]);
    dbglog ("DEBUG: ImagingBoundingBox = [ %d %d %d %d ]\n",
               header->ImagingBoundingBox[0], header->ImagingBoundingBox[1],
               header->ImagingBoundingBox[2], header->ImagingBoundingBox[3]);
    dbglog ("DEBUG: InsertSheet = %d\n", header->InsertSheet);
    dbglog ("DEBUG: Jog = %d\n", header->Jog);
    dbglog ("DEBUG: LeadingEdge = %d\n", header->LeadingEdge);
    dbglog ("DEBUG: Margins = [ %d %d ]\n", header->Margins[0], header->Margins[1]);
    dbglog ("DEBUG: ManualFeed = %d\n", header->ManualFeed);
    dbglog ("DEBUG: MediaPosition = %d\n", header->MediaPosition);
    dbglog ("DEBUG: MediaWeight = %d\n", header->MediaWeight);
    dbglog ("DEBUG: MirrorPrint = %d\n", header->MirrorPrint);
    dbglog ("DEBUG: NegativePrint = %d\n", header->NegativePrint);
    dbglog ("DEBUG: NumCopies = %d\n", header->NumCopies);
    dbglog ("DEBUG: Orientation = %d\n", header->Orientation);
    dbglog ("DEBUG: OutputFaceUp = %d\n", header->OutputFaceUp);
    dbglog ("DEBUG: PageSize = [ %d %d ]\n", header->PageSize[0], header->PageSize[1]);
    dbglog ("DEBUG: Separations = %d\n", header->Separations);
    dbglog ("DEBUG: TraySwitch = %d\n", header->TraySwitch);
    dbglog ("DEBUG: Tumble = %d\n", header->Tumble);
    dbglog ("DEBUG: cupsWidth = %d\n", header->cupsWidth);
    dbglog ("DEBUG: cupsHeight = %d\n", header->cupsHeight);
    dbglog ("DEBUG: cupsMediaType = %d\n", header->cupsMediaType);
    dbglog ("DEBUG: cupsRowStep = %d\n", header->cupsRowStep);
    dbglog ("DEBUG: cupsBitsPerColor = %d\n", header->cupsBitsPerColor);
    dbglog ("DEBUG: cupsBitsPerPixel = %d\n", header->cupsBitsPerPixel);
    dbglog ("DEBUG: cupsBytesPerLine = %d\n", header->cupsBytesPerLine);
    dbglog ("DEBUG: cupsColorOrder = %d\n", header->cupsColorOrder);
    dbglog ("DEBUG: cupsColorSpace = %d\n", header->cupsColorSpace);
    dbglog ("DEBUG: cupsRowStep = %d\n", header->cupsRowStep);
    dbglog ("DEBUG: cupsCompression = %d\n", header->cupsCompression);
    dbglog ("DEBUG: cupsPageSizeName = %s\n", header->cupsPageSizeName);
    dbglog ("DEBUG: cupsInteger0 = %d\n", header->cupsInteger[0]); // max jpeg filesize
    dbglog ("DEBUG: cupsInteger1 = %d\n", header->cupsInteger[1]); // Red eye removal
    dbglog ("DEBUG: cupsInteger2 = %d\n", header->cupsInteger[2]); // Photo fix (RLT)
    dbglog ("DEBUG: cupsString0 = %s\n", header->cupsString[0]);   // print_mode_name
    dbglog ("DEBUG: cupsReal0 = %f\n", header->cupsReal[0]); // Left overspray
    dbglog ("DEBUG: cupsReal1 = %f\n", header->cupsReal[1]); // Top overspray
}
