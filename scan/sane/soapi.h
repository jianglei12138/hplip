/************************************************************************************\

  soapi.h - HP SANE backend support for soap based multi-function peripherals

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

  Author: David Suffield

\************************************************************************************/

#ifndef _SOAPI_H
#define _SOAPI_H

#define SOAP_CONTRAST_MIN -1000 /*According the SOAP spec*/
#define SOAP_CONTRAST_MAX 1000
#define SOAP_CONTRAST_DEFAULT 0
#define SOAP_BRIGHTNESS_MIN -1000 /*According the SOAP spec*/
#define SOAP_BRIGHTNESS_MAX 1000
#define SOAP_BRIGHTNESS_DEFAULT 0

#define MM_PER_INCH     25.4

enum SOAP_OPTION_NUMBER
{ 
   SOAP_OPTION_COUNT = 0,
   SOAP_OPTION_GROUP_SCAN_MODE,
                   SOAP_OPTION_SCAN_MODE,
                   SOAP_OPTION_SCAN_RESOLUTION,
   SOAP_OPTION_GROUP_ADVANCED,
                   SOAP_OPTION_BRIGHTNESS,
                   SOAP_OPTION_CONTRAST,
                   SOAP_OPTION_COMPRESSION,
                   SOAP_OPTION_JPEG_QUALITY,
   SOAP_OPTION_GROUP_GEOMETRY,
                   SOAP_OPTION_TL_X,
                   SOAP_OPTION_TL_Y,
                   SOAP_OPTION_BR_X,
                   SOAP_OPTION_BR_Y,
   SOAP_OPTION_MAX
};

#define MAX_LIST_SIZE 32
#define MAX_STRING_SIZE 32

enum SCAN_FORMAT
{
   SF_HPRAW = 1,
   SF_JFIF,
   SF_MMR,
   SF_MAX
};

enum COLOR_ENTRY
{
   CE_BLACK_AND_WHITE1 = 1,  /* Lineart is not supported on Horse Thief (ie: LJM1522). Windows converts GRAY8 to MONO. Ditto for us. */
   CE_GRAY8, 
   CE_RGB24, 
   CE_RGB48,      /* for test only */
   CE_MAX
};

enum SCAN_PARAM_OPTION
{
   SPO_BEST_GUESS = 0,   /* scan not started, return "best guess" scan parameters */
   SPO_STARTED = 1,      /* scan started, return "job resonse" or "image processor" scan parameters */ 
   SPO_STARTED_JR = 2,   /* scan started, but return "job response" scan parameters only */
};

struct soap_session
{
   char *tag;  /* handle identifier */
   HPMUD_DEVICE dd;  /* hpiod device descriptor */
   HPMUD_CHANNEL cd;  /* hpiod soap channel descriptor */
   char uri[HPMUD_LINE_SIZE];
   char model[HPMUD_LINE_SIZE];
   int scan_type;
   int user_cancel;
   IP_IMAGE_TRAITS image_traits;   /* specified by image header */      

   SANE_Option_Descriptor option[SOAP_OPTION_MAX];

   SANE_String_Const scanModeList[CE_MAX];
   enum COLOR_ENTRY scanModeMap[CE_MAX];
   enum COLOR_ENTRY currentScanMode;

   SANE_Int resolutionList[MAX_LIST_SIZE];
   SANE_Int currentResolution;

   SANE_Range contrastRange;
   SANE_Int currentContrast;

   SANE_Range brightnessRange;
   SANE_Int currentBrightness;

   SANE_String_Const compressionList[SF_MAX];
   enum SCAN_FORMAT compressionMap[SF_MAX];
   enum SCAN_FORMAT currentCompression; 

   SANE_Range jpegQualityRange;
   SANE_Int currentJpegQuality;

   SANE_Range tlxRange, tlyRange, brxRange, bryRange;
   SANE_Fixed currentTlx, currentTly, currentBrx, currentBry;
   SANE_Fixed effectiveTlx, effectiveTly, effectiveBrx, effectiveBry;
   SANE_Fixed min_width, min_height;

   IP_HANDLE ip_handle;

   int index;                    /* dime buffer index */
   int cnt;                      /* dime buffer count */
   unsigned char buf[65536];    /* dime buffer */ /*Setting to max 64K for dime buffer*/

   void *hpmud_handle;         /* returned by dlopen */
   void *math_handle;         /* returned by dlopen */
   void *bb_handle;            /* returned by dlopen */
   void *bb_session;
   int (*bb_open)(struct soap_session *ps);
   int (*bb_close)(struct soap_session *ps);
   int (*bb_get_parameters)(struct soap_session *ps, SANE_Parameters *pp, int scan_started); 
   int (*bb_is_paper_in_adf)(struct soap_session *ps); /* 0 = no paper in adf, 1 = paper in adf, -1 = error */
   int (*bb_start_scan)(struct soap_session *ps);
   int (*bb_get_image_data)(struct soap_session *ps, int max_length); /* see cnt and buf above */
   int (*bb_end_page)(struct soap_session *ps, int io_error);
   int (*bb_end_scan)(struct soap_session *ps, int io_error);
/* Add new elements here. */
};

#endif  // _SOAPI_H
