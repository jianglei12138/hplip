/************************************************************************************\

  soap.c - HP SANE backend support for soap based multi-function peripherals

  (c) 2006,2008 Copyright HP Development Company, LP

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

  Note the CLJ CM1017 windows driver supports jpeg, but no hpraw (over the wire).
  The problem is hpraw does not return bytes_per_line and number_of_lines at the
  start of the scan job. Even though we perform the same calculation as firmware
  for hpraw, due to round-off error between different math libraries determining 
  the correct bytes_per_line is not always possible (especially at 600dpi and 1200dpi). 

  Also the CM1017 linart mode only works with hpraw (over the wire).

  Given the hpraw problem stated above this driver will only support jpeg for all scan
  modes. Linart will use 8-bit gray then convert to mono. Same as windows.

  Author: David Suffield

\************************************************************************************/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdarg.h>
#include <syslog.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <dlfcn.h>
#include "sane.h"
#include "saneopts.h"
#include "hpmud.h"
#include "hpip.h"
#include "common.h"
#include "soap.h"
#include "soapi.h"
#include "io.h"
#include "utils.h"

#define DEBUG_DECLARE_ONLY
#include "sanei_debug.h"

static struct soap_session *session = NULL;   /* assume one sane_open per process */

static int bb_load(struct soap_session *ps, const char *so)
{
   int stat=1;

   /* Load hpmud manually with symbols exported. Otherwise the plugin will not find it. */ 
   if ((ps->hpmud_handle = load_library("libhpmud.so")) == NULL)
   {
	   if ((ps->hpmud_handle = load_library("libhpmud.so.0")) == NULL)
           goto bugout;
   }

   /* Load math library manually with symbols exported (Ubuntu 8.04). Otherwise the plugin will not find it. */ 
   if ((ps->math_handle = load_library("libm.so")) == NULL)
   {
      if ((ps->math_handle = load_library("libm.so.6")) == NULL)
         goto bugout;
   } 

   if ((ps->bb_handle = load_plugin_library(UTILS_SCAN_PLUGIN_LIBRARY, so)) == NULL)
   {
      SendScanEvent(ps->uri, EVENT_PLUGIN_FAIL);
      goto bugout;
   } 
   
   if ((ps->bb_open = get_library_symbol(ps->bb_handle, "bb_open")) == NULL)
      goto bugout;

   if ((ps->bb_close = get_library_symbol(ps->bb_handle, "bb_close")) == NULL)
      goto bugout;

   if ((ps->bb_get_parameters = get_library_symbol(ps->bb_handle, "bb_get_parameters")) == NULL)
      goto bugout;

   if ((ps->bb_is_paper_in_adf = get_library_symbol(ps->bb_handle, "bb_is_paper_in_adf")) == NULL)
      goto bugout;

   if ((ps->bb_start_scan = get_library_symbol(ps->bb_handle, "bb_start_scan")) == NULL)
      goto bugout;

    if ((ps->bb_end_scan = get_library_symbol(ps->bb_handle, "bb_end_scan")) == NULL)
      goto bugout;

   if ((ps->bb_get_image_data = get_library_symbol(ps->bb_handle, "bb_get_image_data")) == NULL)
      goto bugout;

   if ((ps->bb_end_page = get_library_symbol(ps->bb_handle, "bb_end_page")) == NULL)
      goto bugout;


   stat=0;

bugout:
   return stat;
} /* bb_load */

static int bb_unload(struct soap_session *ps)
{
    unload_library(ps->bb_handle);
    ps->bb_handle = NULL;

    unload_library(ps->hpmud_handle);
    ps->hpmud_handle = NULL;

    unload_library(ps->math_handle);
    ps->math_handle = NULL;

   return 0;
} /* bb_unload */

/* Get raw data (ie: uncompressed data) from image processor. */
static int get_ip_data(struct soap_session *ps, SANE_Byte *data, SANE_Int maxLength, SANE_Int *length)
{
   int ip_ret=IP_INPUT_ERROR;
   unsigned int outputAvail=maxLength, outputUsed=0, outputThisPos;
   unsigned char *input, *output = data;
   unsigned int inputAvail, inputUsed=0, inputNextPos;

   if (!ps->ip_handle)
   {
      BUG("invalid ipconvert state\n");
      goto bugout;
   }      

   if (ps->bb_get_image_data(ps, outputAvail))
      goto bugout;

   if (ps->cnt > 0)
   {
      inputAvail = ps->cnt;
      input = &ps->buf[ps->index];
   }
   else
   {
      input = NULL;   /* no more scan data, flush ipconvert pipeline */
      inputAvail = 0;
   }

   /* Transform input data to output. Note, output buffer may consume more bytes than input buffer (ie: jpeg to raster). */
   ip_ret = ipConvert(ps->ip_handle, inputAvail, input, &inputUsed, &inputNextPos, outputAvail, output, &outputUsed, &outputThisPos);

   DBG6("cnt=%d index=%d input=%p inputAvail=%d inputUsed=%d inputNextPos=%d output=%p outputAvail=%d outputThisPos=%d\n", ps->cnt, ps->index, input, 
         inputAvail, inputUsed, inputNextPos, output, outputAvail, outputThisPos);

   if (input != NULL)
   {
      if (inputAvail == inputUsed)
      {
         ps->index = ps->cnt = 0;   /* reset buffer */
      }
      else
      {
         ps->cnt -= inputUsed;    /* save left over buffer for next soap_read */
         ps->index += inputUsed;
      }
   }

   if (data)
      *length = outputUsed;

   /* For sane do not send output data simultaneously with IP_DONE. */
   if (ip_ret & IP_DONE && outputUsed)
      ip_ret &= ~IP_DONE;                               

bugout:
   return ip_ret;
} /* get_ip_data */

static int set_scan_mode_side_effects(struct soap_session *ps, enum COLOR_ENTRY scanMode)
{
   int j=0;

   memset(ps->compressionList, 0, sizeof(ps->compressionList));
   memset(ps->compressionMap, 0, sizeof(ps->compressionMap));

   switch (scanMode)
   {
      case CE_BLACK_AND_WHITE1:
      case CE_GRAY8:
      case CE_RGB24:
      default:
         ps->compressionList[j] = STR_COMPRESSION_JPEG; 
         ps->compressionMap[j++] = SF_JFIF;
         ps->currentCompression = SF_JFIF;
         ps->option[SOAP_OPTION_JPEG_QUALITY].cap |= SANE_CAP_SOFT_SELECT;   /* enable jpeg quality */        
         break;
   }

   return 0;
} /* set_scan_mode_side_effects */

static struct soap_session *create_session()
{
   struct soap_session *ps;

   if ((ps = malloc(sizeof(struct soap_session))) == NULL)
   {
      BUG("malloc failed: %m\n");
      return NULL;
   }
   memset(ps, 0, sizeof(struct soap_session));
   ps->tag = "SOAP";
   ps->dd = -1;
   ps->cd = -1;

   return ps;
} /* create_session */

static int init_options(struct soap_session *ps)
{
   ps->option[SOAP_OPTION_COUNT].name = "option-cnt";
   ps->option[SOAP_OPTION_COUNT].title = SANE_TITLE_NUM_OPTIONS;
   ps->option[SOAP_OPTION_COUNT].desc = SANE_DESC_NUM_OPTIONS;
   ps->option[SOAP_OPTION_COUNT].type = SANE_TYPE_INT;
   ps->option[SOAP_OPTION_COUNT].unit = SANE_UNIT_NONE;
   ps->option[SOAP_OPTION_COUNT].size = sizeof(SANE_Int);
   ps->option[SOAP_OPTION_COUNT].cap = SANE_CAP_SOFT_DETECT;
   ps->option[SOAP_OPTION_COUNT].constraint_type = SANE_CONSTRAINT_NONE;

   ps->option[SOAP_OPTION_GROUP_SCAN_MODE].name = "mode-group";
   ps->option[SOAP_OPTION_GROUP_SCAN_MODE].title = SANE_TITLE_SCAN_MODE;
   ps->option[SOAP_OPTION_GROUP_SCAN_MODE].type = SANE_TYPE_GROUP;

   ps->option[SOAP_OPTION_SCAN_MODE].name = SANE_NAME_SCAN_MODE;
   ps->option[SOAP_OPTION_SCAN_MODE].title = SANE_TITLE_SCAN_MODE;
   ps->option[SOAP_OPTION_SCAN_MODE].desc = SANE_DESC_SCAN_MODE;
   ps->option[SOAP_OPTION_SCAN_MODE].type = SANE_TYPE_STRING;
   ps->option[SOAP_OPTION_SCAN_MODE].unit = SANE_UNIT_NONE;
   ps->option[SOAP_OPTION_SCAN_MODE].size = MAX_STRING_SIZE;
   ps->option[SOAP_OPTION_SCAN_MODE].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
   ps->option[SOAP_OPTION_SCAN_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
   ps->option[SOAP_OPTION_SCAN_MODE].constraint.string_list = ps->scanModeList;

   ps->option[SOAP_OPTION_SCAN_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
   ps->option[SOAP_OPTION_SCAN_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
   ps->option[SOAP_OPTION_SCAN_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
   ps->option[SOAP_OPTION_SCAN_RESOLUTION].type = SANE_TYPE_INT;
   ps->option[SOAP_OPTION_SCAN_RESOLUTION].unit = SANE_UNIT_DPI;
   ps->option[SOAP_OPTION_SCAN_RESOLUTION].size = sizeof(SANE_Int);
   ps->option[SOAP_OPTION_SCAN_RESOLUTION].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
   ps->option[SOAP_OPTION_SCAN_RESOLUTION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
   ps->option[SOAP_OPTION_SCAN_RESOLUTION].constraint.word_list = ps->resolutionList;

   ps->option[SOAP_OPTION_GROUP_ADVANCED].name = "advanced-group";
   ps->option[SOAP_OPTION_GROUP_ADVANCED].title = STR_TITLE_ADVANCED;
   ps->option[SOAP_OPTION_GROUP_ADVANCED].type = SANE_TYPE_GROUP;
   ps->option[SOAP_OPTION_GROUP_ADVANCED].cap = SANE_CAP_ADVANCED;

   ps->option[SOAP_OPTION_BRIGHTNESS].name = SANE_NAME_BRIGHTNESS;
   ps->option[SOAP_OPTION_BRIGHTNESS].title = SANE_TITLE_BRIGHTNESS;
   ps->option[SOAP_OPTION_BRIGHTNESS].desc = SANE_DESC_BRIGHTNESS;
   ps->option[SOAP_OPTION_BRIGHTNESS].type = SANE_TYPE_INT;
   ps->option[SOAP_OPTION_BRIGHTNESS].unit = SANE_UNIT_NONE;
   ps->option[SOAP_OPTION_BRIGHTNESS].size = sizeof(SANE_Int);
   ps->option[SOAP_OPTION_BRIGHTNESS].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
   ps->option[SOAP_OPTION_BRIGHTNESS].constraint_type = SANE_CONSTRAINT_RANGE;
   ps->option[SOAP_OPTION_BRIGHTNESS].constraint.range = &ps->brightnessRange;
   ps->brightnessRange.min = SOAP_BRIGHTNESS_MIN;
   ps->brightnessRange.max = SOAP_BRIGHTNESS_MAX;
   ps->brightnessRange.quant = 0;

   ps->option[SOAP_OPTION_CONTRAST].name = SANE_NAME_CONTRAST;
   ps->option[SOAP_OPTION_CONTRAST].title = SANE_TITLE_CONTRAST;
   ps->option[SOAP_OPTION_CONTRAST].desc = SANE_DESC_CONTRAST;
   ps->option[SOAP_OPTION_CONTRAST].type = SANE_TYPE_INT;
   ps->option[SOAP_OPTION_CONTRAST].unit = SANE_UNIT_NONE;
   ps->option[SOAP_OPTION_CONTRAST].size = sizeof(SANE_Int);
   ps->option[SOAP_OPTION_CONTRAST].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
   ps->option[SOAP_OPTION_CONTRAST].constraint_type = SANE_CONSTRAINT_RANGE;
   ps->option[SOAP_OPTION_CONTRAST].constraint.range = &ps->contrastRange;
   ps->contrastRange.min = SOAP_CONTRAST_MIN;
   ps->contrastRange.max = SOAP_CONTRAST_MAX;
   ps->contrastRange.quant = 0;

   ps->option[SOAP_OPTION_COMPRESSION].name = STR_NAME_COMPRESSION;
   ps->option[SOAP_OPTION_COMPRESSION].title = STR_TITLE_COMPRESSION;
   ps->option[SOAP_OPTION_COMPRESSION].desc = STR_DESC_COMPRESSION;
   ps->option[SOAP_OPTION_COMPRESSION].type = SANE_TYPE_STRING;
   ps->option[SOAP_OPTION_COMPRESSION].unit = SANE_UNIT_NONE;
   ps->option[SOAP_OPTION_COMPRESSION].size = MAX_STRING_SIZE;
   ps->option[SOAP_OPTION_COMPRESSION].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
   ps->option[SOAP_OPTION_COMPRESSION].constraint_type = SANE_CONSTRAINT_STRING_LIST;
   ps->option[SOAP_OPTION_COMPRESSION].constraint.string_list = ps->compressionList;

   ps->option[SOAP_OPTION_JPEG_QUALITY].name = STR_NAME_JPEG_QUALITY;
   ps->option[SOAP_OPTION_JPEG_QUALITY].title = STR_TITLE_JPEG_QUALITY;
   ps->option[SOAP_OPTION_JPEG_QUALITY].desc = STR_DESC_JPEG_QUALITY;
   ps->option[SOAP_OPTION_JPEG_QUALITY].type = SANE_TYPE_INT;
   ps->option[SOAP_OPTION_JPEG_QUALITY].unit = SANE_UNIT_NONE;
   ps->option[SOAP_OPTION_JPEG_QUALITY].size = sizeof(SANE_Int);
   ps->option[SOAP_OPTION_JPEG_QUALITY].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
   ps->option[SOAP_OPTION_JPEG_QUALITY].constraint_type = SANE_CONSTRAINT_RANGE;
   ps->option[SOAP_OPTION_JPEG_QUALITY].constraint.range = &ps->jpegQualityRange;
   ps->jpegQualityRange.min = MIN_JPEG_COMPRESSION_FACTOR;
   ps->jpegQualityRange.max = MAX_JPEG_COMPRESSION_FACTOR;
   ps->jpegQualityRange.quant = 0;

   ps->option[SOAP_OPTION_GROUP_GEOMETRY].name = "geometry-group";
   ps->option[SOAP_OPTION_GROUP_GEOMETRY].title = STR_TITLE_GEOMETRY;
   ps->option[SOAP_OPTION_GROUP_GEOMETRY].type = SANE_TYPE_GROUP;
   ps->option[SOAP_OPTION_GROUP_GEOMETRY].cap = SANE_CAP_ADVANCED;

   ps->option[SOAP_OPTION_TL_X].name = SANE_NAME_SCAN_TL_X;
   ps->option[SOAP_OPTION_TL_X].title = SANE_TITLE_SCAN_TL_X;
   ps->option[SOAP_OPTION_TL_X].desc = SANE_DESC_SCAN_TL_X;
   ps->option[SOAP_OPTION_TL_X].type = SANE_TYPE_FIXED;
   ps->option[SOAP_OPTION_TL_X].unit = SANE_UNIT_MM;
   ps->option[SOAP_OPTION_TL_X].size = sizeof(SANE_Int);
   ps->option[SOAP_OPTION_TL_X].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
   ps->option[SOAP_OPTION_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
   ps->option[SOAP_OPTION_TL_X].constraint.range = &ps->tlxRange;
   ps->tlxRange.min = 0;
   ps->tlxRange.quant = 0;

   ps->option[SOAP_OPTION_TL_Y].name = SANE_NAME_SCAN_TL_Y;
   ps->option[SOAP_OPTION_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
   ps->option[SOAP_OPTION_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
   ps->option[SOAP_OPTION_TL_Y].type = SANE_TYPE_FIXED;
   ps->option[SOAP_OPTION_TL_Y].unit = SANE_UNIT_MM;
   ps->option[SOAP_OPTION_TL_Y].size = sizeof(SANE_Int);
   ps->option[SOAP_OPTION_TL_Y].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
   ps->option[SOAP_OPTION_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
   ps->option[SOAP_OPTION_TL_Y].constraint.range = &ps->tlyRange;
   ps->tlyRange.min = 0;
   ps->tlyRange.quant = 0;

   ps->option[SOAP_OPTION_BR_X].name = SANE_NAME_SCAN_BR_X;
   ps->option[SOAP_OPTION_BR_X].title = SANE_TITLE_SCAN_BR_X;
   ps->option[SOAP_OPTION_BR_X].desc = SANE_DESC_SCAN_BR_X;
   ps->option[SOAP_OPTION_BR_X].type = SANE_TYPE_FIXED;
   ps->option[SOAP_OPTION_BR_X].unit = SANE_UNIT_MM;
   ps->option[SOAP_OPTION_BR_X].size = sizeof(SANE_Int);
   ps->option[SOAP_OPTION_BR_X].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
   ps->option[SOAP_OPTION_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
   ps->option[SOAP_OPTION_BR_X].constraint.range = &ps->brxRange;
   ps->brxRange.min = 0;
   ps->brxRange.quant = 0;

   ps->option[SOAP_OPTION_BR_Y].name = SANE_NAME_SCAN_BR_Y;
   ps->option[SOAP_OPTION_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
   ps->option[SOAP_OPTION_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
   ps->option[SOAP_OPTION_BR_Y].type = SANE_TYPE_FIXED;
   ps->option[SOAP_OPTION_BR_Y].unit = SANE_UNIT_MM;
   ps->option[SOAP_OPTION_BR_Y].size = sizeof(SANE_Int);
   ps->option[SOAP_OPTION_BR_Y].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
   ps->option[SOAP_OPTION_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
   ps->option[SOAP_OPTION_BR_Y].constraint.range = &ps->bryRange;
   ps->bryRange.min = 0;
   ps->bryRange.quant = 0;

   return 0;
} /* init_options */

/* Verify current x/y extents and set effective extents. */ 
static int set_extents(struct soap_session *ps)
{
   int stat = 0;

   if ((ps->currentBrx > ps->currentTlx) && (ps->currentBrx - ps->currentTlx >= ps->min_width) && (ps->currentBrx - ps->currentTlx <= ps->tlxRange.max))
   {
     ps->effectiveTlx = ps->currentTlx;
     ps->effectiveBrx = ps->currentBrx;
   }
   else
   {
     ps->effectiveTlx = 0;  /* current setting is not valid, zero it */
     ps->effectiveBrx = 0;
     stat = 1;
   }
   if ((ps->currentBry > ps->currentTly) && (ps->currentBry - ps->currentTly > ps->min_height) && (ps->currentBry - ps->currentTly <= ps->tlyRange.max))
   {
     ps->effectiveTly = ps->currentTly;
     ps->effectiveBry = ps->currentBry;
   }
   else
   {
     ps->effectiveTly = 0;  /* current setting is not valid, zero it */
     ps->effectiveBry = 0;
     stat = 1;
   }
   return stat;
} /* set_extents */

/*
 * SANE APIs.
 */

SANE_Status soap_open(SANE_String_Const device, SANE_Handle *handle)
{
   struct hpmud_model_attributes ma;
   int stat = SANE_STATUS_IO_ERROR, i;

   DBG8("sane_hpaio_open(%s)\n", device);

   if (session)
   {
      BUG("session in use\n");
      return SANE_STATUS_DEVICE_BUSY;
   }

   if ((session = create_session()) == NULL)
      return SANE_STATUS_NO_MEM;
    
   /* Set session to specified device. */
   snprintf(session->uri, sizeof(session->uri)-1, "hp:%s", device);   /* prepend "hp:" */

   /* Get actual model attributes from models.dat. */
   hpmud_query_model(session->uri, &ma);
   session->scan_type = ma.scantype;

   if (hpmud_open_device(session->uri, ma.mfp_mode, &session->dd) != HPMUD_R_OK)
   {
      BUG("unable to open device %s\n", session->uri);
      goto bugout;

      free(session);
      session = NULL;
      return SANE_STATUS_IO_ERROR;
   }

   if (bb_load(session, SCAN_PLUGIN_SOAP))
   {
      stat = SANE_STATUS_IO_ERROR;
      goto bugout;
   }

   /* Init sane option descriptors. */
   init_options(session);  

   if (session->bb_open(session))
   {
      stat = SANE_STATUS_IO_ERROR;
      goto bugout;
   }

   /* Set supported Scan Modes as determined by bb_open. */
   soap_control_option(session, SOAP_OPTION_SCAN_MODE, SANE_ACTION_SET_AUTO, NULL, NULL); /* set default option */

   /* Set supported resolutions. */
   i=1;
   session->resolutionList[i++] = 75;
   session->resolutionList[i++] = 100;
   session->resolutionList[i++] = 150;
   session->resolutionList[i++] = 200;
   session->resolutionList[i++] = 300;
   session->resolutionList[i++] = 600;
   session->resolutionList[i++] = 1200;
   session->resolutionList[0] = i-1;    /* length of word_list */
   soap_control_option(session, SOAP_OPTION_SCAN_RESOLUTION, SANE_ACTION_SET_AUTO, NULL, NULL); /* set default option */

   /* Set supported brightness. */
   soap_control_option(session, SOAP_OPTION_BRIGHTNESS, SANE_ACTION_SET_AUTO, NULL, NULL); /* set default option */

   /* Set supported contrast. */
   soap_control_option(session, SOAP_OPTION_CONTRAST, SANE_ACTION_SET_AUTO, NULL, NULL); /* set default option */

   /* Set supported compression. (Note, cm1017 may say it supports MMR, but it doesn't) */
   soap_control_option(session, SOAP_OPTION_COMPRESSION, SANE_ACTION_SET_AUTO, NULL, NULL); /* set default option */
   
   /* Set jpeg quality factor as determined by bb_open. */
   soap_control_option(session, SOAP_OPTION_JPEG_QUALITY, SANE_ACTION_SET_AUTO, NULL, NULL); /* set default option */

   /* Set x,y extents. See bb_open */
   soap_control_option(session, SOAP_OPTION_TL_X, SANE_ACTION_SET_AUTO, NULL, NULL); /* set default option */
   soap_control_option(session, SOAP_OPTION_TL_Y, SANE_ACTION_SET_AUTO, NULL, NULL); /* set default option */
   soap_control_option(session, SOAP_OPTION_BR_X, SANE_ACTION_SET_AUTO, NULL, NULL); /* set default option */
   soap_control_option(session, SOAP_OPTION_BR_Y, SANE_ACTION_SET_AUTO, NULL, NULL); /* set default option */

   *handle = (SANE_Handle *)session;

   stat = SANE_STATUS_GOOD;

bugout:

   if (stat != SANE_STATUS_GOOD)
   {
      if (session)
      {
         bb_unload(session);
         if (session->dd > 0)
            hpmud_close_device(session->dd);
         free(session);
         session = NULL;
      }
   }

   return stat;
} /* sane_open */

void soap_close(SANE_Handle handle)
{
   struct soap_session *ps = (struct soap_session *)handle;

   DBG8("sane_hpaio_close()\n"); 

   if (ps == NULL || ps != session)
   {
      BUG("invalid sane_close\n");
      return;
   }

   ps->bb_close(ps);
   bb_unload(ps);

   if (ps->dd > 0)
      hpmud_close_device(ps->dd);
    
   free(ps);
   session = NULL;
} /* soap_close */

const SANE_Option_Descriptor *soap_get_option_descriptor(SANE_Handle handle, SANE_Int option)
{
   struct soap_session *ps = (struct soap_session *)handle;

   DBG8("sane_hpaio_get_option_descriptor(option=%s)\n", ps->option[option].name);

   if (option < 0 || option >= SOAP_OPTION_MAX)
      return NULL;

   return &ps->option[option];
} /* soap_get_option_descriptor */

SANE_Status soap_control_option(SANE_Handle handle, SANE_Int option, SANE_Action action, void *value, SANE_Int *set_result)
{
   struct soap_session *ps = (struct soap_session *)handle;
   SANE_Int *int_value = value, mset_result=0;
   int i, stat=SANE_STATUS_INVAL;
   char sz[64];

   switch(option)
   {
      case SOAP_OPTION_COUNT:
         if (action == SANE_ACTION_GET_VALUE)
         {
            *int_value = SOAP_OPTION_MAX;
            stat = SANE_STATUS_GOOD;
         }
         break;
      case SOAP_OPTION_SCAN_MODE:
         if (action == SANE_ACTION_GET_VALUE)
         {
            for (i=0; ps->scanModeList[i]; i++)
            {
               if (ps->currentScanMode == ps->scanModeMap[i])
               {
                  strcpy(value, ps->scanModeList[i]);
                  stat = SANE_STATUS_GOOD;
                  break;
               }
            }
         }
         else if (action == SANE_ACTION_SET_VALUE)
         {
            for (i=0; ps->scanModeList[i]; i++)
            {
               if (strcasecmp(ps->scanModeList[i], value) == 0)
               {
                  ps->currentScanMode = ps->scanModeMap[i];
                  set_scan_mode_side_effects(ps, ps->currentScanMode);
                  mset_result |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
                  stat = SANE_STATUS_GOOD;
                  break;
               }
            }
         }
         else
         {  /* Set default. */
            ps->currentScanMode = ps->scanModeMap[0];
            set_scan_mode_side_effects(ps, ps->currentScanMode);
            stat = SANE_STATUS_GOOD;
         }
         break;
      case SOAP_OPTION_SCAN_RESOLUTION:
         if (action == SANE_ACTION_GET_VALUE)
         {
            *int_value = ps->currentResolution;
            stat = SANE_STATUS_GOOD;
         }
         else if (action == SANE_ACTION_SET_VALUE)
         {
            for (i=1; i <= ps->resolutionList[0]; i++)
            {
               if (ps->resolutionList[i] == *int_value)
               {
                  ps->currentResolution = *int_value;
                  mset_result |= SANE_INFO_RELOAD_PARAMS;
                  stat = SANE_STATUS_GOOD;
                  break;
               }
            }
            if (stat != SANE_STATUS_GOOD)
            {
                ps->currentResolution = ps->resolutionList[1];
                stat = SANE_STATUS_GOOD;
            }
         }
         else
         {  /* Set default. */
            ps->currentResolution = 75;
            stat = SANE_STATUS_GOOD;
         }
         break;
      case SOAP_OPTION_CONTRAST:
         if (action == SANE_ACTION_GET_VALUE)
         {
            *int_value = ps->currentContrast;
            stat = SANE_STATUS_GOOD;
         }
         else if (action == SANE_ACTION_SET_VALUE)
         {
            if (*int_value >= SOAP_CONTRAST_MIN && *int_value <= SOAP_CONTRAST_MAX)
            {
               ps->currentContrast = *int_value;
            }
            else
            {
               ps->currentContrast = SOAP_CONTRAST_DEFAULT;
            }
            mset_result |= SANE_INFO_RELOAD_PARAMS;
            stat = SANE_STATUS_GOOD;
         }
         else
         {  /* Set default. */
            ps->currentContrast = SOAP_CONTRAST_DEFAULT;
            stat = SANE_STATUS_GOOD;
         }
         break;
      case SOAP_OPTION_BRIGHTNESS:
         if (action == SANE_ACTION_GET_VALUE)
         {
            *int_value = ps->currentBrightness;
            stat = SANE_STATUS_GOOD;
         }
         else if (action == SANE_ACTION_SET_VALUE)
         {
            if (*int_value >= SOAP_BRIGHTNESS_MIN && *int_value <= SOAP_BRIGHTNESS_MAX)
            {
               ps->currentBrightness = *int_value;
            }
            else
            {
              ps->currentBrightness = SOAP_BRIGHTNESS_DEFAULT;
            }
            stat = SANE_STATUS_GOOD;
         }
         else
         {  /* Set default. */
            ps->currentBrightness = SOAP_BRIGHTNESS_DEFAULT;
            stat = SANE_STATUS_GOOD;
         }
         break;
      case SOAP_OPTION_COMPRESSION:
         if (action == SANE_ACTION_GET_VALUE)
         {
            for (i=0; ps->compressionList[i]; i++)
            {
               if (ps->currentCompression == ps->compressionMap[i])
               {
                  strcpy(value, ps->compressionList[i]);
                  stat = SANE_STATUS_GOOD;
                  break;
               }
            }
         }
         else if (action == SANE_ACTION_SET_VALUE)
         {
            for (i=0; ps->compressionList[i]; i++)
            {
               if (strcasecmp(ps->compressionList[i], value) == 0)
               {
                  ps->currentCompression = ps->compressionMap[i];
                  stat = SANE_STATUS_GOOD;
                  break;
               }
            }
         }
         else
         {  /* Set default. */
            ps->currentCompression = SF_JFIF;
            stat = SANE_STATUS_GOOD;
         }
         break;
      case SOAP_OPTION_JPEG_QUALITY:
         if (action == SANE_ACTION_GET_VALUE)
         {
            *int_value = ps->currentJpegQuality;
            stat = SANE_STATUS_GOOD;
         }
         else if (action == SANE_ACTION_SET_VALUE)
         {
            if (*int_value >= MIN_JPEG_COMPRESSION_FACTOR && *int_value <= MAX_JPEG_COMPRESSION_FACTOR)
            {
               ps->currentJpegQuality = *int_value;
               stat = SANE_STATUS_GOOD;
               break;
            }
         }
         else
         {  /* Set default. */
            ps->currentJpegQuality = SAFER_JPEG_COMPRESSION_FACTOR;
            stat = SANE_STATUS_GOOD;
         }
         break;
      case SOAP_OPTION_TL_X:
         if (action == SANE_ACTION_GET_VALUE)
         {
            *int_value = ps->currentTlx;
            stat = SANE_STATUS_GOOD;
         }
         else if (action == SANE_ACTION_SET_VALUE)
         {
            if (*int_value >= ps->tlxRange.min && *int_value <= ps->tlxRange.max)
            {
               ps->currentTlx = *int_value;
               mset_result |= SANE_INFO_RELOAD_PARAMS;
               stat = SANE_STATUS_GOOD;
               break;
            }
         }
         else
         {  /* Set default. */
            ps->currentTlx = ps->tlxRange.min;
            stat = SANE_STATUS_GOOD;
         }
         break;
      case SOAP_OPTION_TL_Y:
         if (action == SANE_ACTION_GET_VALUE)
         {
            *int_value = ps->currentTly;
            stat = SANE_STATUS_GOOD;
         }
         else if (action == SANE_ACTION_SET_VALUE)
         {
            if (*int_value >= ps->tlyRange.min && *int_value <= ps->tlyRange.max)
            {
               
               ps->currentTly = *int_value;
               mset_result |= SANE_INFO_RELOAD_PARAMS;
               stat = SANE_STATUS_GOOD;
               break;
            }
         }
         else
         {  /* Set default. */
            ps->currentTly = ps->tlyRange.min;
            stat = SANE_STATUS_GOOD;
         }
         break;
      case SOAP_OPTION_BR_X:
         if (action == SANE_ACTION_GET_VALUE)
         {
            *int_value = ps->currentBrx;
            stat = SANE_STATUS_GOOD;
         }
         else if (action == SANE_ACTION_SET_VALUE)
         {
            if (*int_value >= ps->brxRange.min && *int_value <= ps->brxRange.max)
            {
               ps->currentBrx = *int_value;
               mset_result |= SANE_INFO_RELOAD_PARAMS;
               stat = SANE_STATUS_GOOD;
               break;
            }
         }
         else
         {  /* Set default. */
            ps->currentBrx = ps->brxRange.max;
            stat = SANE_STATUS_GOOD;
         }
         break;
      case SOAP_OPTION_BR_Y:
         if (action == SANE_ACTION_GET_VALUE)
         {
            *int_value = ps->currentBry;
            stat = SANE_STATUS_GOOD;
         }
         else if (action == SANE_ACTION_SET_VALUE)
         {
            if (*int_value >= ps->bryRange.min && *int_value <= ps->bryRange.max)
            {
               ps->currentBry = *int_value;
               mset_result |= SANE_INFO_RELOAD_PARAMS;
               stat = SANE_STATUS_GOOD;
               break;
            }
         }
         else
         {  /* Set default. */
            ps->currentBry = ps->bryRange.max;
            stat = SANE_STATUS_GOOD;
         }
         break;
      default:
         break;
   }

   if (set_result)
      *set_result = mset_result;

   if (stat != SANE_STATUS_GOOD)
   {
      BUG("control_option failed: option=%s action=%s\n", ps->option[option].name, 
                  action==SANE_ACTION_GET_VALUE ? "get" : action==SANE_ACTION_SET_VALUE ? "set" : "auto");
   }

   DBG8("sane_hpaio_control_option (option=%s action=%s value=%s)\n", ps->option[option].name, 
                        action==SANE_ACTION_GET_VALUE ? "get" : action==SANE_ACTION_SET_VALUE ? "set" : "auto",
     value ? ps->option[option].type == SANE_TYPE_STRING ? (char *)value : psnprintf(sz, sizeof(sz), "%d", *(int *)value) : "na");

   return stat;
} /* soap_control_option */

SANE_Status soap_get_parameters(SANE_Handle handle, SANE_Parameters *params)
{
   struct soap_session *ps = (struct soap_session *)handle;

   set_extents(ps);

   /* Get scan parameters for sane client. */
   ps->bb_get_parameters(ps, params, ps->ip_handle ? SPO_STARTED : SPO_BEST_GUESS);

   DBG8("sane_hpaio_get_parameters(): format=%d, last_frame=%d, lines=%d, depth=%d, pixels_per_line=%d, bytes_per_line=%d\n",
                    params->format, params->last_frame, params->lines, params->depth, params->pixels_per_line, params->bytes_per_line);

   return SANE_STATUS_GOOD;
} /* soap_get_parameters */

SANE_Status soap_start(SANE_Handle handle)
{
   struct soap_session *ps = (struct soap_session *)handle;
   SANE_Parameters pp;
   IP_IMAGE_TRAITS traits;
   IP_XFORM_SPEC xforms[IP_MAX_XFORMS], *pXform=xforms;
   int stat, ret;

   DBG8("sane_hpaio_start()\n");
   ps->user_cancel = 0;
   if (set_extents(ps))
   {
      BUG("invalid extents: tlx=%d brx=%d tly=%d bry=%d minwidth=%d minheight%d maxwidth=%d maxheight=%d\n",
         ps->currentTlx, ps->currentTly, ps->currentBrx, ps->currentBry, ps->min_width, ps->min_height, ps->tlxRange.max, ps->tlyRange.max);
      stat = SANE_STATUS_INVAL;
      goto bugout;
   }   

   /* Start scan. */
   if (ps->bb_start_scan(ps))
   {
      stat = SANE_STATUS_IO_ERROR;
      goto bugout;
   }
   SendScanEvent(ps->uri, EVENT_START_SCAN_JOB);
   memset(xforms, 0, sizeof(xforms));    

   /* Setup image-processing pipeline for xform. */
   if (ps->currentScanMode == CE_RGB24 || ps->currentScanMode == CE_GRAY8)
   {
      pXform->aXformInfo[IP_JPG_DECODE_FROM_DENALI].dword = 0;    /* 0=no */
      ADD_XFORM(X_JPG_DECODE);
      pXform->aXformInfo[IP_CNV_COLOR_SPACE_WHICH_CNV].dword = IP_CNV_YCC_TO_SRGB;
      pXform->aXformInfo[IP_CNV_COLOR_SPACE_GAMMA].dword = 0x00010000;
      ADD_XFORM(X_CNV_COLOR_SPACE);
   }
   else
   {  /* Must be BLACK_AND_WHITE1 (Lineart). */
      pXform->aXformInfo[IP_JPG_DECODE_FROM_DENALI].dword = 0;    /* 0=no */
      ADD_XFORM(X_JPG_DECODE);
      pXform->aXformInfo[IP_GRAY_2_BI_THRESHOLD].dword = 127;
      ADD_XFORM(X_GRAY_2_BI);
   }

   /* Setup x/y cropping for xform. (Actually we let cm1017 do it's own cropping) */
   pXform->aXformInfo[IP_CROP_LEFT].dword = 0;
   pXform->aXformInfo[IP_CROP_RIGHT].dword = 0;
   pXform->aXformInfo[IP_CROP_TOP].dword = 0;
   pXform->aXformInfo[IP_CROP_MAXOUTROWS].dword = 0;
   ADD_XFORM(X_CROP);

   /* Setup x/y padding for xform. (Actually we let cm1017 do it's own padding) */
   pXform->aXformInfo[IP_PAD_LEFT].dword = 0; /* # of pixels to add to left side */
   pXform->aXformInfo[IP_PAD_RIGHT].dword = 0; /* # of pixels to add to right side */
   pXform->aXformInfo[IP_PAD_TOP].dword = 0; /* # of rows to add to top */
   pXform->aXformInfo[IP_PAD_BOTTOM].dword = 0;  /* # of rows to add to bottom */
   pXform->aXformInfo[IP_PAD_VALUE].dword = ps->currentScanMode == CE_BLACK_AND_WHITE1 ? 0 : -1;   /* lineart white = 0, rgb white = -1 */ 
   pXform->aXformInfo[IP_PAD_MIN_HEIGHT].dword = 0;
   ADD_XFORM(X_PAD);

   /* Open image processor. */
   if ((ret = ipOpen(pXform-xforms, xforms, 0, &ps->ip_handle)) != IP_DONE)
   {
      BUG("unable open image processor: err=%d\n", ret);
      stat = SANE_STATUS_INVAL;
      goto bugout;
   }

   /* Set known input image attributes. */
   ps->bb_get_parameters(ps, &pp, SPO_BEST_GUESS);
   traits.iPixelsPerRow = pp.pixels_per_line;
   switch(ps->currentScanMode)
   {
      case CE_BLACK_AND_WHITE1:     /* linart uses 8-bit gray */   
      case CE_GRAY8:
         traits.iBitsPerPixel = 8;     /* grayscale */
         break;
      case CE_RGB24:
      default:
         traits.iBitsPerPixel = 24;      /* color */
         break;
   }
   traits.lHorizDPI = ps->currentResolution << 16;
   traits.lVertDPI = ps->currentResolution << 16;
   traits.lNumRows =  pp.lines;
   traits.iNumPages = 1;
   traits.iPageNum = 1;
   traits.iComponentsPerPixel = ((traits.iBitsPerPixel % 3) ? 1 : 3);
   DBG6("set traits iPixelsPerRow=%d iBitsPerPixel=%d lNumRows=%d iComponentsPerPixel=%d\n", traits.iPixelsPerRow, 
           traits.iBitsPerPixel, (int)traits.lNumRows, traits.iComponentsPerPixel);
   ipSetDefaultInputTraits(ps->ip_handle, &traits);

   /* If jpeg get output image attributes from the image processor. */
   if (ps->currentCompression == SF_JFIF)
   {
      /* Enable parsed header flag. */
      ipResultMask(ps->ip_handle, IP_PARSED_HEADER);

      /* Wait for image processor to process header so we know the exact size of the image for sane_get_params. */
      while (1)
      {
         ret = get_ip_data(ps, NULL, 0, NULL);

         if (ret & (IP_INPUT_ERROR | IP_FATAL_ERROR | IP_DONE))
         {
            BUG("ipConvert error=%x\n", ret);
            stat = SANE_STATUS_IO_ERROR;
            goto bugout;
         }

         if (ret & IP_PARSED_HEADER)
         {
            ipGetImageTraits(ps->ip_handle, NULL, &ps->image_traits);  /* get valid image traits */
            ipResultMask(ps->ip_handle, 0);                          /* disable parsed header flag */
            break;
         }
      }
   }
   else
      ipGetImageTraits(ps->ip_handle, NULL, &ps->image_traits);  /* get valid image traits */

   DBG6("act traits iPixelsPerRow=%d iBitsPerPixel=%d lNumRows=%d iComponentsPerPixel=%d\n", ps->image_traits.iPixelsPerRow, 
           ps->image_traits.iBitsPerPixel,  (int)ps->image_traits.lNumRows, ps->image_traits.iComponentsPerPixel);

   stat = SANE_STATUS_GOOD;

bugout:
   if (stat != SANE_STATUS_GOOD)
   {
      if (ps->ip_handle)
      {
         ipClose(ps->ip_handle); 
         ps->ip_handle = 0;
      }   
      ps->bb_end_scan(ps, stat == SANE_STATUS_IO_ERROR ? 1: 0);
   }

   return stat;
} /* soap_start */

SANE_Status soap_read(SANE_Handle handle, SANE_Byte *data, SANE_Int maxLength, SANE_Int *length)
{
   struct soap_session *ps = (struct soap_session *)handle;
   int ret, stat=SANE_STATUS_IO_ERROR;

   DBG8("sane_hpaio_read() handle=%p data=%p maxLength=%d\n", (void *)handle, data, maxLength);
   if(ps->user_cancel)
   {
     DBG8("soap_read() EVENT_SCAN_CANCEL****uri=%s\n", ps->uri);
     SendScanEvent(ps->uri, EVENT_SCAN_CANCEL);
     return SANE_STATUS_CANCELLED;
   }
   
   ret = get_ip_data(ps, data, maxLength, length);

   if(ret & (IP_INPUT_ERROR | IP_FATAL_ERROR))
   {
      BUG("ipConvert error=%x\n", ret);
      goto bugout;
   }

   if (ret & IP_DONE)
   {
      stat = SANE_STATUS_EOF;
      SendScanEvent (ps->uri, EVENT_END_SCAN_JOB);
   }
   else
      stat = SANE_STATUS_GOOD;

bugout:
   if (stat != SANE_STATUS_GOOD)
   {
      if (ps->ip_handle)
      {
         /* Note always call ipClose when SANE_STATUS_EOF, do not depend on sane_cancel because sane_cancel is only called at the end of a batch job. */ 
         ipClose(ps->ip_handle);  
         ps->ip_handle = 0;
      } 
      ps->bb_end_page(ps, 0);
   }

   DBG8("-sane_hpaio_read() output=%p bytes_read=%d maxLength=%d status=%d\n", data, *length, maxLength, stat);

   return stat;
} /* soap_read */

void soap_cancel(SANE_Handle handle)
{
   struct soap_session *ps = (struct soap_session *)handle;

   DBG8("sane_hpaio_cancel()\n"); 

   /*
    * Sane_cancel is always called at the end of the scan job. Note that on a multiple page scan job 
    * sane_cancel is called only once.
    */
   ps->user_cancel = 1;
   if (ps->ip_handle)
   {
      ipClose(ps->ip_handle); 
      ps->ip_handle = 0;
   }
   ps->bb_end_scan(ps, 0);
} /* soap_cancel */

