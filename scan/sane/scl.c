/************************************************************************************\

  scl.c - HP SANE backend for multi-function peripherals (libsane-hpaio)

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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "hpmud.h"
#include "io.h"
#include "common.h"
#include "scl.h"
#include "hpaio.h"

#define DEBUG_DECLARE_ONLY
#include "sanei_debug.h"

static int SclBufferIsPartialReply( unsigned char * data, int datalen )
{
    int i = 0, value = 0;
    unsigned char d;  

    if( i >= datalen )
    {
        return 0;
    }
    if( data[i++] != 27 )
    {
        return 0;
    }
    if( i >= datalen )
    {
        return 0;
    }
    if( data[i++] != '*' )
    {
        return 0;
    }
    if( i >= datalen )
    {
        return 0;
    }
    if( data[i++] != 's' )
    {
        return 0;
    }
    while( 42 )
    {
        if( i >= datalen )
        {
            return 0;
        }
        d = data[i] - '0';
        if( d > 9 )
        {
            break;
        }
        i++;
    }
    d = data[i++];
    if( d<'a' || d>'z' )
    {
        return 0;
    }
    while( 42 )
    {
        if( i >= datalen )
        {
            return 0;
        }
        d = data[i] - '0';
        if( d > 9 )
        {
            break;
        }
        i++;
        value = ( value * 10 ) + d;
    }
    if( i >= datalen )
    {
        return 0;
    }
    if( data[i++] != 'W' )
    {
        return 0;
    }
    value = i + value - datalen;
    if( value < 0 )
    {
        value = 0;
    }
    return value;
}


static int SclChannelRead(int deviceid, int channelid, char * buffer, int countdown, int isSclResponse)
{
    char * bufferStart = buffer;
    int bufferLen = countdown, countup = 0, len;
    enum HPMUD_RESULT stat;

    if(!isSclResponse)
    {
        stat = hpmud_read_channel(deviceid, channelid, buffer, bufferLen, EXCEPTION_TIMEOUT, &len);  
        return len;
    }

    while(1)
    {
        stat = hpmud_read_channel(deviceid, channelid, buffer, countdown, EXCEPTION_TIMEOUT, &len);                                      

        if(stat != HPMUD_R_OK)
        {
            break;
        }
        countup += len;

        countdown = SclBufferIsPartialReply( (unsigned char *)bufferStart, countup );
        
        if( countup + countdown > bufferLen )
        {
            countdown = bufferLen - countup;
        }
        if( countdown <= 0 )
        {
            break;
        }

        buffer += len;
        //startTimeout = continueTimeout;
    }

    if(!countup)
    {
        return len;
    }
    return countup;

}

SANE_Status __attribute__ ((visibility ("hidden"))) SclSendCommand(int deviceid, int channelid, int cmd, int param)
{
    char buffer[LEN_SCL_BUFFER];
    int datalen, len;
    char punc = SCL_CMD_PUNC( cmd );
    char letter1 = SCL_CMD_LETTER1( cmd),letter2 = SCL_CMD_LETTER2( cmd );

    if( cmd == SCL_CMD_RESET )
    {
        datalen = snprintf( buffer, LEN_SCL_BUFFER, "\x1B%c", letter2 );
    }
    else
    {
        if( cmd == SCL_CMD_CLEAR_ERROR_STACK )
        {
            datalen = snprintf( buffer,
                                LEN_SCL_BUFFER,
                                "\x1B%c%c%c",
                                punc,
                                letter1,
                                letter2 );
        }
        else
        {
            datalen = snprintf( buffer,
                                LEN_SCL_BUFFER,
                                "\x1B%c%c%d%c",
                                punc,
                                letter1,
                                param,
                                letter2 );
        }
    }

    hpmud_write_channel(deviceid, channelid, buffer, datalen, EXCEPTION_TIMEOUT, &len);

    DBG(6, "SclSendCommand: size=%d bytes_wrote=%d: %s %d\n", datalen, len, __FILE__, __LINE__);
    if (DBG_LEVEL >= 6)
       sysdump(buffer, datalen);

    if(len != datalen)
    {
        return SANE_STATUS_IO_ERROR;
    }

    return SANE_STATUS_GOOD;
}

SANE_Status __attribute__ ((visibility ("hidden"))) SclInquire(int deviceid, int channelid, int cmd, int param, int * pValue, char * buffer, int maxlen)
{
    SANE_Status retcode;
    int lenResponse, len, value;
    char _response[LEN_SCL_BUFFER + 1], * response = _response;
    char expected[LEN_SCL_BUFFER], expectedChar;

    if( !pValue )
    {
        pValue = &value;
    }
    if( buffer && maxlen > 0 )
    {
        memset( buffer, 0, maxlen );
    }
    memset( _response, 0, LEN_SCL_BUFFER + 1 );

    /* Send inquiry command. */
    if( ( retcode = SclSendCommand( deviceid, channelid, cmd, param ) ) != SANE_STATUS_GOOD )
    {
        return retcode;
    }

    /* Figure out what format of response we expect. */
    expectedChar = SCL_CMD_LETTER2( cmd ) - 'A' + 'a' - 1;
    if( expectedChar == 'q' )
    {
        expectedChar--;
    }
    len = snprintf( expected,
                    LEN_SCL_BUFFER,
                    "\x1B%c%c%d%c",
                    SCL_CMD_PUNC( cmd ),
                    SCL_CMD_LETTER1( cmd ),
                    param,
                    expectedChar );

    /* Read the response. */
    lenResponse = SclChannelRead( deviceid, channelid, response, LEN_SCL_BUFFER, 1 );
                                      
    DBG(6, "SclChannelRead: len=%d: %s %d\n", lenResponse, __FILE__, __LINE__);
    if (DBG_LEVEL >= 6)
       sysdump(response, lenResponse);

    /* Validate the first part of the response. */
    if( lenResponse <= len || memcmp( response, expected, len ) )
    {
        bug("invalid SclInquire(cmd=%x,param=%d) exp(len=%d)/act(len=%d): %s %d\n", cmd, param, len, lenResponse, __FILE__, __LINE__);
        bug("exp:\n");
        bugdump(expected, len);
        bug("act:\n");
        bugdump(response, lenResponse);
        return SANE_STATUS_IO_ERROR;
    }
    response += len;
    lenResponse -= len;

    /* Null response? */
    if( response[0] == 'N' )
    {
        DBG(6, "SclInquire null response. %s %d\n", __FILE__, __LINE__);
        return SANE_STATUS_UNSUPPORTED;
    }

    /* Parse integer part of non-null response.
     * If this is a binary-data response, then this value is the
     * length of the binary-data portion. */
    if( sscanf( response, "%d%n", pValue, &len ) != 1 )
    {
        bug("invalid SclInquire(cmd=%x,param=%d) integer response: %s %d\n", cmd, param, __FILE__, __LINE__);
        return SANE_STATUS_IO_ERROR;
    }

    /* Integer response? */
    if( response[len] == 'V' )
    {
        return SANE_STATUS_GOOD;
    }

    /* Binary-data response? */
    if( response[len] != 'W' )
    {
        bug("invalid SclInquire(cmd=%x,param=%d) unexpected character '%c': %s %d\n", cmd, param, response[len], __FILE__, __LINE__);
        return SANE_STATUS_IO_ERROR;
    }
    response += len + 1;
    lenResponse -= len + 1;

    /* Make sure we got the right length of binary data. */
    if( lenResponse<0 || lenResponse != *pValue || lenResponse>maxlen )
    {
        bug("invalid SclInquire(cmd=%x,param=%d) binary data lenResponse=%d *pValue=%d maxlen=%d: %s %d\n", 
                             cmd, param, lenResponse, *pValue, maxlen, __FILE__, __LINE__);
        return SANE_STATUS_IO_ERROR;
    }

    /* Copy binary data into user's buffer. */
    if( buffer )
    {
        maxlen = *pValue;
        memcpy( buffer, response, maxlen );
    }

    return SANE_STATUS_GOOD;
}

/*
 * Phase 2 partial rewrite. des 9/26/07
 */

SANE_Status __attribute__ ((visibility ("hidden"))) scl_send_cmd(HPAIO_RECORD *hpaio, const char *buf, int size)
{
    int len;
    
    hpmud_write_channel(hpaio->deviceid, hpaio->scan_channelid, buf, size, EXCEPTION_TIMEOUT, &len);

    DBG(6, "scl cmd sent size=%d bytes_wrote=%d: %s %d\n", size, len, __FILE__, __LINE__);
    if (DBG_LEVEL >= 6)
       sysdump(buf, size);

    if(len != size)
    {
        return SANE_STATUS_IO_ERROR;
    }

    return SANE_STATUS_GOOD;
}

SANE_Status __attribute__ ((visibility ("hidden"))) scl_query_int(HPAIO_RECORD *hpaio, const char *buf, int size, int *result)
{
    char rbuf[256];
    int len, stat;
    char *tail;

    *result=0;

    if ((stat = scl_send_cmd(hpaio, buf, size)) != SANE_STATUS_GOOD)
    {
        return stat;
    }

    if ((stat = hpmud_read_channel(hpaio->deviceid, hpaio->scan_channelid, rbuf, sizeof(rbuf), EXCEPTION_TIMEOUT, &len)) != HPMUD_R_OK)
    {
        return SANE_STATUS_IO_ERROR;
    }

    DBG(6, "scl response size=%d: %s %d\n", len, __FILE__, __LINE__);
    if (DBG_LEVEL >= 6)
       sysdump(buf, size);

    /* Null response? */
    if(rbuf[len-1] == 'N')
    {
        DBG(6, "scl null response: %s %d\n", __FILE__, __LINE__);
        return SANE_STATUS_UNSUPPORTED;
    }
        
    /* Integer response? */
    if(rbuf[len-1] != 'V' )
    {
        bug("invalid scl integer response: %s %d\n", __FILE__, __LINE__);
        return SANE_STATUS_IO_ERROR;
    }

    *result = strtol(&rbuf[size], &tail, 10);

    return SANE_STATUS_GOOD;
}

