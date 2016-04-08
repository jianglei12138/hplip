/************************************************************************************\

  pml.c - HP SANE backend for multi-function peripherals (libsane-hpaio)

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

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "hpmud.h"
#include "io.h"
#include "common.h"
#include "pml.h"

#define DEBUG_DECLARE_ONLY
#include "sanei_debug.h"

int __attribute__ ((visibility ("hidden"))) PmlSetID( PmlObject_t obj, char * oid )
{
    int len = 0;    /* TODO: Do we need this parameter? */

    //DBG( 0,  "PmlSetID(obj=0x%8.8X)\n", obj );

    if( !len )
    {
        len = strlen( oid );
        if( !len )
        {
            len++;
        }
    }
    if( len > PML_MAX_OID_LEN )
    {
        return ERROR;
    }

    /* TODO: Disable trap (if enabled) on old OID. */

    memcpy( obj->oid, oid, len );
    obj->oid[len] = 0;

    obj->numberOfValidValues = 0;

    /* TODO: Clear out other trap-related fields. */

    //DBG( 0,  "PmlSetID(obj=0x%8.8X) returns OK.\n", obj );
    return OK;
}

static PmlValue_t PmlGetLastValue( PmlObject_t obj )
{
    if( obj->numberOfValidValues <= 0 )
    {
        return 0;
    }
    return &obj->value[obj->indexOfLastValue];
}

static PmlValue_t PmlPrepareNextValue( PmlObject_t obj )
{
    obj->indexOfLastValue = ( obj->indexOfLastValue + 1 ) %
                            PML_MAX_OID_VALUES;
    if( obj->numberOfValidValues < PML_MAX_OID_VALUES )
    {
        obj->numberOfValidValues++;
    }
    return &obj->value[obj->indexOfLastValue];
}

static int PmlSetPrefixValue( PmlObject_t obj,
                           int type,
                           char * prefix,
                           int lenPrefix,
                           char * value,
                           int lenValue )
{
    PmlValue_t v = PmlPrepareNextValue( obj );
    int r = ERROR;

    /*DBG( 0,  "PmlSetPrefixValue(obj=0x%8.8X,type=0x%4.4X,"
                    "lenPrefix=%d,lenValue=%d)\n",
                    obj,
                    type,
                    lenPrefix,
                    lenValue );*/

    if( lenPrefix < 0 ||
        lenValue<0 ||
        ( lenPrefix + lenValue )>PML_MAX_VALUE_LEN )
    {
        /*DBG( 0, "PmlSetPrefixValue(obj=0x%8.8X): "
                       "invalid lenPrefix=%d and/or lenValue=%d!\n",
                       obj,
                       lenPrefix,
                       lenValue );*/
        goto abort;
    }

    v->type = type;
    v->len = lenPrefix + lenValue;
    if( lenPrefix )
    {
        memcpy( v->value, prefix, lenPrefix );
    }
    if( lenValue )
    {
        memcpy( v->value + lenPrefix, value, lenValue );
    }
    v->value[lenPrefix + lenValue] = 0;

    r = OK;
abort:
    /*DBG( 0,  "PmlSetPrefixValue(obj=0x%8.8X) returns %d.\n",
                    obj,
                    r );*/
    return r;
}

int __attribute__ ((visibility ("hidden"))) PmlSetValue( PmlObject_t obj, int type, char * value, int len )
{
    return PmlSetPrefixValue( obj, type, 0, 0, value, len );
}

int __attribute__ ((visibility ("hidden"))) PmlSetIntegerValue( PmlObject_t obj, int type, int value )
{
    char buffer[sizeof( int )];
    int len = sizeof( int ), i = len - 1;

    while( 1 )
    {
        buffer[i] = value & 0xFF;
        value >>= 8;
        if( !i )
        {
            break;
        }
        i--;
    }
    for( ; !buffer[i] && i < ( len ); i++ )
        ;

    return PmlSetPrefixValue( obj, type, buffer + i, len - i, 0, 0 );
}

static int PmlGetPrefixValue( PmlObject_t obj,
                           int * pType,
                           char * prefix,
                           int lenPrefix,
                           char * buffer,
                           int maxlen )
{
    int len;
    PmlValue_t v = PmlGetLastValue( obj );

    if( !v )
    {
        return ERROR;
    }
    if( pType )
    {
        *pType = v->type;
    }
    if( !prefix && !buffer )
    {
        return OK;
    }

    if( lenPrefix < 0 || maxlen < 0 )
    {
        return ERROR;
    }

    if( v->len > lenPrefix + maxlen )
    {
        return ERROR;
    }
    if( v->len < lenPrefix )
    {
        return ERROR;
    }

    if( lenPrefix )
    {
        memcpy( prefix, v->value, lenPrefix );
    }
    len = v->len - lenPrefix;
    if( len )
    {
        memcpy( buffer, v->value + lenPrefix, len );
    }
    if( len < maxlen )
    {
        buffer[len] = 0;
    }

    return len;
}

int __attribute__ ((visibility ("hidden"))) PmlGetValue(PmlObject_t obj, int *pType, char *buffer, int maxlen)
{
    return PmlGetPrefixValue( obj, pType, 0, 0, buffer, maxlen );
}

int __attribute__ ((visibility ("hidden"))) PmlGetStringValue( PmlObject_t obj,
                           int * pSymbolSet,
                           char * buffer,
                           int maxlen )
{
    int type, len;
    unsigned char prefix[2];

    if( PmlGetPrefixValue( obj, &type, 0, 0, 0, 0 ) == ERROR )
    {
        return ERROR;
    }

    len = PmlGetPrefixValue( obj, &type, (char *)prefix, 2, buffer, maxlen );
    if( len == ERROR )
    {
        return ERROR;
    }
    if( pSymbolSet )
    {
        *pSymbolSet = ( ( prefix[0] << 8 ) | prefix[1] );
    }

    return len;
}

int __attribute__ ((visibility ("hidden"))) PmlGetIntegerValue( PmlObject_t obj, int * pType, int * pValue )
{
    int type;
    unsigned char svalue[sizeof( int )];
    int accum = 0, i, len;

    if( !pType )
    {
        pType = &type;
    }

    len = PmlGetPrefixValue( obj, pType, 0, 0, (char *)svalue, sizeof( int ) );
    /*if( len == ERROR )
            {
                return ERROR;
            }*/

    for( i = 0; i < len; i++ )
    {
        accum = ( ( accum << 8 ) | ( svalue[i] & 0xFF ) );
    }
    if( pValue )
    {
        *pValue = accum;
    }

    return OK;
}

static int PmlSetStatus( PmlObject_t obj, int status )
{
    obj->status = status;

    return status;
}

static int PmlGetStatus( PmlObject_t obj )
{
    return obj->status;
}

int __attribute__ ((visibility ("hidden"))) PmlRequestSet( int deviceid, int channelid, PmlObject_t obj )
{
    unsigned char data[PML_MAX_DATALEN];
    int datalen=0, status=ERROR, type, result, pml_result;

    PmlSetStatus(obj, PML_ERROR);
                
    datalen = PmlGetValue(obj, &type, (char *)data, sizeof(data));

    result = hpmud_set_pml(deviceid, channelid, obj->oid, type, data, datalen, &pml_result); 

    PmlSetStatus(obj, pml_result);

    if (result == HPMUD_R_OK)
        status = OK;

    return status;  /* OK = valid I/O result */
}

int __attribute__ ((visibility ("hidden"))) PmlRequestSetRetry( int deviceid, int channelid, PmlObject_t obj, int count, int delay )
{
   int stat=ERROR, r;

   if(count <= 0)
   {
      count = 10;
   }
   if(delay <= 0)
   {
      delay = 1;
   }
   while( 1 )
   {
      if ((r = PmlRequestSet(deviceid, channelid, obj)) == ERROR)
         goto bugout;
      if (PmlGetStatus(obj) == PML_ERROR_ACTION_CAN_NOT_BE_PERFORMED_NOW && count > 0)
      {
         sleep(delay);
         count--;
         continue;
      }
      break;
   }

   /* Check PML result. */
   if (PmlGetStatus(obj) & PML_ERROR)
   {
      DBG(6, "PML set failed: oid=%s count=%d delay=%d %s %d\n", obj->oid, count, delay, __FILE__, __LINE__);
      goto bugout;
   }

   stat = OK; 

bugout:
   return stat;  /* OK = valid I/O result AND PML result */
}

int __attribute__ ((visibility ("hidden"))) PmlRequestGet( int deviceid, int channelid, PmlObject_t obj ) 
{
    unsigned char data[PML_MAX_DATALEN];
    int datalen=0, stat=ERROR, type, pml_result;
    enum HPMUD_RESULT result;

    result = hpmud_get_pml(deviceid, channelid, obj->oid, data, sizeof(data), &datalen, &type, &pml_result); 

    PmlSetStatus(obj, pml_result);

    if (result == HPMUD_R_OK)
    {
       PmlSetValue(obj, type, (char *)data, datalen);
       stat = OK;
    }

    return stat; 
}

/*
 * Phase 2 rewrite. des
 */

static int is_zero(char *buf, int size)
{
   int i;

   for (i=0; i<size; i++)
   {
      if (buf[i] != 0)
         return 0;  /* no */
   }
   return 1; /* yes */
}

/* Unlock Scanner. */
static int clr_scan_token(HPAIO_RECORD *hpaio)
{
   int len, i, stat=ERROR;
   int max = sizeof(hpaio->pml.scanToken);

   if (PmlRequestGet(hpaio->deviceid, hpaio->cmd_channelid, hpaio->pml.objScanToken) == ERROR)
      goto bugout;
   len = PmlGetValue(hpaio->pml.objScanToken, 0, hpaio->pml.scanToken, max);

   if (len > 0 && !is_zero(hpaio->pml.scanToken, len))     
   {
      /* Zero token. */
      len = (len > max) ? max : len; 
      for(i=0; i<len; i++)
         hpaio->pml.scanToken[i] = 0;
      hpaio->pml.lenScanToken = len;
      if (PmlSetValue(hpaio->pml.objScanToken, PML_TYPE_BINARY, hpaio->pml.scanToken, len) == ERROR)
         goto bugout;
      if (PmlRequestSet(hpaio->deviceid, hpaio->cmd_channelid, hpaio->pml.objScanToken) == ERROR)
         goto bugout;
   }

   hpaio->pml.lenScanToken = len;
   stat = OK;

bugout:
   return stat;
}

/* Lock Scanner. */
static int set_scan_token(HPAIO_RECORD *hpaio)
{
   int stat=ERROR;

   /* Make sure token==0. */
   if (clr_scan_token(hpaio) == ERROR)
      goto bugout;

   if (hpaio->pml.lenScanToken > 0)
   {
      strncpy(hpaio->pml.scanToken, "555", hpaio->pml.lenScanToken);
      if (PmlSetValue(hpaio->pml.objScanToken, PML_TYPE_BINARY, hpaio->pml.scanToken, hpaio->pml.lenScanToken) == ERROR)
         goto bugout;
      if (PmlRequestSet(hpaio->deviceid, hpaio->cmd_channelid, hpaio->pml.objScanToken) == ERROR)
         goto bugout;
   }
   stat = OK;

bugout:
   return stat;
}

static int set_scan_parameters(HPAIO_RECORD *hpaio)
{
   int pixelDataType, stat=ERROR;
   struct PmlResolution resolution;
   int copierReduction = 100;
   int compression;

   hpaio->effectiveScanMode = hpaio->currentScanMode;
   hpaio->effectiveResolution = hpaio->currentResolution;

   /* Set upload timeout. */
   PmlSetIntegerValue(hpaio->pml.objUploadTimeout, PML_TYPE_SIGNED_INTEGER, PML_UPLOAD_TIMEOUT);
   if (PmlRequestSet(hpaio->deviceid, hpaio->cmd_channelid, hpaio->pml.objUploadTimeout) == ERROR)
      goto bugout;

   /* Set pixel data type. */
   switch(hpaio->currentScanMode)
   {
      case SCAN_MODE_LINEART:
         pixelDataType = PML_DATA_TYPE_LINEART;
         break;
      case SCAN_MODE_GRAYSCALE:
         pixelDataType = PML_DATA_TYPE_GRAYSCALE;
         break;
      case SCAN_MODE_COLOR:
      default:
         pixelDataType = PML_DATA_TYPE_COLOR;
         break;
   }
   PmlSetIntegerValue(hpaio->pml.objPixelDataType, PML_TYPE_ENUMERATION, pixelDataType);
   if (PmlRequestSet(hpaio->deviceid, hpaio->cmd_channelid, hpaio->pml.objPixelDataType) == ERROR)
      goto bugout;

   /* Set resolution. */
   BEND_SET_LONG(resolution.x, hpaio->currentResolution << 16);
   BEND_SET_LONG(resolution.y, hpaio->currentResolution << 16);
   PmlSetValue(hpaio->pml.objResolution, PML_TYPE_BINARY, (char *)&resolution, sizeof(resolution));
   if (PmlRequestSet(hpaio->deviceid, hpaio->cmd_channelid, hpaio->pml.objResolution) == ERROR)
      goto bugout;

   /* Set compression. */
   switch(hpaio->currentCompression)
   {
      case COMPRESSION_NONE:
         compression = PML_COMPRESSION_NONE;
         break;
      case COMPRESSION_MH:
         compression = PML_COMPRESSION_MH;
         break;
      case COMPRESSION_MR:
         compression = PML_COMPRESSION_MR;
         break;
      case COMPRESSION_MMR:
         compression = PML_COMPRESSION_MMR;
         break;
      case COMPRESSION_JPEG:
      default:
         compression = PML_COMPRESSION_JPEG;
         break;
   }
            
   PmlSetIntegerValue(hpaio->pml.objCompression, PML_TYPE_ENUMERATION, compression);
   if (PmlRequestSet(hpaio->deviceid, hpaio->cmd_channelid, hpaio->pml.objCompression) == ERROR)
      goto bugout;

   /* Set JPEG compression factor. */
   PmlSetIntegerValue(hpaio->pml.objCompressionFactor, PML_TYPE_SIGNED_INTEGER, hpaio->currentJpegCompressionFactor);
   if (PmlRequestSet(hpaio->deviceid, hpaio->cmd_channelid, hpaio->pml.objCompressionFactor) == ERROR)
      goto bugout;

#if 0  /* Removed, let host side perform contrast adjustments. des */
   /* Set scan contrast. */
   if (SANE_OPTION_IS_ACTIVE(hpaio->option[OPTION_CONTRAST].cap))
   {
      /* Note although settable, contrast is ignored by LJ3320, CLJ2840, LJ3055, LJ3050. */
      PmlSetIntegerValue(hpaio->pml.objContrast, PML_TYPE_SIGNED_INTEGER, hpaio->currentContrast);
      if (PmlRequestSet(hpaio->deviceid, hpaio->cmd_channelid, hpaio->pml.objContrast) == ERROR)
         goto bugout;
   }
#endif

   /* Set copier reduction. */
   PmlSetIntegerValue(hpaio->pml.objCopierReduction, PML_TYPE_SIGNED_INTEGER, copierReduction);
   if (PmlRequestSet(hpaio->deviceid, hpaio->cmd_channelid, hpaio->pml.objCopierReduction) == ERROR)
      goto bugout;

   stat = OK;

bugout:
   return stat;
}

static int pml_to_sane_status(HPAIO_RECORD *hpaio)
{
   int stat=SANE_STATUS_IO_ERROR, status;

   if (PmlRequestGet(hpaio->deviceid, hpaio->cmd_channelid, hpaio->pml.objScannerStatus) == ERROR)
      goto bugout;
   PmlGetIntegerValue(hpaio->pml.objScannerStatus, 0, &status);

   DBG(6, "PML scannerStatus=%x: %s %d\n", status, __FILE__, __LINE__);

   if(status & PML_SCANNER_STATUS_FEEDER_JAM)
   {
      stat = SANE_STATUS_JAMMED;
   }
   else if(status & PML_SCANNER_STATUS_FEEDER_OPEN)
   {
      stat = SANE_STATUS_COVER_OPEN;
   }
   else if(status & PML_SCANNER_STATUS_FEEDER_EMPTY)
   {
      if(hpaio->currentAdfMode == ADF_MODE_FLATBED || (hpaio->currentBatchScan == SANE_FALSE && hpaio->currentAdfMode == ADF_MODE_AUTO))
      {
         stat = SANE_STATUS_GOOD;
      }
      else
      {
         stat = SANE_STATUS_NO_DOCS;
      }
   }
   else if(status & PML_SCANNER_STATUS_INVALID_MEDIA_SIZE)
   {
      stat = SANE_STATUS_INVAL;
   }
   else if(status)
   {
      stat = SANE_STATUS_IO_ERROR;
   }
   else
   {
      stat = SANE_STATUS_GOOD;
   }

bugout:
    return stat;
}

static int check_pml_done(HPAIO_RECORD *hpaio)
{
   int stat=ERROR, state;

   /* See if pml side is done scanning. */
   if (PmlRequestGet(hpaio->deviceid, hpaio->cmd_channelid, hpaio->pml.objUploadState) == ERROR)
      goto bugout;
   PmlGetIntegerValue(hpaio->pml.objUploadState, 0, &state);
   hpaio->upload_state = state;
           
   if (state == PML_UPLOAD_STATE_DONE || state == PML_UPLOAD_STATE_NEWPAGE)
      hpaio->pml_done=1;
   else if (state != PML_UPLOAD_STATE_ACTIVE)
      goto bugout;
   else if (hpaio->ip_done && hpaio->mfpdtf_done)
   {
      if (hpaio->pml_timeout_cnt++ > 15)
      {
         bug("check_pml_done timeout cnt=%d: %s %d\n", hpaio->pml_timeout_cnt, __FILE__, __LINE__);
         goto bugout;
      }
      else
         sleep(1);
   }

   stat = OK;

bugout:
   return stat;
}

int __attribute__ ((visibility ("hidden"))) pml_start(HPAIO_RECORD *hpaio)
{
   MFPDTF_FIXED_HEADER *ph;
   MFPDTF_START_PAGE *ps;
   IP_IMAGE_TRAITS traits;
   IP_XFORM_SPEC xforms[IP_MAX_XFORMS], * pXform = xforms;
   int stat = SANE_STATUS_DEVICE_BUSY;
   int i, bsize, state, wResult, index, r;
   int oldStuff = (hpaio->preDenali || hpaio->fromDenali || hpaio->denali) ? 1 : 0;

   if (hpaio->cmd_channelid < 0)
   {
      if (hpmud_open_channel(hpaio->deviceid, "HP-MESSAGE", &hpaio->cmd_channelid) != HPMUD_R_OK)
      {
         bug("failed to open pml channel: %s %d\n", __FILE__, __LINE__);
         goto bugout;
      }
      SendScanEvent(hpaio->deviceuri, EVENT_START_SCAN_JOB); 
   }
   if (!oldStuff)
   {
      if (hpaio->scan_channelid < 0)
      {
         if (hpmud_open_channel(hpaio->deviceid, "HP-SCAN", &hpaio->scan_channelid) != HPMUD_R_OK)
         {
            bug("failed to open scan channel: %s %d\n", __FILE__, __LINE__);
            goto bugout;
         }
      }
   }

   r = pml_to_sane_status(hpaio);
   if (r != SANE_STATUS_GOOD)
   {
      stat = r;
      goto bugout;
   }

   /* Make sure scanner is idle. */
   if (PmlRequestGet(hpaio->deviceid, hpaio->cmd_channelid, hpaio->pml.objUploadState) == ERROR)
   {
      stat=SANE_STATUS_IO_ERROR;
      goto bugout;
   }
   PmlGetIntegerValue(hpaio->pml.objUploadState, 0, &state);
   DBG(6, "PML uploadState=%d before scan: %s %d\n", state, __FILE__, __LINE__);
   switch (state)
   {
      case PML_UPLOAD_STATE_IDLE:
         if (set_scan_token(hpaio) == ERROR)
            goto bugout;
         if (set_scan_parameters(hpaio) == ERROR)
            goto bugout;
         break;
      case PML_UPLOAD_STATE_NEWPAGE:
         break;
      case PML_UPLOAD_STATE_DONE:
         PmlSetIntegerValue(hpaio->pml.objUploadState, PML_TYPE_ENUMERATION, PML_UPLOAD_STATE_IDLE);
         if (PmlRequestSetRetry(hpaio->deviceid, hpaio->cmd_channelid, hpaio->pml.objUploadState, 0, 0) == ERROR)
            goto bugout;
         break;
      case PML_UPLOAD_STATE_START:
      case PML_UPLOAD_STATE_ACTIVE:
         goto bugout;                /* scanner is busy */
      case PML_UPLOAD_STATE_ABORTED:
      default:
         stat = hpaioScannerToSaneError(hpaio);
         PmlSetIntegerValue(hpaio->pml.objUploadState, PML_TYPE_ENUMERATION, PML_UPLOAD_STATE_IDLE);
         if (PmlRequestSetRetry(hpaio->deviceid, hpaio->cmd_channelid, hpaio->pml.objUploadState, 0, 0) == ERROR)
            goto bugout;
         break;
   }
         
   hpaio->scanParameters = hpaio->prescanParameters;
   memset(xforms, 0, sizeof(xforms));
   traits.iPixelsPerRow = -1;
    
   switch(hpaio->effectiveScanMode)
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

   /* Start scanning. */
   PmlSetIntegerValue(hpaio->pml.objUploadState, PML_TYPE_ENUMERATION, PML_UPLOAD_STATE_START);
   if (PmlRequestSetRetry(hpaio->deviceid, hpaio->cmd_channelid, hpaio->pml.objUploadState, 0, 0) == ERROR)
      goto bugout;

   /* Look for a confirmation that the scan started or failed. */
   for(i=0; i < PML_START_SCAN_WAIT_ACTIVE_MAX_RETRIES; i++)
   {
      if (PmlRequestGet(hpaio->deviceid, hpaio->cmd_channelid, hpaio->pml.objUploadState) == ERROR)
      {
         stat=SANE_STATUS_IO_ERROR;
         goto bugout;
      }
      PmlGetIntegerValue(hpaio->pml.objUploadState, 0, &state);
            
      if(state == PML_UPLOAD_STATE_ACTIVE)
         break;

      if(state != PML_UPLOAD_STATE_START)
         break;        /* bail */

      sleep(1);
   }

   if (state != PML_UPLOAD_STATE_ACTIVE)
   {
      /* Found and error, see if we can classify it otherwise use default. */
      r = hpaioScannerToSaneError(hpaio);
      if (r != SANE_STATUS_GOOD) 
         stat = r;
      goto bugout;
   }

   /* For older all-in-ones open the scan channel now. */
   if (oldStuff)
   {
      if (hpaio->scan_channelid < 0)
      {
         if (hpmud_open_channel(hpaio->deviceid, "HP-SCAN", &hpaio->scan_channelid) != HPMUD_R_OK)
            goto bugout;
      }
   }

   /* Find mfpdtf "New Page" block. */
   while (1)
   {
     if ((bsize = read_mfpdtf_block(hpaio->deviceid, hpaio->scan_channelid, (char *)hpaio->inBuffer, sizeof(hpaio->inBuffer), 45)) <= 0)
         goto bugout;  /* i/o error or timeout */

      ph = (MFPDTF_FIXED_HEADER *)hpaio->inBuffer;
      if ((ph->DataType == DT_SCAN) && (ph->PageFlag & PF_NEW_PAGE))
         break;  /* found it */
   }

   index = sizeof(MFPDTF_FIXED_HEADER);
   ps = (MFPDTF_START_PAGE *)(hpaio->inBuffer + index);
   if (ps->ID != ID_START_PAGE)
      goto bugout; 

   /* Read SOP record and set image pipeline input traits. */
   traits.iPixelsPerRow = le16toh(ps->BlackPixelsPerRow);
   traits.iBitsPerPixel = le16toh(ps->BlackBitsPerPixel);
   traits.lHorizDPI = le16toh(ps->BlackHorzDPI);
   traits.lVertDPI = le16toh(ps->BlackVertDPI);

   /* Set up image-processing pipeline. */
   switch(ps->Code)
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
         pXform->aXformInfo[IP_FAX_FORMAT].dword = IP_FAX_MMR;   /* possible lineart compression */
         ADD_XFORM( X_FAX_DECODE );
         break;
      case MFPDTF_RASTER_BITMAP:
      case MFPDTF_RASTER_GRAYMAP:
      case MFPDTF_RASTER_RGB:
         /* rawDecode */ 
         break;
      case MFPDTF_RASTER_JPEG:
         /* jpegDecode */
         pXform->aXformInfo[IP_JPG_DECODE_FROM_DENALI].dword = hpaio->fromDenali;
         ADD_XFORM( X_JPG_DECODE );
         pXform->aXformInfo[IP_CNV_COLOR_SPACE_WHICH_CNV].dword = IP_CNV_YCC_TO_SRGB;
         pXform->aXformInfo[IP_CNV_COLOR_SPACE_GAMMA].dword = 0x00010000;
         ADD_XFORM( X_CNV_COLOR_SPACE );
         break;
      default:
         /* Skip processing for unknown encodings. */
         bug("unknown image encoding sane_start: name=%s sop=%d %s %d\n", hpaio->saneDevice.name, ps->Code, __FILE__, __LINE__);
   }

   index += sizeof(MFPDTF_START_PAGE);
   hpaio->BlockSize = bsize;
   hpaio->BlockIndex = index; 
   hpaio->RecordSize = 0;
   hpaio->RecordIndex = 0; 
   hpaio->mfpdtf_done = 0;
   hpaio->pml_done = 0;
   hpaio->ip_done = 0;
   hpaio->page_done = 0;
   hpaio->mfpdtf_timeout_cnt = 0;
   hpaio->pml_timeout_cnt = 0;

   hpaio->scanParameters.pixels_per_line = traits.iPixelsPerRow;
   hpaio->scanParameters.lines = traits.lNumRows;
    
   if(hpaio->scanParameters.lines < 0)
   {
      hpaio->scanParameters.lines = MILLIMETERS_TO_PIXELS(hpaio->bryRange.max, hpaio->effectiveResolution);
   }

   int mmWidth = PIXELS_TO_MILLIMETERS(traits.iPixelsPerRow, hpaio->effectiveResolution);

   /* Set up X_CROP xform. */
   pXform->aXformInfo[IP_CROP_LEFT].dword = MILLIMETERS_TO_PIXELS( hpaio->effectiveTlx, hpaio->effectiveResolution );
   if( hpaio->effectiveBrx < hpaio->brxRange.max && hpaio->effectiveBrx < mmWidth )
   {
      pXform->aXformInfo[IP_CROP_RIGHT].dword = MILLIMETERS_TO_PIXELS( mmWidth-hpaio->effectiveBrx, hpaio->effectiveResolution );
   }
   pXform->aXformInfo[IP_CROP_TOP].dword = MILLIMETERS_TO_PIXELS( hpaio->effectiveTly, hpaio->effectiveResolution );
   if( hpaio->currentLengthMeasurement != LENGTH_MEASUREMENT_UNLIMITED )
   {
      hpaio->scanParameters.lines = pXform->aXformInfo[IP_CROP_MAXOUTROWS].dword = 
          MILLIMETERS_TO_PIXELS(hpaio->effectiveBry - hpaio->effectiveTly, hpaio->effectiveResolution);
   }
   hpaio->scanParameters.pixels_per_line -= pXform->aXformInfo[IP_CROP_LEFT].dword + pXform->aXformInfo[IP_CROP_RIGHT].dword;
   ADD_XFORM( X_CROP );

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
      stat = SANE_STATUS_INVAL;
      goto bugout;
   }

   traits.iComponentsPerPixel = ( ( traits.iBitsPerPixel % 3 ) ? 1 : 3 );
   wResult = ipSetDefaultInputTraits( hpaio->hJob, &traits );
    
   if( wResult != IP_DONE )
   {
      stat = SANE_STATUS_INVAL;
      goto bugout;
   }

   hpaio->scanParameters.bytes_per_line = 
          BYTES_PER_LINE(hpaio->scanParameters.pixels_per_line, hpaio->scanParameters.depth * (hpaio->scanParameters.format == SANE_FRAME_RGB ? 3 : 1));
    
   if( hpaio->currentLengthMeasurement == LENGTH_MEASUREMENT_UNKNOWN || hpaio->currentLengthMeasurement == LENGTH_MEASUREMENT_UNLIMITED )
   {
      hpaio->scanParameters.lines = -1;
   }

   stat = SANE_STATUS_GOOD;

bugout:
   return stat;
}

int __attribute__ ((visibility ("hidden"))) pml_read(HPAIO_RECORD *hpaio, SANE_Byte *data, SANE_Int maxLength, SANE_Int *pLength)
{
   MFPDTF_RASTER *pd;
   int stat=SANE_STATUS_IO_ERROR;
   unsigned int outputAvail=maxLength, outputUsed=0, outputThisPos;
   unsigned char *output = data;
   unsigned int inputAvail=0, inputUsed=0, inputNextPos;
   unsigned char *input;
   int bsize, wResult;

   DBG(8, "sane_hpaio_read called handle=%p data=%p maxLength=%d length=%d: %s %d\n", hpaio, data, maxLength, *pLength, __FILE__, __LINE__);

   /* Process any bytes in current record. */
   if (hpaio->RecordIndex < hpaio->RecordSize)
   {
      inputAvail = hpaio->RecordSize - hpaio->RecordIndex;
      input = hpaio->inBuffer + hpaio->BlockIndex + hpaio->RecordIndex + sizeof(MFPDTF_RASTER);

      /* Transform input data to output. Note, output buffer may consume more bytes than input buffer (ie: jpeg to raster). */
      wResult = ipConvert(hpaio->hJob, inputAvail, input, &inputUsed, &inputNextPos, outputAvail, output, &outputUsed, &outputThisPos);
      if(wResult & (IP_INPUT_ERROR | IP_FATAL_ERROR))
      {
         bug("ipConvert error=%x: %s %d\n", wResult, __FILE__, __LINE__);
         goto bugout;
      }
      *pLength = outputUsed;
      hpaio->RecordIndex += inputUsed;  /* bump record index */
      if (hpaio->RecordIndex >= hpaio->RecordSize)
         hpaio->BlockIndex += sizeof(MFPDTF_RASTER) + hpaio->RecordSize;  /* bump block index to next record */
   }
   else if (hpaio->BlockIndex < hpaio->BlockSize)
   {
      /* Process next record in current mfpdtf block. */
      pd = (MFPDTF_RASTER *)(hpaio->inBuffer + hpaio->BlockIndex);
      if (pd->ID == ID_RASTER_DATA)
      {
         /* Raster Record */
         hpaio->RecordSize = le16toh(pd->Size);
         hpaio->RecordIndex = 0;
      }
      else if (pd->ID == ID_END_PAGE)
      {
         /* End Page Record */
         hpaio->page_done = 1;
         hpaio->BlockIndex += sizeof(MFPDTF_END_PAGE);  /* bump index to next record */
      }  
      else
      {
         bug("unknown mfpdtf record id=%d: pml_read %s %d\n", pd->ID, __FILE__, __LINE__);
         goto bugout;
      }
   } 
   else if (!hpaio->mfpdtf_done)
   {
      /* Read new mfpdtf block. */
      if ((bsize = read_mfpdtf_block(hpaio->deviceid, hpaio->scan_channelid, (char *)hpaio->inBuffer, sizeof(hpaio->inBuffer), 1)) < 0)
         goto bugout;  /* i/o error */

      hpaio->BlockSize = 0;
      hpaio->BlockIndex = 0;

      if (bsize == 0)
      {
         if (hpaio->page_done || hpaio->pml_done)
            hpaio->mfpdtf_done = 1;  /* normal timeout */
         else if (hpaio->mfpdtf_timeout_cnt++ > 5)
         {
            bug("read_mfpdtf_block timeout cnt=%d: %s %d\n", hpaio->mfpdtf_timeout_cnt, __FILE__, __LINE__);
            goto bugout;
         }
         else
         { 
            if (check_pml_done(hpaio) == ERROR)
               goto bugout;
         }
      }
      else
      {
         hpaio->mfpdtf_timeout_cnt = 0;

         if (bsize > sizeof(MFPDTF_FIXED_HEADER))
         {
            hpaio->BlockSize = bsize;  /* set for next sane_read */
            hpaio->BlockIndex = sizeof(MFPDTF_FIXED_HEADER);
         }
      }
   } 
   else if ((hpaio->page_done || hpaio->pml_done) && !hpaio->ip_done)
   {
      /* No more scan data, flush ipconvert pipeline. */
      input = NULL;
      wResult = ipConvert(hpaio->hJob, inputAvail, input, &inputUsed, &inputNextPos, outputAvail, output, &outputUsed, &outputThisPos);
      if (wResult & (IP_INPUT_ERROR | IP_FATAL_ERROR))
      {
         bug("hpaio: ipConvert error=%x\n", wResult);
         goto bugout;
      }
      *pLength = outputUsed;
      if (outputUsed == 0)
         hpaio->ip_done = 1;
   }
   else if (!hpaio->pml_done)
   {
      if (check_pml_done(hpaio) == ERROR)
         goto bugout;
   }

   if(hpaio->ip_done && hpaio->mfpdtf_done && hpaio->pml_done)
      stat = SANE_STATUS_EOF;  /* done scan_read */
   else
      stat = SANE_STATUS_GOOD; /* repeat scan_read */ 

 bugout:
    if (stat != SANE_STATUS_GOOD)
    {
       if (hpaio->hJob)
       {
          ipClose(hpaio->hJob); 
          hpaio->hJob = 0;
       }   
    }

    //   bug("ipConvert result: inputAvail=%d input=%p inputUsed=%d inputNextPos=%d outputAvail=%d output=%p outputUsed=%d outputThisPos=%d\n", 
    //                                                 inputAvail, input, inputUsed, inputNextPos, outputAvail, output, outputUsed, outputThisPos);

    DBG(8, "sane_hpaio_read returned output=%p outputUsed=%d length=%d status=%d: %s %d\n", output, outputUsed, *pLength, stat, __FILE__, __LINE__);

    return stat;
}

int __attribute__ ((visibility ("hidden"))) pml_cancel(HPAIO_RECORD *hpaio)
{
   int oldStuff = (hpaio->preDenali || hpaio->fromDenali || hpaio->denali) ? 1 : 0;

   if(hpaio->hJob)
   {
      ipClose(hpaio->hJob);
      hpaio->hJob = 0;
   }
 
   /* If batch mode and page remains in ADF, leave pml/scan channels open. */
   if(hpaio->currentBatchScan == SANE_TRUE && hpaio->upload_state == PML_UPLOAD_STATE_NEWPAGE)
      return OK;

   /* If newer scanner or old scanner and ADF is empty, set to scanner to idle and unlock the scanner. */
   if(!oldStuff || (oldStuff && hpaio->upload_state != PML_UPLOAD_STATE_NEWPAGE))
   {
      PmlSetIntegerValue(hpaio->pml.objUploadState, PML_TYPE_ENUMERATION, PML_UPLOAD_STATE_IDLE);
      if (PmlRequestSetRetry(hpaio->deviceid, hpaio->cmd_channelid, hpaio->pml.objUploadState, 0, 0) != ERROR)
         clr_scan_token(hpaio);
   }

   if (hpaio->scan_channelid >= 0)
   {
      hpmud_close_channel(hpaio->deviceid, hpaio->scan_channelid);
      hpaio->scan_channelid = -1;
   }
   if (hpaio->cmd_channelid >= 0)
   {
      hpmud_close_channel(hpaio->deviceid, hpaio->cmd_channelid);
      hpaio->cmd_channelid = -1;
      SendScanEvent(hpaio->deviceuri, EVENT_END_SCAN_JOB);
   }

   return OK;
}
