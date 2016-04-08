/************************************************************************************\

  escli.h - HP SANE backend support for eSCL based multi-function peripherals

  (c) 2012-15 Copyright HP Development Company, LP

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

  Primary Author: Sarbeswar Meher

\************************************************************************************/

# ifndef _ESCLI_H
# define _ESCLI_H

# include "sane.h"
# include "hpip.h"
# include "hpmud.h"

# define ESCL_CONTRAST_MIN -127
# define ESCL_CONTRAST_MAX 127
# define ESCL_CONTRAST_DEFAULT 0
# define MM_PER_INCH     25.4


# define _STRINGIZE(x) #x
# define STRINGIZE(x) _STRINGIZE(x)

# define ESCL_DEBUG //(uncomment to enable log)
# define _BUG(args...) syslog(LOG_ERR, __FILE__ " " STRINGIZE(__LINE__) ": " args)
# ifdef ESCL_DEBUG
# define _DBG(args...) syslog(LOG_INFO, __FILE__ " " STRINGIZE(__LINE__) ": " args)
# else
# define _DBG(args...)
# endif

enum ESCL_OPTION_NUMBER
{
   ESCL_OPTION_COUNT = 0,
   ESCL_OPTION_GROUP_SCAN_MODE,
   ESCL_OPTION_SCAN_MODE,
   ESCL_OPTION_SCAN_RESOLUTION,
   ESCL_OPTION_INPUT_SOURCE,     /* platen, ADF, Camera */
   ESCL_OPTION_GROUP_ADVANCED,
   ESCL_OPTION_CONTRAST,
   ESCL_OPTION_COMPRESSION,
   ESCL_OPTION_JPEG_QUALITY,
   ESCL_OPTION_GROUP_GEOMETRY,
   ESCL_OPTION_TL_X,
   ESCL_OPTION_TL_Y,
   ESCL_OPTION_BR_X,
   ESCL_OPTION_BR_Y,
   ESCL_OPTION_PAGES_TO_SCAN,
   ESCL_OPTION_MAX
};

# define MAX_LIST_SIZE 32
# define MAX_STRING_SIZE 32

enum SCAN_FORMAT
{
  SF_RAW = 1,
  SF_JPEG,
  SF_MAX
};

enum INPUT_SOURCE
{
  IS_PLATEN = 1,
  IS_ADF,
  IS_ADF_DUPLEX,
  IS_CAMERA,
  IS_MAX
};

enum COLOR_ENTRY
{
  CE_K1 = 1,
  CE_GRAY8,
  CE_COLOR8,
  CE_MAX
};

enum SCAN_PARAM_OPTION
{
  SPO_BEST_GUESS = 0,             /* scan not started, return "best guess" scan parameters */
  SPO_STARTED = 1,                /* scan started, return "job resonse" or "image processor" scan parameters */
  SPO_STARTED_JR = 2,             /* scan started, but return "job response" scan parameters only */
};

enum ESCL_RESULT
{
   ESCL_R_OK = 0,
   ESCL_R_IO_ERROR,
   ESCL_R_EOF,
   ESCL_R_IO_TIMEOUT,
   ESCL_R_MALLOC_ERROR,
   ESCL_R_INVALID_BUF_SIZE,
};

struct escl_session
{
  char *tag;                      /* handle identifier */
  char uri[HPMUD_LINE_SIZE]; /* device uri */
  HPMUD_DEVICE dd;                /* hpiod device descriptor */
  HPMUD_CHANNEL cd;               /* hpiod eSCL channel descriptor */
  char model[HPMUD_LINE_SIZE];
  char url[256];
  char ip[32];
  int scan_type;
  int user_cancel;
  
  IP_IMAGE_TRAITS image_traits;   /* specified by image header */

  SANE_Option_Descriptor option[ESCL_OPTION_MAX];

  SANE_String_Const inputSourceList[IS_MAX];
  enum INPUT_SOURCE inputSourceMap[IS_MAX];
  enum INPUT_SOURCE currentInputSource;

  SANE_Int resolutionList[MAX_LIST_SIZE];
  SANE_Int currentResolution;

  SANE_String_Const scanModeList[CE_MAX];
  enum COLOR_ENTRY scanModeMap[CE_MAX];
  enum COLOR_ENTRY currentScanMode;

  SANE_String_Const compressionList[SF_MAX];
  enum SCAN_FORMAT compressionMap[SF_MAX];
  enum SCAN_FORMAT currentCompression;

  SANE_Range jpegQualityRange;
  SANE_Int currentJpegQuality;

  SANE_Range tlxRange, tlyRange, brxRange, bryRange;
  SANE_Fixed currentTlx, currentTly, currentBrx, currentBry;
  SANE_Fixed effectiveTlx, effectiveTly, effectiveBrx, effectiveBry;
  SANE_Fixed min_width, min_height;

  SANE_Int platen_resolutionList[MAX_LIST_SIZE];
  SANE_Fixed platen_min_width, platen_min_height;
  SANE_Range platen_tlxRange, platen_tlyRange, platen_brxRange, platen_bryRange;
 
  SANE_Int adf_resolutionList[MAX_LIST_SIZE];
  SANE_Fixed adf_min_width, adf_min_height;
  SANE_Range adf_tlxRange, adf_tlyRange, adf_brxRange, adf_bryRange;

  SANE_Int duplex_resolutionList[MAX_LIST_SIZE];
  SANE_Fixed duplex_min_width, duplex_min_height;
  SANE_Range duplex_tlxRange, duplex_tlyRange, duplex_brxRange, duplex_bryRange;

  SANE_Int camera_resolutionList[MAX_LIST_SIZE];
  SANE_Fixed camera_min_width, camera_min_height;
  SANE_Range camera_tlxRange, camera_tlyRange, camera_brxRange, camera_bryRange;

  IP_HANDLE ip_handle;

  int index;                      /* image buffer index */
  int cnt;                        /* image buffer count */
  //unsigned char buf[32768];       /* image chunk buffer */
  unsigned char buf[4*1000*1000];       /* image chunk buffer */

  void *bb_session;
  /* Add new elements here. */
  char job_id[64];
  int page_id;
  
   void *hpmud_handle;         /* returned by dlopen */
   void *math_handle;         /* returned by dlopen */
   void *bb_handle;            /* returned by dlopen */

   int (*bb_open)(struct escl_session *ps);
   int (*bb_close)(struct escl_session *ps);
   int (*bb_get_parameters)(struct escl_session *ps, SANE_Parameters *pp, int scan_started); 
   SANE_Status (*bb_check_scanner_to_continue)(struct escl_session *ps); 
   SANE_Status (*bb_start_scan)(struct escl_session *ps);
   enum ESCL_RESULT (*bb_get_image_data)(struct escl_session *ps, int max_length); /* see cnt and buf above */
   int (*bb_end_page)(struct escl_session *ps, int io_error);
   int (*bb_end_scan)(struct escl_session *ps, int io_error);
};


# endif  // _ESCLI_H

