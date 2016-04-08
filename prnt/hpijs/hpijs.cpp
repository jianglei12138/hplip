/*****************************************************************************\
    hpijs.cpp : HP Inkjet Server

    Copyright (c) 2001 - 2008, HP Co.
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
#include "utils.h"

extern void SendDbusMessage (const char *dev, const char *printer, int code, 
                             const char *username, const int jobid, const char *title);

#ifdef HAVE_LIBHPIP
extern  int hpijsFaxServer (int argc, char **argv);
#endif

#if 0
int bug(const char *fmt, ...)
{
   char buf[256];
   va_list args;
   int n;

   va_start(args, fmt);

   if ((n = vsnprintf(buf, 256, fmt, args)) == -1)
      buf[255] = 0;     /* output was truncated */

   fprintf(stderr, buf);
   syslog(LOG_WARNING, buf);

   fflush(stderr);
   va_end(args);
   return n;
}
#endif

void setLogLevel(UXServices *pSS, char*user_name)
{
    FILE    *fp;
    char    str[258];
    char    *p;
    fp = fopen ("/etc/cups/cupsd.conf", "r");
    if (fp == NULL)
        return;
    while (!feof (fp))
    {
        if (!fgets (str, 256, fp))
	{
	    break;
	}
	if ((p = strstr (str, "hpLogLevel")))
	{
	    p += strlen ("hpLogLevel") + 1;
	    pSS->m_iLogLevel = atoi (p);
	    break;
	}
    }
    fclose (fp);

    if (pSS->m_iLogLevel & SAVE_PCL_FILE)
    {
        char    szFileName[MAX_FILE_PATH_LEN];
        snprintf (szFileName,sizeof(szFileName), "%s/hp_%s_ijs_%d_XXXXXX", CUPS_TMP_DIR, user_name,  getpid());
        createTempFile(szFileName, &pSS->outfp);

//	pSS->outfp = fopen (szFileName, "w");
	if (pSS->outfp)
	{
	    chmod (szFileName, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	}
    }
}

/* Set Print Context. */
int hpijs_set_context(UXServices *pSS)
{
   int r;

   if (pSS->PenSet != DUMMY_PEN)
   {
     if ((r = pSS->pPC->SetPenSet((PEN_TYPE)pSS->PenSet)) != NO_ERROR)
        BUG("unable to SetPenSet set=%d, err=%d\n", pSS->PenSet, r);
   }

   if ((r = pSS->pPC->SelectPrintMode((QUALITY_MODE)pSS->Quality, (MEDIATYPE)pSS->MediaType, (COLORMODE)pSS->ColorMode)) !=  NO_ERROR)
   {
      BOOL        bDevText;
      BUG("unable to set Quality=%d, ColorMode=%d, MediaType=%d, err=%d\n", pSS->Quality, pSS->ColorMode, pSS->MediaType, r);
      pSS->pPC->GetPrintModeSettings((QUALITY_MODE &)pSS->Quality, (MEDIATYPE &)pSS->MediaType, (COLORMODE &)pSS->ColorMode, bDevText);
      BUG("following will be used Quality=%d, ColorMode=%d, MediaType=%d\n", pSS->Quality, pSS->ColorMode, pSS->MediaType);
   }

   /* Map ghostscript paper size to APDK paper size. */
   pSS->MapPaperSize(pSS->PaperWidth, pSS->PaperHeight); 

   /* Do duplex stuff now, since we have a valid print mode. */
   if (pSS->Duplex && !pSS->Tumble)
   {
      if (pSS->pPC->SelectDuplexPrinting(DUPLEXMODE_BOOK) != TRUE)
         BUG("unable to set duplex mode=book\n");
   }
   else if (pSS->Duplex && pSS->Tumble)
   {
      if (pSS->pPC->SelectDuplexPrinting(DUPLEXMODE_TABLET) != TRUE)
         BUG("unable to set duplex mode=tablet\n");
   }
   else
   {
      pSS->pPC->SelectDuplexPrinting(DUPLEXMODE_NONE);
   }

   if (pSS->MediaPosition != sourceTrayAuto)
   {
      if (pSS->pPC->SetMediaSource((MediaSource)pSS->MediaPosition) != NO_ERROR)
         BUG("unable to set MediaPosition=%d\n", pSS->MediaPosition);
   }

   return 0;
}

int hpijs_status_cb(void *status_cb_data, IjsServerCtx *ctx, IjsJobId job_id)
{
   return 0;
}

int hpijs_list_cb(void *list_cb_data, IjsServerCtx *ctx, IjsJobId job_id,
        char *val_buf, int val_size)
{
   return snprintf(val_buf, val_size, "OutputFD,DeviceManufacturer,DeviceModel,PageImageFormat,Dpi,Width,Height,BitsPerSample,ColorSpace,PaperSize,PrintableArea,PrintableTopLeft,DryTime,PS:Duplex,PS:Tumble,Quality:Quality,Quality:MediaType,Quality:ColorMode,Quality:PenSet,Quality:FullBleed,PS:MediaPosition");
}

int hpijs_enum_cb(void *enum_cb_data, IjsServerCtx *ctx, IjsJobId job_id,
       const char *key, char *val_buf, int val_size)
{
   UXServices *pSS = (UXServices*)enum_cb_data;

   if (!strcmp (key, "ColorSpace"))
   {
      if (pSS->pPC->SupportSeparateBlack())
         return snprintf(val_buf, val_size, "sRGB,KRGB");
      else
         return snprintf(val_buf, val_size, "sRGB");
   }
   else if (!strcmp (key, "DeviceManufacturer"))
      return snprintf(val_buf, val_size, "HEWLETT-PACKARD,APOLLO,HP");
   else if (!strcmp (key, "PageImageFormat"))
      return snprintf(val_buf, val_size, "Raster");
   else if (!strcmp (key, "BitsPerSample"))
      return snprintf(val_buf, val_size, "8");
   else if (!strcmp (key, "PS:Duplex"))
   {
      if (pSS->pPC->SelectDuplexPrinting(DUPLEXMODE_BOOK) == TRUE)
         return snprintf(val_buf, val_size, "true");
      else
         return snprintf(val_buf, val_size, "false");
      pSS->pPC->SelectDuplexPrinting(DUPLEXMODE_NONE);
   }
   else if (!strcmp (key, "PS:Tumble"))
   {
      if (pSS->pPC->SelectDuplexPrinting(DUPLEXMODE_TABLET) == TRUE)
         return snprintf(val_buf, val_size, "true");
      else
         return snprintf(val_buf, val_size, "false");
      pSS->pPC->SelectDuplexPrinting(DUPLEXMODE_NONE);
   }
   else
      BUG("unable to enum key=%s\n", key);    
  return IJS_ERANGE;
}

/*
 * Set parameter (in the server) call back. Note, OutputFD is the only call that can be
 * preceded by set DeviceManufacturer and DeviceModel.
 */
int hpijs_set_cb (void *set_cb_data, IjsServerCtx *ctx, IjsJobId job_id,
                  const char *key, const char *value, int value_size)
{
    UXServices  *pSS = (UXServices*)set_cb_data;
    int         fd, r;
    char        *tail;
    int         status = 0;
    char        svalue[IJS_MAX_PARAM+1];   
    float       w, h, dx, dy;
    int         iVal;

    /* Sanity check input value. */
    if (value_size > IJS_MAX_PARAM)
    {
        memcpy(svalue, value, IJS_MAX_PARAM);
        svalue[IJS_MAX_PARAM] = 0;
    }
    else
    {
        memcpy(svalue, value, value_size);
        svalue[value_size] = 0;
    }

    if (!strcmp (key, "OutputFD"))
    {
        fd = strtol(svalue, &tail, 10);
        pSS->OutputPath = fd;   /* set prn_stream as output of SS::ToDevice */
    }
    else if (!strcmp (key, "DeviceManufacturer"))
    {
        if ((strncasecmp(svalue, "HEWLETT-PACKARD", 15) != 0) &&
            (strncasecmp(svalue, "APOLLO", 6) != 0) && (strncasecmp(svalue, "HP", 2) != 0))
        {
            BUG("unable to set DeviceManufacturer=%s\n", svalue);
            status = -1;
        }
    }
    else if (!strcmp (key, "DeviceModel"))
    {
        if ((r = pSS->pPC->SelectDevice(svalue)) != NO_ERROR)
        {
	    if (r == PLUGIN_LIBRARY_MISSING)
	    {
		// call dbus here
		const char    *user_name = " ";
		const char    *title     = " ";
                const char *device_uri = getenv ("DEVICE_URI");
                const char *printer = getenv ("PRINTER");
		int     job_id = 0;

                if (device_uri == NULL)
                    device_uri = "";
                if (printer == NULL)
                    printer = "";

                SendDbusMessage (device_uri, printer,
	     	                 EVENT_PRINT_FAILED_MISSING_PLUGIN,
				 user_name, job_id, title);
                BUG("unable to set device=%s, err=%d\n", svalue, r);
                status = -1;
	    }
	    else
	    {
                /* OfficeJet LX is not very unique, do separate check here. */
                if (!strncmp(svalue,"OfficeJet", 10))
                r = pSS->pPC->SelectDevice("DESKJET 540");
	    }
        }

        if (r == NO_ERROR)
        {
            pSS->Model = 1;

            /* Got a valid device class, let's set some print mode defaults. */
            BOOL        bDevText;
            pSS->pPC->GetPrintModeSettings((QUALITY_MODE &)pSS->Quality, (MEDIATYPE &)pSS->MediaType, (COLORMODE &)pSS->ColorMode, bDevText);
        }
        else
        {
            BUG("unable to set device=%s, err=%d\n", svalue, r);
            status = -1;
        }
    }
    else if ((strcmp (key, "PS:Duplex") == 0) || (strcmp (key, "Duplex") == 0))
    {
        if (strncmp(svalue, "true", 4) == 0)
            pSS->Duplex = 1;
        else
            pSS->Duplex = 0;
    }
    else if ((strcmp (key, "PS:Tumble") == 0) || (strcmp (key, "Tumble") == 0))
    {
        if (strncmp(svalue, "true", 4) == 0)
            pSS->Tumble = 1;
        else
            pSS->Tumble = 0;
    }
    else if (!strcmp (key, "PaperSize"))
    {
        w = (float)strtod(svalue, &tail);
        h = (float)strtod(tail+1, &tail);

        if (pSS->FirstRaster)
        {
            /* Normal start of print Job. */
            pSS->PaperWidth = w;
            pSS->PaperHeight = h;
            hpijs_set_context(pSS);
        }
        else
        {

            dx = w > pSS->PaperWidth ? w - pSS->PaperWidth : pSS->PaperWidth - w;
            dy = h > pSS->PaperHeight ? h - pSS->PaperHeight :  pSS->PaperHeight - h;

            /* Middle of print Job, ignore paper size if same. */
            if ((dx > 0.25) || (dy > 0.25))
            {
                pSS->FirstRaster = 1;  /* force new Job */
                pSS->PaperWidth = w;   /* set new paper size */
                pSS->PaperHeight = h;
                hpijs_set_context(pSS);
            }
        }
    }
    else if (!strcmp (key, "TopLeft"))
    {
        /* not currently used */
    }
    else if (!strcmp (key, "Quality:Quality"))
    {
        pSS->Quality = (QUALITY_MODE) strtol(svalue, &tail, 10);
    }
    else if (!strcmp (key, "Quality:MediaType"))
    {
        pSS->MediaType = (MEDIATYPE) strtol(svalue, &tail, 10);
    }
    else if (!strcmp (key, "Quality:ColorMode"))
    {
        pSS->ColorMode = (COLORMODE) strtol(svalue, &tail, 10);
    }
    else if (!strcmp (key, "Quality:PenSet"))
    {
        pSS->PenSet = (PEN_TYPE) strtol(svalue, &tail, 10);
    }
    else if (!strcmp (key, "Quality:FullBleed"))
    {
        pSS->FullBleed = strtol(svalue, &tail, 10);
    }
    else if (!strcmp (key, "PS:MediaPosition"))
    {
        pSS->MediaPosition = strtol(svalue, &tail, 10);
    }
    else if (!strcmp (key, "DryTime"))
    {
        iVal = strtol (svalue, &tail, 10);
        pSS->pPC->SetPrinterHint (EXTRA_DRYTIME_HINT, iVal);
    }
    else if (!strcmp (key, "RedEye"))
    {
        iVal = strtol (svalue, &tail, 10);
        pSS->pPC->SetPrinterHint (RED_EYE_REMOVAL_HINT, iVal);
    }
    else if (!strcmp (key, "PhotoFix"))
    {
        iVal = strtol (svalue, &tail, 10);
        pSS->pPC->SetPrinterHint (PHOTO_FIX_HINT, iVal);
    }
    else if (!strcmp (key, "MaxJpegFileSize"))
    {
        iVal = strtol (svalue, &tail, 10);
        pSS->pPC->SetPrinterHint (MAX_FILE_SIZE_HINT, iVal);
    }  
    else if (!strcmp (key, "Quality:SpeedMech") && !pSS->Duplex)
    {
        pSS->pPC->SetPrinterHint (PAGES_IN_DOC_HINT, 512);
	pSS->EnableSpeedMech (TRUE);
    }
    else if (!strcmp (key, "Quality:MediaSubtype"))
    {
        iVal = strtol (svalue, &tail, 10);
        pSS->pPC->SetMediaSubtype (iVal);
    }
//  The next 5 values are passed in as inch * 1000
    else if (!strcmp (key, "Margin:TopPadding"))
    {
        iVal = strtol (svalue, &tail, 10);
        pSS->pPC->SetMechOffset (iVal);
    }
    else if (!strcmp (key, "Overspray:Left"))
    {
        iVal = strtol (svalue, &tail, 10);
        pSS->pPC->SetPrinterHint (LEFT_OVERSPRAY_HINT, iVal);
    }
    else if (!strcmp (key, "Overspray:Top"))
    {
        iVal = strtol (svalue, &tail, 10);
        pSS->pPC->SetPrinterHint (TOP_OVERSPRAY_HINT, iVal);
    }
    else if (!strcmp (key, "Overspray:Right"))
    {
        iVal = strtol (svalue, &tail, 10);
        pSS->pPC->SetPrinterHint (RIGHT_OVERSPRAY_HINT, iVal);
    }
    else if (!strcmp (key, "Overspray:Bottom"))
    {
        iVal = strtol (svalue, &tail, 10);
        pSS->pPC->SetPrinterHint (BOTTOM_OVERSPRAY_HINT, iVal);
    }
    else
        BUG("unable to set key=%s, value=%s\n", key, svalue);    

    return status;
}

/* Get parameter (from the server) call back. Note, all calls must be preceded by set DeviceName. */
int hpijs_get_cb(void *get_cb_data, IjsServerCtx *ctx, IjsJobId job_id, const char *key, char *value_buf, int value_size)
{
   UXServices *pSS = (UXServices*)get_cb_data;
   float        fX;
   float        fY;

   if (!strcmp (key, "PrintableArea"))
   {
       fY = pSS->pPC->PrintableHeight ();
      /* If duplexing, adjust printable height to 1/2 inch top/bottom margins, except laserjets. */
      if ((pSS->pPC->QueryDuplexMode() != DUPLEXMODE_NONE) && pSS->pPC->RotateImageForBackPage() && (FALSE == pSS->pPC->IsBorderless()))
      {
          // User has requested fullbleed printing and printer supports 4-sided fullbleed or
          // top and bottom margin are equal (0.125"), then
          // don't need top and bottom margins to be 0.5" each. In other words, if physical page size -
          // printable height is more than 0.25 inches, adjustment is required for symmetry.
          // Update - 11/22/05: Not so, the autoduplexer still requires a half inch margin.
#if 0
         if ((pSS->pPC->PrintableHeight () + 0.28) < pSS->pPC->PhysicalPageSizeY ())
         {
             fY = pSS->pPC->PhysicalPageSizeY () - 1.0;
         }
#endif
         fY = pSS->pPC->PhysicalPageSizeY () - 1.0;

         // SuperB size paper requires larger margins
         if (pSS->pPC->PhysicalPageSizeY () > 18.0)
         {
             fY = 1.5;
         }

      }

/*
 *      Fullbleed printing is requested and printer supports it, then
 *      return the unadjusted physical width and height.
 */

      if ((fX = pSS->pPC->PrintableWidth ()) > pSS->PaperWidth)
      {
          fX = pSS->PaperWidth;
          fY = pSS->PaperHeight;
      }

      return (snprintf (value_buf, value_size, "%.4fx%.4f", fX, fY));
   }
   else if (!strcmp (key, "PrintableTopLeft"))
   {
       fY = pSS->pPC->PrintableStartY (); 
      /* If duplexing, adjust printable top to 1/2 inch top margin, except laserjets. */
      if ((pSS->pPC->QueryDuplexMode() != DUPLEXMODE_NONE) && pSS->pPC->RotateImageForBackPage() && (FALSE == pSS->pPC->IsBorderless()))
      {
#if 0
         if ((pSS->pPC->PrintableHeight () + 0.28) < pSS->pPC->PhysicalPageSizeY ())
         {
             fY = 0.5;
         }
#endif
         fY = 0.5;
         // SuperB size paper requires larger margins
         if (pSS->pPC->PhysicalPageSizeY () > 18.0)
         {
             fY = 0.75;
         }

      }

      return snprintf (value_buf, value_size, "%.4fx%.4f", pSS->pPC->PrintableStartX (), fY);
   }
   else if ((!strcmp (key, "Duplex")) || (!strcmp (key, "PS:Duplex")))
   {
      if (pSS->pPC->QueryDuplexMode() == DUPLEXMODE_NONE)
         return snprintf(value_buf, value_size, "false");
      else
         return snprintf(value_buf, value_size, "true");
   }
   else if ((!strcmp (key, "Tumble")) || (!strcmp (key, "PS:Tumble")))
   {
      if (pSS->pPC->QueryDuplexMode() == DUPLEXMODE_TABLET)
         return snprintf(value_buf, value_size, "true");
      else
         return snprintf(value_buf, value_size, "false");
   }
   else if (!strcmp (key, "PaperSize"))
   {
      return snprintf(value_buf, value_size, "%.4fx%.4f", pSS->pPC->PhysicalPageSizeX(), pSS->pPC->PhysicalPageSizeY());
   }
   else if (!strcmp (key, "Dpi"))
   {
      return snprintf(value_buf, value_size, "%dx%d", pSS->pPC->EffectiveResolutionX(), pSS->pPC->EffectiveResolutionY());
   }
   else if (!strcmp (key, "DeviceModel"))
   {
      return snprintf(value_buf, value_size, "%s", pSS->pPC->PrinterModel());
   }
   else if (!strcmp (key, "Quality:Quality"))
   {
      return snprintf(value_buf, value_size, "%d", pSS->Quality);
   }
   else if (!strcmp (key, "Quality:ColorMode"))
   {
      return snprintf(value_buf, value_size, "%d", pSS->ColorMode);
   }
   else if (!strcmp (key, "Quality:MediaType"))
   {
      return snprintf(value_buf, value_size, "%d", pSS->MediaType);
   }
   else if (!strcmp (key, "ColorSpace"))
   {
      return snprintf(value_buf, value_size, "%s", pSS->ph.cs);
   }
   else if (!strcmp (key, "PageImageFormat"))
   {
      return snprintf(value_buf, value_size, "Raster");
   }
   else if (!strcmp (key, "BitsPerSample"))
   {
      return snprintf(value_buf, value_size, "8");
   }
   else if (!strcmp (key, "PS:MediaPosition"))
   {
      return snprintf(value_buf, value_size, "%d", pSS->MediaPosition);
   }
   else
      BUG("unable to get key=%s\n", key);    

   return IJS_EUNKPARAM;
}

/* Get raster from the client. */
int hpijs_get_client_raster(IjsServerCtx *ctx, char *buf, int size, char white)
{
   int status, clean=1, i;

   status = ijs_server_get_data(ctx, (char *)buf, size);

   if (status < 0)
      return status;  /* error */

   /* Check for blank raster. */
   for (i = 0; i < size; i++)
   {
      if (buf[i] != white)
      {
         clean = 0;
         break;
      }
   }

   if (clean)
      return 0;

   return size;
}

int main (int argc, char *argv[], char *evenp[])
{
   UXServices *pSS = NULL;
   IjsServerCtx *ctx = NULL;
   char *raster = NULL, *k_raster = NULL;
   int status = EXIT_FAILURE;
   int ret, n, i, kn=0, width, k_width;
   char user_name[32]={0,};
        
   openlog("hpijs", LOG_PID,  LOG_DAEMON);

   if (argc > 1)
   {
      const char *arg = argv[1];
      if ((arg[0] == '-') && (arg[1] == 'h'))
      {
         fprintf(stdout, "\nHP Co. Inkjet Server %s\n", VERSION);
         fprintf(stdout, "Copyright (c) 2001-2004, HP Co.\n");
         exit(0);
      }
   }

   if (argc > 2)
        strncpy(user_name, argv[2], sizeof(user_name));

#ifdef HAVE_LIBHPIP
   char *pDev;
   if ((pDev = getenv ("DEVICE_URI")) &&
       ((strncmp (pDev, "hpfax:", 6)) == 0))
   {
       exit ( hpijsFaxServer (argc, argv));
   }
#endif

   ctx = ijs_server_init();
   if (ctx == NULL)
   {
      BUG("unable to init hpijs server\n");
      goto BUGOUT;
   }

   pSS = new UXServices();
   if (pSS->constructor_error != NO_ERROR)
   {
      BUG("unable to open Services object err=%d\n", pSS->constructor_error);
      goto BUGOUT;
   }

   setLogLevel(pSS, user_name);

#ifdef CAPTURE
   char szCapOutFile[MAX_FILE_PATH_LEN];
   snprintf(szCapOutFile, sizeof(szCapOutFile),"%s/hp_%s_ijs_capout_XXXXXX",CUPS_TMP_DIR, user_name);
   if ((pSS->InitScript(szCapOutFile, TRUE)) != NO_ERROR)
      BUG("unable to init capture");
#endif

   
   pSS->pPC = new PrintContext (pSS, 0, 0);

   /* Ignore JOB_CANCELED. This a bi-di hack that allows the job to continue even if bi-di communication failed. */
   if (pSS->pPC->constructor_error > 0 && pSS->DisplayStatus != DISPLAY_PRINTING_CANCELED)
   {
      BUG("unable to open PrintContext object err=%d\n", pSS->pPC->constructor_error);
      goto BUGOUT;
   }

/*
 *  Ignore the WARN_MODE_MISMATCH warning. This will happen if we are talking to a monochrome printer.
 *  We will select the correct printmode later.
 */

    if (pSS->pPC->constructor_error < 0 &&
        pSS->pPC->constructor_error != WARN_MODE_MISMATCH)
    {
        BUG ("WARNING: %s\n", pSS->GetDriverMessage (pSS->pPC->constructor_error));
		switch (pSS->pPC->constructor_error)
		{
			case WARN_LOW_INK_BOTH_PENS:
			case WARN_LOW_INK_BLACK:
			case WARN_LOW_INK_COLOR:
			case WARN_LOW_INK_PHOTO:
			case WARN_LOW_INK_GREY:
			case WARN_LOW_INK_BLACK_PHOTO:
			case WARN_LOW_INK_COLOR_PHOTO:
			case WARN_LOW_INK_GREY_PHOTO:
			case WARN_LOW_INK_COLOR_GREY:
			case WARN_LOW_INK_COLOR_GREY_PHOTO:
			case WARN_LOW_INK_COLOR_BLACK_PHOTO:
			case WARN_LOW_INK_CYAN:
			case WARN_LOW_INK_MAGENTA:
			case WARN_LOW_INK_YELLOW:
			case WARN_LOW_INK_MULTIPLE_PENS:
                        {
                           fputs("STATE: +marker-supply-low-warning\n", stderr);
                           break;
                        }
			default:
                           fputs("STATE: +marker-supply-low-warning\n", stderr);
		}
    }

#if 0
   BUG("device model=%s\n", pSS->pPC->PrinterModel());
   BUG("device class=%s\n",  pSS->pPC->PrintertypeToString(pSS->pPC->SelectedDevice()));
   BUG("default pen=%d\n",  pSS->pPC->GetDefaultPenSet());
   BUG("installed pen=%d\n",  pSS->pPC->GetInstalledPens());
#endif

   ijs_server_install_status_cb (ctx, hpijs_status_cb, pSS);
   ijs_server_install_list_cb (ctx, hpijs_list_cb, pSS);
   ijs_server_install_enum_cb (ctx, hpijs_enum_cb, pSS);
   ijs_server_install_set_cb (ctx, hpijs_set_cb, pSS);
   ijs_server_install_get_cb (ctx, hpijs_get_cb, pSS);

   while (1)
   {
      if ((ret = ijs_server_get_page_header(ctx, &pSS->ph)) < 0)
      {
         BUG("unable to read client data err=%d\n", ret);
         goto BUGOUT;
      }

      if (pSS->Model == -1)
         goto BUGOUT;      /* no device selected */

      if (ret)
      {
         status = 0; /* normal exit */
         break;
      }

      if (pSS->FirstRaster)
      {
          char  *pEnv = getenv ("COPY_COUNT");
          if (pEnv)
          {
              i = atoi (pEnv);
              pSS->pPC->SetCopyCount (i);
          }

         pSS->FirstRaster = 0;

         width = (int)(pSS->ph.xres * pSS->pPC->PrintableWidth() + 0.5);

         /* Desensitize input width, may be off by one due to paper size conversions. */
         if (pSS->ph.width < width)
            width = pSS->ph.width;

         if ((ret = pSS->pPC->SetPixelsPerRow(width, width)) != NO_ERROR)
         {
            BUG("unable to SetPixelsPerRow width=%d, err=%d\n", pSS->ph.width, ret);
         }

         /* Turn off any bi-di support. Allow bi-di for printer capabilities only. */
//         pSS->IOMode.bDevID = pSS->IOMode.bStatus = FALSE;
         pSS->ResetIOMode (FALSE, FALSE);

//       Turn off SpeedMech in duplex printing mode
         if (pSS->Duplex)
	 {
	     pSS->EnableSpeedMech (FALSE);
	 }

	 if (pSS->IsSpeedMechEnabled ())
	 {
	     pSS->InitSpeedMechBuffer ();
	 }

         if (pSS->pJob != NULL)
            delete pSS->pJob;
         pSS->pJob = new Job(pSS->pPC);
         if (pSS->pJob->constructor_error != NO_ERROR)
         {
            BUG("unable to create Job object err=%d\n", pSS->pJob->constructor_error);
            goto BUGOUT;
         }

         if (pSS->pPC->QueryDuplexMode() != DUPLEXMODE_NONE)
         {
            if ((pSS->pPC->QueryDuplexMode() == DUPLEXMODE_BOOK) && pSS->pPC->RotateImageForBackPage())
               pSS->InitDuplexBuffer();
            pSS->BackPage = FALSE;
         }

         pSS->KRGB=0;
         if (strcmp(pSS->ph.cs, "KRGB") == 0)
            pSS->KRGB=1;
         
#if 0
         BUG("papersize=%d\n", pSS->pPC->GetPaperSize());
         BUG("width=%d, height=%d\n", pSS->ph.width, pSS->ph.height);
         BUG("EffResX=%d, EffResY=%d, InPixelsPerRow=%d, OutPixelsPerRow=%d\n", 
            pSS->pPC->EffectiveResolutionX(), pSS->pPC->EffectiveResolutionY(), 
            pSS->pPC->InputPixelsPerRow(), pSS->pPC->OutputPixelsPerRow());
         BUG("device=%s\n", pSS->pPC->PrinterModel());
#endif
      } // pSS->FirstRaster

      if ((raster = (char *)malloc(pSS->ph.width*3)) == NULL)
      {
         BUG("unable to allocate raster buffer size=%d: %m\n", pSS->ph.width*3);
         goto BUGOUT;
      }

      k_width = (pSS->ph.width + 7) >> 3;  /* width of k plane in bytes, byte aligned */

      if ((k_raster = (char *)malloc(k_width)) == NULL)
      {
         BUG("unable to allocate black raster buffer size=%d: %m\n", k_width);
         goto BUGOUT;
      }
      memset(k_raster, 0, k_width);

      pSS->SendPreviousPage ();
      for (i=0; i < pSS->ph.height; i++)      
      {
         if ((n = hpijs_get_client_raster(ctx, raster, pSS->ph.width*3, 0xff)) < 0)
            break;    /* error */
         if (pSS->KRGB)
         {
            if ((kn = hpijs_get_client_raster(ctx, k_raster, k_width, 0)) < 0)
               break;    /* error */
         }
         if (n == 0 && kn == 0)
            pSS->ProcessRaster((char *)0, (char *)0);  /* blank raster */
         else if (kn == 0)
            pSS->ProcessRaster(raster, (char *)0);
         else if (n == 0)
            pSS->ProcessRaster((char *)0, k_raster);
         else 
            pSS->ProcessRaster(raster, k_raster);
      }

      free(raster);
      raster = NULL;
      free(k_raster);
      k_raster = NULL;

      if (pSS->pPC->QueryDuplexMode() != DUPLEXMODE_NONE)
      {
         if ((pSS->pPC->QueryDuplexMode() == DUPLEXMODE_BOOK) && pSS->pPC->RotateImageForBackPage() && pSS->BackPage)
         {
            pSS->SendBackPage();
         }
         pSS->BackPage = (BOOL)((int)pSS->BackPage + 1) % 2;
      }

      pSS->pJob->NewPage();

      
   } /* end while (1) */

   if (pSS->pPC->QueryDuplexMode() != DUPLEXMODE_NONE)
   {
      if (pSS->BackPage)
      {
         /* Send extra blank line & newpage to eject the page. (for VIP printers). */
         /* For malibu send enough blank lines to cause at least two blank rasters in Job::sendrasters(). 5/1/03, des */
         //         for (int i = 0; i < 201; i++)
         for (int i = 0; i < 401; i++)
            pSS->pJob->SendRasters((unsigned char *)0);
         pSS->pJob->NewPage();
      }
   }

BUGOUT:
   if (pSS != NULL)
   {
      if (pSS->pJob != NULL)
         delete pSS->pJob;
      if (pSS->pPC != NULL)
         delete pSS->pPC;
#ifdef CAPTURE
      pSS->EndScript();
#endif
      delete pSS;
   }
   if (raster != NULL)
      free(raster);
   if (k_raster != NULL)
      free(k_raster);
   if (ctx != NULL)
      ijs_server_done(ctx);

   exit(status);
}

