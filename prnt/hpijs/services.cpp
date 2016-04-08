/*****************************************************************************\
    services.cpp : HP Inkjet Server

    Copyright (c) 2001 - 2004, HP Co.
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:
    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
    3. Neither the name of the Hewlett-Packard nor the names of its
       contributors may be used to endorse or promote products derived
       from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
    IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
    OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
    IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
    NOT LIMITED TO, PATENT INFRINGEMENT; PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
    STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
    IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.
\*****************************************************************************/

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "header.h"
#include "ijs.h"
#include "ijs_server.h"
#include "hpijs.h"
#include "services.h"

#if defined(HAVE_LIBHPIP) && defined(HAVE_DBUS) 
#include <dbus/dbus.h>
#define DBUS_INTERFACE "com.hplip.StatusService"
#define DBUS_PATH "/"
static DBusError dbus_err;
static DBusConnection *dbus_conn;
void InitDbus (void);
void SendDbusMessage (const char *dev, const char *printer, int code, 
                      const char *username, const int jobid, const char *title);
#else
void SendDbusMessage (const char *dev, const char *printer, int code, 
                      const char *username, const int jobid, const char *title)
{
}
#endif

int UXServices::InitDuplexBuffer()
{
    /* Free buffer if new page size in middle of print job. */
    if (RastersOnPage)
       delete [] RastersOnPage;
    if (KRastersOnPage)
       delete [] KRastersOnPage;

    /* Calculate duplex page buffer */
    CurrentRaster = ph.height - 1;  /* Height = physical page in pixels */
    RastersOnPage = (BYTE **) new BYTE[(ph.height) * sizeof (BYTE *)];
    KRastersOnPage = (BYTE **) new BYTE[(ph.height) * sizeof (BYTE *)];
    for (int i = 0; i < ph.height; i++)
    {
       RastersOnPage[i] = NULL;
       KRastersOnPage[i] = NULL;
    }
    return 0;
}

int UXServices::SendBackPage()
{
    DRIVER_ERROR    err;
    int i = CurrentRaster+1;

    while (i < ph.height)
    {
       if (KRGB)
       {
          if ((err = pJob->SendRasters(KRastersOnPage[i], RastersOnPage[i])) != NO_ERROR)
            return err;
       }
       else
       {
          if ((err = pJob->SendRasters(RastersOnPage[i])) != NO_ERROR)
            return err;
       }

       if (RastersOnPage[i])
           delete [] RastersOnPage[i];
       if (KRastersOnPage[i])
           delete [] KRastersOnPage[i];
       i++;
    }

    CurrentRaster = ph.height - 1;   /* reset raster index */

    return 0;
}

static unsigned char xmask[] =
{
   0x80,    /* x=0 */
   0x40,    /* 1 */
   0x20,    /* 2 */
   0x10,    /* 3 */
   0x08,    /* 4 */
   0x04,    /* 5 */
   0x02,    /* 6 */
   0x01     /* 7 */
};

int UXServices::ProcessRaster(char *raster, char *k_raster)
{
    if (!((pPC->QueryDuplexMode() == DUPLEXMODE_BOOK) && pPC->RotateImageForBackPage() && BackPage))
    {
       if (KRGB)
          return pJob->SendRasters((unsigned char *)k_raster, (unsigned char *)raster);
       else
          return pJob->SendRasters((unsigned char *)raster);
    }
    else
    {
        if (CurrentRaster < 0)
           return -1;

        BYTE   *new_raster;
        int    new_raster_size;
        int    i,w;

        if (raster == NULL)
        {
           RastersOnPage[CurrentRaster] = NULL;
        }
        else
        {
           new_raster_size = pPC->InputPixelsPerRow() * 3;
           new_raster = new BYTE[new_raster_size];
           if (new_raster == 0)
           {
               BUG("unable to create duplex buffer, size=%d: %m\n", new_raster_size);
               return -1;
           }
           memset(new_raster, 0xFF, new_raster_size);
           RastersOnPage[CurrentRaster] = new_raster;
           BYTE *p = new_raster + new_raster_size - 3;
           for (i = 0; i < new_raster_size; i += 3)
           {
               memcpy (p, raster+i, 3);  /* rotate rgb image */
               p -= 3;
           }
        }

        if (k_raster == NULL)
        {
           KRastersOnPage[CurrentRaster] = NULL;
        }
        else
        {
           new_raster_size = (pPC->InputPixelsPerRow() + 7) >> 3;
           new_raster = new BYTE[new_raster_size];
           if (new_raster == 0)
           {
               BUG("unable to create black duplex buffer, size=%d: %m\n", new_raster_size);
               return -1;
           }
           memset(new_raster, 0, new_raster_size);
           KRastersOnPage[CurrentRaster] = new_raster;
           w = pPC->InputPixelsPerRow();
           for (i=0; i<w; i++)
           {
               if (k_raster[i>>3] & xmask[i&7])
                  new_raster[(w-i)>>3] |= xmask[(w-i)&7];  /* rotate k image */
           }
	   int    k = ((w + 7) / 8) * 8 - w;
	   BYTE   c = 0xff << k;
	   if (k != 0)
	       new_raster[0] = c & k_raster[new_raster_size-1];
        }

        CurrentRaster--;

        return 0;
    }
}


#ifdef HAVE_LIBHPIP

/*
 *  Check models.xml for bi-di flag and also check the
 *  device id string for integrity. Some devices return
 *  device id without some expected fields.
 *
 */
BOOL UXServices::CanDoBiDi ()
{
    char            *hpDev;
    struct hpmud_model_attributes ma;
    char            strDevID[512];

    // Check for CUPS environment

    if ((hpDev = getenv ("DEVICE_URI")) == NULL)
    {
        return FALSE;
    }

    // Check for HP Backend

    if (strncmp (hpDev, "hp:", 3))
    {
        return FALSE;
    }

    // Check io-mode in models.xml for this device

    hpmud_query_model(hpDev, &ma);

    if (ma.prt_mode == HPMUD_UNI_MODE)
    {
        return FALSE;
    }
    if (hpmud_open_device(hpDev, ma.prt_mode, &hpFD) != HPMUD_R_OK)
    {
        return FALSE;
    }
    memset (strDevID, 0, 512);
    if ((ReadDeviceID ((BYTE *) strDevID, 512)) != NO_ERROR)
    {
        return FALSE;
    }

    // Check if this is a laser device
    if (strstr (strDevID, "Laser") || strstr (strDevID, "laser"))
    {
        return TRUE;
    }

    // Check if device id is complete
    if (!(strstr (strDevID, ";S:")) && !(strstr (strDevID, "VSTATUS")))
    {
        return FALSE;
    }
    return TRUE;
}

#else

BOOL UXServices::CanDoBiDi ()
{
    return FALSE;
}

#endif  // HAVE_LIBHPIP

UXServices::UXServices():SystemServices()
{
   constructor_error = NO_ERROR;
   hpFD = -1;

   // instead of InitDeviceComm(), just do...
   IOMode.bDevID = IOMode.bStatus = FALSE;   /* uni-di support is default */

#if 0 // Old code

   /* Check for CUPS environment and HP backend. */
   if ((hpDev = getenv("DEVICE_URI")) != NULL)
   {
      if (strncmp(hpDev, "hp:", 3) == 0)
      {
         hplip_Init();
         hplip_ModelQuery(hpDev, &ma);      /* check io-mode in models.xml for this device */  
         if (ma.prt_mode != UNI_MODE)
         {
            if ((hpFD = hplip_OpenHP(hpDev, &ma)) >= 0)
            {
                InitDeviceComm();            /* lets try bi-di support */
            }
            if(IOMode.bDevID == FALSE)
               BUG("unable to set bi-di for hp backend\n");
         }
      }
   }

#endif // Old code

   if (CanDoBiDi ())
   {
       InitDeviceComm ();
       if (IOMode.bDevID == FALSE)
       {
           BUG ("Unable to set bi-di for hp backend\n");
       }
   }

   Quality = QUALITY_NORMAL;
   MediaType = MEDIA_PLAIN;
   ColorMode = COLOR;
   PenSet = DUMMY_PEN;
   
   RastersOnPage = 0;
   KRastersOnPage = 0;
   pPC = NULL;
   pJob = NULL;
   Duplex = 0;
   Tumble = 0;
   FullBleed = 0;
   FirstRaster = 1;
   MediaPosition = sourceTrayAuto;
   Model = -1;
   strcpy(ph.cs, "sRGB");
   VertAlign = -1;
   DisplayStatus = NODISPLAYSTATUS;
   OutputPath = -1;
   outfp = NULL;
   m_iLogLevel = 0;

   m_pbyPclBuffer      = NULL;
   m_iPclBufferSize    = BUFFER_CHUNK_SIZE;
   m_iCurPclBufferPos  = 0;
   m_iPageCount        = 0;
   m_bSpeedMechEnabled = FALSE;
}

UXServices::~UXServices()
{
   if (m_bSpeedMechEnabled)
   {
       SendLastPage ();
   }

   if (RastersOnPage)
      delete [] RastersOnPage;
   if (KRastersOnPage)
      delete [] KRastersOnPage;
#ifdef HAVE_LIBHPIP
   if (hpFD >= 0)
      hpmud_close_device(hpFD);  
#endif
    if (outfp)
    {
        fclose (outfp);
    }
}

DRIVER_ERROR UXServices::ToDevice(const BYTE * pBuffer, DWORD * Count)
{
    if (OutputPath == -1)
    {
        return IO_ERROR;
    }

   if (m_bSpeedMechEnabled)
   {
       if ((CopyData (pBuffer, *Count)) == 0)
       {
           *Count = 0;
	   return NO_ERROR;
       }
   }

    if (outfp)
    {
        fwrite (pBuffer, 1, *Count, outfp);
	if (!(m_iLogLevel & SEND_TO_PRINTER))
	{
	    *Count = 0;
	    return NO_ERROR;
	}
    }

   /* Write must be not-buffered, don't use streams */
   if (write(OutputPath, pBuffer, *Count) != (ssize_t)*Count) 
   {
      static int cnt=0;
      if (cnt++ < 5)
         BUG("unable to write to output, fd=%d, count=%d: %m\n", OutputPath, *Count);
      return IO_ERROR;
   }

   *Count = 0;
   return NO_ERROR;
}

BOOL UXServices::GetStatusInfo (BYTE * bStatReg)
{
#ifdef HAVE_LIBHPIP
   unsigned int s;
   if (hpmud_get_device_status(hpFD, &s) == HPMUD_R_OK)
   {
      *bStatReg = (BYTE)s;
      return TRUE;
   }
#endif
   return FALSE;
}

DRIVER_ERROR UXServices::ReadDeviceID (BYTE * strID, int iSize)
{
#ifdef HAVE_LIBHPIP
   int len;
   hpmud_get_device_id(hpFD, (char *)strID, iSize, &len);
   if (len < 3)
      return IO_ERROR;
#endif
   return NO_ERROR;
}

#ifdef HP_PRINTVIEW
const char *szPJLHeader = "@PJL SET JOBATTR=\"JobAcct7=HPPrintView.exe\"\012";
int UXServices::GetPJLHeaderBuffer (char **szPJLBuffer)
{
    *szPJLBuffer = (char *) szPJLHeader;
    return strlen (szPJLHeader);
}
#endif // HP_PRINTVIEW

BOOL UXServices::GetVerticalAlignmentValue(BYTE* cVertAlignVal)
{
   if (VertAlign == -1)
      return FALSE;

   *cVertAlignVal = (BYTE)VertAlign;
   return TRUE;
}

BOOL UXServices::GetVertAlignFromDevice()
{
#ifdef HAVE_LIBHPIP
   if ((VertAlign = ReadHPVertAlign(hpFD)) == -1)
      return FALSE;
#endif
   return TRUE;
}

void UXServices::DisplayPrinterStatus (DISPLAY_STATUS ePrinterStatus)
{
   DisplayStatus = ePrinterStatus;
}

DRIVER_ERROR UXServices::BusyWait (DWORD msec)
{
   switch (DisplayStatus)
   {
      case DISPLAY_ERROR_TRAP:
      case DISPLAY_COMM_PROBLEM:
      case DISPLAY_PRINTER_NOT_SUPPORTED:
      case DISPLAY_OUT_OF_PAPER:
      case DISPLAY_PHOTOTRAY_MISMATCH:
      case DISPLAY_TOP_COVER_OPEN:
      case DISPLAY_NO_COLOR_PEN:
      case DISPLAY_NO_BLACK_PEN:
      case DISPLAY_NO_PENS:
         BUG("WARNING: printer bi-di error=%d\n", DisplayStatus);
         DisplayStatus = DISPLAY_PRINTING_CANCELED;
         return JOB_CANCELED;   /* bail-out otherwise APDK will wait forever */
      default:
         break;
   }
   return NO_ERROR;
}

const char * UXServices::GetDriverMessage (DRIVER_ERROR err)
{
   const char *p=NULL;

	/* Map driver error to text message. TODO: text needs to be localized. */
   switch(err)
   {
      case(WARN_MODE_MISMATCH):
         p = "printmode mismatch with pen, tray, etc.";
         break;
      case(WARN_LOW_INK_BOTH_PENS):
         p = "both pens have low ink";
         break;
      case(WARN_LOW_INK_BLACK):
         p = "black pen has low ink";
         break;
      case(WARN_LOW_INK_COLOR):
         p = "color pen has low ink";
         break;
      case(WARN_LOW_INK_PHOTO):
         p = "photo pen has low ink";
         break;
      case(WARN_LOW_INK_GREY):
         p = "grey pen has low ink";
         break;
      case(WARN_LOW_INK_BLACK_PHOTO):
         p = "black photo has low ink";
         break;
      case(WARN_LOW_INK_COLOR_PHOTO):
         p = "color photo pen has low ink";
         break;
      case(WARN_LOW_INK_GREY_PHOTO):
         p = "grey photo pen has low ink";
         break;
      case(WARN_LOW_INK_COLOR_GREY):
         p = "grey pen has low ink";
         break;
      case(WARN_LOW_INK_COLOR_GREY_PHOTO):
         p = "color grey photo pen has low ink";
         break;
      case(WARN_LOW_INK_COLOR_BLACK_PHOTO):
         p = "color back pen has low ink";
         break;
      case(WARN_LOW_INK_CYAN):
         p = "cyan has low ink";
         break;
      case(WARN_LOW_INK_MAGENTA):
         p = "magenta has low ink";
         break;
      case(WARN_LOW_INK_YELLOW):
         p = "yellow has low ink";
         break;
      case(WARN_LOW_INK_MULTIPLE_PENS):
         p = "more that one ink is low";
         break;
      case(WARN_FULL_BLEED_UNSUPPORTED):
         p = "fullbleed is not supported";
         break;
      case(WARN_FULL_BLEED_3SIDES):
         p = "fullbleed is 3 sides";
         break;
      case(WARN_FULL_BLEED_PHOTOPAPER_ONLY):
         p = "fullbleed photo paper only";
         break;
      case(WARN_FULL_BLEED_3SIDES_PHOTOPAPER_ONLY):
         p = "fullbleed 3 sides photo paper only";
         break;
      case(WARN_ILLEGAL_PAPERSIZE):
         p = "illegal paper size";
         break;
      case(WARN_INVALID_MEDIA_SOURCE):
         p = "invalid media source";
         break;
      default:
         p = "driver error";
         BUG("driver error=%d\n", err);
         break;
   }
   return p;
}

int UXServices::MapPaperSize (float width, float height)
{
    int    i, r, size;
    float  dx, dy;

    /* Map gs paper sizes to APDK paper sizes, or do custom. */
    size = CUSTOM_SIZE;
    for (i=0; i<MAX_PAPER_SIZE; i++)
    {
        r = pPC->SetPaperSize ((PAPER_SIZE)i);

        if (r != NO_ERROR)
            continue;

        dx = width  > pPC->PhysicalPageSizeX () ? width  - pPC->PhysicalPageSizeX () : pPC->PhysicalPageSizeX () - width;
        dy = height > pPC->PhysicalPageSizeY () ? height - pPC->PhysicalPageSizeY () : pPC->PhysicalPageSizeY () - height;

        if ((dx < 0.05) && (dy < 0.05))
        {
            size = i;   /* found standard paper size */
            break;
        }
    }

    if (size == CUSTOM_SIZE)
        pPC->SetCustomSize (width, height);

    if ((r = pPC->SetPaperSize ((PAPER_SIZE)size, FullBleed)) != NO_ERROR)
    {
        if (r > 0)
        {
            BUG("unable to set paper size=%d, err=%d, width=%0.5g, height=%0.5g\n", size, r, width, height);
        }
        else 
        {
            BUG("warning setting paper size=%d, err=%d, width=%0.5g, height=%0.5g\n", size, r, width, height);
        }
/*
 *      Call failed, reset our PaperWidth and PaperHeight values.
 *      This ensures that we return correct values when gs queries for printable area.
 */

        PaperWidth  = pPC->PhysicalPageSizeX ();
        PaperHeight = pPC->PhysicalPageSizeY ();
        return -1;
    }

    PaperWidth  = pPC->PhysicalPageSizeX ();
    PaperHeight = pPC->PhysicalPageSizeY ();

    return 0; 
}

void UXServices::ResetIOMode (BOOL bDevID, BOOL bStatus)
{
    if (pPC)
    {
        IOMode.bDevID  = bDevID;
        IOMode.bStatus = bStatus;
        pPC->ResetIOMode (bDevID, bStatus);
    }
}

void UXServices::InitSpeedMechBuffer ()
{
    if (m_pbyPclBuffer)
    {
        return;
    }
    m_pbyPclBuffer = new BYTE[m_iPclBufferSize + 2];
    if (m_pbyPclBuffer)
    {
        iSendBufferSize = 0;
    }
}

int UXServices::SendPreviousPage ()
{
    DRIVER_ERROR    err;
    if (m_bSpeedMechEnabled == FALSE)
    {
        return 0;
    }
    m_iPageCount++;
    if (m_iPageCount == 1)
    {
        return 0;
    }
    m_bSpeedMechEnabled = FALSE;
    err = ToDevice (m_pbyPclBuffer, (DWORD *) &m_iCurPclBufferPos);
    if (err != NO_ERROR)
    {
        return 1;
    }
    m_bSpeedMechEnabled = TRUE;
    m_iCurPclBufferPos = 0;

//  Request the printer to inject speed mech command. Also, let it know this is not the last page

    pPC->SetPrinterHint (SPEED_MECH_HINT, 0);
    return 0;
}

int UXServices::CopyData (const BYTE *pBuffer, DWORD iCount)
{
    if (m_iCurPclBufferPos + (int) iCount < m_iPclBufferSize)
    {
        memcpy (m_pbyPclBuffer + m_iCurPclBufferPos, pBuffer, iCount);
        m_iCurPclBufferPos += iCount;
       return 0;
    }
    BYTE    *p = new BYTE[m_iPclBufferSize + BUFFER_CHUNK_SIZE + 2];
    if (p == NULL)
    {
        m_bSpeedMechEnabled = FALSE;
	return 1;
    }
    memcpy (p, m_pbyPclBuffer, m_iCurPclBufferPos);
    delete [] m_pbyPclBuffer;
    m_pbyPclBuffer = p;
    memcpy (m_pbyPclBuffer + m_iCurPclBufferPos, pBuffer, iCount);
    m_iCurPclBufferPos += iCount;
    m_iPclBufferSize += BUFFER_CHUNK_SIZE;
    return 0;
}

//  Note that this is good only for VIP printers
const char *pbySpeedMechCmd = "\x1B*o5W\x0D\x02\x00";

void UXServices::SendLastPage ()
{
    if (m_pbyPclBuffer == NULL)
    {
        return;
    }
    // Look for speed mech command in the buffer, set the page count and last page flag
    int    i = 0;
    BYTE   *p = m_pbyPclBuffer;
    while (i < m_iPclBufferSize)
    {
        if (*p == '\x1B')
	{
	    if (!(memcmp (p, pbySpeedMechCmd, 8)))
	    {
	        p += 8;
		*p++ = (BYTE) ((m_iPageCount & 0xFF00) >> 8);
		*p++ = (BYTE) ((m_iPageCount & 0x00FF));
		*(p + 9) = 1;
		break;
	    }
	}
        i++;
	p++;
    }
    m_bSpeedMechEnabled = FALSE;
    ToDevice (m_pbyPclBuffer, (DWORD *) &m_iCurPclBufferPos);
    delete [] m_pbyPclBuffer;
}

#if defined(HAVE_LIBHPIP) && defined(HAVE_DBUS) 
void SendDbusMessage (const char *dev, const char *printer, int code, 
                      const char *username, const int jobid, const char *title)
{
    DBusMessage * msg = NULL;

    InitDbus ();
    if (dbus_conn == NULL)
        return;
    msg = dbus_message_new_signal(DBUS_PATH, DBUS_INTERFACE, "Event");

    if (NULL == msg)
    {
        BUG("dbus message is NULL!\n");
        return;
    }

    dbus_message_append_args(msg, 
        DBUS_TYPE_STRING, &dev,
        DBUS_TYPE_STRING, &printer,
        DBUS_TYPE_UINT32, &code, 
        DBUS_TYPE_STRING, &username, 
        DBUS_TYPE_UINT32, &jobid,
        DBUS_TYPE_STRING, &title, 
        DBUS_TYPE_INVALID);

    if (!dbus_connection_send(dbus_conn, msg, NULL))
    {
        BUG("dbus message send failed!\n");
        return;
    }

    dbus_connection_flush(dbus_conn);
    dbus_message_unref(msg);

    return;
}

void InitDbus (void)
{
   dbus_error_init (&dbus_err);
   dbus_conn = dbus_bus_get (DBUS_BUS_SYSTEM, &dbus_err);
    
   if (dbus_error_is_set (&dbus_err))
   { 
      BUG ("dBus Connection Error (%s)!\n", dbus_err.message); 
      dbus_error_free (&dbus_err); 
   }

   return;
}
#endif  /* HAVE_DBUS */


