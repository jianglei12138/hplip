/************************************************************************************\

  hpaio.h - HP SANE backend for multi-function peripherals (libsane-hpaio)

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

  Contributing Authors: David Paschal, Don Welch, David Suffield, Sarbeswar Meher 

\************************************************************************************/

#if !defined( __HPAIO_H__ )
#define __HPAIO_H__

#include "sane.h"
#include "common.h"
#include "mfpdtf.h"
#include "hpip.h"
#include "scl.h"
#include "pml.h"

/************************************************************************************/

#define MAX_DEVICE 64     /* Max devices. */

#define LEN_BUFFER    17408         /* 16384 + 1024, clj28xx used 16396 */
#define LEN_DEVICE_ID_STRING  4096
#define LEN_STRING_OPTION_VALUE 20
#define LEN_MODEL_RESPONSE  20

#define PAD_VALUE_LINEART            0
#define PAD_VALUE_GRAYSCALE_COLOR    -1

enum hpaioOption_e { 
    
    OPTION_FIRST = 0,
    OPTION_NUM_OPTIONS = 0,
    
    GROUP_SCAN_MODE,
                    OPTION_SCAN_MODE,
                    OPTION_SCAN_RESOLUTION,
    GROUP_ADVANCED,
                    OPTION_CONTRAST,
                    OPTION_BRIGHTNESS,
                    OPTION_COMPRESSION,
                    OPTION_JPEG_COMPRESSION_FACTOR,
                    OPTION_BATCH_SCAN,
                    OPTION_ADF_MODE, 
                    OPTION_DUPLEX,

    GROUP_GEOMETRY,
                    OPTION_LENGTH_MEASUREMENT,
                    OPTION_TL_X,
                    OPTION_TL_Y,
                    OPTION_BR_X,
                    OPTION_BR_Y,

    OPTION_LAST };

//#define STR_SCAN_MODE_LINEART "Lineart"
//#define STR_SCAN_MODE_GRAYSCALE "Grayscale"
//#define STR_SCAN_MODE_COLOR "Color"

enum hpaioScanMode_e { SCAN_MODE_FIRST = 0,
                       SCAN_MODE_LINEART = 0,
                       SCAN_MODE_GRAYSCALE,
                       SCAN_MODE_COLOR,
                       SCAN_MODE_LAST 
                     };

#define COMPRESSION_NONE  0x01
#define COMPRESSION_MH    0x02
#define COMPRESSION_MR    0x04
#define COMPRESSION_MMR   0x08
#define COMPRESSION_JPEG  0x10

#define ADF_MODE_AUTO   0x01     /* flatbed or ADF */
#define ADF_MODE_FLATBED  0x02   /* flatbed only */
#define ADF_MODE_ADF    0x04     /* ADF only */

#define LENGTH_MEASUREMENT_UNKNOWN    0
#define LENGTH_MEASUREMENT_UNLIMITED    1
#define LENGTH_MEASUREMENT_APPROXIMATE    2
#define LENGTH_MEASUREMENT_PADDED   3
#define LENGTH_MEASUREMENT_EXACT    4

struct  hpaioScanner_s
{
        char *tag;   /* handle identifier */
        char deviceuri[128];
        HPMUD_DEVICE deviceid;
        HPMUD_CHANNEL scan_channelid;
        HPMUD_CHANNEL cmd_channelid;
        
        struct hpaioScanner_s * prev;
        struct hpaioScanner_s * next;

        SANE_Device     saneDevice; /* "vendor", "model" dynamically allocated. */
        SANE_Parameters prescanParameters;
        SANE_Parameters scanParameters;
        
        struct PmlObject_s *    firstPmlObject;
        struct PmlObject_s *    lastPmlObject;

        enum { SCANNER_TYPE_SCL, SCANNER_TYPE_PML } scannerType;
        int                     decipixelsPerInch;

        /* These are bitfields of COMPRESSION_* values. */
        int                     supportsScanMode[SCAN_MODE_LAST];
        SANE_String_Const       scanModeList[MAX_LIST_SIZE];
        enum hpaioScanMode_e    currentScanMode, effectiveScanMode;

        SANE_Range              resolutionRange;
        SANE_Int                resolutionList[MAX_LIST_SIZE];
        SANE_Int                lineartResolutionList[MAX_LIST_SIZE];  /* 300 dpi. */
        SANE_Int                currentResolution, effectiveResolution;

        SANE_Range              contrastRange;
        SANE_Int                defaultContrast, currentContrast;

        SANE_Range              brightnessRange;
        SANE_Int                defaultBrightness, currentBrightness;
    
        SANE_String_Const       compressionList[MAX_LIST_SIZE];
        int                     defaultCompression[SCAN_MODE_LAST];
        SANE_Int                currentCompression;  /* One of the COMPRESSION_* values. */

        SANE_Range              jpegCompressionFactorRange;
        SANE_Int                defaultJpegCompressionFactor;
        SANE_Int                currentJpegCompressionFactor;

        SANE_Bool               currentBatchScan;
        int                     beforeScan;
        int                     alreadyPreAdvancedDocument;
        int                     alreadyPostAdvancedDocument;
        int                     noDocsConditionPending;

        int                     supportedAdfModes;
        SANE_String_Const       adfModeList[MAX_LIST_SIZE];
        int                     currentAdfMode;
        int                     currentPageNumber;

        int                     supportsDuplex;
        SANE_Bool               currentDuplex;
        int                     currentSideNumber;

        SANE_Int                currentLengthMeasurement;
        SANE_String_Const       lengthMeasurementList[MAX_LIST_SIZE];

        SANE_Range              tlxRange, tlyRange, brxRange, bryRange;
        SANE_Fixed              currentTlx, currentTly, currentBrx, currentBry;
        SANE_Fixed              effectiveTlx,
                                effectiveTly,
                                effectiveBrx,
                                effectiveBry;

        SANE_Option_Descriptor  option[OPTION_LAST];

        Mfpdtf_t                mfpdtf;
        IP_HANDLE               hJob;
        int                     fromDenali;
        int                     preDenali;
        int                     denali;
        unsigned char           inBuffer[LEN_BUFFER];     /* mfpdtf block buffer */
        int                     bufferOffset;
        int                     bufferBytesRemaining;
        int                     totalBytesRemaining;
        int                     endOfData;
        int BlockSize;                                    /* mfpdtf block size, including fixed header */
        int BlockIndex;                                   /* record index in mfpdtf block */
        int RecordSize;                                    /* record size, does not include header */
        int RecordIndex;                                   /* data index in record */
        int mfpdtf_done; 
        int mfpdtf_timeout_cnt; 
        int pml_timeout_cnt;                              /* pml done timeout count */ 
        int pml_done;
        int ip_done;
        int page_done;
        int upload_state;                                 /* last pml upload state */
        int user_cancel;                                  /* user cancelled operation */

        struct 
        {
                char            compat1150[LEN_MODEL_RESPONSE + 1];
                char            compatPost1150[LEN_MODEL_RESPONSE + 1];
                int             compat;
                char            decipixelChar;

                int             minRes, maxRes;
                int             maxXExtent, maxYExtent;
                int             unloadAfterScan;
                int             flatbedCapability, adfCapability;
                int             minResAdf, maxResAdf;

                PmlObject_t     objSupportedFunctions;
        } scl;

        struct 
        {
                PmlObject_t     objScannerStatus,
                                objResolutionRange,
                                objUploadTimeout,
                                objContrast,
                                objResolution,
                                objPixelDataType,
                                objCompression,
                                objCompressionFactor,
                                objUploadError,
                                objUploadState,
                                objAbcThresholds,
                                objSharpeningCoefficient,
                                objNeutralClipThresholds,
                                objToneMap,
                                objCopierReduction,
                                objScanToken,
                                objModularHardware;

                char            scanToken[ PML_MAX_VALUE_LEN ];
                char            zeroScanToken[ PML_MAX_VALUE_LEN ];
                int             lenScanToken;
                int             scanTokenIsSet;

                int             openFirst;
                int             dontResetBeforeNextNonBatchPage;
                int             startNextBatchPageEarly;
                int             flatbedCapability;

                int             alreadyRestarted;
                int             scanDone;
                int             previousUploadState;
        } pml;
};

typedef struct hpaioScanner_s * hpaioScanner_t;
typedef struct hpaioScanner_s HPAIO_RECORD;

#define UNDEFINED_MODEL(hpaio) (!hpaio->saneDevice.model)

#define _SET_DEFAULT_MODEL(hpaio,s,len) \
  do { \
    if (UNDEFINED_MODEL(hpaio)) { \
      hpaio->saneDevice.model=malloc(len+1); \
      memcpy((char *)hpaio->saneDevice.model,s,len); \
      ((char *)hpaio->saneDevice.model)[len]=0; \
    } \
  } while(0)
#define SET_DEFAULT_MODEL(hpaio,s) _SET_DEFAULT_MODEL(hpaio,s,strlen(s))

#define FIX_GEOMETRY(low,high,min,max) \
  do { \
    if (high<low) high=low; \
    if (high==low) { \
      if (high==max) { \
        low--; \
      } else { \
        high++; \
      } \
    } \
  } while(0)

SANE_Status __attribute__ ((visibility ("hidden"))) hpaioScannerToSaneStatus( hpaioScanner_t hpaio );
SANE_Status __attribute__ ((visibility ("hidden"))) hpaioScannerToSaneError( hpaioScanner_t hpaio );
void sane_hpaio_cancel(SANE_Handle handle);

#endif
