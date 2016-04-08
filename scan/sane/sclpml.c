/************************************************************************************\

  scl-pml.c - HP SANE backend for multi-function peripherals (libsane-hpaio)

  (c) 2001-2014 Copyright HP Development Company, LP

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

  Authors: Sarbeswar Meher

\************************************************************************************/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "sane.h"
#include "saneopts.h"
#include "common.h"
#include "io.h"
#include "mfpdtf.h"
#include "tables.h"
#include "hpmud.h"
#include "io.h"
#include "common.h"
#include "scl.h"
#include "hpaio.h"
#include "utils.h"

#define DEBUG_DECLARE_ONLY
#include "sanei_debug.h"

//# define  SCLPML_DEBUG
# ifdef SCLPML_DEBUG
   # define _DBG(args...) syslog(LOG_INFO, __FILE__ " " STRINGIZE(__LINE__) ": " args)
# else
   # define _DBG(args...)
# endif

static struct hpaioScanner_s * session;

SANE_Status __attribute__ ((visibility ("hidden"))) hpaioScannerToSaneError( hpaioScanner_t hpaio )
{
    SANE_Status retcode;

    if( hpaio->scannerType == SCANNER_TYPE_SCL )
    {
        int sclError;

        retcode = SclInquire( hpaio->deviceid, hpaio->scan_channelid,
                              SCL_CMD_INQUIRE_DEVICE_PARAMETER,
                              SCL_INQ_CURRENT_ERROR,
                              &sclError,
                              0,
                              0 );

        if( retcode == SANE_STATUS_UNSUPPORTED )
        {
            retcode = SANE_STATUS_GOOD;
        }
        else if( retcode == SANE_STATUS_GOOD )
        {
            bug("hpaio: hpaioScannerToSaneError: sclError=%d.\n", sclError);
            switch( sclError )
            {
                case SCL_ERROR_UNRECOGNIZED_COMMAND:
                case SCL_ERROR_PARAMETER_ERROR:
                    retcode = SANE_STATUS_UNSUPPORTED;
                    break;

                case SCL_ERROR_NO_MEMORY:
                    retcode = SANE_STATUS_NO_MEM;
                    break;

                case SCL_ERROR_CANCELLED:
                    retcode = SANE_STATUS_CANCELLED;
                    break;

                case SCL_ERROR_PEN_DOOR_OPEN:
                    retcode = SANE_STATUS_COVER_OPEN;
                    break;

                case SCL_ERROR_SCANNER_HEAD_LOCKED:
                case SCL_ERROR_ADF_PAPER_JAM:
                case SCL_ERROR_HOME_POSITION_MISSING:
                case SCL_ERROR_ORIGINAL_ON_GLASS:
                    retcode = SANE_STATUS_JAMMED;
                    break;

                case SCL_ERROR_PAPER_NOT_LOADED:
                    retcode = SANE_STATUS_NO_DOCS;
                    break;

                default:
                    retcode = SANE_STATUS_IO_ERROR;
                    break;
            }
        }
    }
    else /* if (hpaio->scannerType==SCANNER_TYPE_PML) */
    {
        int pmlError, type;

        if( PmlRequestGet( hpaio->deviceid, hpaio->cmd_channelid, hpaio->pml.objUploadError ) == ERROR )
        {
            retcode = SANE_STATUS_GOOD;
        }
        else if( PmlGetIntegerValue( hpaio->pml.objUploadError,
                                         &type,
                                         &pmlError ) == ERROR )
        {
            bug("hpaio: hpaioScannerToSaneError: PmlGetIntegerValue failed, type=%d!\n", type);
            retcode = SANE_STATUS_IO_ERROR;
        }
        else
        {
            bug("hpaio: hpaioScannerToSaneError: pmlError=%d.\n", pmlError);

            switch( pmlError )
            {
                case PML_UPLOAD_ERROR_SCANNER_JAM:
                    retcode = SANE_STATUS_JAMMED;
                    break;

                case PML_UPLOAD_ERROR_MLC_CHANNEL_CLOSED:
                case PML_UPLOAD_ERROR_STOPPED_BY_HOST:
                case PML_UPLOAD_ERROR_STOP_KEY_PRESSED:
                    retcode = SANE_STATUS_CANCELLED;
                    break;

                case PML_UPLOAD_ERROR_NO_DOC_IN_ADF:
                case PML_UPLOAD_ERROR_DOC_LOADED:
                    retcode = SANE_STATUS_NO_DOCS;
                    break;

                case PML_UPLOAD_ERROR_COVER_OPEN:
                    retcode = SANE_STATUS_COVER_OPEN;
                    break;

                case PML_UPLOAD_ERROR_DEVICE_BUSY:
                    retcode = SANE_STATUS_DEVICE_BUSY;
                    break;

                default:
                    retcode = SANE_STATUS_IO_ERROR;
                    break;
            }
        }
    }

    return retcode;
}


SANE_Status __attribute__ ((visibility ("hidden"))) hpaioScannerToSaneStatus( hpaioScanner_t hpaio )
{
    SANE_Status retcode;

    int sclStatus;

    retcode = SclInquire( hpaio->deviceid, hpaio->scan_channelid,
                              SCL_CMD_INQUIRE_DEVICE_PARAMETER,
                              SCL_INQ_ADF_FEED_STATUS,
                              &sclStatus,
                              0,
                              0 );

    if( retcode == SANE_STATUS_UNSUPPORTED )
    {
        retcode = SANE_STATUS_GOOD;
    }
    else if( retcode == SANE_STATUS_GOOD )
    {
        switch( sclStatus )
        {
            case SCL_ADF_FEED_STATUS_OK:
                retcode = SANE_STATUS_GOOD;
                break;

            case SCL_ADF_FEED_STATUS_BUSY:
                /* retcode=SANE_STATUS_DEVICE_BUSY; */
                retcode = SANE_STATUS_GOOD;
                break;

            case SCL_ADF_FEED_STATUS_PAPER_JAM:
            case SCL_ADF_FEED_STATUS_ORIGINAL_ON_GLASS:
                retcode = SANE_STATUS_JAMMED;
                break;

            case SCL_ADF_FEED_STATUS_PORTRAIT_FEED:
                retcode = SANE_STATUS_UNSUPPORTED;
                break;

            default:
                retcode = SANE_STATUS_IO_ERROR;
                break;
        }
    }

    return retcode;
}

static int hpaioScannerIsUninterruptible( hpaioScanner_t hpaio,
                                          int * pUploadState )
{
    int uploadState;
    if( !pUploadState )
    {
        pUploadState = &uploadState;
    }

    return ( hpaio->scannerType == SCANNER_TYPE_PML &&
             hpaio->pml.scanDone &&
             PmlRequestGet( hpaio->deviceid, hpaio->cmd_channelid, 
                            hpaio->pml.objUploadState ) != ERROR &&
             PmlGetIntegerValue( hpaio->pml.objUploadState, 0, pUploadState ) != ERROR &&
             ( *pUploadState == PML_UPLOAD_STATE_START ||
               *pUploadState == PML_UPLOAD_STATE_ACTIVE ||
               *pUploadState == PML_UPLOAD_STATE_NEWPAGE ) );
}

static SANE_Status hpaioResetScanner( hpaioScanner_t hpaio )
{
    SANE_Status retcode;

    if( hpaio->scannerType == SCANNER_TYPE_SCL )
    {
        retcode = SclSendCommand( hpaio->deviceid, hpaio->scan_channelid, SCL_CMD_RESET, 0 );
        if( retcode != SANE_STATUS_GOOD )
        {
            return retcode;
        }
        sleep(1);       /* delay for embeded jetdirect scl scanners (ie: PS 3300, PS C7280, PS C6100) */
    }
    else /* if (hpaio->scannerType==SCANNER_TYPE_PML) */
    {
        if( !hpaioScannerIsUninterruptible( hpaio, 0 ) )
        {
            PmlSetIntegerValue( hpaio->pml.objUploadState,
                                PML_TYPE_ENUMERATION,
                                PML_UPLOAD_STATE_IDLE );
                                
            if( PmlRequestSetRetry( hpaio->deviceid, hpaio->cmd_channelid, 
                                    hpaio->pml.objUploadState, 0, 0 ) == ERROR )
            {
                return SANE_STATUS_IO_ERROR;
            }
        }

        /* Clear upload error for the sake of the LaserJet 1100A. */
        PmlSetIntegerValue( hpaio->pml.objUploadError,
                            PML_TYPE_SIGNED_INTEGER,
                            0 );
                            
        PmlRequestSet( hpaio->deviceid, hpaio->cmd_channelid, hpaio->pml.objUploadError );  /* No retry. */
    }

    return SANE_STATUS_GOOD;
}

static PmlObject_t hpaioPmlAllocate( hpaioScanner_t hpaio )
{
    int size = sizeof( struct PmlObject_s );
    PmlObject_t obj;

    /* Malloc and zero object. */
    obj = malloc( size );

    memset( obj, 0, size );

    /* Insert into linked list of PML objects for this device. */
    if( !hpaio->firstPmlObject )
    {
        hpaio->firstPmlObject = obj;
    }
    obj->prev = hpaio->lastPmlObject;
    obj->next = 0;
    if( hpaio->lastPmlObject )
    {
        hpaio->lastPmlObject->next = obj;
    }
    hpaio->lastPmlObject = obj;

    return obj;
}

static PmlObject_t hpaioPmlAllocateID( hpaioScanner_t hpaio, char * oid )
{
    PmlObject_t obj = hpaioPmlAllocate( hpaio );

    if( !obj )
    {
        bug("hpaioPmlAllocateID: out of memory!\n");
    }

    PmlSetID( obj, oid );

    return obj;
}

static void hpaioPmlDeallocateObjects( hpaioScanner_t hpaio )
{
    PmlObject_t current, next;

    current = hpaio->firstPmlObject;
    
    while( current )
    {
        next = current->next;
        
        free( current );

        current = next;
    }
}

static SANE_Status hpaioPmlAllocateObjects(hpaioScanner_t hpaio)
{
        /* SNMP oids for PML scanners. */
        hpaio->pml.objScannerStatus = hpaioPmlAllocateID( hpaio,          "1.3.6.1.4.1.11.2.3.9.4.2.1.2.2.2.1.0" );
        hpaio->pml.objResolutionRange = hpaioPmlAllocateID( hpaio,        "1.3.6.1.4.1.11.2.3.9.4.2.1.2.2.2.3.0" );
        hpaio->pml.objUploadTimeout = hpaioPmlAllocateID( hpaio,          "1.3.6.1.4.1.11.2.3.9.4.2.1.1.1.18.0" );
        hpaio->pml.objContrast = hpaioPmlAllocateID( hpaio,               "1.3.6.1.4.1.11.2.3.9.4.2.1.2.2.1.1.0" );
        hpaio->pml.objResolution = hpaioPmlAllocateID( hpaio,             "1.3.6.1.4.1.11.2.3.9.4.2.1.2.2.1.2.0" );
        hpaio->pml.objPixelDataType = hpaioPmlAllocateID( hpaio,          "1.3.6.1.4.1.11.2.3.9.4.2.1.2.2.1.3.0" );
        hpaio->pml.objCompression = hpaioPmlAllocateID( hpaio,            "1.3.6.1.4.1.11.2.3.9.4.2.1.2.2.1.4.0" );
        hpaio->pml.objCompressionFactor = hpaioPmlAllocateID( hpaio,      "1.3.6.1.4.1.11.2.3.9.4.2.1.2.2.1.5.0" );
        hpaio->pml.objUploadError = hpaioPmlAllocateID( hpaio,            "1.3.6.1.4.1.11.2.3.9.4.2.1.2.2.1.6.0" );
        hpaio->pml.objUploadState = hpaioPmlAllocateID( hpaio,            "1.3.6.1.4.1.11.2.3.9.4.2.1.2.2.1.12.0" );
        hpaio->pml.objAbcThresholds = hpaioPmlAllocateID( hpaio,          "1.3.6.1.4.1.11.2.3.9.4.2.1.2.2.1.14.0" );
        hpaio->pml.objSharpeningCoefficient = hpaioPmlAllocateID( hpaio,  "1.3.6.1.4.1.11.2.3.9.4.2.1.2.2.1.15.0" );
        hpaio->pml.objNeutralClipThresholds = hpaioPmlAllocateID( hpaio,  "1.3.6.1.4.1.11.2.3.9.4.2.1.2.2.1.31.0" );
        hpaio->pml.objToneMap = hpaioPmlAllocateID( hpaio,                "1.3.6.1.4.1.11.2.3.9.4.2.1.2.2.1.32.0" );
        hpaio->pml.objCopierReduction = hpaioPmlAllocateID( hpaio,        "1.3.6.1.4.1.11.2.3.9.4.2.1.5.1.4.0" );
        hpaio->pml.objScanToken = hpaioPmlAllocateID( hpaio,              "1.3.6.1.4.1.11.2.3.9.4.2.1.1.1.25.0" );
        hpaio->pml.objModularHardware = hpaioPmlAllocateID( hpaio,        "1.3.6.1.4.1.11.2.3.9.4.2.1.2.2.1.75.0" );

        /* Some PML objects for SCL scanners. */
        hpaio->scl.objSupportedFunctions = hpaioPmlAllocateID( hpaio, "1.3.6.1.4.1.11.2.3.9.4.2.1.1.2.67.0" );

    return SANE_STATUS_GOOD;
}

static int hpaioConnClose( hpaioScanner_t hpaio )
{
    if (hpaio->cmd_channelid > 0)
       hpmud_close_channel(hpaio->deviceid, hpaio->cmd_channelid);
    hpaio->cmd_channelid = -1;
    if (hpaio->scan_channelid > 0)
       hpmud_close_channel(hpaio->deviceid, hpaio->scan_channelid);
    hpaio->scan_channelid = -1;

    return 0;
} // hpaioConnClose()

static SANE_Status hpaioConnOpen( hpaioScanner_t hpaio )
{
    SANE_Status retcode;
    enum HPMUD_RESULT stat;

    if (hpaio->scannerType==SCANNER_TYPE_SCL) 
    {
       stat = hpmud_open_channel(hpaio->deviceid, "HP-SCAN", &hpaio->scan_channelid);
       if(stat != HPMUD_R_OK)
       {
          bug("failed to open scan channel: %s %d\n", __FILE__, __LINE__);
          retcode = SANE_STATUS_DEVICE_BUSY;
          goto abort;
       }
    }

    stat = hpmud_open_channel(hpaio->deviceid, "HP-MESSAGE", &hpaio->cmd_channelid);
    if(stat != HPMUD_R_OK)
    {
       bug("failed to open pml channel: %s %d\n", __FILE__, __LINE__);
       retcode = SANE_STATUS_IO_ERROR;
       goto abort;
    }
    
    retcode = SANE_STATUS_GOOD;

abort:
    if( retcode != SANE_STATUS_GOOD )
    {
        SendScanEvent( hpaio->deviceuri, EVENT_SCANNER_FAIL);
    }
    return retcode;
}

static SANE_Status hpaioConnPrepareScan( hpaioScanner_t hpaio )
{
    SANE_Status retcode;
    int i;

    /* ADF may already have channel(s) open. */
    if (hpaio->cmd_channelid < 0)
    {
       retcode = hpaioConnOpen( hpaio );
    
       if( retcode != SANE_STATUS_GOOD )
       {
          return retcode;
       }
    }

    retcode = hpaioResetScanner( hpaio );

        /* Reserve scanner and make sure it got reserved. */
        SclSendCommand( hpaio->deviceid, hpaio->scan_channelid, SCL_CMD_SET_DEVICE_LOCK, 1 );
        SclSendCommand( hpaio->deviceid, hpaio->scan_channelid,
                        SCL_CMD_SET_DEVICE_LOCK_TIMEOUT,
                        SCL_DEVICE_LOCK_TIMEOUT );
                        
        for( i = 0; ; i++ )
        {
            char buffer[LEN_SCL_BUFFER];
            int len, j;
            struct timeval tv1, tv2;
            gettimeofday( &tv1, 0 );
            
            if( SclInquire( hpaio->deviceid, hpaio->scan_channelid,
                            SCL_CMD_INQUIRE_DEVICE_PARAMETER,
                            SCL_INQ_SESSION_ID,
                            &len,
                            buffer,
                            LEN_SCL_BUFFER ) != SANE_STATUS_GOOD )
            {
//                break;
                return SANE_STATUS_IO_ERROR;
            }
            
            gettimeofday( &tv2, 0 );
            
            for( j = 0; j < len && buffer[j] == '0'; j++ ) ;
            
            if( j < len )
            {
                break;
            }
            
            if( i >= SCL_PREPARE_SCAN_DEVICE_LOCK_MAX_RETRIES )
            {
                return SANE_STATUS_DEVICE_BUSY;
            }
            
            DBG(8, "hpaioConnPrepareScan: Waiting for device lock %s %d\n", __FILE__, __LINE__);
                     
            if( ( ( unsigned ) ( tv2.tv_sec - tv1.tv_sec ) ) <= SCL_PREPARE_SCAN_DEVICE_LOCK_DELAY )
            {
                sleep( SCL_PREPARE_SCAN_DEVICE_LOCK_DELAY );
            }
        }

    SendScanEvent( hpaio->deviceuri, EVENT_START_SCAN_JOB);
 
    return SANE_STATUS_GOOD;
}

static void hpaioConnEndScan( hpaioScanner_t hpaio )
{
    hpaioResetScanner( hpaio );
    hpaioConnClose( hpaio );
    
    SendScanEvent( hpaio->deviceuri, EVENT_END_SCAN_JOB);
}

static SANE_Status SetResolutionListSCL(hpaioScanner_t hpaio)
{
     int supported_res[] = {50, 75, 100, 150, 200, 300, 600, 1200, 2400, 4800, 9600};
     int i, len = sizeof(supported_res)/sizeof(int);
     
     if (hpaio->currentAdfMode == ADF_MODE_ADF || hpaio->currentAdfMode == ADF_MODE_AUTO)
     {
        hpaio->resolutionRange.min = hpaio->scl.minResAdf;
        hpaio->resolutionRange.max = hpaio->scl.maxResAdf;
     }
     else
     {
        hpaio->resolutionRange.min = hpaio->scl.minRes;
        hpaio->resolutionRange.max = hpaio->scl.maxRes;
     }
    
    _DBG("currentAdfMode=%d resolutionRange[%d, %d]\n", 
    hpaio->currentAdfMode,  hpaio->resolutionRange.min, hpaio->resolutionRange.max);

    NumListClear( hpaio->resolutionList );
    NumListClear( hpaio->lineartResolutionList );
    for (i = 0; i < len; i++)
    {
      if (supported_res[i] >= hpaio->resolutionRange.min &&
      supported_res[i] <= hpaio->resolutionRange.max)
     {
          NumListAdd (hpaio->resolutionList, supported_res[i]);
          NumListAdd (hpaio->lineartResolutionList, supported_res[i]);
      }
    }
    hpaio->option[OPTION_SCAN_RESOLUTION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
    
    return SANE_STATUS_GOOD;
}

static SANE_Status hpaioSetDefaultValue( hpaioScanner_t hpaio, int option )
{
    switch( option )
    {
        case OPTION_SCAN_MODE:
            if( hpaio->supportsScanMode[SCAN_MODE_COLOR] )
            {
                hpaio->currentScanMode = SCAN_MODE_COLOR;
            }
            else if( hpaio->supportsScanMode[SCAN_MODE_GRAYSCALE] )
            {
                hpaio->currentScanMode = SCAN_MODE_GRAYSCALE;
            }
            else /* if (hpaio->supportsScanMode[SCAN_MODE_LINEART]) */
            {
                hpaio->currentScanMode = SCAN_MODE_LINEART;
            }
            break;

        case OPTION_SCAN_RESOLUTION:
            if( hpaio->option[OPTION_SCAN_RESOLUTION].constraint_type == SANE_CONSTRAINT_WORD_LIST )
            {
                hpaio->currentResolution = NumListGetFirst( ( SANE_Int * )
                    hpaio->option[OPTION_SCAN_RESOLUTION].constraint.word_list );
            }
            else
            {
                hpaio->currentResolution = hpaio->resolutionRange.min;
            }
            break;

        case OPTION_CONTRAST:
            hpaio->currentContrast = hpaio->defaultContrast;
            break;
        case OPTION_BRIGHTNESS:
            hpaio->currentBrightness = hpaio->defaultBrightness;
            break;

        case OPTION_COMPRESSION:
            {
                int supportedCompression = hpaio->supportsScanMode[hpaio->currentScanMode];
                int defaultCompression = hpaio->defaultCompression[hpaio->currentScanMode];

                if( supportedCompression & defaultCompression )
                {
                    hpaio->currentCompression = defaultCompression;
                }
                else if( supportedCompression & COMPRESSION_JPEG )
                {
                    hpaio->currentCompression = COMPRESSION_JPEG;
                }
                else if( supportedCompression & COMPRESSION_MH )
                {
                    hpaio->currentCompression = COMPRESSION_MH;
                }
                else if( supportedCompression & COMPRESSION_MR )
                {
                    hpaio->currentCompression = COMPRESSION_MR;
                }
                else if( supportedCompression & COMPRESSION_MMR )
                {
                    hpaio->currentCompression = COMPRESSION_MMR;
                }
                else
                {
                    hpaio->currentCompression = COMPRESSION_NONE;
                }
            }
            break;

        case OPTION_JPEG_COMPRESSION_FACTOR:
            hpaio->currentJpegCompressionFactor = hpaio->defaultJpegCompressionFactor;
            break;

        case OPTION_BATCH_SCAN:
            hpaio->currentBatchScan = SANE_FALSE;
            break;

        case OPTION_ADF_MODE:
            if( hpaio->supportedAdfModes & ADF_MODE_AUTO )
            {
                if( hpaio->scannerType == SCANNER_TYPE_PML &&
                    !hpaio->pml.flatbedCapability &&
                    hpaio->supportedAdfModes & ADF_MODE_ADF )
                {
                    goto defaultToAdf;
                }
                hpaio->currentAdfMode = ADF_MODE_AUTO;
            }
            else if( hpaio->supportedAdfModes & ADF_MODE_FLATBED )
            {
                hpaio->currentAdfMode = ADF_MODE_FLATBED;
            }
            else if( hpaio->supportedAdfModes & ADF_MODE_ADF )
            {
                defaultToAdf:
                hpaio->currentAdfMode = ADF_MODE_ADF;
            }
            else
            {
                hpaio->currentAdfMode = ADF_MODE_AUTO;
            }
            break;
#if 1
        case OPTION_DUPLEX:
            hpaio->currentDuplex = SANE_FALSE;
            break;
#endif
        case OPTION_LENGTH_MEASUREMENT:
            hpaio->currentLengthMeasurement = LENGTH_MEASUREMENT_PADDED;
            break;

        case OPTION_TL_X:
            hpaio->currentTlx = hpaio->tlxRange.min;
            break;

        case OPTION_TL_Y:
            hpaio->currentTly = hpaio->tlyRange.min;
            break;

        case OPTION_BR_X:
            hpaio->currentBrx = hpaio->brxRange.max;
            break;

        case OPTION_BR_Y:
            hpaio->currentBry = hpaio->bryRange.max;
            break;

        default:
            return SANE_STATUS_INVAL;
    }

    return SANE_STATUS_GOOD;
}

static int hpaioUpdateDescriptors( hpaioScanner_t hpaio, int option )
{
    int initValues = ( option == OPTION_FIRST );
    int reload = 0;

    /* OPTION_SCAN_MODE: */
    if( initValues )
    {
        StrListClear( hpaio->scanModeList );
        if( hpaio->supportsScanMode[SCAN_MODE_LINEART] )
        {
            StrListAdd( hpaio->scanModeList, SANE_VALUE_SCAN_MODE_LINEART );
        }
        if( hpaio->supportsScanMode[SCAN_MODE_GRAYSCALE] )
        {
            StrListAdd( hpaio->scanModeList, SANE_VALUE_SCAN_MODE_GRAY );
        }
        if( hpaio->supportsScanMode[SCAN_MODE_COLOR] )
        {
            StrListAdd( hpaio->scanModeList, SANE_VALUE_SCAN_MODE_COLOR );
        }
        hpaioSetDefaultValue( hpaio, OPTION_SCAN_MODE );
        reload |= SANE_INFO_RELOAD_OPTIONS;
        reload |= SANE_INFO_RELOAD_PARAMS;
    }
    else if( option == OPTION_SCAN_MODE )
    {
        reload |= SANE_INFO_RELOAD_PARAMS;
    }
    
    if (hpaio->scannerType == SCANNER_TYPE_SCL)
            SetResolutionListSCL(hpaio);
            
    /* OPTION_SCAN_RESOLUTION: */
    if( hpaio->option[OPTION_SCAN_RESOLUTION].constraint_type ==
        SANE_CONSTRAINT_WORD_LIST )
    {
        SANE_Int ** pList = ( SANE_Int ** ) &hpaio->option[OPTION_SCAN_RESOLUTION].constraint.word_list;

        if( hpaio->currentScanMode == SCAN_MODE_LINEART )
        {
            if( *pList != hpaio->lineartResolutionList )
            {
                *pList = hpaio->lineartResolutionList;
                reload |= SANE_INFO_RELOAD_OPTIONS;
            }
        }
        else
        {
            if( *pList != hpaio->resolutionList )
            {
                *pList = hpaio->resolutionList;
                reload |= SANE_INFO_RELOAD_OPTIONS;
            }
        }
        if( initValues || !NumListIsInList( *pList,
                                             hpaio->currentResolution ) )
        {
            hpaioSetDefaultValue( hpaio, OPTION_SCAN_RESOLUTION );
            reload |= SANE_INFO_RELOAD_OPTIONS;
            reload |= SANE_INFO_RELOAD_PARAMS;
        }
    }
    else
    {
        if( initValues ||
            hpaio->currentResolution<hpaio->resolutionRange.min ||
            hpaio->currentResolution>hpaio->resolutionRange.max )
        {
            hpaioSetDefaultValue( hpaio, OPTION_SCAN_RESOLUTION );
            reload |= SANE_INFO_RELOAD_OPTIONS;
            reload |= SANE_INFO_RELOAD_PARAMS;
        }
    }
    if( option == OPTION_SCAN_RESOLUTION )
    {
        reload |= SANE_INFO_RELOAD_PARAMS;
    }

    /* OPTION_CONTRAST, OPTION_BRIGHTNESS */
    if( initValues )
    {
        hpaioSetDefaultValue( hpaio, OPTION_CONTRAST );
        hpaioSetDefaultValue (hpaio, OPTION_BRIGHTNESS);
		reload |= SANE_INFO_RELOAD_OPTIONS;
    }

    /* OPTION_COMPRESSION: */
    {
        int supportedCompression = hpaio->supportsScanMode[hpaio->currentScanMode];
        if( initValues ||
            !( supportedCompression & hpaio->currentCompression ) ||
            ( ( ( supportedCompression & COMPRESSION_NONE ) != 0 ) !=
              ( StrListIsInList( hpaio->compressionList,
                                      STR_COMPRESSION_NONE ) != 0 ) ) ||
            ( ( ( supportedCompression & COMPRESSION_MH ) != 0 ) !=
              ( StrListIsInList( hpaio->compressionList,
                                      STR_COMPRESSION_MH ) != 0 ) ) ||
            ( ( ( supportedCompression & COMPRESSION_MR ) != 0 ) !=
              ( StrListIsInList( hpaio->compressionList,
                                      STR_COMPRESSION_MR ) != 0 ) ) ||
            ( ( ( supportedCompression & COMPRESSION_MMR ) != 0 ) !=
              ( StrListIsInList( hpaio->compressionList,
                                      STR_COMPRESSION_MMR ) != 0 ) ) ||
            ( ( ( supportedCompression & COMPRESSION_JPEG ) != 0 ) !=
              ( StrListIsInList( hpaio->compressionList,
                                      STR_COMPRESSION_JPEG ) != 0 ) ) )
        {
            StrListClear( hpaio->compressionList );
            if( supportedCompression & COMPRESSION_NONE )
            {
                StrListAdd( hpaio->compressionList, STR_COMPRESSION_NONE );
            }
            if( supportedCompression & COMPRESSION_MH )
            {
                StrListAdd( hpaio->compressionList, STR_COMPRESSION_MH );
            }
            if( supportedCompression & COMPRESSION_MR )
            {
                StrListAdd( hpaio->compressionList, STR_COMPRESSION_MR );
            }
            if( supportedCompression & COMPRESSION_MMR )
            {
                StrListAdd( hpaio->compressionList, STR_COMPRESSION_MMR );
            }
            if( supportedCompression & COMPRESSION_JPEG )
            {
                StrListAdd( hpaio->compressionList, STR_COMPRESSION_JPEG );
            }
            hpaioSetDefaultValue( hpaio, OPTION_COMPRESSION );
            reload |= SANE_INFO_RELOAD_OPTIONS;
        }
    }

    /* OPTION_JPEG_COMPRESSION_FACTOR: */
    if( initValues ||
        ( ( hpaio->currentCompression == COMPRESSION_JPEG ) !=
          ( ( hpaio->option[OPTION_JPEG_COMPRESSION_FACTOR].cap & SANE_CAP_INACTIVE ) == 0 ) ) )
    {
        if( hpaio->currentCompression == COMPRESSION_JPEG )
        {
            hpaio->option[OPTION_JPEG_COMPRESSION_FACTOR].cap &= ~SANE_CAP_INACTIVE;
        }
        else
        {
            hpaio->option[OPTION_JPEG_COMPRESSION_FACTOR].cap |= SANE_CAP_INACTIVE;
        }
        hpaioSetDefaultValue( hpaio, OPTION_JPEG_COMPRESSION_FACTOR );
        reload |= SANE_INFO_RELOAD_OPTIONS;
    }

    /* OPTION_BATCH_SCAN: */
    if( initValues )
    {
        hpaioSetDefaultValue( hpaio, OPTION_BATCH_SCAN );
        if( hpaio->preDenali )
        {
            hpaio->option[OPTION_BATCH_SCAN].cap |= SANE_CAP_INACTIVE;
        }
        reload |= SANE_INFO_RELOAD_OPTIONS;
    }
    if( !hpaio->currentBatchScan )
    {
        hpaio->noDocsConditionPending = 0;
    }

    /* OPTION_ADF_MODE: */
    if( initValues )
    {
        StrListClear( hpaio->adfModeList );
        if( hpaio->supportedAdfModes & ADF_MODE_AUTO )
        {
            StrListAdd( hpaio->adfModeList, STR_ADF_MODE_AUTO );
        }
        if( hpaio->supportedAdfModes & ADF_MODE_FLATBED )
        {
            StrListAdd( hpaio->adfModeList, STR_ADF_MODE_FLATBED );
        }
        if( hpaio->supportedAdfModes & ADF_MODE_ADF )
        {
            StrListAdd( hpaio->adfModeList, STR_ADF_MODE_ADF );
        }
        hpaioSetDefaultValue( hpaio, OPTION_ADF_MODE );
        reload |= SANE_INFO_RELOAD_OPTIONS;
    }

#if 1
    /* OPTION_DUPLEX: */
    if( initValues ||
        ( ( hpaio->supportsDuplex &&
            hpaio->currentAdfMode != ADF_MODE_FLATBED ) !=
          ( ( hpaio->option[OPTION_DUPLEX].cap & SANE_CAP_INACTIVE ) == 0 ) ) )
    {
        if( hpaio->supportsDuplex &&
            hpaio->currentAdfMode != ADF_MODE_FLATBED )
        {
            hpaio->option[OPTION_DUPLEX].cap &= ~SANE_CAP_INACTIVE;
        }
        else
        {
            hpaio->option[OPTION_DUPLEX].cap |= SANE_CAP_INACTIVE;
        }
        hpaioSetDefaultValue( hpaio, OPTION_DUPLEX );
        reload |= SANE_INFO_RELOAD_OPTIONS;
    }
#endif

    /* OPTION_LENGTH_MEASUREMENT: */
    if( initValues )
    {
        hpaioSetDefaultValue( hpaio, OPTION_LENGTH_MEASUREMENT );
        StrListClear( hpaio->lengthMeasurementList );
        StrListAdd( hpaio->lengthMeasurementList,
                         STR_LENGTH_MEASUREMENT_UNKNOWN );
        if( hpaio->scannerType == SCANNER_TYPE_PML )
        {
            StrListAdd( hpaio->lengthMeasurementList,
                             STR_LENGTH_MEASUREMENT_UNLIMITED );
        }
        StrListAdd( hpaio->lengthMeasurementList,
                         STR_LENGTH_MEASUREMENT_APPROXIMATE );
        StrListAdd( hpaio->lengthMeasurementList,
                         STR_LENGTH_MEASUREMENT_PADDED );
        /* TODO: hpaioStrListAdd(hpaio->lengthMeasurementList,
          STR_LENGTH_MEASUREMENT_EXACT); */
    }

    /* OPTION_TL_X, OPTION_TL_Y, OPTION_BR_X, OPTION_BR_Y: */
    if( initValues )
    {
        hpaioSetDefaultValue( hpaio, OPTION_TL_X );
        hpaioSetDefaultValue( hpaio, OPTION_TL_Y );
        hpaioSetDefaultValue( hpaio, OPTION_BR_X );
        hpaioSetDefaultValue( hpaio, OPTION_BR_Y );
        reload |= SANE_INFO_RELOAD_OPTIONS;
        goto processGeometry;
    }
    else if( option == OPTION_TL_X ||
             option == OPTION_TL_Y ||
             option == OPTION_BR_X ||
             option == OPTION_BR_Y )
    {
        processGeometry : hpaio->effectiveTlx = hpaio->currentTlx;
        hpaio->effectiveBrx = hpaio->currentBrx;
        FIX_GEOMETRY( hpaio->effectiveTlx,
                      hpaio->effectiveBrx,
                      hpaio->brxRange.min,
                      hpaio->brxRange.max );
        hpaio->effectiveTly = hpaio->currentTly;
        hpaio->effectiveBry = hpaio->currentBry;
        FIX_GEOMETRY( hpaio->effectiveTly,
                      hpaio->effectiveBry,
                      hpaio->bryRange.min,
                      hpaio->bryRange.max );
        reload |= SANE_INFO_RELOAD_PARAMS;
    }
    if( ( hpaio->currentLengthMeasurement != LENGTH_MEASUREMENT_UNLIMITED ) !=
        ( ( hpaio->option[OPTION_BR_Y].cap & SANE_CAP_INACTIVE ) == 0 ) )
    {
        if( hpaio->currentLengthMeasurement == LENGTH_MEASUREMENT_UNLIMITED )
        {
            hpaio->option[OPTION_BR_Y].cap |= SANE_CAP_INACTIVE;
        }
        else
        {
            hpaio->option[OPTION_BR_Y].cap &= ~SANE_CAP_INACTIVE;
        }
        reload |= SANE_INFO_RELOAD_OPTIONS;
    }

    /* Pre-scan parameters: */
    if( reload & SANE_INFO_RELOAD_PARAMS )
    {
        switch( hpaio->currentScanMode )
        {
            case SCAN_MODE_LINEART:
                hpaio->prescanParameters.format = SANE_FRAME_GRAY;
                hpaio->prescanParameters.depth = 1;
                break;

            case SCAN_MODE_GRAYSCALE:
                hpaio->prescanParameters.format = SANE_FRAME_GRAY;
                hpaio->prescanParameters.depth = 8;
                break;

            case SCAN_MODE_COLOR:
            default:
                hpaio->prescanParameters.format = SANE_FRAME_RGB;
                hpaio->prescanParameters.depth = 8;
                break;
        }
        hpaio->prescanParameters.last_frame = SANE_TRUE;

        
        hpaio->prescanParameters.lines = MILLIMETERS_TO_PIXELS( hpaio->effectiveBry - hpaio->effectiveTly,
                                                                hpaio->currentResolution );
        
        hpaio->prescanParameters.pixels_per_line = MILLIMETERS_TO_PIXELS( hpaio->effectiveBrx - hpaio->effectiveTlx,
                                                                          hpaio->currentResolution );

        hpaio->prescanParameters.bytes_per_line = BYTES_PER_LINE( hpaio->prescanParameters.pixels_per_line,
                                                                  hpaio->prescanParameters.depth * ( hpaio->prescanParameters.format ==
                                                                                                     SANE_FRAME_RGB ?
                                                                                                     3 :
                                                                                                     1 ) );
    }

    return reload;
}

static void init_options( hpaioScanner_t hpaio )
{
    hpaio->option[OPTION_NUM_OPTIONS].name = SANE_NAME_NUM_OPTIONS;
    hpaio->option[OPTION_NUM_OPTIONS].title = SANE_TITLE_NUM_OPTIONS;
    hpaio->option[OPTION_NUM_OPTIONS].desc = SANE_DESC_NUM_OPTIONS;
    hpaio->option[OPTION_NUM_OPTIONS].type = SANE_TYPE_INT;
    hpaio->option[OPTION_NUM_OPTIONS].unit = SANE_UNIT_NONE;
    hpaio->option[OPTION_NUM_OPTIONS].size = sizeof( SANE_Int );
    hpaio->option[OPTION_NUM_OPTIONS].cap = SANE_CAP_SOFT_DETECT;
    hpaio->option[OPTION_NUM_OPTIONS].constraint_type = SANE_CONSTRAINT_NONE;

    hpaio->option[GROUP_SCAN_MODE].title = SANE_TITLE_SCAN_MODE;
    hpaio->option[GROUP_SCAN_MODE].type = SANE_TYPE_GROUP;

    hpaio->option[OPTION_SCAN_MODE].name = SANE_NAME_SCAN_MODE;
    hpaio->option[OPTION_SCAN_MODE].title = SANE_TITLE_SCAN_MODE;
    hpaio->option[OPTION_SCAN_MODE].desc = SANE_DESC_SCAN_MODE;
    hpaio->option[OPTION_SCAN_MODE].type = SANE_TYPE_STRING;
    hpaio->option[OPTION_SCAN_MODE].unit = SANE_UNIT_NONE;
    hpaio->option[OPTION_SCAN_MODE].size = LEN_STRING_OPTION_VALUE;
    hpaio->option[OPTION_SCAN_MODE].cap = SANE_CAP_SOFT_SELECT |
                                          SANE_CAP_SOFT_DETECT;
    hpaio->option[OPTION_SCAN_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
    hpaio->option[OPTION_SCAN_MODE].constraint.string_list = hpaio->scanModeList;

    hpaio->option[OPTION_SCAN_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
    hpaio->option[OPTION_SCAN_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
    hpaio->option[OPTION_SCAN_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
    hpaio->option[OPTION_SCAN_RESOLUTION].type = SANE_TYPE_INT;
    hpaio->option[OPTION_SCAN_RESOLUTION].unit = SANE_UNIT_DPI;
    hpaio->option[OPTION_SCAN_RESOLUTION].size = sizeof( SANE_Int );
    hpaio->option[OPTION_SCAN_RESOLUTION].cap = SANE_CAP_SOFT_SELECT |
                                                SANE_CAP_SOFT_DETECT;
    hpaio->option[OPTION_SCAN_RESOLUTION].constraint_type = SANE_CONSTRAINT_RANGE;
    hpaio->option[OPTION_SCAN_RESOLUTION].constraint.range = &hpaio->resolutionRange;
    hpaio->resolutionRange.quant = 0;

    hpaio->option[GROUP_ADVANCED].title = STR_TITLE_ADVANCED;
    hpaio->option[GROUP_ADVANCED].type = SANE_TYPE_GROUP;
    hpaio->option[GROUP_ADVANCED].cap = SANE_CAP_ADVANCED;

    hpaio->option[OPTION_CONTRAST].name = SANE_NAME_CONTRAST;
    hpaio->option[OPTION_CONTRAST].title = SANE_TITLE_CONTRAST;
    hpaio->option[OPTION_CONTRAST].desc = SANE_DESC_CONTRAST;
    hpaio->option[OPTION_CONTRAST].type = SANE_TYPE_INT;
    hpaio->option[OPTION_CONTRAST].unit = SANE_UNIT_NONE;
    hpaio->option[OPTION_CONTRAST].size = sizeof( SANE_Int );
    hpaio->option[OPTION_CONTRAST].cap = SANE_CAP_SOFT_SELECT |
                                         SANE_CAP_SOFT_DETECT |
                                         SANE_CAP_ADVANCED;
    hpaio->option[OPTION_CONTRAST].constraint_type = SANE_CONSTRAINT_RANGE;
    hpaio->option[OPTION_CONTRAST].constraint.range = &hpaio->contrastRange;
    hpaio->contrastRange.min = PML_CONTRAST_MIN;
    hpaio->contrastRange.max = PML_CONTRAST_MAX;
    hpaio->contrastRange.quant = 0;
    hpaio->defaultContrast = PML_CONTRAST_DEFAULT;

    hpaio->option[OPTION_BRIGHTNESS].name = SANE_NAME_BRIGHTNESS;
    hpaio->option[OPTION_BRIGHTNESS].title = SANE_TITLE_BRIGHTNESS;
    hpaio->option[OPTION_BRIGHTNESS].desc = SANE_DESC_BRIGHTNESS;
    hpaio->option[OPTION_BRIGHTNESS].type = SANE_TYPE_INT;
    hpaio->option[OPTION_BRIGHTNESS].unit = SANE_UNIT_NONE;
    hpaio->option[OPTION_BRIGHTNESS].size = sizeof( SANE_Int );
    hpaio->option[OPTION_BRIGHTNESS].cap = SANE_CAP_SOFT_SELECT |
                                         SANE_CAP_SOFT_DETECT |
                                         SANE_CAP_ADVANCED;
    hpaio->option[OPTION_BRIGHTNESS].constraint_type = SANE_CONSTRAINT_RANGE;
    hpaio->option[OPTION_BRIGHTNESS].constraint.range = &hpaio->brightnessRange;
    hpaio->brightnessRange.min = PML_BRIGHTNESS_MIN;
    hpaio->brightnessRange.max = PML_BRIGHTNESS_MAX;
    hpaio->brightnessRange.quant = 0;
    hpaio->defaultBrightness = PML_BRIGHTNESS_DEFAULT;
    
    hpaio->option[OPTION_COMPRESSION].name = STR_NAME_COMPRESSION;
    hpaio->option[OPTION_COMPRESSION].title = STR_TITLE_COMPRESSION;
    hpaio->option[OPTION_COMPRESSION].desc = STR_DESC_COMPRESSION;
    hpaio->option[OPTION_COMPRESSION].type = SANE_TYPE_STRING;
    hpaio->option[OPTION_COMPRESSION].unit = SANE_UNIT_NONE;
    hpaio->option[OPTION_COMPRESSION].size = LEN_STRING_OPTION_VALUE;
    hpaio->option[OPTION_COMPRESSION].cap = SANE_CAP_SOFT_SELECT |
                                            SANE_CAP_SOFT_DETECT |
                                            SANE_CAP_ADVANCED;
    hpaio->option[OPTION_COMPRESSION].constraint_type = SANE_CONSTRAINT_STRING_LIST;
    hpaio->option[OPTION_COMPRESSION].constraint.string_list = hpaio->compressionList;

    hpaio->option[OPTION_JPEG_COMPRESSION_FACTOR].name = STR_NAME_JPEG_QUALITY;
    hpaio->option[OPTION_JPEG_COMPRESSION_FACTOR].title = STR_TITLE_JPEG_QUALITY;
    hpaio->option[OPTION_JPEG_COMPRESSION_FACTOR].desc = STR_DESC_JPEG_QUALITY;
    hpaio->option[OPTION_JPEG_COMPRESSION_FACTOR].type = SANE_TYPE_INT;
    hpaio->option[OPTION_JPEG_COMPRESSION_FACTOR].unit = SANE_UNIT_NONE;
    hpaio->option[OPTION_JPEG_COMPRESSION_FACTOR].size = sizeof( SANE_Int );
    hpaio->option[OPTION_JPEG_COMPRESSION_FACTOR].cap = SANE_CAP_SOFT_SELECT |
                                                        SANE_CAP_SOFT_DETECT |
                                                        SANE_CAP_ADVANCED;
    hpaio->option[OPTION_JPEG_COMPRESSION_FACTOR].constraint_type = SANE_CONSTRAINT_RANGE;
    hpaio->option[OPTION_JPEG_COMPRESSION_FACTOR].constraint.range = &hpaio->jpegCompressionFactorRange;
    hpaio->jpegCompressionFactorRange.min = MIN_JPEG_COMPRESSION_FACTOR;
    hpaio->jpegCompressionFactorRange.max = MAX_JPEG_COMPRESSION_FACTOR;
    hpaio->jpegCompressionFactorRange.quant = 0;
    hpaio->defaultJpegCompressionFactor = SAFER_JPEG_COMPRESSION_FACTOR;

    hpaio->option[OPTION_BATCH_SCAN].name = STR_NAME_BATCH_SCAN;
    hpaio->option[OPTION_BATCH_SCAN].title = STR_TITLE_BATCH_SCAN;
    hpaio->option[OPTION_BATCH_SCAN].desc = STR_DESC_BATCH_SCAN;
    hpaio->option[OPTION_BATCH_SCAN].type = SANE_TYPE_BOOL;
    hpaio->option[OPTION_BATCH_SCAN].unit = SANE_UNIT_NONE;
    hpaio->option[OPTION_BATCH_SCAN].size = sizeof( SANE_Bool );
    hpaio->option[OPTION_BATCH_SCAN].cap = SANE_CAP_SOFT_SELECT |
                                           SANE_CAP_SOFT_DETECT |
                                           SANE_CAP_ADVANCED;
    hpaio->option[OPTION_BATCH_SCAN].constraint_type = SANE_CONSTRAINT_NONE;

    hpaio->option[OPTION_ADF_MODE].name = SANE_NAME_SCAN_SOURCE;  // xsane expects this.
    hpaio->option[OPTION_ADF_MODE].title = SANE_TITLE_SCAN_SOURCE;
    hpaio->option[OPTION_ADF_MODE].desc = SANE_DESC_SCAN_SOURCE;
    hpaio->option[OPTION_ADF_MODE].type = SANE_TYPE_STRING;
    hpaio->option[OPTION_ADF_MODE].unit = SANE_UNIT_NONE;
    hpaio->option[OPTION_ADF_MODE].size = LEN_STRING_OPTION_VALUE;
    hpaio->option[OPTION_ADF_MODE].cap = SANE_CAP_SOFT_SELECT |
                                         SANE_CAP_SOFT_DETECT |
                                         SANE_CAP_ADVANCED;
    hpaio->option[OPTION_ADF_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
    hpaio->option[OPTION_ADF_MODE].constraint.string_list = hpaio->adfModeList;

    // Duplex scanning is supported
    if (hpaio->supportsDuplex  == 1)
    {
       hpaio->option[OPTION_DUPLEX].name = STR_NAME_DUPLEX;
       hpaio->option[OPTION_DUPLEX].title = STR_TITLE_DUPLEX;
       hpaio->option[OPTION_DUPLEX].desc = STR_DESC_DUPLEX;
       hpaio->option[OPTION_DUPLEX].type = SANE_TYPE_BOOL;
       hpaio->option[OPTION_DUPLEX].unit = SANE_UNIT_NONE;
       hpaio->option[OPTION_DUPLEX].size = sizeof( SANE_Bool );
       hpaio->option[OPTION_DUPLEX].cap = SANE_CAP_SOFT_SELECT |
                                       SANE_CAP_SOFT_DETECT |
                                       SANE_CAP_ADVANCED;
       hpaio->option[OPTION_DUPLEX].constraint_type = SANE_CONSTRAINT_NONE;
    }
    hpaio->option[GROUP_GEOMETRY].title = STR_TITLE_GEOMETRY;
    hpaio->option[GROUP_GEOMETRY].type = SANE_TYPE_GROUP;
    hpaio->option[GROUP_GEOMETRY].cap = SANE_CAP_ADVANCED;

    hpaio->option[OPTION_LENGTH_MEASUREMENT].name = STR_NAME_LENGTH_MEASUREMENT;
    hpaio->option[OPTION_LENGTH_MEASUREMENT].title = STR_TITLE_LENGTH_MEASUREMENT;
    hpaio->option[OPTION_LENGTH_MEASUREMENT].desc = STR_DESC_LENGTH_MEASUREMENT;
    hpaio->option[OPTION_LENGTH_MEASUREMENT].type = SANE_TYPE_STRING;
    hpaio->option[OPTION_LENGTH_MEASUREMENT].unit = SANE_UNIT_NONE;
    hpaio->option[OPTION_LENGTH_MEASUREMENT].size = LEN_STRING_OPTION_VALUE;
    hpaio->option[OPTION_LENGTH_MEASUREMENT].cap = SANE_CAP_SOFT_SELECT |
                                                   SANE_CAP_SOFT_DETECT |
                                                   SANE_CAP_ADVANCED;
    hpaio->option[OPTION_LENGTH_MEASUREMENT].constraint_type = SANE_CONSTRAINT_STRING_LIST;
    hpaio->option[OPTION_LENGTH_MEASUREMENT].constraint.string_list = hpaio->lengthMeasurementList;

    hpaio->option[OPTION_TL_X].name = SANE_NAME_SCAN_TL_X;
    hpaio->option[OPTION_TL_X].title = SANE_TITLE_SCAN_TL_X;
    hpaio->option[OPTION_TL_X].desc = SANE_DESC_SCAN_TL_X;
    hpaio->option[OPTION_TL_X].type = GEOMETRY_OPTION_TYPE;
    hpaio->option[OPTION_TL_X].unit = SANE_UNIT_MM;
    hpaio->option[OPTION_TL_X].size = sizeof( SANE_Int );
    hpaio->option[OPTION_TL_X].cap = SANE_CAP_SOFT_SELECT |
                                     SANE_CAP_SOFT_DETECT;
    hpaio->option[OPTION_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
    hpaio->option[OPTION_TL_X].constraint.range = &hpaio->tlxRange;
    hpaio->tlxRange.min = 0;
    hpaio->tlxRange.quant = 0;

    hpaio->option[OPTION_TL_Y].name = SANE_NAME_SCAN_TL_Y;
    hpaio->option[OPTION_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
    hpaio->option[OPTION_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
    hpaio->option[OPTION_TL_Y].type = GEOMETRY_OPTION_TYPE;
    hpaio->option[OPTION_TL_Y].unit = SANE_UNIT_MM;
    hpaio->option[OPTION_TL_Y].size = sizeof( SANE_Int );
    hpaio->option[OPTION_TL_Y].cap = SANE_CAP_SOFT_SELECT |
                                     SANE_CAP_SOFT_DETECT;
    hpaio->option[OPTION_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
    hpaio->option[OPTION_TL_Y].constraint.range = &hpaio->tlyRange;
    hpaio->tlyRange.min = 0;
    hpaio->tlyRange.quant = 0;

    hpaio->option[OPTION_BR_X].name = SANE_NAME_SCAN_BR_X;
    hpaio->option[OPTION_BR_X].title = SANE_TITLE_SCAN_BR_X;
    hpaio->option[OPTION_BR_X].desc = SANE_DESC_SCAN_BR_X;
    hpaio->option[OPTION_BR_X].type = GEOMETRY_OPTION_TYPE;
    hpaio->option[OPTION_BR_X].unit = SANE_UNIT_MM;
    hpaio->option[OPTION_BR_X].size = sizeof( SANE_Int );
    hpaio->option[OPTION_BR_X].cap = SANE_CAP_SOFT_SELECT |
                                     SANE_CAP_SOFT_DETECT;
    hpaio->option[OPTION_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
    hpaio->option[OPTION_BR_X].constraint.range = &hpaio->brxRange;
    hpaio->brxRange.min = 0;
    hpaio->brxRange.quant = 0;

    hpaio->option[OPTION_BR_Y].name = SANE_NAME_SCAN_BR_Y;
    hpaio->option[OPTION_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
    hpaio->option[OPTION_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
    hpaio->option[OPTION_BR_Y].type = GEOMETRY_OPTION_TYPE;
    hpaio->option[OPTION_BR_Y].unit = SANE_UNIT_MM;
    hpaio->option[OPTION_BR_Y].size = sizeof( SANE_Int );
    hpaio->option[OPTION_BR_Y].cap = SANE_CAP_SOFT_SELECT |
                                     SANE_CAP_SOFT_DETECT;
    hpaio->option[OPTION_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
    hpaio->option[OPTION_BR_Y].constraint.range = &hpaio->bryRange;
    hpaio->bryRange.min = 0;
    hpaio->bryRange.quant = 0;
}

static int hpaioSclSendCommandCheckError( hpaioScanner_t hpaio, int cmd, int param )
{
    SANE_Status retcode;

    SclSendCommand( hpaio->deviceid, hpaio->scan_channelid, SCL_CMD_CLEAR_ERROR_STACK, 0 );

    retcode = SclSendCommand( hpaio->deviceid, hpaio->scan_channelid, cmd, param );
    
    if( retcode == SANE_STATUS_GOOD &&
        ( ( cmd != SCL_CMD_CHANGE_DOCUMENT && cmd != SCL_CMD_UNLOAD_DOCUMENT ) ||
          hpaio->beforeScan ) )
    {
        retcode = hpaioScannerToSaneError( hpaio );
    }

    return retcode;
}

static SANE_Status hpaioProgramOptions( hpaioScanner_t hpaio )
{
    int bytes_wrote;

    hpaio->effectiveScanMode = hpaio->currentScanMode;
    hpaio->effectiveResolution = hpaio->currentResolution;

        /* Set output data type and width. */
        switch( hpaio->currentScanMode )
        {
            case SCAN_MODE_LINEART:
                SclSendCommand( hpaio->deviceid, hpaio->scan_channelid,
                                SCL_CMD_SET_OUTPUT_DATA_TYPE,
                                SCL_DATA_TYPE_LINEART );
                SclSendCommand( hpaio->deviceid, hpaio->scan_channelid,
                                SCL_CMD_SET_DATA_WIDTH,
                                SCL_DATA_WIDTH_LINEART );
                break;
            
            case SCAN_MODE_GRAYSCALE:
                SclSendCommand( hpaio->deviceid, hpaio->scan_channelid,
                                SCL_CMD_SET_OUTPUT_DATA_TYPE,
                                SCL_DATA_TYPE_GRAYSCALE );
                SclSendCommand( hpaio->deviceid, hpaio->scan_channelid,
                                SCL_CMD_SET_DATA_WIDTH,
                                SCL_DATA_WIDTH_GRAYSCALE );
                break;
            
            case SCAN_MODE_COLOR:
            default:
                SclSendCommand( hpaio->deviceid, hpaio->scan_channelid,
                                SCL_CMD_SET_OUTPUT_DATA_TYPE,
                                SCL_DATA_TYPE_COLOR );
                SclSendCommand( hpaio->deviceid, hpaio->scan_channelid,
                                SCL_CMD_SET_DATA_WIDTH,
                                SCL_DATA_WIDTH_COLOR );
                break;
        }

        /* Set MFPDTF. */
        SclSendCommand( hpaio->deviceid, hpaio->scan_channelid,
                        SCL_CMD_SET_MFPDTF,
                        hpaio->mfpdtf ? SCL_MFPDTF_ON : SCL_MFPDTF_OFF );

        /* Set compression. */
        SclSendCommand( hpaio->deviceid, hpaio->scan_channelid,
                        SCL_CMD_SET_COMPRESSION,
                        ( hpaio->currentCompression ==
                          COMPRESSION_JPEG ? SCL_COMPRESSION_JPEG : SCL_COMPRESSION_NONE ) );

        /* Set JPEG compression factor. */
        SclSendCommand( hpaio->deviceid, hpaio->scan_channelid,
                        SCL_CMD_SET_JPEG_COMPRESSION_FACTOR,
                        hpaio->currentJpegCompressionFactor );

        /* Set X and Y resolution. */
        SclSendCommand( hpaio->deviceid, hpaio->scan_channelid,
                        SCL_CMD_SET_X_RESOLUTION,
                        hpaio->currentResolution );
        
        SclSendCommand( hpaio->deviceid, hpaio->scan_channelid,
                        SCL_CMD_SET_Y_RESOLUTION,
                        hpaio->currentResolution );

        /* Set X and Y position and extent. */
        SclSendCommand( hpaio->deviceid, hpaio->scan_channelid,
                        SCL_CMD_SET_X_POSITION,
                        MILLIMETERS_TO_DECIPIXELS( hpaio->effectiveTlx ) );
        
        SclSendCommand( hpaio->deviceid, hpaio->scan_channelid,
                        SCL_CMD_SET_Y_POSITION,
                        MILLIMETERS_TO_DECIPIXELS( hpaio->effectiveTly ) );
        
        SclSendCommand( hpaio->deviceid, hpaio->scan_channelid,
                        SCL_CMD_SET_X_EXTENT,
                        MILLIMETERS_TO_DECIPIXELS( hpaio->effectiveBrx -
                                                   hpaio->effectiveTlx ) );
                                                   
        SclSendCommand( hpaio->deviceid, hpaio->scan_channelid,
                        SCL_CMD_SET_Y_EXTENT,
                        MILLIMETERS_TO_DECIPIXELS( hpaio->effectiveBry -
                                                   hpaio->effectiveTly ) );
        /* Set Contrast */
        SclSendCommand( hpaio->deviceid, hpaio->scan_channelid,
                        SCL_CMD_SET_CONTRAST,
                        hpaio->currentContrast );

        /* Set Brightness */
        SclSendCommand( hpaio->deviceid, hpaio->scan_channelid,
                        SCL_CMD_SET_BRIGHTNESS,
                        hpaio->currentBrightness );


        /* Download color map to OfficeJet Pro 11xx. */
        if( hpaio->scl.compat & ( SCL_COMPAT_1150 | SCL_COMPAT_1170 ) )
        {
            SclSendCommand( hpaio->deviceid, hpaio->scan_channelid,
                            SCL_CMD_SET_DOWNLOAD_TYPE,
                            SCL_DOWNLOAD_TYPE_COLORMAP );
            
            SclSendCommand( hpaio->deviceid, hpaio->scan_channelid,
                            SCL_CMD_DOWNLOAD_BINARY_DATA,
                            sizeof( hp11xxSeriesColorMap ) );
            
            hpmud_write_channel(hpaio->deviceid, hpaio->scan_channelid, hp11xxSeriesColorMap, sizeof(hp11xxSeriesColorMap),
                        EXCEPTION_TIMEOUT, &bytes_wrote);
        }

        /* For OfficeJet R and PSC 500 series, set CCD resolution to 600
         * for lineart. */
        if( hpaio->scl.compat & SCL_COMPAT_R_SERIES &&
            hpaio->currentScanMode == SCAN_MODE_LINEART )
        {
            SclSendCommand( hpaio->deviceid, hpaio->scan_channelid, 
                            SCL_CMD_SET_CCD_RESOLUTION, 600 );
        }

    return SANE_STATUS_GOOD;
}

static SANE_Status hpaioAdvanceDocument(hpaioScanner_t hpaio)
{
    SANE_Status retcode = SANE_STATUS_GOOD;
    int documentLoaded = 0;

    DBG(8, "hpaioAdvanceDocument: papersource=%s batch=%d %s %d\n",
              hpaio->currentAdfMode==ADF_MODE_FLATBED ? "FLATBED" : hpaio->currentAdfMode==ADF_MODE_AUTO ? "AUTO" : "ADF",
              hpaio->currentBatchScan, __FILE__, __LINE__);

    if (hpaio->currentAdfMode == ADF_MODE_FLATBED)
       goto bugout;         /* nothing to do */

    /* If there is an ADF see if paper is loaded. */
    if (hpaio->supportedAdfModes & ADF_MODE_ADF)
    {
        if (hpaio->currentDuplex && hpaio->currentSideNumber == 2)
            documentLoaded = 1;//No need to check paper in ADF
        else
        {
            retcode = SclInquire(hpaio->deviceid, hpaio->scan_channelid, SCL_CMD_INQUIRE_DEVICE_PARAMETER,
                              SCL_INQ_ADF_DOCUMENT_LOADED, &documentLoaded, 0, 0);
            if (retcode != SANE_STATUS_GOOD)
                goto bugout;
        }
    }

    /* If in Batch mode, by definition we are in ADF mode. */
    if (hpaio->currentBatchScan && !documentLoaded)
    {
       retcode = SANE_STATUS_NO_DOCS;
       goto bugout;    /* no paper loaded */
    }

    /* If in Auto mode and no paper loaded use flatbed. */
    if (hpaio->currentAdfMode == ADF_MODE_AUTO && !documentLoaded)
       goto bugout;    /* no paper loaded, use flatbed */
    
    /* Assume ADF mode. */
    if (documentLoaded || (hpaio->currentSideNumber == 2) ) 
    {
        if (hpaio->currentDuplex)
        {
            /* Duplex change document. */
            if(hpaio->currentSideNumber == 2)
               hpaio->currentSideNumber = 1;
            else
               hpaio->currentSideNumber = 2;

            retcode=hpaioSclSendCommandCheckError(hpaio, SCL_CMD_CHANGE_DOCUMENT, SCL_CHANGE_DOC_DUPLEX); 
        }
        else 
        {
             /* Simplex change document. */
             retcode = hpaioSclSendCommandCheckError(hpaio, SCL_CMD_CHANGE_DOCUMENT, SCL_CHANGE_DOC_SIMPLEX);
        }
        hpaio->currentPageNumber++;
    }
    else
       retcode = SANE_STATUS_NO_DOCS;

bugout:

    DBG(8, "hpaioAdvanceDocument returns %d ADF-loaded=%d: %s %d\n", retcode, documentLoaded, __FILE__, __LINE__);

    return retcode;
}

static  struct hpaioScanner_s *create_sclpml_session()
{
   struct hpaioScanner_s *ps;

  if ((ps = malloc(sizeof(struct hpaioScanner_s))) == NULL)
  {
    return NULL;
  }
  memset(ps, 0, sizeof(struct hpaioScanner_s));
  ps->tag = "SCL-PML";
  ps->scan_channelid = -1;
  ps->cmd_channelid = -1;

  return ps;
}

static SANE_Status setSCLParams(hpaioScanner_t hpaio,     struct hpmud_model_attributes* ma)
{
    SANE_Status retcode = SANE_STATUS_UNSUPPORTED;
    int supportsMfpdtf = 1;
    
    /* Probing and setup for PML scanners... */
    SclSendCommand( hpaio->deviceid, hpaio->scan_channelid, SCL_CMD_CLEAR_ERROR_STACK, 0 );

    /* Probe the SCL model. */
    retcode = SclInquire( hpaio->deviceid, hpaio->scan_channelid,
                          SCL_CMD_INQUIRE_DEVICE_PARAMETER,
                          SCL_INQ_HP_MODEL_11, 0, hpaio->scl.compat1150,
                          LEN_MODEL_RESPONSE );

    if( retcode == SANE_STATUS_GOOD )
    {
        hpaio->scl.compat |= SCL_COMPAT_OFFICEJET;
    }
    else if( retcode != SANE_STATUS_UNSUPPORTED )
    {
        goto abort;
    }
    DBG(6, "scl.compat1150=<%s>: %s %d\n", hpaio->scl.compat1150, __FILE__, __LINE__);

    retcode = SclInquire( hpaio->deviceid, hpaio->scan_channelid,
                          SCL_CMD_INQUIRE_DEVICE_PARAMETER,
                          SCL_INQ_HP_MODEL_12, 0, hpaio->scl.compatPost1150,
                          LEN_MODEL_RESPONSE );

    if( retcode == SANE_STATUS_GOOD )
    {
        hpaio->scl.compat |= SCL_COMPAT_POST_1150;
    }
    else if( retcode != SANE_STATUS_UNSUPPORTED )
    {
        goto abort;
    }
    DBG(6, "scl.compatPost1150=<%s>: %s %d\n", hpaio->scl.compatPost1150, __FILE__, __LINE__);

    if( !hpaio->scl.compat )
    {
        SET_DEFAULT_MODEL( hpaio, "(unknown scanner)" );
    }
    else if( hpaio->scl.compat == SCL_COMPAT_OFFICEJET )
    {
        hpaio->scl.compat |= SCL_COMPAT_1150;
        SET_DEFAULT_MODEL( hpaio, "(OfficeJet 1150)" );
    }
    else if( !strcmp( hpaio->scl.compatPost1150, "5400A" ) )
    {
        hpaio->scl.compat |= SCL_COMPAT_1170;
        SET_DEFAULT_MODEL( hpaio, "(OfficeJet 1170)" );
    }
    else if( !strcmp( hpaio->scl.compatPost1150, "5500A" ) )
    {
        hpaio->scl.compat |= SCL_COMPAT_R_SERIES;
        SET_DEFAULT_MODEL( hpaio, "(OfficeJet R Series)" );
    }
    else if( !strcmp( hpaio->scl.compatPost1150, "5600A" ) )
    {
        hpaio->scl.compat |= SCL_COMPAT_G_SERIES;
        SET_DEFAULT_MODEL( hpaio, "(OfficeJet G Series)" );
    }
    else if( !strcmp( hpaio->scl.compatPost1150, "5700A" ) )
    {
        hpaio->scl.compat |= SCL_COMPAT_K_SERIES;
        SET_DEFAULT_MODEL( hpaio, "(OfficeJet K Series)" );
    }
    else if( !strcmp( hpaio->scl.compatPost1150, "5800A" ) )
    {
        hpaio->scl.compat |= SCL_COMPAT_D_SERIES;
        SET_DEFAULT_MODEL( hpaio, "(OfficeJet D Series)" );
    }
    else if( !strcmp( hpaio->scl.compatPost1150, "5900A" ) )
    {
        hpaio->scl.compat |= SCL_COMPAT_6100_SERIES;
        SET_DEFAULT_MODEL( hpaio, "(OfficeJet 6100 Series)" );
    }
    else
    {
        SET_DEFAULT_MODEL( hpaio, "(unknown OfficeJet)" );
    }
    DBG(6, "scl.compat=0x%4.4X: %s %d\n", hpaio->scl.compat, __FILE__, __LINE__);

    /* Decide which position/extent unit to use.  "Device pixels" works
     * better on most models, but the 1150 requires "decipoints." */
    if( hpaio->scl.compat & ( SCL_COMPAT_1150 ) )
    {
        hpaio->scl.decipixelChar = SCL_CHAR_DECIPOINTS;
        hpaio->decipixelsPerInch = DECIPOINTS_PER_INCH;
    }
    else
    {
        hpaio->scl.decipixelChar = SCL_CHAR_DEVPIXELS;
        hpaio->decipixelsPerInch = DEVPIXELS_PER_INCH;
        /* Check for non-default decipixelsPerInch definition. */
        SclInquire( hpaio->deviceid, hpaio->scan_channelid,
                    SCL_CMD_INQUIRE_DEVICE_PARAMETER,
                    SCL_INQ_DEVICE_PIXELS_PER_INCH,
                    &hpaio->decipixelsPerInch, 0, 0 );
    }
    DBG(6, "decipixelChar='%c', decipixelsPerInch=%d: %s %d\n", hpaio->scl.decipixelChar, hpaio->decipixelsPerInch, __FILE__, __LINE__);

    /* Is MFPDTF supported? */
    if( hpaioSclSendCommandCheckError( hpaio,
                                  SCL_CMD_SET_MFPDTF,
                                  SCL_MFPDTF_ON ) != SANE_STATUS_GOOD )
    {
        DBG(6, "Doesn't support MFPDTF: %s %d\n", __FILE__, __LINE__);
        supportsMfpdtf = 0;
    }

    /* All scan modes are supported with no compression. */
    hpaio->supportsScanMode[SCAN_MODE_LINEART] = COMPRESSION_NONE;
    hpaio->supportsScanMode[SCAN_MODE_GRAYSCALE] = COMPRESSION_NONE;
    hpaio->supportsScanMode[SCAN_MODE_COLOR] = COMPRESSION_NONE;

    if( supportsMfpdtf )
    {
        if( hpaioSclSendCommandCheckError( hpaio,
                                      SCL_CMD_SET_COMPRESSION,
                                      SCL_COMPRESSION_JPEG ) == SANE_STATUS_GOOD )
        {
            hpaio->supportsScanMode[SCAN_MODE_GRAYSCALE] |= COMPRESSION_JPEG;
            hpaio->supportsScanMode[SCAN_MODE_COLOR] |= COMPRESSION_JPEG;
        }
    }

    /* Determine the minimum and maximum resolution.
              * Probe for both X and Y, and pick largest min and smallest max.
             * For the 1150, set min to 50 to prevent scan head crashes (<42). */
    int minXRes, minYRes, maxXRes, maxYRes;
    retcode = SclInquire( hpaio->deviceid, hpaio->scan_channelid,
                SCL_CMD_INQUIRE_MINIMUM_VALUE,
                SCL_CMD_SET_X_RESOLUTION,
                &minXRes, 0, 0 );
    retcode = SclInquire( hpaio->deviceid, hpaio->scan_channelid,
                SCL_CMD_INQUIRE_MINIMUM_VALUE,
                SCL_CMD_SET_Y_RESOLUTION,
                &minYRes, 0, 0 );
    retcode = SclInquire( hpaio->deviceid, hpaio->scan_channelid,
                SCL_CMD_INQUIRE_MAXIMUM_VALUE,
                SCL_CMD_SET_X_RESOLUTION,
                &maxXRes, 0, 0 );
    retcode = SclInquire( hpaio->deviceid, hpaio->scan_channelid,
                SCL_CMD_INQUIRE_MAXIMUM_VALUE,
                SCL_CMD_SET_Y_RESOLUTION,
                &maxYRes, 0, 0 );

    _DBG("Flatbed minXRes=%d  minYRes=%d maxXRes=%d maxYRes=%d retcode=%d\n", 
    minXRes, minYRes, maxXRes, maxYRes, retcode);
    
    if( hpaio->scl.compat & SCL_COMPAT_1150 && minYRes < SCL_MIN_Y_RES_1150 )
        minYRes = SCL_MIN_Y_RES_1150;

    hpaio->scl.minRes = minXRes;
    if( hpaio->scl.minRes < minYRes )
        hpaio->scl.minRes = minYRes;
    
    hpaio->resolutionRange.min = hpaio->scl.minRes;
    
    hpaio->scl.maxRes = maxXRes;
    if( hpaio->scl.maxRes > maxYRes )
        hpaio->scl.maxRes = maxYRes;
    
    if( hpaio->scl.compat & ( SCL_COMPAT_1150 | SCL_COMPAT_1170 ) 
        && hpaio->scl.maxRes > SCL_MAX_RES_1150_1170 )
    {
        hpaio->scl.maxRes = SCL_MAX_RES_1150_1170;
    }
    hpaio->resolutionRange.max = hpaio->scl.maxRes;
                     
    /* Determine ADF/duplex capabilities. */
    retcode = SclInquire( hpaio->deviceid, hpaio->scan_channelid,
                SCL_CMD_INQUIRE_DEVICE_PARAMETER,
                SCL_INQ_ADF_CAPABILITY,
                &hpaio->scl.adfCapability, 0, 0 );
    
    DBG(6, "ADF capability=%d retcode=%d: %s %d\n", hpaio->scl.adfCapability, retcode,__FILE__, __LINE__);

    /*Set min-max resolution for ADF*/
    if (hpaio->scl.adfCapability)
    {
       retcode = SclInquire( hpaio->deviceid, hpaio->scan_channelid,
                SCL_CMD_INQUIRE_MINIMUM_VALUE,
                SCL_PSEUDO_ADF_X_RESOLUTION,
                &minXRes, 0, 0 );
      retcode = SclInquire( hpaio->deviceid, hpaio->scan_channelid,
                SCL_CMD_INQUIRE_MINIMUM_VALUE,
                SCL_PSEUDO_ADF_Y_RESOLUTION,
                &minYRes, 0, 0 );
      retcode = SclInquire( hpaio->deviceid, hpaio->scan_channelid,
                SCL_CMD_INQUIRE_MAXIMUM_VALUE,
                SCL_PSEUDO_ADF_X_RESOLUTION,
                &maxXRes, 0, 0 );
      retcode = SclInquire( hpaio->deviceid, hpaio->scan_channelid,
                SCL_CMD_INQUIRE_MAXIMUM_VALUE,
                SCL_PSEUDO_ADF_Y_RESOLUTION,
                &maxYRes, 0, 0 );
      _DBG("minXResAdf=%d minYResAdf=%d maxXResAdf=%d maxYResAdf=%d retcode=%d \n", 
        minXRes, minYRes,maxXRes, maxYRes, retcode);

      if( hpaio->scl.compat & SCL_COMPAT_1150 && minYRes < SCL_MIN_Y_RES_1150 )
          minYRes = SCL_MIN_Y_RES_1150;

      hpaio->scl.minResAdf = minXRes;
      if( hpaio->scl.minResAdf < minYRes )
          hpaio->scl.minResAdf = minYRes;
      
      hpaio->scl.maxResAdf = maxXRes;
      if( hpaio->scl.maxResAdf > maxYRes )
          hpaio->scl.maxResAdf = maxYRes;
    
      if( hpaio->scl.compat & ( SCL_COMPAT_1150 | SCL_COMPAT_1170 ) 
       && hpaio->scl.maxResAdf > SCL_MAX_RES_1150_1170 )
      {
          hpaio->scl.maxResAdf = SCL_MAX_RES_1150_1170;
      }
    }//end if (hpaio->scl.adfCapability)
    
    if(ma->scansrc & HPMUD_SCANSRC_FLATBED)
    {
         hpaio->scl.flatbedCapability = 1;
        hpaio->supportedAdfModes = ADF_MODE_FLATBED;
    }
    if (hpaio->scl.adfCapability)
    {
       if( hpaio->scl.compat & SCL_COMPAT_K_SERIES)
       {
            hpaio->supportedAdfModes  |= ADF_MODE_ADF;
       }
       else
       {
          int supportedFunctions;

          hpaio->supportedAdfModes |=  ADF_MODE_ADF;
          if (hpaio->scl.flatbedCapability)
                hpaio->supportedAdfModes |= ADF_MODE_AUTO;

          if( hpaio->scl.compat & ( SCL_COMPAT_1170 | SCL_COMPAT_R_SERIES |SCL_COMPAT_G_SERIES ) )
          {
              hpaio->scl.unloadAfterScan = 1;
          }
          if( PmlRequestGet( hpaio->deviceid, hpaio->cmd_channelid, hpaio->scl.objSupportedFunctions ) != ERROR &&
              PmlGetIntegerValue( hpaio->scl.objSupportedFunctions,
                                0,
                                &supportedFunctions ) != ERROR &&
            supportedFunctions & PML_SUPPFUNC_DUPLEX )
          {
              hpaio->supportsDuplex = 1;
          }
       }
    }

    /* Determine maximum X and Y extents. */
    SclInquire( hpaio->deviceid, hpaio->scan_channelid,
                SCL_CMD_INQUIRE_MAXIMUM_VALUE,
                SCL_CMD_SET_X_EXTENT,
                &hpaio->scl.maxXExtent, 0, 0 );
    
    SclInquire( hpaio->deviceid, hpaio->scan_channelid,
                SCL_CMD_INQUIRE_MAXIMUM_VALUE,
                SCL_CMD_SET_Y_EXTENT,
                &hpaio->scl.maxYExtent, 0, 0 );

    DBG(8, "Maximum extents: x=%d, y=%d %s %d\n", hpaio->scl.maxXExtent, hpaio->scl.maxYExtent, __FILE__, __LINE__);
    
    hpaio->tlxRange.max = hpaio->brxRange.max = DECIPIXELS_TO_MILLIMETERS( hpaio->scl.maxXExtent );
    hpaio->tlyRange.max = hpaio->bryRange.max = DECIPIXELS_TO_MILLIMETERS( hpaio->scl.maxYExtent );

    /* Allocate MFPDTF parser if supported. */
    if( supportsMfpdtf )
    {
        hpaio->mfpdtf = MfpdtfAllocate( hpaio->deviceid, hpaio->scan_channelid );
        MfpdtfSetChannel( hpaio->mfpdtf, hpaio->scan_channelid );
        
        if( hpaio->preDenali )
        {
            MfpdtfReadSetSimulateImageHeaders( hpaio->mfpdtf, 1 );
        }
    }

   retcode = SANE_STATUS_GOOD;
   
abort:
   return retcode;
}

static SANE_Status setPMLParams(hpaioScanner_t hpaio,  int forceJpegForGrayAndColor, 
       int force300dpiForLineart, int force300dpiForGrayscale)
{
    SANE_Status retcode = SANE_STATUS_INVAL;
    int comps = 0;
    int modularHardware = 0;
    
    hpaio->decipixelsPerInch = DECIPOINTS_PER_INCH;

    /* Determine supported scan modes and compression settings. */
    if( hpaio->preDenali )
    {
        comps |= COMPRESSION_MMR;
    }
    
    PmlSetIntegerValue( hpaio->pml.objCompression,
                        PML_TYPE_ENUMERATION,
                        PML_COMPRESSION_NONE );
    
    if( PmlRequestSetRetry( hpaio->deviceid, hpaio->cmd_channelid, 
                            hpaio->pml.objCompression, 0, 0 ) != ERROR )
    {
        comps |= COMPRESSION_NONE;
    }
    
    PmlSetIntegerValue( hpaio->pml.objCompression,
                        PML_TYPE_ENUMERATION,
                        PML_COMPRESSION_MH );
    
    if( PmlRequestSetRetry( hpaio->deviceid, hpaio->cmd_channelid, 
                            hpaio->pml.objCompression, 0, 0 ) != ERROR )
    {
        comps |= COMPRESSION_MH;
    }

    PmlSetIntegerValue( hpaio->pml.objCompression,
                        PML_TYPE_ENUMERATION,
                        PML_COMPRESSION_MR );
                        
    if( PmlRequestSetRetry( hpaio->deviceid, hpaio->cmd_channelid, 
                            hpaio->pml.objCompression, 0, 0 ) != ERROR )
    {
        comps |= COMPRESSION_MR;
    }
    
    PmlSetIntegerValue( hpaio->pml.objCompression,
                        PML_TYPE_ENUMERATION,
                        PML_COMPRESSION_MMR );
    
    if( PmlRequestSetRetry( hpaio->deviceid, hpaio->cmd_channelid, 
                            hpaio->pml.objCompression, 0, 0 ) != ERROR )
    {
        comps |= COMPRESSION_MMR;
    }
    
    PmlSetIntegerValue( hpaio->pml.objPixelDataType,
                        PML_TYPE_ENUMERATION,
                        PML_DATA_TYPE_LINEART );
    
    if( PmlRequestSetRetry( hpaio->deviceid, hpaio->cmd_channelid, 
                            hpaio->pml.objPixelDataType, 0, 0 ) != ERROR )
    {
        hpaio->supportsScanMode[SCAN_MODE_LINEART] = comps;
    }
        comps &= COMPRESSION_NONE;

    if( forceJpegForGrayAndColor )
    {
        comps = 0;
    }
    
    PmlSetIntegerValue( hpaio->pml.objCompression,
                        PML_TYPE_ENUMERATION,
                        PML_COMPRESSION_JPEG );
                        
    if( PmlRequestSetRetry( hpaio->deviceid, hpaio->cmd_channelid, 
                            hpaio->pml.objCompression, 0, 0 ) != ERROR )
    {
        comps |= COMPRESSION_JPEG;
    }
    
    PmlSetIntegerValue( hpaio->pml.objPixelDataType,
                        PML_TYPE_ENUMERATION,
                        PML_DATA_TYPE_GRAYSCALE );
    
    if( PmlRequestSetRetry( hpaio->deviceid, hpaio->cmd_channelid, 
                            hpaio->pml.objPixelDataType, 0, 0 ) != ERROR )
    {
        hpaio->supportsScanMode[SCAN_MODE_GRAYSCALE] = comps;
    }
    
    PmlSetIntegerValue( hpaio->pml.objPixelDataType,
                        PML_TYPE_ENUMERATION,
                        PML_DATA_TYPE_COLOR );
    
    if( PmlRequestSetRetry( hpaio->deviceid, hpaio->cmd_channelid, 
                            hpaio->pml.objPixelDataType, 0, 0 ) != ERROR )
    {
        hpaio->supportsScanMode[SCAN_MODE_COLOR] = comps;
    }

    /* Determine supported resolutions. */
    NumListClear( hpaio->resolutionList );
    NumListClear( hpaio->lineartResolutionList );
    
    if( hpaio->preDenali )
    {
        NumListAdd( hpaio->lineartResolutionList, 200 );
        if( !strcmp( hpaio->saneDevice.model, "OfficeJet_Series_300" ) )
        {
            NumListAdd( hpaio->lineartResolutionList, 300 );
        }
        hpaio->option[OPTION_SCAN_RESOLUTION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
    }
    else if( PmlRequestGet( hpaio->deviceid, hpaio->cmd_channelid, 
                            hpaio->pml.objResolutionRange ) == ERROR )
    {
pmlDefaultResRange:
        /* TODO: What are the correct X and Y resolution ranges
         * for the OfficeJet T series? */
        hpaio->resolutionRange.min = 75;
        hpaio->resolutionRange.max = 600;
    }
    else
    {
        char resList[PML_MAX_VALUE_LEN + 1];
        int i, len, res, consumed;

        PmlGetStringValue( hpaio->pml.objResolutionRange,
                           0,  resList, PML_MAX_VALUE_LEN );
        
        resList[PML_MAX_VALUE_LEN] = 0;
        len = strlen( resList );

        /* Parse "(100)x(100),(150)x(150),(200)x(200),(300)x(300)".
                         * This isn't quite the right way to do it, but it'll do. */
        for( i = 0; i < len; )
        {
            if( resList[i] < '0' || resList[i] > '9' )
            {
                i++;
                continue;
            }
            if( sscanf( resList + i, "%d%n", &res, &consumed ) != 1 )
            {
                break;
            }
            i += consumed;
            if( !force300dpiForGrayscale || res >= 300 )
            {
                NumListAdd( hpaio->resolutionList, res );
            }
            if( !force300dpiForLineart || res >= 300 )
            {
                NumListAdd( hpaio->lineartResolutionList, res );
            }
        }

        if( !NumListGetCount( hpaio->resolutionList ) )
        {
            goto pmlDefaultResRange;
        }
        hpaio->option[OPTION_SCAN_RESOLUTION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
    }


    /* Determine supported ADF modes. */
    if(PmlRequestGet(hpaio->deviceid, hpaio->cmd_channelid, hpaio->pml.objModularHardware) != ERROR && 
           PmlGetIntegerValue(hpaio->pml.objModularHardware, 0, &modularHardware) != ERROR)
    {
        hpaio->pml.flatbedCapability = 1;

        DBG(6, "Valid PML modularHardware object value=%x: %s %d\n", modularHardware, __FILE__, __LINE__);

        /* LJ3200 does not report ADF mode, so we force it. DES 8/5/08 */
        if (strncasecmp( hpaio->saneDevice.model, "hp_laserjet_3200", 16) == 0)
            modularHardware = PML_MODHW_ADF;

        if(modularHardware & PML_MODHW_ADF)
            hpaio->supportedAdfModes = ADF_MODE_AUTO | ADF_MODE_ADF;
        else
            hpaio->supportedAdfModes = ADF_MODE_FLATBED;
    }
    else
    {
        DBG(6, "No valid PML modularHardware object, default to ADF and AUTO support: %s %d\n", __FILE__, __LINE__);
        hpaio->supportedAdfModes = ADF_MODE_AUTO | ADF_MODE_ADF; 
    }
    hpaio->supportsDuplex = 0;

    hpaio->tlxRange.max = hpaio->brxRange.max = INCHES_TO_MILLIMETERS( PML_MAX_WIDTH_INCHES );
    hpaio->tlyRange.max = hpaio->bryRange.max = INCHES_TO_MILLIMETERS( PML_MAX_HEIGHT_INCHES );

   return retcode;
}

static SANE_Status filldata(hpaioScanner_t hpaio, struct hpmud_model_attributes* ma)
{
    SANE_Status retcode = SANE_STATUS_INVAL;
    int forceJpegForGrayAndColor = 0;
    int force300dpiForLineart = 0;
    int force300dpiForGrayscale = 0;
    
    /* Guess the command language (SCL or PML) based on the model string. */
    if( UNDEFINED_MODEL( hpaio ) )
    {
        hpaio->scannerType = SCANNER_TYPE_SCL;
    }
    else if( strcasestr( hpaio->saneDevice.model, "laserjet" ) )
    {
        hpaio->scannerType = SCANNER_TYPE_PML;
        hpaio->pml.openFirst = 1;
        
        if( strcasecmp( hpaio->saneDevice.model, "HP_LaserJet_1100" ) == 0 )
        {
            hpaio->pml.dontResetBeforeNextNonBatchPage = 1;
        }
        else
        {
            hpaio->pml.startNextBatchPageEarly = 1;
        }
    }
    else if( strcasecmp( hpaio->saneDevice.model, "OfficeJet" ) == 0 ||
             strcasecmp( hpaio->saneDevice.model, "OfficeJet_LX" ) == 0 ||
             strcasecmp( hpaio->saneDevice.model, "OfficeJet_Series_300" ) == 0 )
    {
        hpaio->scannerType = SCANNER_TYPE_PML;
        hpaio->preDenali = 1;
    }
    else if( strcasecmp( hpaio->saneDevice.model, "OfficeJet_Series_500" ) == 0 ||
             strcasecmp( hpaio->saneDevice.model, "All-in-One_IJP-V100" ) == 0 )
    {
        hpaio->scannerType = SCANNER_TYPE_PML;
        hpaio->fromDenali = 1;
        force300dpiForLineart = 1;
        force300dpiForGrayscale = 1;
        hpaio->defaultCompression[SCAN_MODE_LINEART] = COMPRESSION_MH;
        hpaio->defaultCompression[SCAN_MODE_GRAYSCALE] = COMPRESSION_JPEG;
        hpaio->defaultJpegCompressionFactor = SAFER_JPEG_COMPRESSION_FACTOR;
    }
    else if( strcasecmp( hpaio->saneDevice.model, "OfficeJet_Series_600" ) == 0 )
    {
        hpaio->scannerType = SCANNER_TYPE_PML;
        hpaio->denali = 1;
        forceJpegForGrayAndColor = 1;
        force300dpiForLineart = 1;
        hpaio->defaultCompression[SCAN_MODE_LINEART] = COMPRESSION_MH;
        hpaio->defaultJpegCompressionFactor = SAFER_JPEG_COMPRESSION_FACTOR;
    }
    else if( strcasecmp( hpaio->saneDevice.model, "Printer/Scanner/Copier_300" ) == 0 )
    {
        hpaio->scannerType = SCANNER_TYPE_PML;
        forceJpegForGrayAndColor = 1;
        force300dpiForLineart = 1;
        hpaio->defaultCompression[SCAN_MODE_LINEART] = COMPRESSION_MH;
        hpaio->defaultJpegCompressionFactor = SAFER_JPEG_COMPRESSION_FACTOR;
    }
    else if( strcasecmp( hpaio->saneDevice.model, "OfficeJet_Series_700" ) == 0 )
    {
        hpaio->scannerType = SCANNER_TYPE_PML;
        forceJpegForGrayAndColor = 1;
        force300dpiForLineart = 1;
        hpaio->defaultCompression[SCAN_MODE_LINEART] = COMPRESSION_MH;
        hpaio->defaultJpegCompressionFactor = SAFER_JPEG_COMPRESSION_FACTOR;
    }
    else if( strcasecmp( hpaio->saneDevice.model, "OfficeJet_T_Series" ) == 0 )
    {
        hpaio->scannerType = SCANNER_TYPE_PML;
        forceJpegForGrayAndColor = 1;
    }
    else
    {
        hpaio->scannerType = SCANNER_TYPE_SCL;
    }

    DBG(6, "Scanner type=%s: %s %d\n", hpaio->scannerType==0 ? "SCL" : "PML", __FILE__, __LINE__);

    hpaioPmlAllocateObjects(hpaio);   /* used by pml scanners and scl scanners */
    
    if ((retcode = hpaioConnOpen(hpaio)) != SANE_STATUS_GOOD)
    {
        goto abort;
    }

    if ((retcode = hpaioResetScanner(hpaio)) != SANE_STATUS_GOOD)
    {
        goto abort;
    }

    /* Probing and setup for SCL scanners... */
    if( hpaio->scannerType == SCANNER_TYPE_SCL )
    {
        setSCLParams(hpaio, ma);
        
    }
    else /* if (hpaio->scannerType==SCANNER_TYPE_PML) */
    {
         setPMLParams(hpaio, forceJpegForGrayAndColor,  force300dpiForLineart, force300dpiForGrayscale);
       
    }  /* if( hpaio->scannerType == SCANNER_TYPE_SCL ) */

    retcode = SANE_STATUS_GOOD;
    
abort:
    return retcode;
}

SANE_Status sclpml_open(SANE_String_Const device, SANE_Handle *pHandle)
{
    struct hpmud_model_attributes ma;
    SANE_Status retcode = SANE_STATUS_INVAL;
    
    int bytes_read;
    char deviceIDString[LEN_DEVICE_ID_STRING];
    char model[256];
    enum HPMUD_RESULT stat;
    
    if(session)
   {
     return SANE_STATUS_DEVICE_BUSY;
   }
   if((session = create_sclpml_session()) == NULL)
     return SANE_STATUS_NO_MEM;

   /* Set session to specified device. */
   snprintf(session->deviceuri, sizeof(session->deviceuri)-1, "hp:%s", device);   /* prepend "hp:" */

   /* Get actual model attributes from models.dat. */
   hpmud_query_model(session->deviceuri, &ma);

    // Set the duplex type supported
    if (ma.scantype == HPMUD_SCANTYPE_SCL_DUPLEX) 
       session->supportsDuplex = 1;
    else
       session->supportsDuplex = 0;


   if (hpmud_open_device(session->deviceuri, ma.mfp_mode, &session->deviceid) != HPMUD_R_OK)
   {
      stat = SANE_STATUS_IO_ERROR;
      goto abort;
   }
   
   /* Get the device ID string and initialize the SANE_Device structure. */
    memset(deviceIDString, 0, sizeof(deviceIDString));
    
    if(hpmud_get_device_id(session->deviceid, deviceIDString, sizeof(deviceIDString), &bytes_read) != HPMUD_R_OK)
    {
        retcode = SANE_STATUS_INVAL;
        goto abort;
    }
    
    DBG(6, "device ID string=<%s>: %s %d\n", deviceIDString, __FILE__, __LINE__);
    
    session->saneDevice.name = strdup( device ); 
    session->saneDevice.vendor = "Hewlett-Packard"; 
    hpmud_get_model(deviceIDString, model, sizeof(model));
    DBG(6, "Model = %s: %s %d\n", model, __FILE__, __LINE__);
    session->saneDevice.model = strdup(model);
    session->saneDevice.type = "multi-function peripheral";

    /* Initialize option descriptors. */
   init_options(session);
   session->currentSideNumber = 1;
   
   if(filldata(session, &ma) != SANE_STATUS_GOOD)
   {
      retcode = SANE_STATUS_INVAL;
      goto abort;
   }
    
done:
    /* Finish setting up option descriptors. */
    hpaioUpdateDescriptors( session, OPTION_FIRST );

    *pHandle = (SANE_Handle *)session;

    retcode = SANE_STATUS_GOOD;

abort:
    if( session )
    {
        hpaioConnClose( session );
    }
    if( retcode != SANE_STATUS_GOOD )
    {
        if( session )
        {
            if( session->saneDevice.name )
            {
                free( ( void * ) session->saneDevice.name );
            }
            if( session->saneDevice.model )
            {
                free( ( void * ) session->saneDevice.model );
            }
            if (session->mfpdtf)
            {
                MfpdtfDeallocate (session->mfpdtf);
            }
            free( session );
            session = NULL;
        }
    }
    return retcode;
}   /* sane_hpaio_open() */

void sclpml_close(SANE_Handle handle)
{
    hpaioScanner_t hpaio = (hpaioScanner_t) handle;

    DBG(8, "sane_hpaio_close(): %s %d\n", __FILE__, __LINE__); 
    if (hpaio == NULL || hpaio != session)
    {
      BUG("invalid sane_close\n");
      return;
     }

    hpaioPmlDeallocateObjects(hpaio);

    /* ADF may leave channel(s) open. */  
    if (hpaio->cmd_channelid > 0)
       hpaioConnEndScan(hpaio);
    
    if (hpaio->deviceid > 0)
    {
       hpmud_close_device(hpaio->deviceid);
       hpaio->deviceid = -1;
    }
    if( hpaio->saneDevice.name )
    {
        free( ( void * ) hpaio->saneDevice.name );
    }
    if( hpaio->saneDevice.model )
    {
        free( ( void * ) hpaio->saneDevice.model );
    }
    if (hpaio->mfpdtf)
    {
        MfpdtfDeallocate (hpaio->mfpdtf);
    }
    free(hpaio);
    session = NULL;
}

const SANE_Option_Descriptor * sclpml_get_option_descriptor(SANE_Handle handle, SANE_Int option)
{
    hpaioScanner_t hpaio = ( hpaioScanner_t ) handle;

    DBG(8, "sane_hpaio_get_option_descriptor(option=%s): %s %d\n", hpaio->option[option].name, __FILE__, __LINE__);

    if( option < 0 || option >= OPTION_LAST )
    {
        return 0;
    }

    return &hpaio->option[option];
}

SANE_Status sclpml_control_option(SANE_Handle handle, SANE_Int option, SANE_Action action, void *pValue, SANE_Int *pInfo)
{
    hpaioScanner_t hpaio = ( hpaioScanner_t ) handle;
    SANE_Int _info;
    SANE_Int * pIntValue = pValue;
    SANE_String pStrValue = pValue;
    SANE_Status retcode;
    char sz[64];

    if( !pInfo )
    {
        pInfo = &_info;
    }

    switch( action )
    {
        case SANE_ACTION_GET_VALUE:
            switch( option )
            {
                case OPTION_NUM_OPTIONS:
                    *pIntValue = OPTION_LAST;
                    break;

                case OPTION_SCAN_MODE:
                    switch( hpaio->currentScanMode )
                    {
                        case SCAN_MODE_LINEART:
                            strcpy( pStrValue, SANE_VALUE_SCAN_MODE_LINEART );
                            break;
                        case SCAN_MODE_GRAYSCALE:
                            strcpy( pStrValue, SANE_VALUE_SCAN_MODE_GRAY );
                            break;
                        case SCAN_MODE_COLOR:
                            strcpy( pStrValue, SANE_VALUE_SCAN_MODE_COLOR );
                            break;
                        default:
                            strcpy( pStrValue, STR_UNKNOWN );
                            break;
                    }
                    break;

                case OPTION_SCAN_RESOLUTION:
                    *pIntValue = hpaio->currentResolution;
                    break;

                case OPTION_CONTRAST:
                    *pIntValue = hpaio->currentContrast;
                    break;
                case OPTION_BRIGHTNESS:
                    *pIntValue = hpaio->currentBrightness;
                    break;

                case OPTION_COMPRESSION:
                    switch( hpaio->currentCompression )
                    {
                        case COMPRESSION_NONE:
                            strcpy( pStrValue, STR_COMPRESSION_NONE );
                            break;
                        case COMPRESSION_MH:
                            strcpy( pStrValue, STR_COMPRESSION_MH );
                            break;
                        case COMPRESSION_MR:
                            strcpy( pStrValue, STR_COMPRESSION_MR );
                            break;
                        case COMPRESSION_MMR:
                            strcpy( pStrValue, STR_COMPRESSION_MMR );
                            break;
                        case COMPRESSION_JPEG:
                            strcpy( pStrValue, STR_COMPRESSION_JPEG );
                            break;
                        default:
                            strcpy( pStrValue, STR_UNKNOWN );
                            break;
                    }
                    break;

                case OPTION_JPEG_COMPRESSION_FACTOR:
                    *pIntValue = hpaio->currentJpegCompressionFactor;
                    break;

                case OPTION_BATCH_SCAN:
                    *pIntValue = hpaio->currentBatchScan;
                    break;

                case OPTION_ADF_MODE:
                    switch( hpaio->currentAdfMode )
                    {
                        case ADF_MODE_AUTO:
                            strcpy( pStrValue, STR_ADF_MODE_AUTO );
                            break;
                        case ADF_MODE_FLATBED:
                            strcpy( pStrValue, STR_ADF_MODE_FLATBED );
                            break;
                        case ADF_MODE_ADF:
                            strcpy( pStrValue, STR_ADF_MODE_ADF );
                            break;
                        default:
                            strcpy( pStrValue, STR_UNKNOWN );
                            break;
                    }
                    break;
#if 1
                case OPTION_DUPLEX:
                    *pIntValue = hpaio->currentDuplex;
                    break;
#endif
                case OPTION_LENGTH_MEASUREMENT:
                    switch( hpaio->currentLengthMeasurement )
                    {
                        case LENGTH_MEASUREMENT_UNKNOWN:
                            strcpy( pStrValue, STR_LENGTH_MEASUREMENT_UNKNOWN );
                            break;
                        case LENGTH_MEASUREMENT_UNLIMITED:
                            strcpy( pStrValue,
                                    STR_LENGTH_MEASUREMENT_UNLIMITED );
                            break;
                        case LENGTH_MEASUREMENT_APPROXIMATE:
                            strcpy( pStrValue,
                                    STR_LENGTH_MEASUREMENT_APPROXIMATE );
                            break;
                        case LENGTH_MEASUREMENT_PADDED:
                            strcpy( pStrValue, STR_LENGTH_MEASUREMENT_PADDED );
                            break;
                        case LENGTH_MEASUREMENT_EXACT:
                            strcpy( pStrValue, STR_LENGTH_MEASUREMENT_EXACT );
                            break;
                        default:
                            strcpy( pStrValue, STR_UNKNOWN );
                            break;
                    }
                    break;

                case OPTION_TL_X:
                    *pIntValue = hpaio->currentTlx;
                    break;

                case OPTION_TL_Y:
                    *pIntValue = hpaio->currentTly;
                    break;

                case OPTION_BR_X:
                    *pIntValue = hpaio->currentBrx;
                    break;

                case OPTION_BR_Y:
                    *pIntValue = hpaio->currentBry;
                    break;

                default:
                    return SANE_STATUS_INVAL;
            }
            break;

        case SANE_ACTION_SET_VALUE:
            if( hpaio->option[option].cap & SANE_CAP_INACTIVE )
            {
                return SANE_STATUS_INVAL;
            }
            switch( option )
            {
                case OPTION_SCAN_MODE:
                    if( !strcasecmp( pStrValue, SANE_VALUE_SCAN_MODE_LINEART ) &&
                        hpaio->supportsScanMode[SCAN_MODE_LINEART] )
                    {
                        hpaio->currentScanMode = SCAN_MODE_LINEART;
                        break;
                    }
                    if( !strcasecmp( pStrValue, SANE_VALUE_SCAN_MODE_GRAY ) &&
                        hpaio->supportsScanMode[SCAN_MODE_GRAYSCALE] )
                    {
                        hpaio->currentScanMode = SCAN_MODE_GRAYSCALE;
                        break;
                    }
                    if( !strcasecmp( pStrValue, SANE_VALUE_SCAN_MODE_COLOR ) &&
                        hpaio->supportsScanMode[SCAN_MODE_COLOR] )
                    {
                        hpaio->currentScanMode = SCAN_MODE_COLOR;
                        break;
                    }
                    return SANE_STATUS_INVAL;

                case OPTION_SCAN_RESOLUTION:
                    if( ( hpaio->option[option].constraint_type ==
                          SANE_CONSTRAINT_WORD_LIST &&
                          !NumListIsInList( ( SANE_Int * )hpaio->option[option].constraint.word_list, *pIntValue ) ) ||
                          ( hpaio->option[option].constraint_type == SANE_CONSTRAINT_RANGE &&
                          ( *pIntValue<hpaio->resolutionRange.min ||
                            *pIntValue>hpaio->resolutionRange.max ) ) )
                    {
                        return SANE_STATUS_INVAL;
                    }
                    hpaio->currentResolution = *pIntValue;
                    break;

                case OPTION_CONTRAST:
                    if( *pIntValue<hpaio->contrastRange.min ||
                        *pIntValue>hpaio->contrastRange.max )
                    {
                        return SANE_STATUS_INVAL;
                    }
                    hpaio->currentContrast = *pIntValue;
                    break;
                case OPTION_BRIGHTNESS:
                    if( *pIntValue<hpaio->brightnessRange.min ||
                        *pIntValue>hpaio->brightnessRange.max )
                    {
                        return SANE_STATUS_INVAL;
                    }
                    hpaio->currentBrightness = *pIntValue;
                    break;

                case OPTION_COMPRESSION:
                    {
                        int supportedCompression = hpaio->supportsScanMode[hpaio->currentScanMode];
                        if( !strcasecmp( pStrValue, STR_COMPRESSION_NONE ) &&
                            supportedCompression & COMPRESSION_NONE )
                        {
                            hpaio->currentCompression = COMPRESSION_NONE;
                            break;
                        }
                        if( !strcasecmp( pStrValue, STR_COMPRESSION_MH ) &&
                            supportedCompression & COMPRESSION_MH )
                        {
                            hpaio->currentCompression = COMPRESSION_MH;
                            break;
                        }
                        if( !strcasecmp( pStrValue, STR_COMPRESSION_MR ) &&
                            supportedCompression & COMPRESSION_MR )
                        {
                            hpaio->currentCompression = COMPRESSION_MR;
                            break;
                        }
                        if( !strcasecmp( pStrValue, STR_COMPRESSION_MMR ) &&
                            supportedCompression & COMPRESSION_MMR )
                        {
                            hpaio->currentCompression = COMPRESSION_MMR;
                            break;
                        }
                        if( !strcasecmp( pStrValue, STR_COMPRESSION_JPEG ) &&
                            supportedCompression & COMPRESSION_JPEG )
                        {
                            hpaio->currentCompression = COMPRESSION_JPEG;
                            break;
                        }
                        return SANE_STATUS_INVAL;
                    }

                case OPTION_JPEG_COMPRESSION_FACTOR:
                    if( *pIntValue<MIN_JPEG_COMPRESSION_FACTOR ||
                        *pIntValue>MAX_JPEG_COMPRESSION_FACTOR )
                    {
                        return SANE_STATUS_INVAL;
                    }
                    hpaio->currentJpegCompressionFactor = *pIntValue;
                    break;

                case OPTION_BATCH_SCAN:
                    if( *pIntValue != SANE_FALSE && *pIntValue != SANE_TRUE )
                    {
                        return SANE_STATUS_INVAL;
                    }
                    hpaio->currentBatchScan = *pIntValue;
                    break;

                case OPTION_ADF_MODE:
                    if( !strcasecmp( pStrValue, STR_ADF_MODE_AUTO ) &&
                        hpaio->supportedAdfModes & ADF_MODE_AUTO )
                    {
                        hpaio->currentAdfMode = ADF_MODE_AUTO;
                        break;
                    }
                    if( !strcasecmp( pStrValue, STR_ADF_MODE_FLATBED ) &&
                        hpaio->supportedAdfModes & ADF_MODE_FLATBED )
                    {
                        hpaio->currentAdfMode = ADF_MODE_FLATBED;
                        break;
                    }
                    if( !strcasecmp( pStrValue, STR_ADF_MODE_ADF ) &&
                        hpaio->supportedAdfModes & ADF_MODE_ADF )
                    {
                        hpaio->currentAdfMode = ADF_MODE_ADF;
                        break;
                    }
                    return SANE_STATUS_INVAL;
#if 1
                case OPTION_DUPLEX:
                    if( *pIntValue != SANE_FALSE && *pIntValue != SANE_TRUE )
                    {
                        return SANE_STATUS_INVAL;
                    }
                    hpaio->currentDuplex = *pIntValue;
                    break;
#endif
                case OPTION_LENGTH_MEASUREMENT:
                    if( !strcasecmp( pStrValue,
                                     STR_LENGTH_MEASUREMENT_UNKNOWN ) )
                    {
                        hpaio->currentLengthMeasurement = LENGTH_MEASUREMENT_UNKNOWN;
                        break;
                    }
                    if( !strcasecmp( pStrValue,
                                     STR_LENGTH_MEASUREMENT_UNLIMITED ) )
                    {
                        if( hpaio->scannerType != SCANNER_TYPE_PML )
                        {
                            return SANE_STATUS_INVAL;
                        }
                        hpaio->currentLengthMeasurement = LENGTH_MEASUREMENT_UNLIMITED;
                        break;
                    }
                    if( !strcasecmp( pStrValue,
                                     STR_LENGTH_MEASUREMENT_APPROXIMATE ) )
                    {
                        hpaio->currentLengthMeasurement = LENGTH_MEASUREMENT_APPROXIMATE;
                        break;
                    }
                    if( !strcasecmp( pStrValue, STR_LENGTH_MEASUREMENT_PADDED ) )
                    {
                        hpaio->currentLengthMeasurement = LENGTH_MEASUREMENT_PADDED;
                        break;
                    }
                    if( !strcasecmp( pStrValue, STR_LENGTH_MEASUREMENT_EXACT ) )
                    {
                        hpaio->currentLengthMeasurement = LENGTH_MEASUREMENT_EXACT;
                        break;
                    }
                    return SANE_STATUS_INVAL;

                case OPTION_TL_X:
                    if( *pIntValue<hpaio->tlxRange.min ||
                        *pIntValue>hpaio->tlxRange.max )
                    {
                        return SANE_STATUS_INVAL;
                    }
                    hpaio->currentTlx = *pIntValue;
                    break;

                case OPTION_TL_Y:
                    if( *pIntValue<hpaio->tlyRange.min ||
                        *pIntValue>hpaio->tlyRange.max )
                    {
                        return SANE_STATUS_INVAL;
                    }
                    hpaio->currentTly = *pIntValue;
                    break;

                case OPTION_BR_X:
                    if( *pIntValue<hpaio->brxRange.min ||
                        *pIntValue>hpaio->brxRange.max )
                    {
                        return SANE_STATUS_INVAL;
                    }
                    hpaio->currentBrx = *pIntValue;
                    break;

                case OPTION_BR_Y:
                    if( *pIntValue<hpaio->bryRange.min ||
                        *pIntValue>hpaio->bryRange.max )
                    {
                        return SANE_STATUS_INVAL;
                    }
                    hpaio->currentBry = *pIntValue;
                    break;

                default:
                    return SANE_STATUS_INVAL;
            }
            goto reload;

        case SANE_ACTION_SET_AUTO:
            retcode = hpaioSetDefaultValue( hpaio, option );
            if( retcode != SANE_STATUS_GOOD )
            {
                return retcode;
            }
            reload : *pInfo = hpaioUpdateDescriptors( hpaio, option );
            break;

        default:
            return SANE_STATUS_INVAL;
    }

    DBG(8, "sane_hpaio_control_option (option=%s action=%s value=%s): %s %d\n", hpaio->option[option].name, 
                        action==SANE_ACTION_GET_VALUE ? "get" : action==SANE_ACTION_SET_VALUE ? "set" : "auto",
     pValue ? hpaio->option[option].type == SANE_TYPE_STRING ? (char *)pValue : psnprintf(sz, sizeof(sz), "%d", *(int *)pValue) : "na", __FILE__, __LINE__);

    return SANE_STATUS_GOOD;
}

SANE_Status sclpml_get_parameters(SANE_Handle handle, SANE_Parameters *pParams)
{
    hpaioScanner_t hpaio = ( hpaioScanner_t ) handle;
    char *s = "";

    if( !hpaio->hJob )
    {
        *pParams = hpaio->prescanParameters;
        s = "pre";
    }
    else
    {
        *pParams = hpaio->scanParameters;
    }
    DBG(8, "sane_hpaio_get_parameters(%sscan): format=%d, last_frame=%d, lines=%d, depth=%d, pixels_per_line=%d, bytes_per_line=%d %s %d\n",
                    s, pParams->format, pParams->last_frame, pParams->lines, pParams->depth, pParams->pixels_per_line, pParams->bytes_per_line, __FILE__, __LINE__);

    return SANE_STATUS_GOOD;
}

SANE_Status sclpml_start(SANE_Handle handle)
{
    hpaioScanner_t hpaio = ( hpaioScanner_t ) handle;
    SANE_Status retcode;
    IP_IMAGE_TRAITS traits;
    IP_XFORM_SPEC xforms[IP_MAX_XFORMS], * pXform = xforms;
    WORD wResult;
    
    DBG(8, "sane_hpaio_start(): %s %d deviceuri=%s\n", __FILE__, __LINE__, hpaio->deviceuri);

    hpaio->user_cancel = FALSE;

    hpaio->endOfData = 0;

    if (hpaio->scannerType==SCANNER_TYPE_PML)
        return pml_start(hpaio);

      /* TODO: convert to scl_start. des */

    /* If we just scanned the last page of a batch scan, then return the obligatory SANE_STATUS_NO_DOCS condition. */
    if( hpaio->noDocsConditionPending )
    {
        hpaio->noDocsConditionPending = 0;
        retcode = SANE_STATUS_NO_DOCS;
        goto abort;
    }
    /* Open scanner command channel. */
    retcode = hpaioConnPrepareScan( hpaio );
    
    if( retcode != SANE_STATUS_GOOD )
    {
        goto abort;
    }
    /* Change document if needed. */
    hpaio->beforeScan = 1;
    retcode = hpaioAdvanceDocument( hpaio );
    hpaio->beforeScan = 0;
    
    if( retcode != SANE_STATUS_GOOD )
    {
        goto abort;
    }

    /* Program options. */
    retcode = hpaioProgramOptions( hpaio );
    
    if( retcode != SANE_STATUS_GOOD )
    {
        goto abort;
    }

    hpaio->scanParameters = hpaio->prescanParameters;
    memset( xforms, 0, sizeof( xforms ) );
    traits.iPixelsPerRow = -1;
    
    switch( hpaio->effectiveScanMode )
    {
        case SCAN_MODE_LINEART:
            hpaio->scanParameters.format = SANE_FRAME_GRAY;
            hpaio->scanParameters.depth = 1;
            traits.iBitsPerPixel = 1;
            break;
        case SCAN_MODE_GRAYSCALE:
            hpaio->scanParameters.format = SANE_FRAME_GRAY;
            hpaio->scanParameters.depth = 8;
            traits.iBitsPerPixel = 8;
            break;
        case SCAN_MODE_COLOR:
        default:
            hpaio->scanParameters.format = SANE_FRAME_RGB;
            hpaio->scanParameters.depth = 8;
            traits.iBitsPerPixel = 24;
            break;
    }
    traits.lHorizDPI = hpaio->effectiveResolution << 16;
    traits.lVertDPI = hpaio->effectiveResolution << 16;
    traits.lNumRows = -1;
    traits.iNumPages = 1;
    traits.iPageNum = 1;

        int lines, pixelsPerLine;

        /* Inquire exact image dimensions. */
        if( SclInquire( hpaio->deviceid, hpaio->scan_channelid, SCL_CMD_INQUIRE_DEVICE_PARAMETER, SCL_INQ_NUMBER_OF_SCAN_LINES,
                        &lines, 0, 0 ) == SANE_STATUS_GOOD )
        {
            traits.lNumRows = lines;
        }
        SclInquire( hpaio->deviceid, hpaio->scan_channelid, SCL_CMD_INQUIRE_DEVICE_PARAMETER, SCL_INQ_PIXELS_PER_SCAN_LINE,
                    &pixelsPerLine, 0, 0 );
        
        traits.iPixelsPerRow = pixelsPerLine;

        SclSendCommand( hpaio->deviceid, hpaio->scan_channelid, SCL_CMD_CLEAR_ERROR_STACK, 0 );

        /* Start scanning. */
        SclSendCommand( hpaio->deviceid, hpaio->scan_channelid, SCL_CMD_SCAN_WINDOW, 0 );

    if( hpaio->mfpdtf )
    {
        MfpdtfSetChannel( hpaio->mfpdtf, hpaio->scan_channelid );

        //MfpdtfReadSetTimeout( hpaio->mfpdtf, MFPDTF_EARLY_READ_TIMEOUT );
        MfpdtfReadStart( hpaio->mfpdtf );  /* inits mfpdtf */
        
#ifdef HPAIO_DEBUG
        int log_output=1;
#else
        int log_output=0;
#endif        

        if( log_output )
        {
            char f[MAX_FILE_PATH_LEN];
            static int cnt=0;

            if (getenv("HOME"))
                sprintf(f, "%s/.hplip/mfpdtf_%d.out", getenv("HOME"), cnt++);
            else
                sprintf(f, "/tmp/mfpdtf_%d.out", cnt++);

            bug("saving raw image to %s \n", f);

            MfpdtfLogToFile( hpaio->mfpdtf,  f );
        }
        
        while( 1 )
        {
            int rService, sopEncoding;

            rService = MfpdtfReadService( hpaio->mfpdtf );
            
            if( retcode != SANE_STATUS_GOOD )
            {
                goto abort;
            }
            
            if( rService & MFPDTF_RESULT_ERROR_MASK )
            {
                retcode = SANE_STATUS_IO_ERROR;
                goto abort;
            }

            if( rService & MFPDTF_RESULT_NEW_VARIANT_HEADER && hpaio->preDenali )
            {
                union MfpdtfVariantHeader_u vheader;
                MfpdtfReadGetVariantHeader( hpaio->mfpdtf, &vheader, sizeof( vheader ) );
                
                traits.iPixelsPerRow = LEND_GET_SHORT( vheader.faxArtoo.pixelsPerRow );
                traits.iBitsPerPixel = 1;
                
                switch( vheader.faxArtoo.dataCompression )
                {
                    case MFPDTF_RASTER_MH:
                        sopEncoding = MFPDTF_RASTER_MH;
                        break;
                    case MFPDTF_RASTER_MR:
                        sopEncoding = MFPDTF_RASTER_MR;
                        break;
                    case MFPDTF_RASTER_MMR:
                    default:
                        sopEncoding = MFPDTF_RASTER_MMR;
                        break;
                }
                goto setupDecoder;
            }
            else if( rService & MFPDTF_RESULT_NEW_START_OF_PAGE_RECORD )
            {
                if( hpaio->currentCompression == COMPRESSION_NONE )
                {
                   goto rawDecode;
                }
                else /* if (hpaio->currentCompression==COMPRESSION_JPEG) */
                {
                   goto jpegDecode;
                }

                /* Read SOP record and set image pipeline input traits. */
		//                MfpdtfReadGetStartPageRecord( hpaio->mfpdtf, &sop, sizeof( sop ) );
		//                traits.iPixelsPerRow = LEND_GET_SHORT( sop.black.pixelsPerRow );
		//                traits.iBitsPerPixel = LEND_GET_SHORT( sop.black.bitsPerPixel );
		//                traits.lHorizDPI = LEND_GET_LONG( sop.black.xres );
		//                traits.lVertDPI = LEND_GET_LONG( sop.black.yres );
		//                sopEncoding = sop.encoding;

setupDecoder:
                /* Set up image-processing pipeline. */
                switch( sopEncoding )
                {
                    case MFPDTF_RASTER_MH:
                        pXform->aXformInfo[IP_FAX_FORMAT].dword = IP_FAX_MH;
                        ADD_XFORM( X_FAX_DECODE );
                        break;
                    case MFPDTF_RASTER_MR:
                        pXform->aXformInfo[IP_FAX_FORMAT].dword = IP_FAX_MR;
                        ADD_XFORM( X_FAX_DECODE );
                        break;
                    case MFPDTF_RASTER_MMR:
                        pXform->aXformInfo[IP_FAX_FORMAT].dword = IP_FAX_MMR;
                        ADD_XFORM( X_FAX_DECODE );
                        break;

                    case MFPDTF_RASTER_BITMAP:
                    case MFPDTF_RASTER_GRAYMAP:
                    case MFPDTF_RASTER_RGB:
rawDecode: 
                        break;

                    case MFPDTF_RASTER_JPEG:
jpegDecode:
                        pXform->aXformInfo[IP_JPG_DECODE_FROM_DENALI].dword = hpaio->fromDenali;
                        ADD_XFORM( X_JPG_DECODE );

                        pXform->aXformInfo[IP_CNV_COLOR_SPACE_WHICH_CNV].dword = IP_CNV_YCC_TO_SRGB;
                        pXform->aXformInfo[IP_CNV_COLOR_SPACE_GAMMA].dword = 0x00010000;
                        ADD_XFORM( X_CNV_COLOR_SPACE );
                        break;

                    case MFPDTF_RASTER_YCC411:
                    case MFPDTF_RASTER_PCL:
                    case MFPDTF_RASTER_NOT:
                    default:
                        /* Skip processing for unknown encodings. */
                        bug("unknown image encoding sane_start: name=%s sop=%d %s %d\n", hpaio->saneDevice.name,sopEncoding, __FILE__, __LINE__);
                }
                continue;
            }

            if( rService & MFPDTF_RESULT_IMAGE_DATA_PENDING )
            {
                /*MfpdtfReadSetTimeout( hpaio->mfpdtf, MFPDTF_LATER_READ_TIMEOUT );*/
                break;
            }
        }
    }
    hpaio->scanParameters.pixels_per_line = traits.iPixelsPerRow;
    hpaio->scanParameters.lines = traits.lNumRows;
    
    if( hpaio->scanParameters.lines < 0 )
    {
        hpaio->scanParameters.lines = MILLIMETERS_TO_PIXELS( hpaio->bryRange.max,
                                                             hpaio->effectiveResolution );
    }

        /* We have to invert bilevel data from SCL scanners. */
        if( hpaio->effectiveScanMode == SCAN_MODE_LINEART )
        {
            ADD_XFORM( X_INVERT );
        }
        else /* if (hpaio->effectiveScanMode==SCAN_MODE_COLOR) */
        {
            /* Do gamma correction on OfficeJet Pro 11xx. */
            if( hpaio->scl.compat & ( SCL_COMPAT_1150 | SCL_COMPAT_1170 ) )
            {
                pXform->aXformInfo[IP_TABLE_WHICH].dword = IP_TABLE_USER;
                pXform->aXformInfo[IP_TABLE_OPTION].pvoid = ( char * )hp11xxSeriesGammaTable;
                ADD_XFORM( X_TABLE );
            }
        }

    if( hpaio->currentLengthMeasurement == LENGTH_MEASUREMENT_PADDED )
    {
        pXform->aXformInfo[IP_PAD_LEFT].dword = 0;
        pXform->aXformInfo[IP_PAD_RIGHT].dword = 0;
        pXform->aXformInfo[IP_PAD_TOP].dword = 0;
        pXform->aXformInfo[IP_PAD_BOTTOM].dword = 0;
        pXform->aXformInfo[IP_PAD_VALUE].dword = ( hpaio->effectiveScanMode == SCAN_MODE_LINEART ) ? PAD_VALUE_LINEART : PAD_VALUE_GRAYSCALE_COLOR;
        pXform->aXformInfo[IP_PAD_MIN_HEIGHT].dword = hpaio->scanParameters.lines;
        ADD_XFORM( X_PAD );
    }

    /* If we didn't set up any xforms by now, then add the dummy "skel" xform to simplify our subsequent code path. */
    if( pXform == xforms )
    {
        ADD_XFORM( X_SKEL );
    }

    wResult = ipOpen( pXform - xforms, xforms, 0, &hpaio->hJob );
    
    if( wResult != IP_DONE || !hpaio->hJob )
    {
        retcode = SANE_STATUS_INVAL;
        goto abort;
    }

    traits.iComponentsPerPixel = ( ( traits.iBitsPerPixel % 3 ) ? 1 : 3 );
    wResult = ipSetDefaultInputTraits( hpaio->hJob, &traits );
    
    if( wResult != IP_DONE )
    {
        retcode = SANE_STATUS_INVAL;
        goto abort;
    }

    hpaio->scanParameters.bytes_per_line = BYTES_PER_LINE( hpaio->scanParameters.pixels_per_line,
                                hpaio->scanParameters.depth * ( hpaio->scanParameters.format == SANE_FRAME_RGB ? 3 : 1 ) );
    
    hpaio->totalBytesRemaining = hpaio->scanParameters.bytes_per_line * hpaio->scanParameters.lines;
    hpaio->bufferOffset = 0;
    hpaio->bufferBytesRemaining = 0;

    if( hpaio->currentLengthMeasurement == LENGTH_MEASUREMENT_UNKNOWN || hpaio->currentLengthMeasurement == LENGTH_MEASUREMENT_UNLIMITED )
    {
        hpaio->scanParameters.lines = -1;
    }
    else if( hpaio->currentLengthMeasurement == LENGTH_MEASUREMENT_EXACT )
    {
        /* TODO: Set up spool file, scan the whole image into it,
         * and set "hpaio->scanParameters.lines" accordingly.
         * Then in sane_hpaio_read, read out of the file. */
    }

    retcode = SANE_STATUS_GOOD;

abort:

    if( retcode != SANE_STATUS_GOOD )
    {
       if (retcode == SANE_STATUS_NO_DOCS) SendScanEvent (hpaio->deviceuri, EVENT_SCAN_ADF_NO_DOCS);
        sane_hpaio_cancel( handle );
    }
    return retcode;
}

SANE_Status sclpml_read(SANE_Handle handle, SANE_Byte *data, SANE_Int maxLength, SANE_Int *pLength)
{
    hpaioScanner_t hpaio = ( hpaioScanner_t ) handle;
    SANE_Status retcode;
    DWORD dwInputAvail;
    LPBYTE pbInputBuf;
    DWORD dwInputUsed, dwInputNextPos;
    DWORD dwOutputAvail = maxLength;
    LPBYTE pbOutputBuf = data;
    DWORD dwOutputUsed, dwOutputThisPos;
    WORD wResult;

    if (hpaio->user_cancel)  {
        bug("sane_hpaio_read(maxLength=%d): User cancelled!\n", maxLength);
        return SANE_STATUS_CANCELLED;
    } 

    *pLength = 0;

    if( !hpaio->hJob )
    {
        bug("sane_hpaio_read(maxLength=%d): No scan pending!\n", maxLength);
        retcode = SANE_STATUS_EOF;
        goto abort;
    }

    if (hpaio->scannerType==SCANNER_TYPE_PML)
    {
        retcode = pml_read(hpaio, data, maxLength, pLength);
        return retcode;
    }

    DBG(8, "sane_hpaio_read called handle=%p data=%p maxLength=%d length=%d: %s %d\n", (void *)handle, data, maxLength, *pLength, __FILE__, __LINE__);

    /* TODO: convert to scl_read. des */

needMoreData:
    if( hpaio->bufferBytesRemaining <= 0 && !hpaio->endOfData )
    {
        if( !hpaio->mfpdtf )
        {
            int r, len = hpaio->totalBytesRemaining;
            if( len <= 0 )
            {
                hpaio->endOfData = 1;
            }
            else
            {
                if( len > LEN_BUFFER )
                {
                    len = LEN_BUFFER;
                }
                
                r = ReadChannelEx(hpaio->deviceid, 
                                   hpaio->scan_channelid, 
                                   hpaio->inBuffer, 
                                   len,
                                   EXCEPTION_TIMEOUT);
                
                if( r < 0 )
                {
                    retcode = SANE_STATUS_IO_ERROR;
                    goto abort;
                }
                hpaio->bufferBytesRemaining = r;
                hpaio->totalBytesRemaining -= r;
            }
        }
        else
        {
            // mfpdtf
                int rService;

                rService = MfpdtfReadService( hpaio->mfpdtf );
                                
                if( rService & MFPDTF_RESULT_ERROR_MASK )
                {
		  //                    retcode = SANE_STATUS_IO_ERROR;
		  //                    goto abort;
                   hpaio->endOfData = 1;     /* display any data (ie: OJ F380 1200dpi non-compressed can timeout after last scan). */
                }

                if( rService & MFPDTF_RESULT_IMAGE_DATA_PENDING )
                {
                    hpaio->bufferBytesRemaining = MfpdtfReadInnerBlock( hpaio->mfpdtf, hpaio->inBuffer, LEN_BUFFER );
                    
                    rService = MfpdtfReadGetLastServiceResult( hpaio->mfpdtf );
                    
                    if( rService & MFPDTF_RESULT_ERROR_MASK )
                    {
                        retcode = SANE_STATUS_IO_ERROR;
                        goto abort;
                    }
                }
                else if( rService & MFPDTF_RESULT_NEW_END_OF_PAGE_RECORD || ( rService & MFPDTF_RESULT_END_PAGE && hpaio->preDenali ))
                {
                    hpaio->endOfData = 1;
                }

        } /* if (!hpaio->mfpdtf) */

        hpaio->bufferOffset = 0;
        if( hpaio->preDenali )
        {
            ipMirrorBytes( hpaio->inBuffer, hpaio->bufferBytesRemaining );
        }

    } /* if( hpaio->bufferBytesRemaining <= 0 && !hpaio->endOfData ) */

    dwInputAvail = hpaio->bufferBytesRemaining;

    if( hpaio->bufferBytesRemaining <= 0 && hpaio->endOfData )
    {
        pbInputBuf = 0;
    }
    else
    {
        pbInputBuf = hpaio->inBuffer + hpaio->bufferOffset;
    }

    wResult = ipConvert( hpaio->hJob,
                         dwInputAvail,
                         pbInputBuf,
                         &dwInputUsed,
                         &dwInputNextPos,
                         dwOutputAvail,
                         pbOutputBuf,
                         &dwOutputUsed,
                         &dwOutputThisPos );

    hpaio->bufferOffset += dwInputUsed;
    hpaio->bufferBytesRemaining -= dwInputUsed;
    *pLength = dwOutputUsed;
    
    if( wResult & ( IP_INPUT_ERROR | IP_FATAL_ERROR ) )
    {
        bug("ipConvert error=%x\n", wResult);
        retcode = SANE_STATUS_IO_ERROR;
        goto abort;
    }
    if( !dwOutputUsed )
    {
        if( wResult & IP_DONE )
        {
            retcode = SANE_STATUS_EOF;
            ipClose(hpaio->hJob);
            hpaio->hJob = 0;
            goto abort;
        }
        goto needMoreData;
    }

    retcode = SANE_STATUS_GOOD;

abort:
    if(!(retcode == SANE_STATUS_GOOD || retcode == SANE_STATUS_EOF))
    {
        sane_hpaio_cancel( handle );
    }

    DBG(8, "sane_hpaio_read returned output=%p outputUsed=%d length=%d status=%d: %s %d\n", pbOutputBuf, dwOutputUsed, *pLength, retcode, __FILE__, __LINE__);

    return retcode;
}

void sclpml_cancel(SANE_Handle handle)
{
    hpaioScanner_t hpaio = ( hpaioScanner_t ) handle;
    DBG(8, "sane_hpaio_cancel(): %s %d\n", __FILE__, __LINE__); 

    if (hpaio->user_cancel)  {
        bug("sane_hpaio_cancel: already cancelled!\n");
    }
    hpaio->user_cancel = TRUE;

    if (hpaio->scannerType==SCANNER_TYPE_PML)
    {
        pml_cancel(hpaio);
        return ;
    }

    /* TODO: convert to scl_cancel. des */

    if( hpaio->mfpdtf )
    {
        MfpdtfLogToFile( hpaio->mfpdtf, 0 );
        //MfpdtfDeallocate( hpaio->mfpdtf );
    }
    
    if( hpaio->hJob )
    {
        ipClose( hpaio->hJob );
        hpaio->hJob = 0;
    }
    
    /* Do not close pml/scan channels if in batch mode. */ 
    if (hpaio->currentBatchScan != SANE_TRUE && hpaio->cmd_channelid > 0)
       hpaioConnEndScan(hpaio);
}
