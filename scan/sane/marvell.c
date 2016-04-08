/************************************************************************************\

  marvell.c - HP SANE backend support for Marvell based multi-function peripherals

  (c) 2008 Copyright HP Development Company, LP

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

  Author: David Suffield, Yashwant Sahu, Sarbeswar Meher

\************************************************************************************/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sys/socket.h>
#include <netdb.h>
#include <stdarg.h>
#include <syslog.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <dlfcn.h>
#include "sane.h"
#include "saneopts.h"
#include "hpmud.h"
#include "hpip.h"
#include "common.h"
#include "marvell.h"
#include "marvelli.h"
#include "io.h"
#include "utils.h"

#define DEBUG_DECLARE_ONLY
#include "sanei_debug.h"

static struct marvell_session *session = NULL;   /* assume one sane_open per process */

static int bb_load(struct marvell_session *ps, const char *so)
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
}

static int bb_unload(struct marvell_session *ps)
{
    unload_library(ps->bb_handle);
    ps->bb_handle = NULL;

    unload_library(ps->hpmud_handle);
    ps->hpmud_handle = NULL;

    unload_library(ps->math_handle);
    ps->math_handle = NULL;

   return 0;
}

/* Get raw data (ie: uncompressed data) from image processor. */
static int get_ip_data(struct marvell_session *ps, SANE_Byte *data, SANE_Int maxLength, SANE_Int *length)
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
      input = ps->buf;
   }
   else
   {
      input = NULL;   /* no more scan data, flush ipconvert pipeline */
      inputAvail = 0;
   }

   /* Transform input data to output. Note, output buffer may consume more bytes than input buffer (ie: jpeg to raster). */
   ip_ret = ipConvert(ps->ip_handle, inputAvail, input, &inputUsed, &inputNextPos, outputAvail, output, &outputUsed, &outputThisPos);

   DBG6("input=%p inputAvail=%d inputUsed=%d inputNextPos=%d output=%p outputAvail=%d outputUsed=%d outputThisPos=%d ret=%x\n", input, 
         inputAvail, inputUsed, inputNextPos, output, outputAvail, outputUsed, outputThisPos, ip_ret);

   if (data)
      *length = outputUsed;

   /* For sane do not send output data simultaneously with IP_DONE. */
   if (ip_ret & IP_DONE && outputUsed)
      ip_ret &= ~IP_DONE;                               

bugout:
   return ip_ret;
}

static int init_options(struct marvell_session *ps)
{
   ps->option[MARVELL_OPTION_COUNT].name = "option-cnt";
   ps->option[MARVELL_OPTION_COUNT].title = SANE_TITLE_NUM_OPTIONS;
   ps->option[MARVELL_OPTION_COUNT].desc = SANE_DESC_NUM_OPTIONS;
   ps->option[MARVELL_OPTION_COUNT].type = SANE_TYPE_INT;
   ps->option[MARVELL_OPTION_COUNT].unit = SANE_UNIT_NONE;
   ps->option[MARVELL_OPTION_COUNT].size = sizeof(SANE_Int);
   ps->option[MARVELL_OPTION_COUNT].cap = SANE_CAP_SOFT_DETECT;
   ps->option[MARVELL_OPTION_COUNT].constraint_type = SANE_CONSTRAINT_NONE;

   ps->option[MARVELL_OPTION_GROUP_SCAN_MODE].name = "mode-group";
   ps->option[MARVELL_OPTION_GROUP_SCAN_MODE].title = SANE_TITLE_SCAN_MODE;
   ps->option[MARVELL_OPTION_GROUP_SCAN_MODE].type = SANE_TYPE_GROUP;

   ps->option[MARVELL_OPTION_SCAN_MODE].name = SANE_NAME_SCAN_MODE;
   ps->option[MARVELL_OPTION_SCAN_MODE].title = SANE_TITLE_SCAN_MODE;
   ps->option[MARVELL_OPTION_SCAN_MODE].desc = SANE_DESC_SCAN_MODE;
   ps->option[MARVELL_OPTION_SCAN_MODE].type = SANE_TYPE_STRING;
   ps->option[MARVELL_OPTION_SCAN_MODE].unit = SANE_UNIT_NONE;
   ps->option[MARVELL_OPTION_SCAN_MODE].size = MAX_STRING_SIZE;
   ps->option[MARVELL_OPTION_SCAN_MODE].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
   ps->option[MARVELL_OPTION_SCAN_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
   ps->option[MARVELL_OPTION_SCAN_MODE].constraint.string_list = ps->scan_mode_list;

   ps->option[MARVELL_OPTION_INPUT_SOURCE].name = SANE_NAME_SCAN_SOURCE;
   ps->option[MARVELL_OPTION_INPUT_SOURCE].title = SANE_TITLE_SCAN_SOURCE;
   ps->option[MARVELL_OPTION_INPUT_SOURCE].desc = SANE_DESC_SCAN_SOURCE;
   ps->option[MARVELL_OPTION_INPUT_SOURCE].type = SANE_TYPE_STRING;
   ps->option[MARVELL_OPTION_INPUT_SOURCE].unit = SANE_UNIT_NONE;
   ps->option[MARVELL_OPTION_INPUT_SOURCE].size = MAX_STRING_SIZE;
   ps->option[MARVELL_OPTION_INPUT_SOURCE].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
   ps->option[MARVELL_OPTION_INPUT_SOURCE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
   ps->option[MARVELL_OPTION_INPUT_SOURCE].constraint.string_list = ps->input_source_list;

   ps->option[MARVELL_OPTION_SCAN_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
   ps->option[MARVELL_OPTION_SCAN_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
   ps->option[MARVELL_OPTION_SCAN_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
   ps->option[MARVELL_OPTION_SCAN_RESOLUTION].type = SANE_TYPE_INT;
   ps->option[MARVELL_OPTION_SCAN_RESOLUTION].unit = SANE_UNIT_DPI;
   ps->option[MARVELL_OPTION_SCAN_RESOLUTION].size = sizeof(SANE_Int);
   ps->option[MARVELL_OPTION_SCAN_RESOLUTION].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
   ps->option[MARVELL_OPTION_SCAN_RESOLUTION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
   ps->option[MARVELL_OPTION_SCAN_RESOLUTION].constraint.word_list = ps->resolution_list;

   ps->option[MARVELL_OPTION_GROUP_ADVANCED].name = "advanced-group";
   ps->option[MARVELL_OPTION_GROUP_ADVANCED].title = STR_TITLE_ADVANCED;
   ps->option[MARVELL_OPTION_GROUP_ADVANCED].type = SANE_TYPE_GROUP;
   ps->option[MARVELL_OPTION_GROUP_ADVANCED].cap = SANE_CAP_ADVANCED;

   ps->option[MARVELL_OPTION_BRIGHTNESS].name = SANE_NAME_BRIGHTNESS;
   ps->option[MARVELL_OPTION_BRIGHTNESS].title = SANE_TITLE_BRIGHTNESS;
   ps->option[MARVELL_OPTION_BRIGHTNESS].desc = SANE_DESC_BRIGHTNESS;
   ps->option[MARVELL_OPTION_BRIGHTNESS].type = SANE_TYPE_INT;
   ps->option[MARVELL_OPTION_BRIGHTNESS].unit = SANE_UNIT_NONE;
   ps->option[MARVELL_OPTION_BRIGHTNESS].size = sizeof(SANE_Int);
   ps->option[MARVELL_OPTION_BRIGHTNESS].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
   ps->option[MARVELL_OPTION_BRIGHTNESS].constraint_type = SANE_CONSTRAINT_RANGE;
   ps->option[MARVELL_OPTION_BRIGHTNESS].constraint.range = &ps->brightnessRange;
   ps->brightnessRange.min = MARVELL_BRIGHTNESS_MIN;
   ps->brightnessRange.max = MARVELL_BRIGHTNESS_MAX;
   ps->brightnessRange.quant = 0;

    ps->option[MARVELL_OPTION_CONTRAST].name = SANE_NAME_CONTRAST;
   ps->option[MARVELL_OPTION_CONTRAST].title = SANE_TITLE_CONTRAST;
   ps->option[MARVELL_OPTION_CONTRAST].desc = SANE_DESC_CONTRAST;
   ps->option[MARVELL_OPTION_CONTRAST].type = SANE_TYPE_INT;
   ps->option[MARVELL_OPTION_CONTRAST].unit = SANE_UNIT_NONE;
   ps->option[MARVELL_OPTION_CONTRAST].size = sizeof(SANE_Int);
   ps->option[MARVELL_OPTION_CONTRAST].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
   ps->option[MARVELL_OPTION_CONTRAST].constraint_type = SANE_CONSTRAINT_RANGE;
   ps->option[MARVELL_OPTION_CONTRAST].constraint.range = &ps->contrast_range;
   ps->contrast_range.min = MARVELL_CONTRAST_MIN;
   ps->contrast_range.max = MARVELL_CONTRAST_MAX;
   ps->contrast_range.quant = 0;

   ps->option[MARVELL_OPTION_GROUP_GEOMETRY].name = "geometry-group";
   ps->option[MARVELL_OPTION_GROUP_GEOMETRY].title = STR_TITLE_GEOMETRY;
   ps->option[MARVELL_OPTION_GROUP_GEOMETRY].type = SANE_TYPE_GROUP;
   ps->option[MARVELL_OPTION_GROUP_GEOMETRY].cap = SANE_CAP_ADVANCED;

   ps->option[MARVELL_OPTION_TL_X].name = SANE_NAME_SCAN_TL_X;
   ps->option[MARVELL_OPTION_TL_X].title = SANE_TITLE_SCAN_TL_X;
   ps->option[MARVELL_OPTION_TL_X].desc = SANE_DESC_SCAN_TL_X;
   ps->option[MARVELL_OPTION_TL_X].type = SANE_TYPE_FIXED;
   ps->option[MARVELL_OPTION_TL_X].unit = SANE_UNIT_MM;
   ps->option[MARVELL_OPTION_TL_X].size = sizeof(SANE_Int);
   ps->option[MARVELL_OPTION_TL_X].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
   ps->option[MARVELL_OPTION_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
   ps->option[MARVELL_OPTION_TL_X].constraint.range = &ps->tlxRange;
   ps->tlxRange.min = 0;
   ps->tlxRange.quant = 0;

   ps->option[MARVELL_OPTION_TL_Y].name = SANE_NAME_SCAN_TL_Y;
   ps->option[MARVELL_OPTION_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
   ps->option[MARVELL_OPTION_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
   ps->option[MARVELL_OPTION_TL_Y].type = SANE_TYPE_FIXED;
   ps->option[MARVELL_OPTION_TL_Y].unit = SANE_UNIT_MM;
   ps->option[MARVELL_OPTION_TL_Y].size = sizeof(SANE_Int);
   ps->option[MARVELL_OPTION_TL_Y].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
   ps->option[MARVELL_OPTION_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
   ps->option[MARVELL_OPTION_TL_Y].constraint.range = &ps->tlyRange;
   ps->tlyRange.min = 0;
   ps->tlyRange.quant = 0;

   ps->option[MARVELL_OPTION_BR_X].name = SANE_NAME_SCAN_BR_X;
   ps->option[MARVELL_OPTION_BR_X].title = SANE_TITLE_SCAN_BR_X;
   ps->option[MARVELL_OPTION_BR_X].desc = SANE_DESC_SCAN_BR_X;
   ps->option[MARVELL_OPTION_BR_X].type = SANE_TYPE_FIXED;
   ps->option[MARVELL_OPTION_BR_X].unit = SANE_UNIT_MM;
   ps->option[MARVELL_OPTION_BR_X].size = sizeof(SANE_Int);
   ps->option[MARVELL_OPTION_BR_X].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
   ps->option[MARVELL_OPTION_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
   ps->option[MARVELL_OPTION_BR_X].constraint.range = &ps->brxRange;
   ps->brxRange.min = 0;
   ps->brxRange.quant = 0;

   ps->option[MARVELL_OPTION_BR_Y].name = SANE_NAME_SCAN_BR_Y;
   ps->option[MARVELL_OPTION_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
   ps->option[MARVELL_OPTION_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
   ps->option[MARVELL_OPTION_BR_Y].type = SANE_TYPE_FIXED;
   ps->option[MARVELL_OPTION_BR_Y].unit = SANE_UNIT_MM;
   ps->option[MARVELL_OPTION_BR_Y].size = sizeof(SANE_Int);
   ps->option[MARVELL_OPTION_BR_Y].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
   ps->option[MARVELL_OPTION_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
   ps->option[MARVELL_OPTION_BR_Y].constraint.range = &ps->bryRange;
   ps->bryRange.min = 0;
   ps->bryRange.quant = 0;

   return 0;
}

/* Verify current x/y extents and set effective extents. */ 
static int set_extents(struct marvell_session *ps)
{
   int stat = 0;

   if ((ps->currentBrx > ps->currentTlx) && (ps->currentBrx - ps->currentTlx >= ps->min_width) && (ps->currentBrx - ps->currentTlx <= ps->tlxRange.max))
   {
     ps->effectiveTlx = ps->currentTlx;
     ps->effectiveBrx = ps->currentBrx;
   }
   else
   {
     ps->effectiveTlx = ps->currentTlx = 0;  /* current setting is not valid, zero it */
     ps->effectiveBrx = ps->currentBrx = ps->brxRange.max;
   }
   if ((ps->currentBry > ps->currentTly) && (ps->currentBry - ps->currentTly > ps->min_height) && (ps->currentBry - ps->currentTly <= ps->tlyRange.max))
   {
     ps->effectiveTly = ps->currentTly;
     ps->effectiveBry = ps->currentBry;
   }
   else
   {
    ps->effectiveTly = ps->currentTly = 0;  /* current setting is not valid, zero it */
    ps->effectiveBry = ps->currentBry = ps->bryRange.max;
   }
   return stat;
}

static struct marvell_session *create_session()
{
   struct marvell_session *ps;

   if ((ps = malloc(sizeof(struct marvell_session))) == NULL)
   {
      BUG("malloc failed: %m\n");
      return NULL;
   }
   memset(ps, 0, sizeof(struct marvell_session));
   ps->tag = "MARVELL";
   ps->dd = -1;
   ps->cd = -1;

   return ps;
}

static void set_supported_resolutions(struct marvell_session *ps)
{
    int i;
    if(ps->scansrc & HPMUD_SCANSRC_ADF)
    {
       i = 0;
       ps->adf_resolution_list[i++] = 1; /*Number of supported resolutions*/
       ps->adf_resolution_list[i++] = 300;
    }
    if(ps->scansrc & HPMUD_SCANSRC_FLATBED)
    {
       i = 0;
       ps->platen_resolution_list[i++] = 7; /*Number of supported resolutions*/
       ps->platen_resolution_list[i++] = 75;
       ps->platen_resolution_list[i++] = 100;
       ps->platen_resolution_list[i++] = 150;
       ps->platen_resolution_list[i++] = 200;
       ps->platen_resolution_list[i++] = 300;
       ps->platen_resolution_list[i++] = 600;
       ps->platen_resolution_list[i++] = 1200;
    }

    if(ps->scansrc & HPMUD_SCANSRC_FLATBED)
    {
        ps->resolution_list[0] = ps->platen_resolution_list[0];
        i = ps->platen_resolution_list[0] + 1;
        while(i--)
        {
            ps->resolution_list[i] = ps->platen_resolution_list[i];
        }
    }
    else 
    {
        ps->resolution_list[0] = ps->adf_resolution_list[0];
        i = ps->adf_resolution_list[0] + 1;
        while(i--)
        {
            ps->resolution_list[i] = ps->adf_resolution_list[i];
        }
        
    }
}
/*
 * SANE APIs.
 */

SANE_Status marvell_open(SANE_String_Const device, SANE_Handle *handle)
{
   struct hpmud_model_attributes ma;
   int stat = SANE_STATUS_IO_ERROR;
   int i;

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
   session->scansrc = ma.scansrc;
   
   switch (ma.scantype)
   {
      case HPMUD_SCANTYPE_MARVELL:
         session->version = MARVELL_1;
		 break;
	  case HPMUD_SCANTYPE_MARVELL2:
         session->version = MARVELL_2;
		 break;
	  default:
         session->version = MARVELL_1;
   };

   if (hpmud_open_device(session->uri, ma.mfp_mode, &session->dd) != HPMUD_R_OK)
   {
      BUG("unable to open device %s\n", session->uri);
      goto bugout;

      free(session);
      session = NULL;
      return SANE_STATUS_IO_ERROR;
   }

   if (hpmud_open_channel(session->dd, HPMUD_S_MARVELL_SCAN_CHANNEL, &session->cd) != HPMUD_R_OK)
   {
      BUG("unable to open %s channel %s\n", HPMUD_S_MARVELL_SCAN_CHANNEL, session->uri);
      stat = SANE_STATUS_DEVICE_BUSY;
      goto bugout;
   }

   if (bb_load(session, SCAN_PLUGIN_MARVELL))
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

   /* Set supported Scan Modes and set sane option. */
   i=0;
   session->scan_mode_list[i] = SANE_VALUE_SCAN_MODE_LINEART;
   session->scan_mode_map[i++] = CE_BLACK_AND_WHITE1;
   session->scan_mode_list[i] = SANE_VALUE_SCAN_MODE_GRAY;
   session->scan_mode_map[i++] = CE_GRAY8;
   session->scan_mode_list[i] = SANE_VALUE_SCAN_MODE_COLOR;
   session->scan_mode_map[i++] = CE_RGB24;
   marvell_control_option(session, MARVELL_OPTION_SCAN_MODE, SANE_ACTION_SET_AUTO, NULL, NULL); /* set default option */


   /* Determine scan input source. */
   i=0;
   /* Some of the marvell devices supports both flatbed and ADF, No command to get the src types supported */
   /* Getting from the model file */
   if ( session->scansrc & HPMUD_SCANSRC_ADF)
   {
         session->input_source_list[i] = STR_ADF_MODE_ADF;
         session->input_source_map[i++] = IS_ADF;
         DBG8("scan src  HPMUD_SCANSRC_ADF \n"); 
   }
   if ( session->scansrc & HPMUD_SCANSRC_FLATBED)
   {
         session->input_source_list[i] = STR_ADF_MODE_FLATBED;
         session->input_source_map[i++] = IS_PLATEN;
         DBG8("scan src  HPMUD_SCANSRC_FLATBED \n");
    }
    /* Values if un specified in the, value is 0,  get ADF state from the printer */   
   if (session->scansrc == HPMUD_SCANSRC_NA)
   {
     if (session->bb_is_paper_in_adf(session) == 2) 
     {
       session->input_source_list[i] = STR_ADF_MODE_FLATBED;
       session->input_source_map[i++] = IS_PLATEN;
       DBG8("scan src  b_is_paper_in_adf value  2 \n");
     }
     else
     {
       session->input_source_list[i] = STR_ADF_MODE_ADF;
       session->input_source_map[i++] = IS_ADF;
       DBG8("scan src  b_is_paper_in_adf value not 2 \n");
     }
    }

   marvell_control_option(session, MARVELL_OPTION_INPUT_SOURCE, SANE_ACTION_SET_AUTO, NULL, NULL); /* set default option */  

   /* Set supported resolutions. */
   set_supported_resolutions(session);
   marvell_control_option(session, MARVELL_OPTION_SCAN_RESOLUTION, SANE_ACTION_SET_AUTO, NULL, NULL); /* set default option */

   /* Set supported contrast. */
   marvell_control_option(session, MARVELL_OPTION_CONTRAST, SANE_ACTION_SET_AUTO, NULL, NULL); /* set default option */

   /* Set supported brightness. */
  marvell_control_option(session, MARVELL_OPTION_BRIGHTNESS, SANE_ACTION_SET_AUTO, NULL, NULL); /* set default option */

   /* Set x,y extents. See bb_open(). */
   marvell_control_option(session, MARVELL_OPTION_TL_X, SANE_ACTION_SET_AUTO, NULL, NULL); /* set default option */
   marvell_control_option(session, MARVELL_OPTION_TL_Y, SANE_ACTION_SET_AUTO, NULL, NULL); /* set default option */
   marvell_control_option(session, MARVELL_OPTION_BR_X, SANE_ACTION_SET_AUTO, NULL, NULL); /* set default option */
   marvell_control_option(session, MARVELL_OPTION_BR_Y, SANE_ACTION_SET_AUTO, NULL, NULL); /* set default option */

   *handle = (SANE_Handle *)session;

   stat = SANE_STATUS_GOOD;

bugout:

   if (stat != SANE_STATUS_GOOD)
   {
      if (session)
      {
         bb_unload(session);
         if (session->cd > 0)
            hpmud_close_channel(session->dd, session->cd);
         if (session->dd > 0)
            hpmud_close_device(session->dd);
         free(session);
         session = NULL;
      }
   }

   return stat;
}

void marvell_close(SANE_Handle handle)
{
   struct marvell_session *ps = (struct marvell_session *)handle;

   DBG8("sane_hpaio_close()\n"); 

   if (ps == NULL || ps != session)
   {
      BUG("invalid sane_close\n");
      return;
   }

   ps->bb_close(ps);
   bb_unload(ps);

   if (ps->dd > 0)
   {
      if (ps->cd > 0)
         hpmud_close_channel(ps->dd, ps->cd);
      hpmud_close_device(ps->dd);
   }
    
   free(ps);
   session = NULL;
}

const SANE_Option_Descriptor *marvell_get_option_descriptor(SANE_Handle handle, SANE_Int option)
{
   struct marvell_session *ps = (struct marvell_session *)handle;

   DBG8("sane_hpaio_get_option_descriptor(option=%s)\n", ps->option[option].name);

   if (option < 0 || option >= MARVELL_OPTION_MAX)
      return NULL;

   return &ps->option[option];
}

SANE_Status marvell_control_option(SANE_Handle handle, SANE_Int option, SANE_Action action, void *value, SANE_Int *set_result)
{
   struct marvell_session *ps = (struct marvell_session *)handle;
   SANE_Int *int_value = value, mset_result=0;
   int i, stat=SANE_STATUS_INVAL;
   char sz[64];

   switch(option)
   {
      case MARVELL_OPTION_COUNT:
         if (action == SANE_ACTION_GET_VALUE)
         {
            *int_value = MARVELL_OPTION_MAX;
            stat = SANE_STATUS_GOOD;
         }
         break;
      case MARVELL_OPTION_SCAN_MODE:
         if (action == SANE_ACTION_GET_VALUE)
         {
            for (i=0; ps->scan_mode_list[i]; i++)
            {
               if (ps->current_scan_mode == ps->scan_mode_map[i])
               {
                  strcpy(value, ps->scan_mode_list[i]);
                  stat = SANE_STATUS_GOOD;
                  break;
               }
            }
         }
         else if (action == SANE_ACTION_SET_VALUE)
         {
            for (i=0; ps->scan_mode_list[i]; i++)
            {
               if (strcasecmp(ps->scan_mode_list[i], value) == 0)
               {
                  ps->current_scan_mode = ps->scan_mode_map[i];
                  mset_result |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
                  stat = SANE_STATUS_GOOD;
                  break;
               }
            }
         }
         else
         {  /* Set default. */
            ps->current_scan_mode = ps->scan_mode_map[0];
            stat = SANE_STATUS_GOOD;
         }
         break;
      case MARVELL_OPTION_INPUT_SOURCE:
         if (action == SANE_ACTION_GET_VALUE)
         {
            for (i=0; ps->input_source_list[i]; i++)
            {
               if (ps->current_input_source == ps->input_source_map[i])
               {
                  strcpy(value, ps->input_source_list[i]);
                  stat = SANE_STATUS_GOOD;
                  break;
               }
            }
         }
         else if (action == SANE_ACTION_SET_VALUE)
         {
            for (i=0; ps->input_source_list[i]; i++)
            {
               if (strcasecmp(ps->input_source_list[i], value) == 0)
               {
                  ps->current_input_source = ps->input_source_map[i];
                  stat = SANE_STATUS_GOOD;
                  if(ps->current_input_source == IS_PLATEN) 
                  {
                    i = ps->platen_resolution_list[0] + 1;
                    while(i--) ps->resolution_list[i] = ps->platen_resolution_list[i];
                  }
                  else
                  {
                     i = ps->adf_resolution_list[0] + 1;
                     while(i--) ps->resolution_list[i] = ps->adf_resolution_list[i];
                  }
                  ps->current_resolution = ps->resolution_list[1];
                  mset_result |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
                  stat = SANE_STATUS_GOOD;
                  break;
               }
            }
         }
         else
         {  /* Set default. */
            ps->current_input_source = ps->input_source_map[0];
            if(ps->current_input_source == IS_PLATEN) 
            {
              i = ps->platen_resolution_list[0] + 1;
              while(i--) ps->resolution_list[i] = ps->platen_resolution_list[i];
            }
            else
            {
              i = ps->adf_resolution_list[0] + 1;
              while(i--) ps->resolution_list[i] = ps->adf_resolution_list[i];
            }
            ps->current_resolution = ps->resolution_list[1];
            mset_result |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
            stat = SANE_STATUS_GOOD;
         }
         break;
      case MARVELL_OPTION_SCAN_RESOLUTION:
         if (action == SANE_ACTION_GET_VALUE)
         {
            *int_value = ps->current_resolution;
            stat = SANE_STATUS_GOOD;
         }
         else if (action == SANE_ACTION_SET_VALUE)
         {
            for (i=1; i <= ps->resolution_list[0]; i++)
            {
               if (ps->resolution_list[i] == *int_value)
               {
                  ps->current_resolution = *int_value;
                  mset_result |= SANE_INFO_RELOAD_PARAMS;
                  stat = SANE_STATUS_GOOD;
                  break;
               }
            }
            if (stat != SANE_STATUS_GOOD)
            {
                ps->current_resolution = ps->resolution_list[1];
                stat = SANE_STATUS_GOOD;
            }
         }
         else
         {  /* Set default. */
            ps->current_resolution = 75;
            stat = SANE_STATUS_GOOD;
         }
         break;
      case MARVELL_OPTION_CONTRAST:
         if (action == SANE_ACTION_GET_VALUE)
         {
            *int_value = ps->current_contrast;
            stat = SANE_STATUS_GOOD;
         }
         else if (action == SANE_ACTION_SET_VALUE)
         {
            if (*int_value >= MARVELL_CONTRAST_MIN && *int_value <= MARVELL_CONTRAST_MAX)
            {
               ps->current_contrast = *int_value;
            }
            else
            {
              ps->current_contrast = MARVELL_CONTRAST_DEFAULT;
            }
            mset_result |= SANE_INFO_RELOAD_PARAMS;
            stat = SANE_STATUS_GOOD;
         }
         else
         {  /* Set default. */
            ps->current_contrast = MARVELL_CONTRAST_DEFAULT;
            stat = SANE_STATUS_GOOD;
         }
         break;
      case MARVELL_OPTION_BRIGHTNESS:
         if (action == SANE_ACTION_GET_VALUE)
         {
            *int_value = ps->currentBrightness;
            stat = SANE_STATUS_GOOD;
         }
         else if (action == SANE_ACTION_SET_VALUE)
         {
            if (*int_value >= MARVELL_BRIGHTNESS_MIN && *int_value <= MARVELL_BRIGHTNESS_MAX)
            {
               ps->currentBrightness = *int_value;
            }
            else
            {
              ps->currentBrightness = MARVELL_BRIGHTNESS_DEFAULT;
            }
            stat = SANE_STATUS_GOOD;
         }
         else
         {  /* Set default. */
            ps->currentBrightness = MARVELL_BRIGHTNESS_DEFAULT;
            stat = SANE_STATUS_GOOD;
         }
         break;
      case MARVELL_OPTION_TL_X:
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
      case MARVELL_OPTION_TL_Y:
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
      case MARVELL_OPTION_BR_X:
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
      case MARVELL_OPTION_BR_Y:
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
            BUG("value=%d brymin=%d brymax=%d\n", *int_value, ps->bryRange.min, ps->bryRange.max);
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
}

SANE_Status marvell_get_parameters(SANE_Handle handle, SANE_Parameters *params)
{
   struct marvell_session *ps = (struct marvell_session *)handle;

   set_extents(ps);

   ps->bb_get_parameters(ps, params, ps->ip_handle ? 1 : 0);

   DBG8("sane_hpaio_get_parameters(): format=%d, last_frame=%d, lines=%d, depth=%d, pixels_per_line=%d, bytes_per_line=%d\n",
                    params->format, params->last_frame, params->lines, params->depth, params->pixels_per_line, params->bytes_per_line);

   return SANE_STATUS_GOOD;
}

SANE_Status marvell_start(SANE_Handle handle)
{
   struct marvell_session *ps = (struct marvell_session *)handle;
   SANE_Parameters pp;
   IP_IMAGE_TRAITS traits;
   IP_XFORM_SPEC xforms[IP_MAX_XFORMS], *pXform=xforms;
   int stat, ret;
//   int tmo=EXCEPTION_TIMEOUT*2;

   DBG8("sane_hpaio_start()\n");
   ps->is_user_cancel = 0;
   
   if (set_extents(ps))
   {
      BUG("invalid extents: tlx=%d brx=%d tly=%d bry=%d minwidth=%d minheight%d maxwidth=%d maxheight=%d\n",
         ps->currentTlx, ps->currentTly, ps->currentBrx, ps->currentBry, ps->min_width, ps->min_height, ps->tlxRange.max, ps->tlyRange.max);
      stat = SANE_STATUS_INVAL;
      goto bugout;
   }   

   /* If input is ADF and ADF is empty, return SANE_STATUS_NO_DOCS. */
   if (ps->current_input_source == IS_ADF)
   {
      ret = ps->bb_is_paper_in_adf(ps);   /* 0 = no paper in adf, 1 = paper in adf, 2 = no adf, -1 = error */
      if (ret == 0)
      {
         stat = SANE_STATUS_NO_DOCS;     /* done scanning */
         SendScanEvent(ps->uri, EVENT_SCAN_ADF_NO_DOCS);
         goto bugout;
      }
      else if (ret < 0)
      {
         stat = SANE_STATUS_IO_ERROR;
         goto bugout;
      }
   }
   /* Start scan and get actual image traits. */
   if (ps->bb_start_scan(ps))
   {
      stat = SANE_STATUS_IO_ERROR;
      goto bugout;
   }

   SendScanEvent(ps->uri, EVENT_START_SCAN_JOB);
   memset(xforms, 0, sizeof(xforms));    

   /* Setup image-processing pipeline for xform. */
   if (ps->current_scan_mode == CE_BLACK_AND_WHITE1)
   {
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
   pXform->aXformInfo[IP_PAD_VALUE].dword = ps->current_scan_mode == CE_BLACK_AND_WHITE1 ? 0 : -1;   /* lineart white = 0, rgb white = -1 */ 
   pXform->aXformInfo[IP_PAD_MIN_HEIGHT].dword = 0;
   ADD_XFORM(X_PAD);

   /* Open image processor. */
   if ((ret = ipOpen(pXform-xforms, xforms, 0, &ps->ip_handle)) != IP_DONE)
   {
      BUG("unable open image processor: err=%d\n", ret);
      stat = SANE_STATUS_INVAL;
      goto bugout;
   }

   /* Get actual input image attributes. See bb_start_scan(). */
   ps->bb_get_parameters(ps, &pp, 1);

   /* Now set known input image attributes. */
   traits.iPixelsPerRow = pp.pixels_per_line;
   switch(ps->current_scan_mode)
   {
      case CE_BLACK_AND_WHITE1:         /* lineart */
      case CE_GRAY8:
         traits.iBitsPerPixel = 8;     /* grayscale */
         break;
      case CE_RGB24:
      default:
         traits.iBitsPerPixel = 24;      /* color */
         break;
   }
   traits.lHorizDPI = ps->current_resolution << 16;
   traits.lVertDPI = ps->current_resolution << 16;
   traits.lNumRows = pp.lines;
   traits.iNumPages = 1;
   traits.iPageNum = 1;
   traits.iComponentsPerPixel = ((traits.iBitsPerPixel % 3) ? 1 : 3);
   ipSetDefaultInputTraits(ps->ip_handle, &traits);

   /* Get output image attributes from the image processor. */
   ipGetImageTraits(ps->ip_handle, NULL, &ps->image_traits);  /* get valid image traits */

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
}

SANE_Status marvell_read(SANE_Handle handle, SANE_Byte *data, SANE_Int maxLength, SANE_Int *length)
{
   struct marvell_session *ps = (struct marvell_session *)handle;
   int ret, stat=SANE_STATUS_IO_ERROR;
//   int tmo=EXCEPTION_TIMEOUT;

   DBG8("sane_hpaio_read() handle=%p data=%p maxLength=%d\n", (void *)handle, data, maxLength);

   ret = get_ip_data(ps, data, maxLength, length);

   if(ret & (IP_INPUT_ERROR | IP_FATAL_ERROR))
   {
      BUG("ipConvert error=%x\n", ret);
      goto bugout;
   }

   if (ret & IP_DONE)
   {
      stat = SANE_STATUS_EOF;
      SendScanEvent(ps->uri, EVENT_END_SCAN_JOB);
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
       //If user has cancelled scan from device
      if (ps->is_user_cancel)
      {
          //Don't do anything. sane_hpaio_cancel() will be invoked automatically
          SendScanEvent(ps->uri, EVENT_SCAN_CANCEL);
          return SANE_STATUS_CANCELLED;

       }
       else 
	   {
	       ps->bb_end_page(ps, stat == SANE_STATUS_IO_ERROR ? 1: 0);
	   }
   }

   DBG8("-sane_hpaio_read() output=%p bytes_read=%d maxLength=%d status=%d\n", data, *length, maxLength, stat);

   return stat;
}

void marvell_cancel(SANE_Handle handle)
{
   struct marvell_session *ps = (struct marvell_session *)handle;

   DBG8("sane_hpaio_cancel()\n"); 

   /*
    * Sane_cancel is always called at the end of the scan job. Note that on a multiple page scan job 
    * sane_cancel is called only once.
    */
   ps->is_user_cancel = 1 ;
   
   if (ps->ip_handle)
   {
      ipClose(ps->ip_handle); 
      ps->ip_handle = 0;
   }
   ps->bb_end_scan(ps, 0);
}

