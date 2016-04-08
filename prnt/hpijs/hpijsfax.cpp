/*****************************************************************************\
    hpijs.cpp : HP Inkjet Server

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

#ifdef HAVE_LIBHPIP

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include "ijs.h"
#include "ijs_server.h"
#ifdef FALSE
#undef FALSE
#endif
#ifdef TRUE
#undef TRUE
#endif
#include "hpip.h"
#include "hpijsfax.h"
#include "utils.h"


int hpijsfax_status_cb (void *status_cb_data, IjsServerCtx *ctx, IjsJobId job_id)
{
    return 0;
}

int hpijsfax_list_cb (void *list_cb_data, IjsServerCtx *ctx, IjsJobId job_id,
                   char *val_buf, int val_size)
{
    return snprintf (val_buf, val_size, "OutputFD,DeviceManufacturer,DeviceModel,\
                                         PageImageFormat,Dpi,Width,Height,BitsPerSample,\
										 ColorSpace,PaperSize,PrintableArea,\
										 PrintableTopLeft");
}

int hpijsfax_enum_cb (void *enum_cb_data, IjsServerCtx *ctx, IjsJobId job_id,
                   const char *key, char *val_buf, int val_size)
{

	if (!strcmp (key, "ColorSpace"))
	{
	     return snprintf(val_buf, val_size, "sRGB");
//	    return snprintf(val_buf, val_size, "DeviceGray");
	}
	else if (!strcmp (key, "DeviceManufacturer"))
	{
		return snprintf(val_buf, val_size, "HEWLETT-PACKARD,HP");
	}
	else if (!strcmp (key, "PageImageFormat"))
	{
		return snprintf(val_buf, val_size, "Raster");
	}
	else if (!strcmp (key, "BitsPerSample"))
	{
		return snprintf(val_buf, val_size, "8");
	}
//	else
//		bug("unable to enum key=%s\n", key);    
	return IJS_ERANGE;
}

/*
 *	Set parameter (in the server) call back. Note, OutputFD is the only call that can be
 *	preceded by set DeviceManufacturer and DeviceModel.
 */

int hpijsfax_set_cb (void *set_cb_data, IjsServerCtx *ctx, IjsJobId job_id,
        			 const char *key, const char *value, int value_size)
{
	HPIJSFax		*pFaxStruct = (HPIJSFax*)set_cb_data;
	int				fd;
	char			*tail;
	int				status = 0;
	char			svalue[IJS_MAX_PARAM+1];   
	float			w;
    float			h;

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
		fd = strtol (svalue, &tail, 10);
		pFaxStruct->iOutputPath = fd;   /* set prn_stream as output of SS::ToDevice */
	}
    else if (!strcmp (key, "DeviceModel"))
    {
        pFaxStruct->SetDeviceName (svalue);
    }
	else if (!strcmp (key, "PaperSize"))
	{
		w = (float) strtod (svalue, &tail);
		h = (float) strtod (tail+1, &tail);
		pFaxStruct->SetPaperSize (w, h);

#if 0
		if (pFaxStruct->IsFirstRaster ())
		{
			/* Normal start of print Job. */
			pFaxStruct->SetPaperSize (w, h);
		}
		else
		{
			/* Middle of print Job, ignore paper size if same. */
			if (!(w == pFaxStruct->GetPaperWidth () &&
			     h == pFaxStruct->GetPaperHeight ()))
			{
//				bug ("w = %f, h = %f, old w = %f, old h = %f\n", w, h,
//				     pFaxStruct->GetPaperWidth (), pFaxStruct->GetPaperHeight ());
				pFaxStruct->SetFirstRaster (1);  /* force new Job */
				pFaxStruct->SetPaperSize (w, h);   /* set new paper size */
//				hpijs_set_context (pFaxStruct);
			}
		}
#endif
	}
	else if (!strcmp (key, "Quality:Quality"))
	{
		pFaxStruct->SetQuality (strtol (svalue, &tail, 10));
	}
	else if (!strcmp (key, "Quality:MediaType"))
	{
		pFaxStruct->SetMediaType (strtol (svalue, &tail, 10));
	}
	else if (!strcmp (key, "Quality:ColorMode"))
	{
		pFaxStruct->SetColorMode (strtol (svalue, &tail, 10));
	}
	else if (!strcmp (key, "FaxEncoding"))
	{
	    pFaxStruct->SetFaxEncoding ((int) strtol (svalue, &tail, 10));
	}
//	else
//		bug("unable to set key = **%s**, value = **%s**\n", key, svalue);    

	return status;
}

/*
 *	Get parameter (from the server) call back. Note, all calls must be preceded by
 *	set DeviceName.
 */

int hpijsfax_get_cb (void *get_cb_data, IjsServerCtx *ctx, IjsJobId job_id,
                  const char *key, char *value_buf, int value_size)
{
	HPIJSFax *pFaxStruct = (HPIJSFax*) get_cb_data;

	if (!strcmp (key, "PrintableArea"))
	{
		return snprintf (value_buf, value_size, "%.4fx%.4f",
		                 pFaxStruct->PrintableWidth (),
						 pFaxStruct->PrintableHeight ());
	}
    else if (!strcmp (key, "DeviceModel"))
    {
        return snprintf (value_buf, value_size, "%s", pFaxStruct->GetDeviceName ());
    }
	else if (!strcmp (key, "PrintableTopLeft"))
	{
		return snprintf (value_buf, value_size, "%.4fx%.4f",
		                 pFaxStruct->PrintableStartX (),
						 pFaxStruct->PrintableStartY ());
	}
	else if (!strcmp (key, "PaperSize"))
	{
		return snprintf (value_buf, value_size, "%.4fx%.4f",
		                 pFaxStruct->PhysicalPageSizeX (),
						 pFaxStruct->PhysicalPageSizeY ());
	}
	else if (!strcmp (key, "Dpi"))
	{
		return snprintf (value_buf, value_size, "%dx%d",
		                 pFaxStruct->EffectiveResolutionX (),
						 pFaxStruct->EffectiveResolutionY ());
	}
	else if (!strcmp (key, "Quality:Quality"))
	{
		return snprintf (value_buf, value_size, "%d", pFaxStruct->GetQuality ());
	}
	else if (!strcmp (key, "Quality:ColorMode"))
	{
		return snprintf (value_buf, value_size, "%d", pFaxStruct->GetColorMode ());
	}
	else if (!strcmp (key, "Quality:MediaType"))
	{
		return snprintf (value_buf, value_size, "%d", pFaxStruct->GetMediaType ());
	}
	else if (!strcmp (key, "FaxEncoding"))
	{
		return snprintf (value_buf, value_size, "%d", pFaxStruct->GetFaxEncoding ());
	}
	else if (!strcmp (key, "ColorSpace"))
	{
	  return snprintf (value_buf, value_size, "%s", pFaxStruct->ph.cs);
	}
	else if (!strcmp (key, "PageImageFormat"))
	{
		return snprintf (value_buf, value_size, "Raster");
	}
	else if (!strcmp (key, "BitsPerSample"))
	{
		return snprintf (value_buf, value_size, "8");
	}
//	else
//		bug ("unable to get key=%s\n", key);    

	return IJS_EUNKPARAM;
}

int hpijsFaxServer (int argc, char **argv)
{
	IjsServerCtx	*ctx = NULL;
	HPIJSFax		*pFaxStruct = NULL;
	int				status = EXIT_FAILURE;
	int				ret;
	int				n;
	int				i;
	int				width;
    int             widthMMR;
	int				iInputBufSize;
	LPBYTE 			pbOutputBuf = NULL;
	LPBYTE 			pThisScanLine = NULL;
	LPBYTE			pInputBuf = NULL;
	IP_XFORM_SPEC	xForm[3];
	IP_IMAGE_TRAITS	traits;
	IP_HANDLE		hJob;
	 FILE *pFilePtrFax;

	char					hpFileName[MAX_FILE_PATH_LEN]; 
	int					fdFax = -1;
	BYTE					szFileHeader[68];
	BYTE					szPageHeader[64];
	BYTE					*p;
	unsigned	int			uiPageNum = 0;
	char user_name[32]={0,};

   if (argc > 2)
        strncpy(user_name, argv[2], sizeof(user_name));

    snprintf(hpFileName,sizeof(hpFileName),"%s/hp_%s_ijsfax_Log_XXXXXX",CUPS_TMP_DIR, user_name);


	pFaxStruct = new HPIJSFax ();

	if (pFaxStruct == NULL)
	{
	    BUG ("unable to allocate HPIJSFax\n");
		exit (0);
	}

	pFaxStruct->SetFirstRaster (TRUE);

	ctx = ijs_server_init ();
	if (ctx == NULL)
	{
        BUG ("unable to init hpijs server\n");
        goto BUGOUT;
    }

	ijs_server_install_status_cb (ctx, hpijsfax_status_cb, pFaxStruct);
	ijs_server_install_list_cb (ctx, hpijsfax_list_cb, pFaxStruct);
	ijs_server_install_enum_cb (ctx, hpijsfax_enum_cb, pFaxStruct);
	ijs_server_install_set_cb (ctx, hpijsfax_set_cb, pFaxStruct);
	ijs_server_install_get_cb (ctx, hpijsfax_get_cb, pFaxStruct);

	while (1)
	{
		if ((ret = ijs_server_get_page_header(ctx, &pFaxStruct->ph)) < 0)
		{
			BUG("unable to read client data err=%d\n", ret);
			goto BUGOUT;
		}

		if (pFaxStruct->IsFirstRaster ())
		{
		    pFaxStruct->SetFirstRaster (0);
			if (fdFax == -1)
			{
//				fdFax = mkstemp (hpFileName);
				fdFax = createTempFile(hpFileName, &pFilePtrFax);
				if (fdFax < 0)
				{
					BUG ("Unable to open Fax output file - %s for writing\n", hpFileName);
					goto BUGOUT;
				}

				memset (szFileHeader, 0, sizeof (szFileHeader));
				memcpy (szFileHeader, "hplip_g3", 8);
				p = szFileHeader + 8;
//				memcpy (szFileHeader, "hplip_g3", 8);
				*p++ = 1;								// Version Number
				HPLIPPUTINT32 (p, 0); p += 4;			// Total number of pages in this job
				HPLIPPUTINT16 (p, (pFaxStruct->EffectiveResolutionX ())); p += 2;
				HPLIPPUTINT16 (p, (pFaxStruct->EffectiveResolutionY ())); p += 2;
				*p++ = pFaxStruct->GetPaperSize ();		// Output paper size
				*p++ = pFaxStruct->GetQuality ();		// Output qulity
				*p++ = pFaxStruct->GetFaxEncoding ();	// MH, MMR or JPEG
				p += 4;									// Reserved 1
				p += 4;									// Reserved 2
				write (fdFax, szFileHeader, (p - szFileHeader));
		    }
		}

		if (ret)
		{
			status = 0; /* normal exit */
			break;
		}

		width = (((pFaxStruct->ph.width + 7) >> 3)) << 3;
        widthMMR = width;

/*
 *      Devices in the HPFax2 category require fixed width of 2528 pixels.
 *      Example: LaserJet 2727 MFP
 */

        if (!strcmp (pFaxStruct->GetDeviceName (), "HPFax2"))
        {
            widthMMR = 2528;
        }

		if ((pThisScanLine = (LPBYTE) malloc (width * 3)) == NULL)
		{
			BUG ("unable to allocate pThisScanLine buffer size = %d: %m\n", width * 3);
			goto BUGOUT;
		}

		memset (pThisScanLine, 0xFF, width * 3);

		iInputBufSize = widthMMR * pFaxStruct->ph.height;
		if (pFaxStruct->GetColorMode () == HPLIPFAX_COLOR)
		{
			iInputBufSize *= 3;
		}

		pInputBuf = (LPBYTE) malloc (iInputBufSize);
		if (pInputBuf == NULL)
		{
			BUG ("Unable to allocate pInputBuf, size = %d\n", iInputBufSize);
			goto BUGOUT;
		}
		memset (pInputBuf, 0xFF, iInputBufSize);

		for (i = 0; i < pFaxStruct->ph.height; i++)      
		{
			if ((n = ijs_server_get_data (ctx, (char *) pThisScanLine, pFaxStruct->ph.width * 3)) < 0)
			{
				BUG ("ijs_server_get_data failed\n");
				break;    /* error */
			}
			if (pFaxStruct->GetColorMode () == HPLIPFAX_MONO)
			{
				RGB2Gray (pThisScanLine, width, pInputBuf + i * widthMMR);
			}
			else
			{
			    memcpy (pInputBuf + (i * widthMMR * 3), pThisScanLine, n);
			}
		}
		WORD		wResult;
		DWORD		dwInputAvail;
		DWORD		dwInputUsed;
		DWORD		dwInputNextPos;
		DWORD		dwOutputAvail;
		DWORD		dwOutputUsed;
		DWORD		dwOutputThisPos;
		pbOutputBuf = (LPBYTE) malloc (iInputBufSize);
		if (pbOutputBuf == NULL)
		{
			BUG ("unable to allocate pbOutputBuf,  buffer size = %d\n", iInputBufSize);
		    goto BUGOUT;
		}
		memset (pbOutputBuf, 0xFF, iInputBufSize);

		memset (xForm, 0, sizeof (xForm));

		if (pFaxStruct->GetColorMode () == HPLIPFAX_MONO)
		{
			xForm[0].eXform = X_GRAY_2_BI;

			// 0   - Error diffusion
			// >0  - Threshold value

			xForm[0].aXformInfo[IP_GRAY_2_BI_THRESHOLD].dword = 127;

			xForm[1].eXform = X_FAX_ENCODE;
			if (pFaxStruct->GetFaxEncoding () == RASTER_MMR)
			{
				xForm[1].aXformInfo[IP_FAX_FORMAT].dword = IP_FAX_MMR;
			}
			else
			{
				xForm[1].aXformInfo[IP_FAX_FORMAT].dword = IP_FAX_MH;
			}
 /*                    0 = EOLs are in data as usual; */
 /*                    1 = no EOLs in data. */
			xForm[1].aXformInfo[IP_FAX_NO_EOLS].dword = 0;
//		    xForm[1].aXformInfo[IP_FAX_MIN_ROW_LEN].dword = ??
			xForm[1].pXform = NULL;
			xForm[1].pfReadPeek = NULL;
			xForm[1].pfWritePeek = NULL;

			traits.iComponentsPerPixel = 1;
			wResult = ipOpen (2, xForm, 0, &hJob);
		}
		else
		{
		    xForm[0].eXform = X_CNV_COLOR_SPACE;
		    xForm[0].aXformInfo[IP_CNV_COLOR_SPACE_WHICH_CNV].dword = IP_CNV_SRGB_TO_YCC;
			xForm[1].eXform = X_CNV_COLOR_SPACE;
		    xForm[1].aXformInfo[IP_CNV_COLOR_SPACE_WHICH_CNV].dword = IP_CNV_YCC_TO_CIELAB;
			xForm[0].eXform = X_JPG_ENCODE;
			xForm[0].aXformInfo[IP_JPG_ENCODE_FOR_COLOR_FAX].dword = 1;
			traits.iComponentsPerPixel = 3;
			wResult = ipOpen (1, xForm, 0, &hJob);
		}

		if (wResult != IP_DONE)
		{
			BUG ("ipOpen failed: wResult = %x\n", wResult);
			goto BUGOUT;
		}
		traits.iBitsPerPixel = 8;
		traits.iPixelsPerRow = widthMMR;
		traits.lHorizDPI = pFaxStruct->EffectiveResolutionX ();
		traits.lVertDPI = pFaxStruct->EffectiveResolutionY ();
		traits.lNumRows = pFaxStruct->ph.height;
		traits.iNumPages = 1;
		traits.iPageNum = 1;

		wResult = ipSetDefaultInputTraits (hJob, &traits);
		if (wResult != IP_DONE)
		{
			BUG ("ipSetDefaultInputTraits failed: wResult = %x\n", wResult);
			wResult = ipClose (hJob);
			goto BUGOUT;
		}
		dwInputAvail = iInputBufSize;
		dwOutputAvail = dwInputAvail;

		wResult = ipConvert (hJob, dwInputAvail, pInputBuf, &dwInputUsed,
							 &dwInputNextPos, dwOutputAvail, pbOutputBuf,
							 &dwOutputUsed, &dwOutputThisPos);

		if (wResult == IP_FATAL_ERROR)
		{
		    BUG ("ipConvert failed, wResult = %d\n", wResult);
            goto BUGOUT;
		}
#if 0
		BUG ("dwInputAvail = %d dwInputUsed = %d dwOutputUsed = %d\n",
		     dwInputAvail, dwInputUsed, dwOutputUsed);
#endif
		wResult = ipClose (hJob);
		hJob = 0;

		uiPageNum++;

		p = szPageHeader;
		HPLIPPUTINT32 (p, uiPageNum); p += 4;				// Current page number
		HPLIPPUTINT32 (p, widthMMR); p += 4;				// Num of pixels per row
		HPLIPPUTINT32 (p, pFaxStruct->ph.height); p += 4;	// Num of rows in this page
		HPLIPPUTINT32 (p, dwOutputUsed); p += 4;			// Size in bytes of encoded data
		HPLIPPUTINT32 (p, 0); p += 4;			            // Thumbnail data size
		HPLIPPUTINT32 (p, 0); p += 4;						// Reserved for future use
		write (fdFax, szPageHeader, (p - szPageHeader));
		write (fdFax, pbOutputBuf, dwOutputUsed);
/*
 *      Write the thumbnail data here
 */

		// Send this to fax handler

		free (pThisScanLine);
		pThisScanLine = NULL;
		free (pbOutputBuf);
		free (pInputBuf);
		pbOutputBuf = NULL;
		pInputBuf = NULL;

	} /* end while (1) */

	lseek (fdFax, 9, SEEK_SET);
	HPLIPPUTINT32 ((szFileHeader + 9), uiPageNum);
	write (fdFax, szFileHeader + 9, 4);
/*
    chmod (hpFileName, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    hpFileName[strlen (hpFileName)] = '\n';
    write (pFaxStruct->iOutputPath, hpFileName, strlen (hpFileName));
 */

    BYTE    *pTmp;
    int     iSize;
    pTmp = NULL;

    iSize = lseek (fdFax, 0, SEEK_END);
    lseek (fdFax, 0, SEEK_SET);

    if (iSize > 0)
        pTmp = new BYTE[iSize];
    if (pTmp == NULL)
    {
        iSize = 1024;
        pTmp = new BYTE[iSize];
        if (pTmp == NULL)
        {
            goto BUGOUT;
        }
    }
    while ((i = read (fdFax, pTmp, iSize)) > 0)
    {
        write (pFaxStruct->iOutputPath, pTmp, i);
    }
    delete [] pTmp;

BUGOUT:
	if (fdFax > 0)
	{
		close (fdFax);
	}
    //unlink (hpFileName);
	if (pFaxStruct != NULL)
	{
		#ifdef CAPTURE
		pFaxStruct->EndScript ();
		#endif
	}
    if (pThisScanLine != NULL)
	{
	    free (pThisScanLine);
	}
	if (pbOutputBuf)
	{
		free (pbOutputBuf);
	}

	if (pInputBuf)
	{
		free (pInputBuf);
	}

	if (pFaxStruct)
	{
	    delete pFaxStruct;
		pFaxStruct = NULL;
	}

	if (ctx != NULL)
	{
		ijs_server_done (ctx);
	}

	exit(status);
}

// GrayLevel = (5/16)R + (9/16)G + (2/16)B
#define RGB2BW(r, g, b) (BYTE) (((r << 2) + r + (g << 3) + g + (b << 1)) >> 4)

void RGB2Gray (BYTE *pRGBData, int iNumPixels, BYTE *pGData)
{
	int		i;
	BYTE	*pIn = pRGBData;
	BYTE	*pOut = pGData;
	for (i = 0; i < iNumPixels; i++, pIn += 3)
	{
	    *pOut++ = RGB2BW ((unsigned short) *pIn, (unsigned short) pIn[1], (unsigned short) pIn[2]);
	}
}

#endif // HAVE_LIBHPIP
