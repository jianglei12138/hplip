/************************************************************************************\

  mfpdtf.c - HP Multi-Function Peripheral Data Transfer Format filter.

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

#include "common.h"
#include "mfpdtf.h"
#include "io.h"

#define DEBUG_DECLARE_ONLY
#include "sanei_debug.h"

static union MfpdtfVariantHeader_u * MfpdtfReadAllocateVariantHeader( Mfpdtf_t mfpdtf,
                                                               int datalen )
{
    if( mfpdtf->read.pVariantHeader )
    {
        free( mfpdtf->read.pVariantHeader );
        mfpdtf->read.pVariantHeader = 0;
    }
    mfpdtf->read.lenVariantHeader = datalen;
    if( datalen )
    {
        mfpdtf->read.pVariantHeader = malloc( datalen );
    }
    return mfpdtf->read.pVariantHeader;
}

static int MfpdtfReadSetTimeout( Mfpdtf_t mfpdtf, int seconds )
{
    mfpdtf->read.timeout.tv_sec = seconds;
    mfpdtf->read.timeout.tv_usec = 0;

    return seconds;
}

Mfpdtf_t __attribute__ ((visibility ("hidden"))) MfpdtfAllocate( int deviceid, int channelid )
{
    int size = sizeof( struct Mfpdtf_s );
    Mfpdtf_t mfpdtf = malloc( size );
    
    if( mfpdtf )
    {
        memset( mfpdtf, 0, size );
        mfpdtf->channelid = channelid;
        mfpdtf->deviceid = deviceid;
        mfpdtf->fdLog = -1;
        MfpdtfReadSetTimeout( mfpdtf, 30 );
        MfpdtfReadStart( mfpdtf );
    }
    
    return mfpdtf;
}

int __attribute__ ((visibility ("hidden"))) MfpdtfDeallocate( Mfpdtf_t mfpdtf )
{
    if( !mfpdtf )
    {
        return ERROR;
    }
    MfpdtfLogToFile( mfpdtf, 0 );
    MfpdtfReadAllocateVariantHeader( mfpdtf, 0 );
    free( mfpdtf );
    return OK;
}

int __attribute__ ((visibility ("hidden"))) MfpdtfSetChannel( Mfpdtf_t mfpdtf, int channelid )
{
    mfpdtf->channelid = channelid;
    /* If necessary, we can query the device ID string using the
     * channel's device pointer. */
    return OK;
}

int __attribute__ ((visibility ("hidden"))) MfpdtfLogToFile( Mfpdtf_t mfpdtf, char * filename )
{
    if( mfpdtf->fdLog != -1 )
    {
        close( mfpdtf->fdLog );
        mfpdtf->fdLog = -1;
    }
    mfpdtf->logOffset = 0;
    if( filename )
    {
        int fd = creat( filename, 0600 );
        if( fd < 0 )
        {
            return ERROR;
        }
        mfpdtf->fdLog = fd;
    }
    return OK;
}

int __attribute__ ((visibility ("hidden"))) MfpdtfReadGetSimulateImageHeaders( Mfpdtf_t mfpdtf )
{
    return mfpdtf->read.simulateImageHeaders;
}

int __attribute__ ((visibility ("hidden"))) MfpdtfReadSetSimulateImageHeaders( Mfpdtf_t mfpdtf,
                                       int simulateImageHeaders )
{
    mfpdtf->read.simulateImageHeaders = simulateImageHeaders;
    return simulateImageHeaders;
}

int __attribute__ ((visibility ("hidden"))) MfpdtfReadStart( Mfpdtf_t mfpdtf )
{
    mfpdtf->read.lastServiceResult = 0;
    mfpdtf->read.dataType = ERROR;
    mfpdtf->read.arrayRecordCount = mfpdtf->read.arrayRecordSize;
    mfpdtf->read.fixedBlockBytesRemaining = 0;
    mfpdtf->read.innerBlockBytesRemaining = 0;
    mfpdtf->read.dontDecrementInnerBlock = 0;
    MfpdtfReadAllocateVariantHeader( mfpdtf, 0 );

    return OK;
}

static int MfpdtfReadIsImageData( Mfpdtf_t mfpdtf )
{
    return ( ( MFPDTF_DT_MASK_IMAGE & ( 1 << mfpdtf->read.dataType ) ) !=
             0 );
}

static int MfpdtfReadIsArrayData( Mfpdtf_t mfpdtf )
{
    return ( !MfpdtfReadIsImageData( mfpdtf ) );
}

#define READ(buffer,datalen) \
    do { \
        int r=MfpdtfReadGeneric(mfpdtf, \
            (unsigned char *)(buffer),datalen); \
        if (r!=datalen) { \
            if (r<0) return MFPDTF_RESULT_READ_ERROR; \
            return MFPDTF_RESULT_READ_TIMEOUT; \
        } \
    } while(0)

#define RETURN(_result) \
    return (mfpdtf->read.lastServiceResult=(_result));

static int MfpdtfReadGeneric( Mfpdtf_t mfpdtf, unsigned char * buffer, int datalen )
{
    int r = 0;

    /* Don't read past the currently-defined fixed block. */
    if( datalen > mfpdtf->read.fixedBlockBytesRemaining )
    {
        datalen = mfpdtf->read.fixedBlockBytesRemaining;
    }

    /* Read the data. */
    if( datalen > 0 )
    {        
        r = ReadChannelEx(mfpdtf->deviceid, 
                           mfpdtf->channelid, 
                           buffer, 
                           datalen, 
                           EXCEPTION_TIMEOUT);

        if( r > 0 )
        {
            /* Account for and log what was read. */
            mfpdtf->read.fixedBlockBytesRemaining -= r;
            
            if( !mfpdtf->read.dontDecrementInnerBlock )
            {
                mfpdtf->read.innerBlockBytesRemaining -= r;
            }
            
            mfpdtf->read.dontDecrementInnerBlock = 0;
        }
        
        if( r != datalen )
        {
            mfpdtf->read.lastServiceResult = r < 0 ?
                                             MFPDTF_RESULT_READ_ERROR :
                                             MFPDTF_RESULT_READ_TIMEOUT;
        }
    }

    return r;
}

int __attribute__ ((visibility ("hidden"))) MfpdtfReadService( Mfpdtf_t mfpdtf )
{
    int result = 0;
    int datalen, blockLength, headerLength;

    if( mfpdtf->read.fixedBlockBytesRemaining <= 0 )
    {
        /* Read fixed header. */
        datalen = sizeof( mfpdtf->read.fixedHeader );
        mfpdtf->read.fixedBlockBytesRemaining = datalen;
	//        DBG( 0, "********************************** FIXED HEADER **********************************.\n" );
        mfpdtf->read.dontDecrementInnerBlock = 1;
        
        //READ( &mfpdtf->read.fixedHeader, datalen );
        
        int r = MfpdtfReadGeneric( mfpdtf, (unsigned char *)&mfpdtf->read.fixedHeader, datalen );

        if( r!= datalen )
        {
            if( r < 0 )
                return MFPDTF_RESULT_READ_ERROR;
            
            return MFPDTF_RESULT_READ_TIMEOUT;
        }
        
        /* Parse fixed header. */
        blockLength = LEND_GET_LONG( mfpdtf->read.fixedHeader.blockLength );
        mfpdtf->read.fixedBlockBytesRemaining = blockLength - datalen;
        headerLength = LEND_GET_SHORT( mfpdtf->read.fixedHeader.headerLength );

        /* Scan data type? */
        if(mfpdtf->read.fixedHeader.dataType != DT_SCAN)
        {
            bug("invalid mfpdtf fixed header datatype=%d\n", mfpdtf->read.fixedHeader.dataType);
            return MFPDTF_RESULT_READ_ERROR;
        }

        /* Is this a new data type? */
        if( mfpdtf->read.dataType != mfpdtf->read.fixedHeader.dataType )
        {
            mfpdtf->read.dataType = mfpdtf->read.fixedHeader.dataType;
            result |= MFPDTF_RESULT_NEW_DATA_TYPE;
        }

        DBG(6, "fixed header page_flags=%x: %s %d\n", mfpdtf->read.fixedHeader.pageFlags, __FILE__, __LINE__);

        /* Read variant header (if any). */
        datalen = headerLength - sizeof( mfpdtf->read.fixedHeader );
        
        if( datalen > 0 )
        {
            DBG(6, "reading variant header size=%d: %s %d\n", datalen, __FILE__, __LINE__);
            
            if( !MfpdtfReadAllocateVariantHeader( mfpdtf, datalen ) )
            {
                RETURN( MFPDTF_RESULT_OTHER_ERROR );
            }
            mfpdtf->read.dontDecrementInnerBlock = 1;
            
            int r = MfpdtfReadGeneric( mfpdtf, (unsigned char *)mfpdtf->read.pVariantHeader, datalen );
    
            if( r!= datalen )
            {
                if( r < 0 )
                    return MFPDTF_RESULT_READ_ERROR;
                
                return MFPDTF_RESULT_READ_TIMEOUT;
            
            }
            
	    //            DBG( 0, "********************************** VARIANT HEADER **********************************.\n" );
            
            result |= MFPDTF_RESULT_NEW_VARIANT_HEADER;

            /* Is this a valid array variant header? */
            mfpdtf->read.arrayRecordSize = 0;
            mfpdtf->read.arrayRecordCount = 0;
            mfpdtf->read.innerBlockBytesRemaining = 0;
            
            if( MfpdtfReadIsArrayData( mfpdtf ) &&
                mfpdtf->read.lenVariantHeader >=
                sizeof( mfpdtf->read.pVariantHeader->array ) )
            {
                mfpdtf->read.arrayRecordCount = LEND_GET_SHORT( mfpdtf->read.pVariantHeader->array.recordCount );
                mfpdtf->read.arrayRecordSize = LEND_GET_SHORT( mfpdtf->read.pVariantHeader->array.recordSize );
                mfpdtf->read.innerBlockBytesRemaining = mfpdtf->read.arrayRecordCount * mfpdtf->read.arrayRecordSize;
            }
        }
    }
    else if( MfpdtfReadIsImageData( mfpdtf ) )
    {
        if( mfpdtf->read.innerBlockBytesRemaining > 0 )
        {
            result |= MFPDTF_RESULT_IMAGE_DATA_PENDING;
        }
        else if( mfpdtf->read.simulateImageHeaders )
        {
            mfpdtf->read.innerBlockBytesRemaining = mfpdtf->read.fixedBlockBytesRemaining;
            
            if( mfpdtf->read.innerBlockBytesRemaining > 0 )
            {
                result |= MFPDTF_RESULT_IMAGE_DATA_PENDING;
            }
        }
        else
        {
            unsigned char id;
            datalen = 1;
            
            //READ( &id, datalen );
            
            int r = MfpdtfReadGeneric( mfpdtf, (unsigned char *)&id, datalen );
    
            if( r!= datalen )
            {
                if( r < 0 )
                    return MFPDTF_RESULT_READ_ERROR;
                
                return MFPDTF_RESULT_READ_TIMEOUT;
            
            }

            if( id == MFPDTF_ID_RASTER_DATA )
            {
                datalen = sizeof( mfpdtf->read.imageRasterDataHeader );
		//                DBG( 0, "Reading raster data header.\n" );
                
		//                DBG( 0, "********************************** RASTER RECORD **********************************.\n" );
                int r = MfpdtfReadGeneric( mfpdtf, (unsigned char *)&mfpdtf->read.imageRasterDataHeader, datalen );
    
                if( r!= datalen )
                {
                    if( r < 0 )
                        return MFPDTF_RESULT_READ_ERROR;
                    
                    return MFPDTF_RESULT_READ_TIMEOUT;
                
                }

                
                mfpdtf->read.innerBlockBytesRemaining = LEND_GET_SHORT( mfpdtf->read.imageRasterDataHeader.byteCount );
                result |= MFPDTF_RESULT_IMAGE_DATA_PENDING;
            }
            else if( id == MFPDTF_ID_START_PAGE )
            {
                datalen = sizeof( mfpdtf->read.imageStartPageRecord );
		//                DBG( 0, "Reading start of page record.\n" );
		//                DBG( 0, "********************************** SOP RECORD **********************************.\n" );
                int r = MfpdtfReadGeneric( mfpdtf, (unsigned char *)&mfpdtf->read.imageStartPageRecord, datalen );
    
                if( r!= datalen )
                {
                    if( r < 0 )
                        return MFPDTF_RESULT_READ_ERROR;
                    
                    return MFPDTF_RESULT_READ_TIMEOUT;
                
                }
                
                result |= MFPDTF_RESULT_NEW_START_OF_PAGE_RECORD;
            }
            else if( id == MFPDTF_ID_END_PAGE )
            {
                datalen = sizeof( mfpdtf->read.imageEndPageRecord );
		//                DBG( 0, "Reading end of page record.\n" );
		//                DBG( 0, "********************************** EOP RECORD **********************************.\n" );
                int r = MfpdtfReadGeneric( mfpdtf, (unsigned char *)&mfpdtf->read.imageEndPageRecord, datalen );
    
                if( r!= datalen )
                {
                    if( r < 0 )
                        return MFPDTF_RESULT_READ_ERROR;
                    
                    return MFPDTF_RESULT_READ_TIMEOUT;
                
                }
                
                
                result |= MFPDTF_RESULT_NEW_END_OF_PAGE_RECORD;
            }
            else
            {
                RETURN( MFPDTF_RESULT_OTHER_ERROR );
            }
        }
    }
    else if( MfpdtfReadIsArrayData( mfpdtf ) )
    {
        if( mfpdtf->read.innerBlockBytesRemaining > 0 )
        {
            result |= MFPDTF_RESULT_ARRAY_DATA_PENDING;
        }
    }

    if( mfpdtf->read.fixedBlockBytesRemaining > 0 )
    {
        result |= MFPDTF_RESULT_GENERIC_DATA_PENDING;
    }

    RETURN( ( result | mfpdtf->read.fixedHeader.pageFlags ) );
}

int __attribute__ ((visibility ("hidden"))) MfpdtfReadGetLastServiceResult( Mfpdtf_t mfpdtf )
{
    return mfpdtf->read.lastServiceResult;
}

int __attribute__ ((visibility ("hidden"))) MfpdtfReadGetVariantHeader( Mfpdtf_t mfpdtf,
                                union MfpdtfVariantHeader_u * buffer,
                                int maxlen )
{
    if( !mfpdtf->read.pVariantHeader )
    {
        return 0;
    }
    if( !buffer )
    {
        return mfpdtf->read.lenVariantHeader;
    }
    if( maxlen > mfpdtf->read.lenVariantHeader )
    {
        maxlen = mfpdtf->read.lenVariantHeader;
    }
    memcpy( buffer, mfpdtf->read.pVariantHeader, maxlen );
    return maxlen;
}

int __attribute__ ((visibility ("hidden"))) MfpdtfReadGetStartPageRecord( Mfpdtf_t mfpdtf,
                                  struct MfpdtfImageStartPageRecord_s * buffer,
                                  int maxlen )
{
    int len = sizeof( struct MfpdtfImageStartPageRecord_s );
    if( maxlen > len )
    {
        maxlen = len;
    }
    memcpy( buffer, &mfpdtf->read.imageStartPageRecord, maxlen );
    return maxlen;
}

int __attribute__ ((visibility ("hidden"))) MfpdtfReadInnerBlock( Mfpdtf_t mfpdtf,
                          unsigned char * buffer,
                          int countdown )
{
    int r, countup = 0;

    while( 1 )
    {
        if( countdown > mfpdtf->read.innerBlockBytesRemaining )
        {
            countdown = mfpdtf->read.innerBlockBytesRemaining;
        }
        
        if( countdown <= 0 )
        {
            break;
        }
        
        r = MfpdtfReadGeneric( mfpdtf, buffer, countdown );
        
        if( MfpdtfReadGetLastServiceResult( mfpdtf ) & MFPDTF_RESULT_ERROR_MASK )
        {
            break;
        }

        if( mfpdtf->fdLog >= 0 )
        {
            write( mfpdtf->fdLog, buffer, r );  /* log raw rgb data, use imagemagick to display */
        }

        buffer += r;
        countdown -= r;
        countup += r;       

        if( countdown <= 0 )
        {
            break;
        }
        
        r = MfpdtfReadService( mfpdtf );
        
        if( r & ( MFPDTF_RESULT_ERROR_MASK |
                  MFPDTF_RESULT_NEW_DATA_TYPE |
                  MFPDTF_RESULT_NEW_VARIANT_HEADER ) )
        {
            break;
        }
    }

    return countup;
}

/*
 * Phase 2 rewrite. des
 */

int __attribute__ ((visibility ("hidden"))) read_mfpdtf_block(int device, int channel, char *buf, int bufSize, int timeout)
{
   MFPDTF_FIXED_HEADER *phd = (MFPDTF_FIXED_HEADER *)buf;
   int size, bsize=0, len;

   /* Read fixed header with timeout in seconds. */
   size = sizeof(MFPDTF_FIXED_HEADER);
   if ((len = ReadChannelEx(device, channel, (unsigned char *)buf, size, timeout)) != size)
      goto bugout;

   bsize = le32toh(phd->BlockLength);
   if (bsize > bufSize)
   {
      bug("invalid bufsize: size=%d max=%d ReadMfpdtfBlock %s %d\n", bsize, bufSize, __FILE__, __LINE__);
      bsize = -1;
      goto bugout;
   }

   size = bsize - sizeof(MFPDTF_FIXED_HEADER);
   //   if ((len = ReadChannelEx(device, channel, (unsigned char *)buf+sizeof(MFPDTF_FIXED_HEADER), size, 5)) != size)
   if ((len = ReadChannelEx(device, channel, (unsigned char *)buf+sizeof(MFPDTF_FIXED_HEADER), size, 10)) != size)
   {
      bug("invalid read: exp=%d act=%d ReadMfpdtfBlock %s %d\n", size, len, __FILE__, __LINE__);
      bsize = -1;
      goto bugout;
   }

bugout:
   return bsize;
}

