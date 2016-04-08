/************************************************************************************\
  escl.c - HP SANE backend support for eSCL based multi-function peripherals
  (c) 2012-15 Copyright HP Development Company, LP

  Primary Author: Sarbeswar Meher
  
\************************************************************************************/

# ifndef _GNU_SOURCE
# define _GNU_SOURCE
# endif

# include <stdarg.h>
# include <syslog.h>
# include <stdio.h>
# include <string.h>
# include <dlfcn.h>
# include <fcntl.h>
# include <unistd.h>
# include "saneopts.h"
# include "common.h"
# include "hpmud.h"
# include "hpip.h"
# include "escl.h"
# include "escli.h"
# include "io.h"
#include "utils.h"

# define DEBUG_DECLARE_ONLY
# include "sanei_debug.h"

static struct escl_session *session = NULL;


static int bb_load(struct escl_session *ps, const char *so)
{
   int stat=1;
   
   /* Load hpmud manually with symbols exported. Otherwise the plugin will not find it. */ 
   if ((ps->hpmud_handle = load_library("libhpmud.so.0")) == NULL)
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
    _DBG("Calling bb_open\n");
   if ((ps->bb_close = get_library_symbol(ps->bb_handle, "bb_close")) == NULL)
      goto bugout;

   if ((ps->bb_get_parameters = get_library_symbol(ps->bb_handle, "bb_get_parameters")) == NULL)
      goto bugout;

   if ((ps->bb_check_scanner_to_continue = get_library_symbol(ps->bb_handle, "bb_check_scanner_to_continue")) == NULL)
      goto bugout;

   if ((ps->bb_start_scan = get_library_symbol(ps->bb_handle, "bb_start_scan")) == NULL)
      goto bugout;

   if ((ps->bb_end_scan = get_library_symbol(ps->bb_handle, "bb_end_scan")) == NULL)
      goto bugout;

   if ((ps->bb_get_image_data = get_library_symbol(ps->bb_handle, "bb_get_image_data")) == NULL)
      goto bugout;

   if ((ps->bb_end_page = get_library_symbol(ps->bb_handle, "bb_end_page")) == NULL)
      goto bugout;
   _DBG("Calling bb_load EXIT\n");
   stat=0;

bugout:
   return stat;
}

static int bb_unload(struct escl_session *ps)
{
   _DBG("Calling escl bb_unload: \n");
   if (ps->bb_handle)
   {
      dlclose(ps->bb_handle);
      ps->bb_handle = NULL;
   }
   if (ps->hpmud_handle)
   {
      dlclose(ps->hpmud_handle);
      ps->hpmud_handle = NULL;
   }
   if (ps->math_handle)
   { 
      dlclose(ps->math_handle);
      ps->math_handle = NULL;
   }
   return 0;
}

static int escl_set_extents(struct escl_session *ps)
{
  int stat = 0;
  _DBG("escl_set_extents minWidth=%d minHeight=%d Source Range[%d, %d, %d, %d] Current Range[%d, %d, %d, %d]\n", 
      ps->min_width, ps->min_height,
      ps->tlxRange.max, ps->brxRange.max, ps->tlyRange.max, ps->bryRange.max,
      ps->currentTlx, ps->currentBrx, ps->currentTly, ps->currentBry);
    
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
} /* escl_set_extents */

static struct escl_session *create_session()
{
  struct escl_session *ps;

  if ((ps = malloc(sizeof(struct escl_session))) == NULL)
  {
    return NULL;
  }
  memset(ps, 0, sizeof(struct escl_session));
  ps->tag = "ESCL";
  ps->dd = -1;
  ps->cd = -1;
  memset(ps->job_id, 0, sizeof(ps->job_id));
  ps->page_id = 0;
  return ps;
} /* create_session */

/* Get raw data (ie: uncompressed data) from image processor. */
static int get_ip_data(struct escl_session *ps, SANE_Byte *data, SANE_Int maxLength, SANE_Int *length)
{
  int ip_ret=IP_INPUT_ERROR;
  unsigned int outputAvail=maxLength, outputUsed=0, outputThisPos;
  unsigned char *input, *output = data;
  unsigned int inputAvail, inputUsed=0, inputNextPos;
  enum ESCL_RESULT stat = ESCL_R_IO_ERROR;
   
  _DBG("get_ip_data....\n");  
  if (!ps->ip_handle) goto bugout;
  
  stat = ps->bb_get_image_data(ps, outputAvail);
  if(stat == ESCL_R_IO_ERROR) goto bugout; 

  if (ps->cnt > 0)
  {
    inputAvail = ps->cnt;
    input = &ps->buf[ps->index];
  }
  else
  {
    input = NULL;
    inputAvail = 0;
  }

   /* Transform input data to output. Note, output buffer may consume more bytes than input buffer (ie: jpeg to raster). */
  ip_ret = ipConvert(ps->ip_handle, inputAvail, input, &inputUsed, &inputNextPos, outputAvail, output, &outputUsed, &outputThisPos);

  _DBG("ip_ret=%x cnt=%d index=%d input=%p inputAvail=%d inputUsed=%d inputNextPos=%d output=%p outputAvail=%d outputUsed=%d outputThisPos=%d\n", ip_ret, ps->cnt, ps->index, input, 
    inputAvail, inputUsed, inputNextPos, output, outputAvail, outputUsed, outputThisPos);
    
  if (input != NULL)
   {
      if (inputAvail == inputUsed)
      {
         ps->index = ps->cnt = 0;   //
      }
      else
      {
         ps->cnt -= inputUsed;    // save left over buffer for next soap_read
         ps->index += inputUsed;
      }
   }

   if (data)
      *length = outputUsed;

  /* For sane do not send output data simultaneously with IP_DONE. */
   if (ip_ret & IP_DONE && outputUsed)
      ip_ret &= ~IP_DONE;                               

bugout:
  _DBG("get_ip_data returning (%d).\n", ip_ret);
   return ip_ret;
} /* get_ip_data */


static int set_scan_mode_side_effects(struct escl_session *ps, enum COLOR_ENTRY scanMode)
{
   int j=0;

   _DBG("set_scan_mode_side_effects....\n"); 
   memset(ps->compressionList, 0, sizeof(ps->compressionList));
   memset(ps->compressionMap, 0, sizeof(ps->compressionMap));

   switch (scanMode)
   {
      case CE_K1:         /* same as GRAY8 */
      case CE_GRAY8:
      case CE_COLOR8:
      default:
         ps->compressionList[j] = STR_COMPRESSION_JPEG;
         ps->compressionMap[j++] = SF_JPEG;
         ps->currentCompression = SF_JPEG;
         ps->option[ESCL_OPTION_JPEG_QUALITY].cap |= SANE_CAP_SOFT_SELECT;   /* enable jpeg quality */
         break;
   }
   return 0;
} /* set_scan_mode_side_effects */

static int set_input_source_side_effects(struct escl_session *ps, enum INPUT_SOURCE source)
{

  _DBG("set_input_source_side_effects....\n"); 
   switch (source)
   {
      case IS_CAMERA:
         ps->min_width = ps->camera_min_width;
         ps->min_height = ps->camera_min_height;
         ps->tlxRange.max = ps->camera_tlxRange.max;
         ps->brxRange.max = ps->camera_brxRange.max;
         ps->tlyRange.max = ps->camera_tlyRange.max;
         ps->bryRange.max = ps->camera_bryRange.max;
         break;
      case IS_ADF:
         ps->min_width = ps->adf_min_width;
         ps->min_height = ps->adf_min_height;
         ps->tlxRange.max = ps->adf_tlxRange.max;
         ps->brxRange.max = ps->adf_brxRange.max;
         ps->tlyRange.max = ps->adf_tlyRange.max;
         ps->bryRange.max = ps->adf_bryRange.max;
         break;
      case IS_ADF_DUPLEX:
         ps->min_width = ps->duplex_min_width;
         ps->min_height = ps->duplex_min_height;
         ps->tlxRange.max = ps->duplex_tlxRange.max;
         ps->brxRange.max = ps->duplex_brxRange.max;
         ps->tlyRange.max = ps->duplex_tlyRange.max;
         ps->bryRange.max = ps->duplex_bryRange.max;
         break;
      case IS_PLATEN:
      default:
         ps->min_width = ps->platen_min_width;
         ps->min_height = ps->platen_min_height;
         ps->tlxRange.max = ps->platen_tlxRange.max;
         ps->brxRange.max = ps->platen_brxRange.max;
         ps->tlyRange.max = ps->platen_tlyRange.max;
         ps->bryRange.max = ps->platen_bryRange.max;
         break;
   }
   return 0;
} /* set_input_source_side_effects */

static int init_options(struct escl_session *ps)
{
  _DBG("init_options....\n"); 
  ps->option[ESCL_OPTION_COUNT].name = "option-cnt";
  ps->option[ESCL_OPTION_COUNT].title = SANE_TITLE_NUM_OPTIONS;
  ps->option[ESCL_OPTION_COUNT].desc = SANE_DESC_NUM_OPTIONS;
  ps->option[ESCL_OPTION_COUNT].type = SANE_TYPE_INT;
  ps->option[ESCL_OPTION_COUNT].unit = SANE_UNIT_NONE;
  ps->option[ESCL_OPTION_COUNT].size = sizeof(SANE_Int);
  ps->option[ESCL_OPTION_COUNT].cap = SANE_CAP_SOFT_DETECT;
  ps->option[ESCL_OPTION_COUNT].constraint_type = SANE_CONSTRAINT_NONE;
  
  ps->option[ESCL_OPTION_GROUP_SCAN_MODE].name = "mode-group";
  ps->option[ESCL_OPTION_GROUP_SCAN_MODE].title = SANE_TITLE_SCAN_MODE;
  ps->option[ESCL_OPTION_GROUP_SCAN_MODE].type = SANE_TYPE_GROUP;

  ps->option[ESCL_OPTION_SCAN_MODE].name = SANE_NAME_SCAN_MODE;
  ps->option[ESCL_OPTION_SCAN_MODE].title = SANE_TITLE_SCAN_MODE;
  ps->option[ESCL_OPTION_SCAN_MODE].desc = SANE_DESC_SCAN_MODE;
  ps->option[ESCL_OPTION_SCAN_MODE].type = SANE_TYPE_STRING;
  ps->option[ESCL_OPTION_SCAN_MODE].unit = SANE_UNIT_NONE;
  ps->option[ESCL_OPTION_SCAN_MODE].size = MAX_STRING_SIZE;
  ps->option[ESCL_OPTION_SCAN_MODE].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  ps->option[ESCL_OPTION_SCAN_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  ps->option[ESCL_OPTION_SCAN_MODE].constraint.string_list = ps->scanModeList;

  ps->option[ESCL_OPTION_INPUT_SOURCE].name = SANE_NAME_SCAN_SOURCE;
  ps->option[ESCL_OPTION_INPUT_SOURCE].title = SANE_TITLE_SCAN_SOURCE;
  ps->option[ESCL_OPTION_INPUT_SOURCE].desc = SANE_DESC_SCAN_SOURCE;
  ps->option[ESCL_OPTION_INPUT_SOURCE].type = SANE_TYPE_STRING;
  ps->option[ESCL_OPTION_INPUT_SOURCE].unit = SANE_UNIT_NONE;
  ps->option[ESCL_OPTION_INPUT_SOURCE].size = MAX_STRING_SIZE;
  ps->option[ESCL_OPTION_INPUT_SOURCE].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  ps->option[ESCL_OPTION_INPUT_SOURCE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  ps->option[ESCL_OPTION_INPUT_SOURCE].constraint.string_list = ps->inputSourceList;

  ps->option[ESCL_OPTION_SCAN_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
  ps->option[ESCL_OPTION_SCAN_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
  ps->option[ESCL_OPTION_SCAN_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
  ps->option[ESCL_OPTION_SCAN_RESOLUTION].type = SANE_TYPE_INT;
  ps->option[ESCL_OPTION_SCAN_RESOLUTION].unit = SANE_UNIT_DPI;
  ps->option[ESCL_OPTION_SCAN_RESOLUTION].size = sizeof(SANE_Int);
  ps->option[ESCL_OPTION_SCAN_RESOLUTION].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  ps->option[ESCL_OPTION_SCAN_RESOLUTION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  ps->option[ESCL_OPTION_SCAN_RESOLUTION].constraint.word_list = ps->resolutionList;

  ps->option[ESCL_OPTION_GROUP_ADVANCED].name = "advanced-group";
  ps->option[ESCL_OPTION_GROUP_ADVANCED].title = STR_TITLE_ADVANCED;
  ps->option[ESCL_OPTION_GROUP_ADVANCED].type = SANE_TYPE_GROUP;
  ps->option[ESCL_OPTION_GROUP_ADVANCED].cap = SANE_CAP_ADVANCED;

  ps->option[ESCL_OPTION_COMPRESSION].name = STR_NAME_COMPRESSION;
  ps->option[ESCL_OPTION_COMPRESSION].title = STR_TITLE_COMPRESSION;
  ps->option[ESCL_OPTION_COMPRESSION].desc = STR_DESC_COMPRESSION;
  ps->option[ESCL_OPTION_COMPRESSION].type = SANE_TYPE_STRING;
  ps->option[ESCL_OPTION_COMPRESSION].unit = SANE_UNIT_NONE;
  ps->option[ESCL_OPTION_COMPRESSION].size = MAX_STRING_SIZE;
  ps->option[ESCL_OPTION_COMPRESSION].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
  ps->option[ESCL_OPTION_COMPRESSION].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  ps->option[ESCL_OPTION_COMPRESSION].constraint.string_list = ps->compressionList;

  ps->option[ESCL_OPTION_JPEG_QUALITY].name = STR_NAME_JPEG_QUALITY;
  ps->option[ESCL_OPTION_JPEG_QUALITY].title = STR_TITLE_JPEG_QUALITY;
  ps->option[ESCL_OPTION_JPEG_QUALITY].desc = STR_DESC_JPEG_QUALITY;
  ps->option[ESCL_OPTION_JPEG_QUALITY].type = SANE_TYPE_INT;
  ps->option[ESCL_OPTION_JPEG_QUALITY].unit = SANE_UNIT_NONE;
  ps->option[ESCL_OPTION_JPEG_QUALITY].size = sizeof(SANE_Int);
  ps->option[ESCL_OPTION_JPEG_QUALITY].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
  ps->option[ESCL_OPTION_JPEG_QUALITY].constraint_type = SANE_CONSTRAINT_RANGE;
  ps->option[ESCL_OPTION_JPEG_QUALITY].constraint.range = &ps->jpegQualityRange;
  ps->jpegQualityRange.min = MIN_JPEG_COMPRESSION_FACTOR;
  ps->jpegQualityRange.max = MAX_JPEG_COMPRESSION_FACTOR;
  ps->jpegQualityRange.quant = 0;

  ps->option[ESCL_OPTION_GROUP_GEOMETRY].name = "geometry-group";
  ps->option[ESCL_OPTION_GROUP_GEOMETRY].title = STR_TITLE_GEOMETRY;
  ps->option[ESCL_OPTION_GROUP_GEOMETRY].type = SANE_TYPE_GROUP;
  ps->option[ESCL_OPTION_GROUP_GEOMETRY].cap = SANE_CAP_ADVANCED;

  ps->option[ESCL_OPTION_TL_X].name = SANE_NAME_SCAN_TL_X;
  ps->option[ESCL_OPTION_TL_X].title = SANE_TITLE_SCAN_TL_X;
  ps->option[ESCL_OPTION_TL_X].desc = SANE_DESC_SCAN_TL_X;
  ps->option[ESCL_OPTION_TL_X].type = SANE_TYPE_FIXED;
  ps->option[ESCL_OPTION_TL_X].unit = SANE_UNIT_MM;
  ps->option[ESCL_OPTION_TL_X].size = sizeof(SANE_Int);
  ps->option[ESCL_OPTION_TL_X].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  ps->option[ESCL_OPTION_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
  ps->option[ESCL_OPTION_TL_X].constraint.range = &ps->tlxRange;
  ps->tlxRange.min = 0;
  ps->tlxRange.quant = 0;

  ps->option[ESCL_OPTION_TL_Y].name = SANE_NAME_SCAN_TL_Y;
  ps->option[ESCL_OPTION_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
  ps->option[ESCL_OPTION_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
  ps->option[ESCL_OPTION_TL_Y].type = SANE_TYPE_FIXED;
  ps->option[ESCL_OPTION_TL_Y].unit = SANE_UNIT_MM;
  ps->option[ESCL_OPTION_TL_Y].size = sizeof(SANE_Int);
  ps->option[ESCL_OPTION_TL_Y].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  ps->option[ESCL_OPTION_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  ps->option[ESCL_OPTION_TL_Y].constraint.range = &ps->tlyRange;
  ps->tlyRange.min = 0;
  ps->tlyRange.quant = 0;

  ps->option[ESCL_OPTION_BR_X].name = SANE_NAME_SCAN_BR_X;
  ps->option[ESCL_OPTION_BR_X].title = SANE_TITLE_SCAN_BR_X;
  ps->option[ESCL_OPTION_BR_X].desc = SANE_DESC_SCAN_BR_X;
  ps->option[ESCL_OPTION_BR_X].type = SANE_TYPE_FIXED;
  ps->option[ESCL_OPTION_BR_X].unit = SANE_UNIT_MM;
  ps->option[ESCL_OPTION_BR_X].size = sizeof(SANE_Int);
  ps->option[ESCL_OPTION_BR_X].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  ps->option[ESCL_OPTION_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
  ps->option[ESCL_OPTION_BR_X].constraint.range = &ps->brxRange;
  ps->brxRange.min = 0;
  ps->brxRange.quant = 0;

  ps->option[ESCL_OPTION_BR_Y].name = SANE_NAME_SCAN_BR_Y;
  ps->option[ESCL_OPTION_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
  ps->option[ESCL_OPTION_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
  ps->option[ESCL_OPTION_BR_Y].type = SANE_TYPE_FIXED;
  ps->option[ESCL_OPTION_BR_Y].unit = SANE_UNIT_MM;
  ps->option[ESCL_OPTION_BR_Y].size = sizeof(SANE_Int);
  ps->option[ESCL_OPTION_BR_Y].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  ps->option[ESCL_OPTION_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  ps->option[ESCL_OPTION_BR_Y].constraint.range = &ps->bryRange;
  ps->bryRange.min = 0;
  ps->bryRange.quant = 0;

return 0;
} /* init_options */

/*----------------------------------- ESCL API Calls ------------------------------------*/

SANE_Status __attribute__ ((visibility ("hidden"))) escl_open(SANE_String_Const device, SANE_Handle *handle)
{
  struct hpmud_model_attributes ma;
  int stat = SANE_STATUS_IO_ERROR;

  _DBG("escl_open() session=%p\n", session);
  if(session)
  {
    return SANE_STATUS_DEVICE_BUSY;
  }
  if((session = create_session()) == NULL)
    return SANE_STATUS_NO_MEM;

  /* Set session to specified device. */
  snprintf(session->uri, sizeof(session->uri)-1, "hp:%s", device);   /* prepend "hp:" */

  /* Get actual model attributes from models.dat */
  hpmud_query_model(session->uri, &ma);
  session->scan_type = ma.scantype;

  if (hpmud_open_device(session->uri, ma.mfp_mode, &session->dd) != HPMUD_R_OK)
     goto bugout;

  if (bb_load(session, SCAN_PLUGIN_ESCL))
      goto bugout;
   _DBG("escl_open() calling %s PASSED\n", SCAN_PLUGIN_ESCL);
  init_options(session);

  if (session->bb_open (session))
     goto bugout;
  
  /* Set supported Scan Modes as determined by bb_open. */
   escl_control_option(session, ESCL_OPTION_SCAN_MODE, SANE_ACTION_SET_AUTO, NULL, NULL); /* set default option */

  /* Set scan input sources as determined by bb_open. */
   escl_control_option(session, ESCL_OPTION_INPUT_SOURCE, SANE_ACTION_SET_AUTO, NULL, NULL); /* set default option */

  /* Set supported resolutions. */
  escl_control_option(session, ESCL_OPTION_SCAN_RESOLUTION, SANE_ACTION_SET_AUTO, NULL, NULL); /* set default option */

  /* Set supported compression  */
  escl_control_option(session, ESCL_OPTION_COMPRESSION, SANE_ACTION_SET_AUTO, NULL, NULL); /* set default option */

  /* Determine supported jpeg quality factor as determined by bb_open. */
  escl_control_option(session, ESCL_OPTION_JPEG_QUALITY, SANE_ACTION_SET_AUTO, NULL, NULL); /* set default option */

  /* Set x,y extents. See bb_open */
  escl_control_option(session, ESCL_OPTION_TL_X, SANE_ACTION_SET_AUTO, NULL, NULL); /* set default option */
  escl_control_option(session, ESCL_OPTION_TL_Y, SANE_ACTION_SET_AUTO, NULL, NULL); /* set default option */
  escl_control_option(session, ESCL_OPTION_BR_X, SANE_ACTION_SET_AUTO, NULL, NULL); /* set default option */
  escl_control_option(session, ESCL_OPTION_BR_Y, SANE_ACTION_SET_AUTO, NULL, NULL); /* set default option */
  
  *handle = (SANE_Handle *)session;

  stat = SANE_STATUS_GOOD;

bugout:
  if(stat != SANE_STATUS_GOOD)
  {
     bb_unload(session);
     if (session->cd > 0)
        hpmud_close_channel(session->dd, session->cd);
     if (session->dd > 0)
        hpmud_close_device(session->dd);
     free(session);
     session = NULL;
  }
  return stat;
} /* escl_open */

const SANE_Option_Descriptor *escl_get_option_descriptor(SANE_Handle handle, SANE_Int option)
{
  struct escl_session *ps = (struct escl_session *)handle;

 // DBG8("sane_hpaio_get_option_descriptor(option=%s) \n", ps->option[option].name);

  if (option < 0 || option >= ESCL_OPTION_MAX)
    return NULL;

  return &ps->option[option];
} /* escl_get_option_descriptor */

SANE_Status escl_control_option(SANE_Handle handle, SANE_Int option, SANE_Action action, void *value, SANE_Int *set_result)
{
  struct escl_session *ps = (struct escl_session *)handle;
  SANE_Int *int_value = value, mset_result=0;
  int i, stat=SANE_STATUS_INVAL;
  int found = 0;

  switch(option)
  {
    case ESCL_OPTION_COUNT:
      if (action == SANE_ACTION_GET_VALUE)
      {
        *int_value = ESCL_OPTION_MAX;
        stat = SANE_STATUS_GOOD;
      }
      break;
    case ESCL_OPTION_SCAN_MODE:
      if(action == SANE_ACTION_GET_VALUE)
      {
        for(i=0; ps->scanModeList[i]; i++)
        {
          if(ps->currentScanMode == ps->scanModeMap[i])
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
        ps->currentScanMode = CE_COLOR8;
        set_scan_mode_side_effects(ps, ps->currentScanMode);
        stat = SANE_STATUS_GOOD;
      }
      break;
      case ESCL_OPTION_INPUT_SOURCE:
         if (action == SANE_ACTION_GET_VALUE)
         {
            for (i=0; ps->inputSourceList[i]; i++)
            {
               if (ps->currentInputSource == ps->inputSourceMap[i])
               {
                  strcpy(value, ps->inputSourceList[i]);
                  stat = SANE_STATUS_GOOD;
                  break;
               }
            }
         }
         else if (action == SANE_ACTION_SET_VALUE)
         {
           for (i=0; ps->inputSourceList[i]; i++)
           {
             if (strcasecmp(ps->inputSourceList[i], value) == 0)
             {
               ps->currentInputSource = ps->inputSourceMap[i];
               set_input_source_side_effects(ps, ps->currentInputSource);
               if(ps->currentInputSource == IS_PLATEN) 
               {
                 i = session->platen_resolutionList[0] + 1;
                 while(i--) 
                 {
                    session->resolutionList[i] = session->platen_resolutionList[i];
                    if(session->resolutionList[i] == ps->currentResolution)
                        found = 1;
                 }
               }
               else if(ps->currentInputSource == IS_ADF)
               {
                 i = session->adf_resolutionList[0] + 1;
                 while(i--) 
                 {
                    session->resolutionList[i] = session->adf_resolutionList[i];
                    if(session->resolutionList[i] == ps->currentResolution)
                        found = 1;
                 }
               }
               else if(ps->currentInputSource == IS_ADF_DUPLEX)
               {
                 i = session->duplex_resolutionList[0] + 1;
                 while(i--) 
                 {
                    session->resolutionList[i] = session->duplex_resolutionList[i];
                    if(session->resolutionList[i] == ps->currentResolution)
                        found = 1;
                 }
               }
               else if(ps->currentInputSource == IS_CAMERA)
               {
                  i = session->camera_resolutionList[0] + 1;
                  while(i--) session->resolutionList[i] = session->camera_resolutionList[i];
               }
               
               if(found == 0) 
               {             
                  _DBG("Resolution (%d) is not supported in input source (%d).\n", ps->currentResolution, ps->currentInputSource);
                  ps->currentResolution = session->resolutionList[1];   
               }

               mset_result |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
               stat = SANE_STATUS_GOOD;
               break;
             }
           }
         }
         else
         {  /* Set default. */
           ps->currentInputSource = IS_PLATEN;
           set_input_source_side_effects(ps, ps->currentInputSource);
           mset_result |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
           stat = SANE_STATUS_GOOD;
         }
         break;
      case ESCL_OPTION_SCAN_RESOLUTION:
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
    	          if(ps->currentResolution == 4800) SendScanEvent(ps->uri, EVENT_SIZE_WARNING);
                  mset_result |= SANE_INFO_RELOAD_PARAMS;
                  stat = SANE_STATUS_GOOD;
                  break;
               }
            }
         }
         else
         {  /* Set default. */
            ps->currentResolution = 75;
            stat = SANE_STATUS_GOOD;
         }
         break;
      case ESCL_OPTION_COMPRESSION:
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
            ps->currentCompression = SF_JPEG;
            stat = SANE_STATUS_GOOD;
         }
         break;
      case ESCL_OPTION_JPEG_QUALITY:
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
      case ESCL_OPTION_TL_X:
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
      case ESCL_OPTION_TL_Y:
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
      case ESCL_OPTION_BR_X:
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
      case ESCL_OPTION_BR_Y:
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
  //DBG8("escl_control_option (option=%s) action=%d\n", ps->option[option].name, action);
  if (stat != SANE_STATUS_GOOD)
  {
     BUG("control_option failed: option=%s action=%s\n", ps->option[option].name, action==SANE_ACTION_GET_VALUE ? "get" : action==SANE_ACTION_SET_VALUE ? "set" : "auto");
  }

   return stat;
} /* escl_control_option */

SANE_Status escl_get_parameters(SANE_Handle handle, SANE_Parameters *params)
{
  struct escl_session *ps = (struct escl_session *)handle;

  escl_set_extents(ps);

  /* Get scan parameters for sane client. */
  ps->bb_get_parameters(ps, params, ps->ip_handle ? SPO_STARTED : SPO_BEST_GUESS);

  /*_DBG("sane_hpaio_get_parameters(): format=%d, last_frame=%d, lines=%d, depth=%d, pixels_per_line=%d, bytes_per_line=%d\n",
    params->format, params->last_frame, params->lines, params->depth, params->pixels_per_line, params->bytes_per_line);
    */
  return SANE_STATUS_GOOD;
} /* escl_get_parameters */

static void escl_send_event(struct escl_session *ps, SANE_Status stat)
{
   int event = 0;
   switch(stat)
   {
   case SANE_STATUS_NO_DOCS:
     event =  EVENT_SCAN_ADF_NO_DOCS;
     break;
   case SANE_STATUS_JAMMED:
     event =  EVENT_SCAN_ADF_JAM;
     break;
   case SANE_STATUS_DEVICE_BUSY:
     event = EVENT_SCAN_BUSY;
     break;
   case SANE_STATUS_UNSUPPORTED:
     event = EVENT_SCAN_ADF_MISPICK;
     break;
   case SANE_STATUS_CANCELLED:
     event = EVENT_SCAN_CANCEL;
     break;
   case SANE_STATUS_ACCESS_DENIED:
   case SANE_STATUS_INVAL:
   case SANE_STATUS_NO_MEM:
     break;
   case SANE_STATUS_GOOD:
   default:
     break;
   }
   
   SendScanEvent (ps->uri, event);
   _DBG("escl_send_event event[%d] uri[%s]\n", event, ps->uri);
}

SANE_Status escl_start(SANE_Handle handle)
{
  struct escl_session *ps = (struct escl_session *)handle;
  SANE_Parameters pp;
  IP_IMAGE_TRAITS traits;
  IP_XFORM_SPEC xforms[IP_MAX_XFORMS], *pXform=xforms;
  SANE_Status stat = SANE_STATUS_IO_ERROR;
  int ret;
  
  _DBG("escl_start entry. uri=[%s] InputSource=[%d]\n", ps->uri, ps->currentInputSource);

  ps -> user_cancel = 0;
  ps -> cnt = 0;
  ps -> index = 0;

  set_input_source_side_effects(ps, ps->currentInputSource);
  if (escl_set_extents(ps))
  {
    stat = SANE_STATUS_INVAL;
    goto bugout;
  }

  stat = ps->bb_check_scanner_to_continue(ps);

  escl_send_event(ps, stat);
  if(stat != SANE_STATUS_GOOD) return stat;

   /* Start scan and get actual image traits. */
   stat = ps->bb_start_scan(ps);

  if (stat != SANE_STATUS_GOOD) return stat;

  if(ps->user_cancel)
  {
    stat = SANE_STATUS_GOOD ;
    goto bugout;
  }
   
  SendScanEvent(ps->uri, EVENT_START_SCAN_JOB);
  _DBG("escl_start() EVENT_START_SCAN_JOB uri=[%s]\n", ps->uri);
  memset(xforms, 0, sizeof(xforms));

  /* Setup image-processing pipeline for xform. */
  if (ps->currentScanMode == CE_COLOR8 || ps->currentScanMode == CE_GRAY8)
  {
    switch(ps->currentCompression)
    {
      case SF_JPEG:
        pXform->aXformInfo[IP_JPG_DECODE_FROM_DENALI].dword = 0;    /* 0=no */
        ADD_XFORM(X_JPG_DECODE);
        pXform->aXformInfo[IP_CNV_COLOR_SPACE_WHICH_CNV].dword = IP_CNV_YCC_TO_SRGB;
        pXform->aXformInfo[IP_CNV_COLOR_SPACE_GAMMA].dword = 0x00010000;
        ADD_XFORM(X_CNV_COLOR_SPACE);
        break;
      case SF_RAW:
      default:
        break;
    }
  }
  else
  {  /* Must be BLACK_AND_WHITE1 (Lineart). */
    switch(ps->currentCompression)
    {
      case SF_JPEG:
        pXform->aXformInfo[IP_JPG_DECODE_FROM_DENALI].dword = 0;    /* 0=no */
        ADD_XFORM(X_JPG_DECODE);
        pXform->aXformInfo[IP_GRAY_2_BI_THRESHOLD].dword = 127;
        ADD_XFORM(X_GRAY_2_BI);
        break;
      case SF_RAW:
        pXform->aXformInfo[IP_GRAY_2_BI_THRESHOLD].dword = 127;
        ADD_XFORM(X_GRAY_2_BI);
      default:
        break;
    }
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
  pXform->aXformInfo[IP_PAD_VALUE].dword = ps->currentScanMode == CE_K1 ? 0 : -1;   /* lineart white = 0, rgb white = -1 */ 
  pXform->aXformInfo[IP_PAD_MIN_HEIGHT].dword = 0;
  ADD_XFORM(X_PAD);

  /* Open image processor. */
  if ((ret = ipOpen(pXform-xforms, xforms, 0, &ps->ip_handle)) != IP_DONE)
  {
    stat = SANE_STATUS_INVAL;
    goto bugout;
  }

  /* Get scan parameters for image processor. */
  if (ps->currentCompression == SF_RAW)
    ps->bb_get_parameters(ps, &pp, SPO_STARTED_JR);     /* hpraw, use actual parameters */
  else
    ps->bb_get_parameters(ps, &pp, SPO_BEST_GUESS);     /* jpeg, use best guess */
  traits.iPixelsPerRow = pp.pixels_per_line;
  switch(ps->currentScanMode)
  {
    case CE_K1:                      /* lineart (let IP create Mono from Gray8) */
      traits.iBitsPerPixel = 1;
      break;
    case CE_GRAY8:
      traits.iBitsPerPixel = 8;     /* grayscale */
      break;
    case CE_COLOR8:
    default:
      traits.iBitsPerPixel = 24;    /* color */
      break;
  }
  traits.lHorizDPI = ps->currentResolution << 16;
  traits.lVertDPI = ps->currentResolution << 16;
  traits.lNumRows =  pp.lines;
  traits.iNumPages = 1;
  traits.iPageNum = 1;
  traits.iComponentsPerPixel = ((traits.iBitsPerPixel % 3) ? 1 : 3);
  ipSetDefaultInputTraits(ps->ip_handle, &traits);
  _DBG("escl_start() ipSetDefaultInputTraits lines=%ld pixels_per_line=%d\n", traits.lNumRows, traits.iPixelsPerRow);
  
  /* If jpeg get output image attributes from the image processor. */
  if (ps->currentCompression == SF_JPEG)
  {
    /* Enable parsed header flag. */
    ipResultMask(ps->ip_handle, IP_PARSED_HEADER);
    _DBG("escl_start() before get_ip_data\n");
    /* Wait for image processor to process header so we know the exact size of the image for sane_get_params. */
    while (1)
    {
      ret = get_ip_data(ps, NULL, 0, NULL);
      if (ret & (IP_INPUT_ERROR | IP_FATAL_ERROR))
      {
        _DBG("escl_start() Inside whileSANE_STATUS_IO_ERROR****\n");
        stat = SANE_STATUS_IO_ERROR;
        goto bugout; 
      }
      else if (ret &  IP_DONE)
      {
        _DBG("escl_start() Inside while SANE_STATUS_EOF****\n");
        stat = SANE_STATUS_EOF;
        goto bugout;
      }

      if (ret & IP_PARSED_HEADER)
      {
        _DBG("escl_start() Inside while  IP_PARSED_HEADER****\n");
        ipGetImageTraits(ps->ip_handle, NULL, &ps->image_traits);  /* get valid image traits */
        _DBG("escl_start() ipGetImageTraits lines=%ld pixels_per_line=%d\n", ps->image_traits.lNumRows, ps->image_traits.iPixelsPerRow);
        ipResultMask(ps->ip_handle, 0);                          /* disable parsed header flag */
        break;
      }
    }
  }
  else
   {
     ipGetImageTraits(ps->ip_handle, NULL, &ps->image_traits);  /* get valid image traits */
     _DBG("escl_start() ipGetImageTraits lines=%ld pixels_per_line=%d\n", ps->image_traits.lNumRows, ps->image_traits.iPixelsPerRow);
   }

  stat = SANE_STATUS_GOOD;

bugout:
  _DBG("escl_start() returning stat=%d****\n", stat);
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
} /* escl_start */

SANE_Status escl_read(SANE_Handle handle, SANE_Byte *data, SANE_Int maxLength, SANE_Int *length)
{
  struct escl_session *ps = (struct escl_session *)handle;
  int ret, stat=SANE_STATUS_IO_ERROR;

  _DBG("escl_read entry (ps->user_cancel = %d)....\n", ps->user_cancel);

  if(ps->user_cancel)
  {
    _DBG("escl_read() EVENT_SCAN_CANCEL****uri=[%s]\n", ps->uri);
    SendScanEvent(ps->uri, EVENT_SCAN_CANCEL);
    return SANE_STATUS_CANCELLED;
  }

  ret = get_ip_data(ps, data, maxLength, length);

  if(ret & (IP_INPUT_ERROR | IP_FATAL_ERROR))
  {
     goto bugout;
   }

  if(ret == IP_DONE)
  {
    stat = SANE_STATUS_EOF;
    SendScanEvent(ps->uri, EVENT_END_SCAN_JOB);
    _DBG("escl_read() EVENT_END_SCAN_JOB uri=%s\n", ps->uri);
  } 
  else stat= SANE_STATUS_GOOD;

bugout:
  _DBG("escl_read() returning stat=[%d]\n", stat);
  if (stat != SANE_STATUS_GOOD)
  {
    if (ps->ip_handle)
    {
      /* Note always call ipClose when SANE_STATUS_EOF, do not depend on sane_cancel because sane_cancel is only called at the end of a batch job. */ 
      ipClose(ps->ip_handle);  
      ps->ip_handle = 0;
    } 
    ps->bb_end_page(ps, stat);
  }

  DBG8("-sane_hpaio_read() output=%p bytes_read=%d maxLength=%d status=%d\n", data, *length, maxLength, stat);

  return stat;
} /* escl_read */

void escl_cancel(SANE_Handle handle)
{
  struct escl_session *ps = (struct escl_session *)handle;

  _DBG("escl_cancel...\n"); 

  ps->user_cancel = 1;
  /* Sane_cancel is always called at the end of the scan job. 
  Note that on a multiple page scan job sane_cancel is called only once */
  if (ps->ip_handle)
  {
    ipClose(ps->ip_handle); 
    ps->ip_handle = 0;
  }
  ps->bb_end_scan(ps, 1);
} /* escl_cancel */

void escl_close(SANE_Handle handle)
{
  struct escl_session *ps = (struct escl_session *)handle;

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
} /* escl_close */
