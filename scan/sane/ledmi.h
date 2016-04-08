/************************************************************************************\

  ledmi.h - HP SANE backend support for LEDM based multi-function peripherals

  (c) 2010 Copyright HP Development Company, LP

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

  Primary Author: Naga Samrat Chowdary, Narla
  Contributing Authors: Yashwant Kumar Sahu, Sarbeswar Meher

\************************************************************************************/

# ifndef _LEDMI_H
# define _LEDMI_H
 
# include "sane.h"
# include "hpip.h"
# include "hpmud.h"

# define LEDM_CONTRAST_MIN 0 /*According the LEDM spec*/
# define LEDM_CONTRAST_MAX 2000
# define LEDM_CONTRAST_DEFAULT 1000
# define LEDM_BRIGHTNESS_MIN 0
# define LEDM_BRIGHTNESS_MAX 2000
# define LEDM_BRIGHTNESS_DEFAULT 1000

# define MM_PER_INCH     25.4

enum LEDM_OPTION_NUMBER
{
  LEDM_OPTION_COUNT = 0,
  LEDM_OPTION_GROUP_SCAN_MODE,
        LEDM_OPTION_SCAN_MODE,
        LEDM_OPTION_SCAN_RESOLUTION,
        LEDM_OPTION_INPUT_SOURCE,     /* platen, ADF, ADFDuplex */
  LEDM_OPTION_GROUP_ADVANCED,
        LEDM_OPTION_BRIGHTNESS,
        LEDM_OPTION_CONTRAST,
        LEDM_OPTION_COMPRESSION,
        LEDM_OPTION_JPEG_QUALITY,
  LEDM_OPTION_GROUP_GEOMETRY,
        LEDM_OPTION_TL_X,
        LEDM_OPTION_TL_Y,
        LEDM_OPTION_BR_X,
        LEDM_OPTION_BR_Y,
  LEDM_OPTION_MAX
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

struct ledm_session
{
  char *tag;                      /* handle identifier */
  HPMUD_DEVICE dd;                /* hpiod device descriptor */
  HPMUD_CHANNEL cd;               /* hpiod LEDM channel descriptor */
  char uri[HPMUD_LINE_SIZE];
  char model[HPMUD_LINE_SIZE];
  char url[256];
  int scan_type;
  int user_cancel;

  IP_IMAGE_TRAITS image_traits;   /* specified by image header */

  SANE_Option_Descriptor option[LEDM_OPTION_MAX];

  SANE_String_Const inputSourceList[IS_MAX];
  enum INPUT_SOURCE inputSourceMap[IS_MAX];
  enum INPUT_SOURCE currentInputSource;

  SANE_Int resolutionList[MAX_LIST_SIZE];
  SANE_Int currentResolution;

  SANE_Range contrastRange;
  SANE_Int currentContrast;

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
  SANE_Fixed adf_min_width, adf_min_height;
  SANE_Range adf_tlxRange, adf_tlyRange, adf_brxRange, adf_bryRange;
  SANE_Int adf_resolutionList[MAX_LIST_SIZE];

  SANE_Range brightnessRange;
  SANE_Int currentBrightness;

  IP_HANDLE ip_handle;

  int index;                      /* image buffer index */
  int cnt;                        /* image buffer count */
  unsigned char buf[32768];       /* image chunk buffer */

  void *bb_session;
  /* Add new elements here. */
  int job_id;
  int page_id;
};

int bb_open(struct ledm_session*);
int bb_close(struct ledm_session*);
int bb_get_parameters(struct ledm_session*, SANE_Parameters*, int);
int bb_is_paper_in_adf();         /* 0 = no paper in adf, 1 = paper in adf, -1 = error */
SANE_Status bb_start_scan(struct ledm_session*);
int bb_get_image_data(struct ledm_session*, int); 
int bb_end_page(struct ledm_session*, int);
int bb_end_scan(struct ledm_session* , int);

#endif  // _LEDMI_H
