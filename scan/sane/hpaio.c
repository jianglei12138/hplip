/************************************************************************************\

  hpaio.c - HP SANE backend for multi-function peripherals (libsane-hpaio)

  (c) 2001-2008 Copyright HP Development Company, LP

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
  of the Software, and to permit persons to whom the Software is furnished to do
  so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
  FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
  COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
  IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

  Contributing Authors: David Paschal, Don Welch, David Suffield, Narla Naga Samrat Chowdary,
                        Yashwant Sahu, Sarbeswar Meher

\************************************************************************************/


#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cups/cups.h>
#include "hpmud.h"
#include "hpip.h"
#include "hp_ipp.h"
#include "soap.h"
#include "soapht.h"
#include "marvell.h"
#include "hpaio.h"
#include "ledm.h"
#include "sclpml.h"
#include "escl.h"
#include "io.h"

#define DEBUG_DECLARE_ONLY
#include "sanei_debug.h"

static SANE_Device **DeviceList = NULL;

static int AddDeviceList(char *uri, char *model, SANE_Device ***pd)
{
   int i = 0, uri_length = 0;

   if (*pd == NULL)
   {
      /* Allocate array of pointers. */
      *pd = malloc(sizeof(SANE_Device *) * MAX_DEVICE);
      memset(*pd, 0, sizeof(SANE_Device *) * MAX_DEVICE);
   }

   uri += 3; /* Skip "hp:" */
   uri_length = strlen(uri);
   if(strstr(uri, "&queue=false"))
       uri_length -= 12; // Last 12 bytes of URI i.e "&queue=false"

   /* Find empty slot in array of pointers. */
   for (i=0; i < MAX_DEVICE; i++)
   {
      if ((*pd)[i] == NULL)
      {
         /* Allocate Sane_Device and members. */
         (*pd)[i] = malloc(sizeof(SANE_Device));
         (*pd)[i]->name = malloc(strlen(uri)+1);
         strcpy((char *)(*pd)[i]->name, uri);
         (*pd)[i]->model = strdup(model);
         (*pd)[i]->vendor = "Hewlett-Packard";
         (*pd)[i]->type = "all-in-one";
         break;
      }

      //Check for Duplicate URI. If URI is added already then don't add it again.
      if( strncasecmp((*pd)[i]->name, uri, uri_length) == 0 )
          break;
   }

   return 0;
}

static int ResetDeviceList(SANE_Device ***pd)
{
   int i;

   if (*pd)
   {
      for (i=0; (*pd)[i] && i<MAX_DEVICE; i++)
      {
         if ((*pd)[i]->name)
            free((void *)(*pd)[i]->name);
         if ((*pd)[i]->model)
            free((void *)(*pd)[i]->model);
         free((*pd)[i]);
      }
      free(*pd);
      *pd = NULL;
   }

   return 0;
}

/* Parse URI record from buf. Assumes one record per line. All returned strings are zero terminated. */
static int GetUriLine(char *buf, char *uri, char **tail)
{
   int i=0, j;
   int maxBuf = HPMUD_LINE_SIZE*64;

   uri[0] = 0;

   if (strncasecmp(&buf[i], "direct ", 7) == 0)
   {
      i = 7;
      j = 0;
      for (; buf[i] == ' ' && i < maxBuf; i++);  /* eat white space before string */
      while ((buf[i] != ' ') && (i < maxBuf) && (j < HPMUD_LINE_SIZE))
         uri[j++] = buf[i++];
      uri[j] = 0;

      for (; buf[i] != '\n' && i < maxBuf; i++);  /* eat rest of line */
   }
   else
   {
      for (; buf[i] != '\n' && i < maxBuf; i++);  /* eat line */
   }

   i++;   /* bump past '\n' */

   if (tail != NULL)
      *tail = buf + i;  /* tail points to next line */

   return i;
}

static int AddCupsList(char *uri, char ***printer)
{
   int i, stat=1;

   /* Look for hp network URIs only. */
   if (strncasecmp(uri, "hp:/net/", 8) !=0)
      goto bugout;

   if (*printer == NULL)
   {
      /* Allocate array of string pointers. */
      *printer = malloc(sizeof(char *) * MAX_DEVICE);
      memset(*printer, 0, sizeof(char *) * MAX_DEVICE);
   }

   /* Ignor duplicates (ie: printer queues using the same device). */
   for (i=0; (*printer)[i] != NULL && i<MAX_DEVICE; i++)
   {
      if (strcmp((*printer)[i], uri) == 0)
         goto bugout;
   }

   /* Find empty slot in array of pointers. */
   for (i=0; i<MAX_DEVICE; i++)
   {
      if ((*printer)[i] == NULL)
      {
         (*printer)[i] = strdup(uri);
         break;
      }
   }

   stat = 0;

bugout:

   return stat;
}


static int GetCupsPrinters(char ***printer)
{
   http_t *http=NULL;     /* HTTP object */
   ipp_t *request=NULL;  /* IPP request object */
   ipp_t *response=NULL; /* IPP response object */
   ipp_attribute_t *attr;     /* Current IPP attribute */
   int cnt=0;

   /* Connect to the HTTP server */
   if ((http = httpConnectEncrypt(cupsServer(), ippPort(), cupsEncryption())) == NULL)
      goto bugout;

   /* Assemble the IPP request */
   request = ippNew();

   ippSetOperation( request, CUPS_GET_PRINTERS );
   ippSetRequestId( request, 1 );

   ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET, "attributes-charset", NULL, "utf-8");
   ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE, "attributes-natural-language", NULL, "en");
   ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "requested-attributes", NULL, "device-uri");

   /* Send the request and get a response. */
   if ((response = cupsDoRequest(http, request, "/")) == NULL)
      goto bugout;

   for (attr = ippFirstAttribute ( response ); attr != NULL; attr = ippNextAttribute( response ))
   {
      /* Skip leading attributes until we hit a printer. */
      while (attr != NULL && ippGetGroupTag( attr ) != IPP_TAG_PRINTER)
         attr = ippNextAttribute( response );

      if (attr == NULL)
         break;

      while (attr != NULL && ippGetGroupTag( attr ) == IPP_TAG_PRINTER)
      {
         if (strcmp(ippGetName( attr ), "device-uri") == 0 && ippGetValueTag( attr ) == IPP_TAG_URI && AddCupsList(ippGetString( attr, 0, NULL ), printer) == 0)
            cnt++;
         attr = ippNextAttribute( response );
      }

      if (attr == NULL)
         break;
   }

   ippDelete(response);

 bugout:
   return cnt;
}

static int AddDevice(char *uri)
{
    struct hpmud_model_attributes ma;
    char model[HPMUD_LINE_SIZE];
    int scan_type;
    int device_added = 0;

    hpmud_query_model(uri, &ma);
    if (ma.scantype > 0)
    {
       hpmud_get_uri_model(uri, model, sizeof(model));
       AddDeviceList(uri, model, &DeviceList);
       device_added = 1;
    }
    else
    {
       DBG(6,"unsupported scantype=%d %s\n", ma.scantype, uri);
    }

    return device_added;
}

static int DevDiscovery(int localOnly)
{
    char message[HPMUD_LINE_SIZE*64];
    char uri[HPMUD_LINE_SIZE];
    char *tail = message;
    int i, scan_type, cnt=0, total=0, bytes_read;
    char **cups_printer=NULL;     /* list of printers */
    char* token = NULL;
    enum HPMUD_RESULT stat;

    stat = hpmud_probe_devices(HPMUD_BUS_ALL, message, sizeof(message), &cnt, &bytes_read);
    if (stat != HPMUD_R_OK)
      goto bugout;

    /* Look for local all-in-one scan devices (defined by hpmud). */
    for (i=0; i<cnt; i++)
    {
        GetUriLine(tail, uri, &tail);
        total += AddDevice(uri);
    }

    /* Look for Network Scan devices if localonly flag if FALSE. */
    if (!localOnly)
    {
        /* Look for all-in-one scan devices for which print queue created */
        cnt = GetCupsPrinters(&cups_printer);
        for (i=0; i<cnt; i++)
        {
            total += AddDevice(cups_printer[i]);
            free(cups_printer[i]);
        }
        if (cups_printer)
            free(cups_printer);
#ifdef HAVE_LIBNETSNMP
        /* Discover NW scanners using Bonjour*/
        bytes_read = mdns_probe_nw_scanners(message, sizeof(message), &cnt);
        token = strtok(message, ";");
        while (token)
        {
            total += AddDevice(token);
            token = strtok(NULL, ";");
        }
#endif
        if(!total)
        {          
          SendScanEvent("hpaio:/net/HP_Scan_Devices?ip=1.1.1.1", EVENT_ERROR_NO_PROBED_DEVICES_FOUND);
        }
    }

bugout:
   return total;
}

/******************************************************* SANE API *******************************************************/

extern SANE_Status sane_hpaio_init(SANE_Int * pVersionCode, SANE_Auth_Callback authorize)
{
    int stat;

    DBG_INIT();
    InitDbus();

    DBG(8, "sane_hpaio_init(): %s %d\n", __FILE__, __LINE__);

    if( pVersionCode )
    {
       *pVersionCode = SANE_VERSION_CODE( 1, 0, 0 );
    }
    stat = SANE_STATUS_GOOD;

    return stat;
}  /* sane_hpaio_init() */

extern void sane_hpaio_exit(void)
{
   DBG(8, "sane_hpaio_exit(): %s %d\n", __FILE__, __LINE__);
   ResetDeviceList(&DeviceList);
}

extern SANE_Status sane_hpaio_get_devices(const SANE_Device ***deviceList, SANE_Bool localOnly)
{
   DBG(8, "sane_hpaio_get_devices(local=%d): %s %d\n", localOnly, __FILE__, __LINE__);
   ResetDeviceList(&DeviceList);
   DevDiscovery(localOnly);
   *deviceList = (const SANE_Device **)DeviceList;
   return SANE_STATUS_GOOD;
}

extern SANE_Status sane_hpaio_open(SANE_String_Const devicename, SANE_Handle * pHandle)
{
    struct hpmud_model_attributes ma;
    char devname[256];

    /* Get device attributes and determine what backend to call. */
    snprintf(devname, sizeof(devname)-1, "hp:%s", devicename);   /* prepend "hp:" */
    hpmud_query_model(devname, &ma);
   DBG(8, "sane_hpaio_open(%s): %s %d scan_type=%d scansrc=%d\n", devicename, __FILE__, __LINE__, ma.scantype, ma.scansrc);

    if ((ma.scantype == HPMUD_SCANTYPE_MARVELL) || (ma.scantype == HPMUD_SCANTYPE_MARVELL2))
       return marvell_open(devicename, pHandle);
    if (ma.scantype == HPMUD_SCANTYPE_SOAP)
       return soap_open(devicename, pHandle);
    if (ma.scantype == HPMUD_SCANTYPE_SOAPHT)
       return soapht_open(devicename, pHandle);
    if (ma.scantype == HPMUD_SCANTYPE_LEDM)
       return ledm_open(devicename, pHandle);
    if ((ma.scantype == HPMUD_SCANTYPE_SCL) || (ma.scantype == HPMUD_SCANTYPE_SCL_DUPLEX) ||(ma.scantype == HPMUD_SCANTYPE_PML))
       return sclpml_open(devicename, pHandle);
    if (ma.scantype == HPMUD_SCANTYPE_ESCL)
       return escl_open(devicename, pHandle);
    else
       return SANE_STATUS_UNSUPPORTED;
}   /* sane_hpaio_open() */

extern void sane_hpaio_close(SANE_Handle handle)
{
    if (strcmp(*((char **)handle), "MARVELL") == 0)
       return marvell_close(handle);
    if (strcmp(*((char **)handle), "SOAP") == 0)
       return soap_close(handle);
    if (strcmp(*((char **)handle), "SOAPHT") == 0)
       return soapht_close(handle);
    if (strcmp(*((char **)handle), "LEDM") == 0)
       return ledm_close(handle);
    if (strcmp(*((char **)handle), "SCL-PML") == 0)
       return sclpml_close(handle);
    if (strcmp(*((char **)handle), "ESCL") == 0)
       return escl_close(handle);
}  /* sane_hpaio_close() */

extern const SANE_Option_Descriptor * sane_hpaio_get_option_descriptor(SANE_Handle handle, SANE_Int option)
{
    if (strcmp(*((char **)handle), "MARVELL") == 0)
       return marvell_get_option_descriptor(handle, option);
    if (strcmp(*((char **)handle), "SOAP") == 0)
       return soap_get_option_descriptor(handle, option);
    if (strcmp(*((char **)handle), "SOAPHT") == 0)
       return soapht_get_option_descriptor(handle, option);
    if (strcmp(*((char **)handle), "LEDM") == 0)
       return ledm_get_option_descriptor(handle, option);
    if (strcmp(*((char **)handle), "SCL-PML") == 0)
       return sclpml_get_option_descriptor(handle, option);
    if (strcmp(*((char **)handle), "ESCL") == 0)
       return escl_get_option_descriptor(handle, option);
    else
       return NULL;
}  /* sane_hpaio_get_option_descriptor() */

extern SANE_Status sane_hpaio_control_option(SANE_Handle handle, SANE_Int option, SANE_Action action, void * pValue, SANE_Int * pInfo )
{
    if (strcmp(*((char **)handle), "MARVELL") == 0)
       return marvell_control_option(handle, option, action, pValue, pInfo);
    if (strcmp(*((char **)handle), "SOAP") == 0)
       return soap_control_option(handle, option, action, pValue, pInfo);
    if (strcmp(*((char **)handle), "SOAPHT") == 0)
       return soapht_control_option(handle, option, action, pValue, pInfo);
    if (strcmp(*((char **)handle), "LEDM") == 0)
       return ledm_control_option(handle, option, action, pValue, pInfo);
    if (strcmp(*((char **)handle), "SCL-PML") == 0)
       return sclpml_control_option(handle, option, action, pValue, pInfo);
    if (strcmp(*((char **)handle), "ESCL") == 0)
       return escl_control_option(handle, option, action, pValue, pInfo);
    else
       return SANE_STATUS_UNSUPPORTED;
}   /* sane_hpaio_control_option() */

extern SANE_Status sane_hpaio_get_parameters(SANE_Handle handle, SANE_Parameters *pParams)
{
    if (strcmp(*((char **)handle), "MARVELL") == 0)
       return marvell_get_parameters(handle, pParams);
    if (strcmp(*((char **)handle), "SOAP") == 0)
       return soap_get_parameters(handle, pParams);
    if (strcmp(*((char **)handle), "SOAPHT") == 0)
       return soapht_get_parameters(handle, pParams);
    if (strcmp(*((char **)handle), "LEDM") == 0)
       return ledm_get_parameters(handle, pParams);
    if (strcmp(*((char **)handle), "SCL-PML") == 0)
       return sclpml_get_parameters(handle, pParams);
    if (strcmp(*((char **)handle), "ESCL") == 0)
       return escl_get_parameters(handle, pParams);
    else
       return SANE_STATUS_UNSUPPORTED;
}  /* sane_hpaio_get_parameters() */

extern SANE_Status sane_hpaio_start(SANE_Handle handle)
{
    if (strcmp(*((char **)handle), "MARVELL") == 0)
       return marvell_start(handle);
    if (strcmp(*((char **)handle), "SOAP") == 0)
       return soap_start(handle);
    if (strcmp(*((char **)handle), "SOAPHT") == 0)
       return soapht_start(handle);
    if (strcmp(*((char **)handle), "LEDM") == 0)
       return ledm_start(handle);
    if (strcmp(*((char **)handle), "SCL-PML") == 0)
       return sclpml_start(handle);
    if (strcmp(*((char **)handle), "ESCL") == 0)
       return escl_start(handle);
    else
       return SANE_STATUS_UNSUPPORTED;
}   /* sane_hpaio_start() */


extern SANE_Status sane_hpaio_read(SANE_Handle handle, SANE_Byte *data, SANE_Int maxLength, SANE_Int *pLength)
{
    if (strcmp(*((char **)handle), "LEDM") == 0)
       return ledm_read(handle, data, maxLength, pLength);
    if (strcmp(*((char **)handle), "MARVELL") == 0)
       return marvell_read(handle, data, maxLength, pLength);
    if (strcmp(*((char **)handle), "SOAP") == 0)
       return soap_read(handle, data, maxLength, pLength);
    if (strcmp(*((char **)handle), "SOAPHT") == 0)
       return soapht_read(handle, data, maxLength, pLength);
    if (strcmp(*((char **)handle), "SCL-PML") == 0)
       return sclpml_read(handle, data, maxLength, pLength);
    if (strcmp(*((char **)handle), "ESCL") == 0)
       return escl_read(handle, data, maxLength, pLength);
    else
       return SANE_STATUS_UNSUPPORTED;

} /* sane_hpaio_read() */

/* Note, sane_cancel is called normally not just during IO abort situations. */
extern void sane_hpaio_cancel( SANE_Handle handle )
{
    if (strcmp(*((char **)handle), "MARVELL") == 0)
       return marvell_cancel(handle);
    if (strcmp(*((char **)handle), "SOAP") == 0)
       return soap_cancel(handle);
    if (strcmp(*((char **)handle), "SOAPHT") == 0)
       return soapht_cancel(handle);
    if (strcmp(*((char **)handle), "LEDM") == 0)
       return ledm_cancel(handle);
    if (strcmp(*((char **)handle), "SCL-PML") == 0)
       return sclpml_cancel(handle);
    if (strcmp(*((char **)handle), "ESCL") == 0)
       return escl_cancel(handle);
}  /* sane_hpaio_cancel() */

extern SANE_Status sane_hpaio_set_io_mode(SANE_Handle handle, SANE_Bool nonBlocking)
{
    return SANE_STATUS_UNSUPPORTED;
}

extern SANE_Status sane_hpaio_get_select_fd(SANE_Handle handle, SANE_Int *pFd)
{
    return SANE_STATUS_UNSUPPORTED;
}


