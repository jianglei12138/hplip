/************************************************************************************\

  scl.h - HP SANE backend for multi-function peripherals (libsane-hpaio)

  (c) 2001-2006 Copyright HP Development Company, LP

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

  Contributing Authors: David Paschal, Don Welch, David Suffield 

\************************************************************************************/

#if !defined( __SCL_H__ )
#define __SCL_H__ 

#include "sane.h"

#define SCL_SEND_COMMAND_START_TIMEOUT      0
#define SCL_SEND_COMMAND_CONTINUE_TIMEOUT   2
#define SCL_INQUIRE_START_TIMEOUT     30
#define SCL_INQUIRE_CONTINUE_TIMEOUT      5
#define SCL_DEVICE_LOCK_TIMEOUT       0
#define SCL_PREPARE_SCAN_DEVICE_LOCK_MAX_RETRIES  4
#define SCL_PREPARE_SCAN_DEVICE_LOCK_DELAY    1
#define SCL_CMD(a,b) ( (('*'-'!'+1)<<10) + (((a)-'`'+1)<<5) + ((b)-'@'+1) )
#define SCL_CMD_PUNC(x)    ((((x)>>10)&0x1F)+'!'-1)
#define SCL_CMD_LETTER1(x) ((((x)>> 5)&0x1F)+'`'-1)
#define SCL_CMD_LETTER2(x) (( (x)     &0x1F)+'@'-1)

#define SCL_af          (hpaio->scl.decipixelChar)
#define SCL_CHAR_DECIPOINTS     'a'
#define SCL_CHAR_DEVPIXELS      'f'

#define SCL_CMD_RESET       SCL_CMD('z','E') /* No param! */
#define SCL_CMD_CLEAR_ERROR_STACK   SCL_CMD('o','E') /* No param! */
#define SCL_CMD_INQUIRE_PRESENT_VALUE   SCL_CMD('s','R')
#define SCL_CMD_INQUIRE_MINIMUM_VALUE   SCL_CMD('s','L')
#define SCL_CMD_INQUIRE_MAXIMUM_VALUE   SCL_CMD('s','H')
#define SCL_CMD_INQUIRE_DEVICE_PARAMETER  SCL_CMD('s','E')

#define SCL_CMD_SET_OUTPUT_DATA_TYPE    SCL_CMD('a','T')
#define SCL_CMD_SET_DATA_WIDTH      SCL_CMD('a','G')
#define SCL_CMD_SET_MFPDTF      SCL_CMD('m','S') /* No inq! */
#define SCL_CMD_SET_COMPRESSION     SCL_CMD('a','C') /* No inq! */
#define SCL_CMD_SET_JPEG_COMPRESSION_FACTOR SCL_CMD('m','Q') /* No inq! */
#define SCL_CMD_SET_X_RESOLUTION    SCL_CMD('a','R')
#define SCL_CMD_SET_Y_RESOLUTION    SCL_CMD('a','S')
#define SCL_CMD_SET_X_POSITION      SCL_CMD(SCL_af,'X')
#define SCL_CMD_SET_Y_POSITION      SCL_CMD(SCL_af,'Y')
#define SCL_CMD_SET_X_EXTENT      SCL_CMD(SCL_af,'P')
#define SCL_CMD_SET_Y_EXTENT      SCL_CMD(SCL_af,'Q')
#define SCL_CMD_SET_DOWNLOAD_TYPE   SCL_CMD('a','D')
#define SCL_CMD_DOWNLOAD_BINARY_DATA    SCL_CMD('a','W')
#define SCL_CMD_SET_CCD_RESOLUTION    SCL_CMD('m','R')
#define SCL_CMD_CHANGE_DOCUMENT     SCL_CMD('u','X')
#define SCL_CMD_UNLOAD_DOCUMENT     SCL_CMD('u','U')
#define SCL_CMD_CHANGE_DOCUMENT_BACKGROUND  SCL_CMD('u','Y')
#define SCL_CMD_SCAN_WINDOW     SCL_CMD('f','S')
#define SCL_CMD_SET_DEVICE_LOCK     SCL_CMD('f','H')
#define SCL_CMD_SET_DEVICE_LOCK_TIMEOUT   SCL_CMD('f','I')
#define SCL_CMD_SET_CONTRAST        SCL_CMD('a', 'K')
#define SCL_CMD_SET_BRIGHTNESS       SCL_CMD('a', 'L')

#if 0
#define SCL_CMD_SET_PRESCAN     SCL_CMD('m','B')
#define SCL_CMD_SET_NUMBER_OF_IMAGES_FOUND  SCL_CMD('m','P')
#define SCL_CMD_SET_SHARPENING      SCL_CMD('a','N')
#endif

/* Pseudo-commands for inquiring flatbed- and ADF-specific min/max values: */
#define SCL_PSEUDO_FLATBED_X_RESOLUTION   (SCL_CMD_SET_X_RESOLUTION+1000)
#define SCL_PSEUDO_FLATBED_Y_RESOLUTION   (SCL_CMD_SET_Y_RESOLUTION+1000)
#define SCL_PSEUDO_FLATBED_Y_EXTENT   (SCL_CMD_SET_Y_EXTENT    +1000)
#define SCL_PSEUDO_ADF_X_RESOLUTION   (SCL_CMD_SET_X_RESOLUTION+2000)
#define SCL_PSEUDO_ADF_Y_RESOLUTION   (SCL_CMD_SET_Y_RESOLUTION+2000)
#define SCL_PSEUDO_ADF_Y_EXTENT     (SCL_CMD_SET_Y_EXTENT    +2000)

#define SCL_DATA_TYPE_LINEART   0
#define SCL_DATA_TYPE_GRAYSCALE   4
#define SCL_DATA_TYPE_COLOR   5

#define SCL_DATA_WIDTH_LINEART    1
#define SCL_DATA_WIDTH_GRAYSCALE  8 /* or 10, 12, 14, 16? */
#define SCL_DATA_WIDTH_COLOR    24  /* or 30, 36, 42, 48? */

#define SCL_MFPDTF_OFF      0
#define SCL_MFPDTF_ON     2

#define SCL_COMPRESSION_NONE    0
#define SCL_COMPRESSION_JPEG    2

#define SCL_MIN_Y_RES_1150    50  /* 42 is absolute minimum. */
#define SCL_MAX_RES_1150_1170   300

#define SCL_DOWNLOAD_TYPE_COLORMAP  15

#define SCL_DEVICE_LOCK_RELEASED  0
#define SCL_DEVICE_LOCK_SET   1
#define SCL_DEVICE_LOCK_TIMEOUT   0

#define SCL_CHANGE_DOC_SIMPLEX    0
#define SCL_CHANGE_DOC_DUPLEX   2
#define SCL_CHANGE_DOC_DUPLEX_SIDE  12


#define SCL_INQ_HP_MODEL_11     18
#define SCL_INQ_HP_MODEL_12     19
#define SCL_INQ_ADF_FEED_STATUS     23
#define SCL_INQ_ADF_CAPABILITY      24
#define SCL_INQ_ADF_DOCUMENT_LOADED   25
#define SCL_INQ_ADF_READY_TO_UNLOAD   27
#define SCL_INQ_MAX_ERROR_STACK     256 /* always 1 */
#define SCL_INQ_CURRENT_ERROR_STACK   257 /* 0 or 1 errors */
#define SCL_INQ_CURRENT_ERROR     259 /* error number */
#define SCL_INQ_SESSION_ID      505
#define SCL_INQ_BULB_WARM_UP_STATUS   506
#define SCL_INQ_PIXELS_PER_SCAN_LINE    1024
#define SCL_INQ_BYTES_PER_SCAN_LINE   1025
#define SCL_INQ_NUMBER_OF_SCAN_LINES    1026
#define SCL_INQ_ADF_READY_TO_LOAD   1027
#define SCL_INQ_DEVICE_PIXELS_PER_INCH    1028  /* 300 */
#if 0
#define SCL_INQ_NATIVE_OPTICAL_RESOLUTION 1029
#endif

#define SCL_ADF_FEED_STATUS_OK      0
#define SCL_ADF_FEED_STATUS_BUSY    1000
#define SCL_ADF_FEED_STATUS_PAPER_JAM   1024
#define SCL_ADF_FEED_STATUS_ORIGINAL_ON_GLASS 1027
#define SCL_ADF_FEED_STATUS_PORTRAIT_FEED 1028


#define SCL_ERROR_COMMAND_FORMAT_ERROR    0
#define SCL_ERROR_UNRECOGNIZED_COMMAND    1
#define SCL_ERROR_PARAMETER_ERROR   2
#define SCL_ERROR_ILLEGAL_WINDOW    3
#define SCL_ERROR_SCALING_ERROR     4
#define SCL_ERROR_DITHER_ID_ERROR   5
#define SCL_ERROR_TONE_MAP_ID_ERROR   6
#define SCL_ERROR_LAMP_ERROR      7
#define SCL_ERROR_MATRIX_ID_ERROR   8
#define SCL_ERROR_CAL_STRIP_PARAM_ERROR   9
#define SCL_ERROR_GROSS_CALIBRATION_ERROR 10
#define SCL_ERROR_NO_MEMORY     500
#define SCL_ERROR_SCANNER_HEAD_LOCKED   501
#define SCL_ERROR_CANCELLED     502
#define SCL_ERROR_PEN_DOOR_OPEN     503
#define SCL_ERROR_ADF_PAPER_JAM     1024
#define SCL_ERROR_HOME_POSITION_MISSING   1025
#define SCL_ERROR_PAPER_NOT_LOADED    1026
#define SCL_ERROR_ORIGINAL_ON_GLASS   1027

#define SCL_COMPAT_1150     0x0001  /* model 11 "5300A", 12 null */
#define SCL_COMPAT_1170     0x0002  /* model 12 "5400A" */
#define SCL_COMPAT_R_SERIES   0x0004  /* model 12 "5500A" */
#define SCL_COMPAT_G_SERIES   0x0008  /* model 12 "5600A" */
#define SCL_COMPAT_K_SERIES   0x0010  /* model 12 "5700A" */
#define SCL_COMPAT_D_SERIES   0x0020  /* model 12 "5800A" */
#define SCL_COMPAT_6100_SERIES    0x0040  /* model 12 "5900A" */
#define SCL_COMPAT_OFFICEJET    0x1000  /* model 11 not null */
#define SCL_COMPAT_POST_1150    0x2000  /* model 12 not null */

//#define LEN_SCL_BUFFER 1024
#define LEN_SCL_BUFFER    256 /* Increase if reading binary data. */

SANE_Status  __attribute__ ((visibility ("hidden"))) SclSendCommand(int deviceid, int channelid, int cmd, int param);
SANE_Status __attribute__ ((visibility ("hidden"))) SclInquire(int deviceid, int channelid, int cmd, int param, int *pValue, char *buffer, int maxlen);

/*
 * Phase 2 partial rewrite. des 9/26/07
 */

/* Note ESC = 0x1b = '\e' . */
#define SCL_QUERY_DUPLEX_SUPPORTED "\e*s13500E"            /* 1 = device can support duplex */
#define SCL_QUERY_DUPLEX_VERT_FLIP_SUPPORTED "\e*s13501E"  /* duplex can flip vertical (they all should) */
#define SCL_QUERY_DUPLEX_HORZ_FLIP_SUPPORTED "\e*s13502E"  /* duplex can flip horizontal (they all should) */

#define SCL_SET_DUPLEX "\e*d%dD13505R"                  /* 1 = duplex is requested, 0 otherwise */
#define SCL_SET_DUPLEX_VERT_FLIP "\e*d%dV13506R"        /* 1 = vertical flip is requested, 0 otherwise */
#define SCL_SET_DUPLEX_HORZ_FLIP "\e*d%dH13507R"        /* 1 = horizontal flip is requested, 0 otherwise */
#define SCL_SET_DUPLEX_ORIENTATION "\e*a%d13507R"       /* 0 = no change, 1 = needs vert flip, 2 = vert and horz flip */

struct hpaioScanner_s;

SANE_Status __attribute__ ((visibility ("hidden"))) scl_send_cmd(struct hpaioScanner_s *hpaio, const char *buf, int size);
SANE_Status __attribute__ ((visibility ("hidden"))) scl_query_int(struct hpaioScanner_s *hpaio, const char *buf, int size, int *result);

#endif

