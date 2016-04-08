/************************************************************************************\

  pml.h - HP SANE backend for multi-function peripherals (libsane-hpaio)

  (c) 2001-2005 Copyright HP Development Company, LP

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

  Contributing Author(s): David Paschal, Don Welch, David Suffield

\************************************************************************************/

#if !defined(_PML_H)
#define _PML_H

#include "sane.h"

#define PML_MAX_OID_VALUES 2
#define PML_MAX_VALUE_LEN       1023
#define PML_MAX_OID_LEN     128
#define PML_MAX_DATALEN     4096

#define PML_TYPE_MASK           0xFC
#define PML_TYPE_OBJECT_IDENTIFIER      0x00
#define PML_TYPE_ENUMERATION        0x04
#define PML_TYPE_SIGNED_INTEGER     0x08
#define PML_TYPE_REAL           0x0C
#define PML_TYPE_STRING         0x10
#define PML_TYPE_BINARY         0x14
#define PML_TYPE_ERROR_CODE     0x18
#define PML_TYPE_NULL_VALUE     0x1C
#define PML_TYPE_COLLECTION     0x20

#define PML_SYMSET_0E               0x000E
#define PML_SYMSET_ISO_8859_2_LATIN_2       0x004E
#define PML_SYMSET_ISO_8859_9_LATIN_5       0x00AE
#define PML_SYMSET_KANA8                0x010B
#define PML_SYMSET_ROMAN8               0x0115
#define PML_SYMSET_ISO_8859_5_LATIN_CYRILLIC    0x014E
#define PML_SYMSET_US_UNICODE           0x024E
#define PML_SYMSET_UTF8_UNICODE         0xFDE8

#define PML_OK                  0x00
#define PML_OK_END_OF_SUPPORTED_OBJECTS     0x01
#define PML_OK_NEAREST_LEGAL_VALUE_SUBSTITUTED  0x02
#define PML_ERROR                   0x80
#define PML_ERROR_UNKNOWN_REQUEST           0x80
#define PML_ERROR_BUFFER_OVERFLOW           0x81
#define PML_ERROR_COMMAND_EXECUTION_ERROR       0x82
#define PML_ERROR_UNKNOWN_OBJECT_IDENTIFIER 0x83
#define PML_ERROR_OBJECT_DOES_NOT_SUPPORT_REQUESTED_ACTION  0x84
#define PML_ERROR_INVALID_OR_UNSUPPORTED_VALUE  0x85
#define PML_ERROR_PAST_END_OF_SUPPORTED_OBJECTS 0x86
#define PML_ERROR_ACTION_CAN_NOT_BE_PERFORMED_NOW   0x87
#define PML_ERROR_SYNTAX_ERROR          0x88

struct PmlValue_s
{
        int     type;
        int     len;
        char    value[ PML_MAX_VALUE_LEN + 1 ];  // null terminated
}; // PmlValue_s, * PmlValue_t;

typedef struct PmlValue_s * PmlValue_t;

//extern hpaioScanner_t;


struct PmlObject_s
{
        //struct hpaioScanner_s *          dev;
        struct PmlObject_s *          prev;
        struct PmlObject_s *          next;
        char                    oid[ PML_MAX_OID_LEN + 1 ];  /* ascii, null terminated */
        int                     indexOfLastValue;
        int                     numberOfValidValues;
        struct PmlValue_s       value[ PML_MAX_OID_VALUES ];
        int                     status;
}; // , * PmlObject_t;

typedef struct PmlObject_s * PmlObject_t;

#include "hpaio.h"

#define PML_REQUEST_GET        0x00
#define PML_REQUEST_GETNEXT    0x01
#define PML_REQUEST_SET        0x04
#define PML_REQUEST_TRAP_ENABLE    0x05
#define PML_REQUEST_TRAP_DISABLE   0x06
#define PML_REQUEST_TRAP       0x07
#define PML_COMMAND_MASK       0x7F
#define PML_COMMAND_REPLY      0x80
#define PML_SELECT_POLL_TIMEOUT       1
#define PML_UPLOAD_TIMEOUT        45
#define PML_START_SCAN_WAIT_ACTIVE_MAX_RETRIES    40
#define PML_START_SCAN_WAIT_ACTIVE_DELAY    1
#define PML_MAX_WIDTH_INCHES    9
#define PML_MAX_HEIGHT_INCHES 15

#define PML_SCANNER_STATUS_UNKNOWN_ERROR  0x01
#define PML_SCANNER_STATUS_INVALID_MEDIA_SIZE 0x02
#define PML_SCANNER_STATUS_FEEDER_OPEN    0x04
#define PML_SCANNER_STATUS_FEEDER_JAM   0x08
#define PML_SCANNER_STATUS_FEEDER_EMPTY   0x10

#define PML_DATA_TYPE_LINEART   1
#define PML_DATA_TYPE_GRAYSCALE   8
#define PML_DATA_TYPE_COLOR   24

#define PML_CONTRAST_MIN    -127
#define PML_CONTRAST_MAX    127
#define PML_CONTRAST_DEFAULT    0

#define PML_BRIGHTNESS_MIN    -127
#define PML_BRIGHTNESS_MAX    127
#define PML_BRIGHTNESS_DEFAULT    0

#define PML_COMPRESSION_NONE    1
#define PML_COMPRESSION_DEFAULT   2
#define PML_COMPRESSION_MH    3
#define PML_COMPRESSION_MR    4
#define PML_COMPRESSION_MMR   5
#define PML_COMPRESSION_JPEG    6

#define PML_MODHW_ADF     1

#define PML_UPLOAD_STATE_IDLE   1
#define PML_UPLOAD_STATE_START    2
#define PML_UPLOAD_STATE_ACTIVE   3
#define PML_UPLOAD_STATE_ABORTED  4
#define PML_UPLOAD_STATE_DONE   5
#define PML_UPLOAD_STATE_NEWPAGE  6

#define PML_UPLOAD_ERROR_SCANNER_JAM    207
#define PML_UPLOAD_ERROR_MLC_CHANNEL_CLOSED 208
#define PML_UPLOAD_ERROR_STOPPED_BY_HOST  209
#define PML_UPLOAD_ERROR_STOP_KEY_PRESSED 210
#define PML_UPLOAD_ERROR_NO_DOC_IN_ADF    211
#define PML_UPLOAD_ERROR_COVER_OPEN   213
#define PML_UPLOAD_ERROR_DOC_LOADED   214
#define PML_UPLOAD_ERROR_DEVICE_BUSY    216

#define PML_SUPPFUNC_DUPLEX   0x00000008

struct PmlResolution
{
        unsigned char   x[4];
        unsigned char   y[4];
} __attribute__(( packed));

int __attribute__ ((visibility ("hidden"))) PmlSetID(PmlObject_t obj, char * oid);
int __attribute__ ((visibility ("hidden"))) PmlSetValue(PmlObject_t obj, int type, char * value, int len);
int __attribute__ ((visibility ("hidden"))) PmlSetIntegerValue(PmlObject_t obj, int type, int value);
int __attribute__ ((visibility ("hidden"))) PmlGetValue(PmlObject_t obj, int * pType, char * buffer, int maxlen);
int __attribute__ ((visibility ("hidden"))) PmlGetStringValue(PmlObject_t obj, int * pSymbolSet, char * buffer, int maxlen);
int __attribute__ ((visibility ("hidden"))) PmlGetIntegerValue(PmlObject_t obj, int * pType, int * pValue);
int __attribute__ ((visibility ("hidden"))) PmlRequestSet(int deviceid, int channelid, PmlObject_t obj);
int __attribute__ ((visibility ("hidden"))) PmlRequestSetRetry(int deviceid, int channelid, PmlObject_t obj, int count, int delay);
int __attribute__ ((visibility ("hidden"))) PmlRequestGet(int deviceid, int channelid, PmlObject_t obj);

/*
 * Phase 2 rewrite. des
 */
struct hpaioScanner_s;

int __attribute__ ((visibility ("hidden"))) pml_start(struct hpaioScanner_s *hpaio);
int __attribute__ ((visibility ("hidden"))) pml_read(struct hpaioScanner_s *hpaio, SANE_Byte *data, SANE_Int maxLength, SANE_Int *pLength);
int __attribute__ ((visibility ("hidden"))) pml_cancel(struct hpaioScanner_s *hpaio);

#endif // _PML_H
