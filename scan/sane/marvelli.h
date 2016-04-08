
/************************************************************************************\

  marvelli.h - HP SANE backend support for Marvell based multi-function peripherals

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

#ifndef _MARVELLI_H
#define _MARVELLI_H

#define MARVELL_CONTRAST_MIN 1
#define MARVELL_CONTRAST_MAX 11
#define MARVELL_CONTRAST_DEFAULT 6
#define MARVELL_BRIGHTNESS_MIN 0
#define MARVELL_BRIGHTNESS_MAX 200
#define MARVELL_BRIGHTNESS_DEFAULT 6

#define MM_PER_INCH     25.4

enum MARVELL_OPTION_NUMBER
{ 
   MARVELL_OPTION_COUNT = 0,
   MARVELL_OPTION_GROUP_SCAN_MODE,
                   MARVELL_OPTION_SCAN_MODE,
                   MARVELL_OPTION_SCAN_RESOLUTION,
                   MARVELL_OPTION_INPUT_SOURCE,     /* platen, ADF */ 
   MARVELL_OPTION_GROUP_ADVANCED,
                   MARVELL_OPTION_CONTRAST,
                   MARVELL_OPTION_BRIGHTNESS,
   MARVELL_OPTION_GROUP_GEOMETRY,
                   MARVELL_OPTION_TL_X,
                   MARVELL_OPTION_TL_Y,
                   MARVELL_OPTION_BR_X,
                   MARVELL_OPTION_BR_Y,
   MARVELL_OPTION_MAX
};

#define MAX_LIST_SIZE 32
#define MAX_STRING_SIZE 32

enum COLOR_ENTRY
{
   CE_BLACK_AND_WHITE1 = 1,
   CE_GRAY8, 
   CE_RGB24, 
   CE_MAX,
};

enum INPUT_SOURCE 
{
   IS_PLATEN = 1,
   IS_ADF,
   IS_MAX,
};

enum MARVELL_VERSION
{
    MARVELL_1 = 1,
    MARVELL_2,
};

struct marvell_session
{
   char *tag;  /* handle identifier */
   HPMUD_DEVICE dd;  /* hpiod device descriptor */
   HPMUD_CHANNEL cd;  /* hpiod soap channel descriptor */
   char uri[HPMUD_LINE_SIZE];
   char model[HPMUD_LINE_SIZE];
   int scan_type;
   int is_user_cancel;
   IP_IMAGE_TRAITS image_traits;   /* specified by image processor */      

   SANE_Option_Descriptor option[MARVELL_OPTION_MAX];

   SANE_String_Const scan_mode_list[CE_MAX];
   enum COLOR_ENTRY scan_mode_map[CE_MAX];
   enum COLOR_ENTRY current_scan_mode;

   SANE_String_Const input_source_list[IS_MAX];
   enum INPUT_SOURCE input_source_map[IS_MAX];
   enum INPUT_SOURCE current_input_source;

   SANE_Int resolution_list[MAX_LIST_SIZE];
   SANE_Int current_resolution;

   SANE_Range contrast_range;
   SANE_Int current_contrast;
   SANE_Range brightnessRange;
   SANE_Int currentBrightness;

   SANE_Range tlxRange, tlyRange, brxRange, bryRange;
   SANE_Fixed currentTlx, currentTly, currentBrx, currentBry;
   SANE_Fixed effectiveTlx, effectiveTly, effectiveBrx, effectiveBry;
   SANE_Fixed min_width, min_height;

    SANE_Int platen_resolution_list[MAX_LIST_SIZE];
    SANE_Int adf_resolution_list[MAX_LIST_SIZE];

    IP_HANDLE ip_handle;

   int cnt;                   /* number bytes available in buf[] */ 
   unsigned char buf[32768];  /* line buffer (max = 1200dpi * 8.5inches * 3pixels) */

   void *hpmud_handle;         /* returned by dlopen */
   void *bb_handle;            /* returned by dlopen */
   void *bb_session;
   int (*bb_open)(struct marvell_session *ps);
   int (*bb_close)(struct marvell_session *ps);
   int (*bb_get_parameters)(struct marvell_session *ps, SANE_Parameters *pp, int scan_started); 
   int (*bb_is_paper_in_adf)(struct marvell_session *ps); /* 0 = no paper in adf, 1 = paper in adf, 2 = no adf, -1 = error */
   int (*bb_start_scan)(struct marvell_session *ps);
   int (*bb_get_image_data)(struct marvell_session *ps, int max_length); /* see cnt and buf above */
   int (*bb_end_page)(struct marvell_session *ps, int io_error);
   int (*bb_end_scan)(struct marvell_session *ps, int io_error);
/* Add new elements here. */
   void *math_handle;         /* returned by dlopen */
   enum HPMUD_SCANSRC scansrc;       /* 0=NA */
   enum MARVELL_VERSION version;
};

#endif  // _MARVELLI_H


