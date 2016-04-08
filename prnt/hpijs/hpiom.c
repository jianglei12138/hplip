/*****************************************************************************\

  hpiom.c - HP I/O message handler
 
  (c) 2003-2004 Copyright HP Development Company, LP

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
  3. Neither the name of HP nor the names of its
     contributors may be used to endorse or promote products derived
     from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR IMPLIED
  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
  NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
  TO, PATENT INFRINGEMENT; PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
  ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

\*****************************************************************************/

#ifdef HAVE_LIBHPIP

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include "hpijs.h"
#include "hpiom.h"

static const int gnMaxDataSize = 2048;
static const int gnMaxCmdOptionSize = 4;

static const char gcFrameMarker                    = '$';
static const int  gnPadding                        = 255;
static const int  gnRequiredSize                   = 11;
static const int  gnMinCommandSize                 = 16;
static const int  gnMinDecodeSize                  = 10;   // needed six bytes to determine command number,
                                                    // command length and data length
 
static const int  gUV8FrameOffset                  = 0;
static const int  gUV16CommandLengthOffset         = 1;
static const int  gUV8UnitNumberOffset             = 3;
static const int  gE8PacketTypeOffset              = 4;
static const int  gUV8CommandNumberOffset          = 5;
static const int  gUV16ReferenceNumberOffset       = 6;
static const int  gUV16DataLengthOffset            = 8;
static const int  gUV8CommandOptionsOffset         = 10;

static const int  gUV8RespFrameOffset              = 0;
static const int  gUV16RespCommandLengthOffset     = 1;
static const int  gUV8RespUnitNumberOffset         = 3;
static const int  gE8RespPacketTypeOffset          = 4;
static const int  gUV8RespCommandNumberOffset      = 5;
static const int  gUV16RespReferenceNumberOffset   = 6;
static const int  gUV16RespDataLengthOffset        = 8;
static const int  gE8RespCompleteOffset            = 10;

// reserved reference number
//
static const int gnMinRefNum = 0xF000;
static const int gnMaxRefNum = 0xFFFD;

unsigned short gwSynchRefNum                  = 0xFFEC;
unsigned short gwSynchCompleteRefNum          = 0xFFEB;
unsigned short gwResetRefNum                  = 0xFFEA;
  
unsigned short gwPrinterVersionQueryRefNum      = 0xFFD0;
unsigned short gwPrinterStatusQueryRefNum       = 0xFFD1;
unsigned short gwPrinterAttributesQueryRefNum   = 0xFFD2;
unsigned short gwAlignmentQueryRefNum           = 0xFFD3;
unsigned short gwDeviceIdQueryRefNum            = 0xFFDD;
unsigned short gwHueCompensationQueryRefNum     = 0xFFDE;

// command options
//
// printer query command options
//
static const int  gnPrinterQueryOptionsSize = 4;

static unsigned char gpPrinterVersionQuery[]      = { 0x00, 0x00, 0x00, 0x00 };  //  0 - return UV32 version data
//static unsigned char gpPrinterStatusQuery[]       = { 0x01, 0x00, 0x00, 0x00 };  //  1 - return status string
//static unsigned char gpPrinterAttributesQuery[]   = { 0x02, 0x00, 0x00, 0x00 };  //  2 - return printer attributes
static unsigned char gpAlignmentQuery[]           = { 0x03, 0x00, 0x00, 0x00 };  //  3 - return primitive alignment value
//static unsigned char gpDeviceIdQuery[]            = { 0x0D, 0x00, 0x00, 0x00 };  // 13 - return device id
//static unsigned char gpHueCompensationQuery[]     = { 0x0E, 0x00, 0x00, 0x00 };  // 14 - return hue compensation
static unsigned char gpPenAlignmentQuery[]        = { 0x0F, 0x00, 0x00, 0x00 };  // 15 - return pen alignment value

/*
 * Lidil commands.
 */

int EncodeCommand
(
   unsigned char *lpBuffer,
   unsigned short wBufferSize,
   unsigned char unUnitNumber,
   int ePacketType,
   int eCommandNumber,
   char *lpData,
   unsigned short wDataLength,
   unsigned char *lpCommandOptions,
   unsigned short wCommandOptionsSize,
   int *dPacketSize,
   unsigned short wRefNum
)
{
   int x;
   int lNumPaddingNeeded = 0;
   unsigned char *lpTemp = NULL;

   memset( lpBuffer, 0, wBufferSize );
   lpBuffer [ gUV8FrameOffset ]         = gcFrameMarker;
   lpBuffer [ gUV8UnitNumberOffset ]    = unUnitNumber;
   lpBuffer [ gE8PacketTypeOffset ]     = ePacketType;
   lpBuffer [ gUV8CommandNumberOffset ] = eCommandNumber;
   *(short *)(lpBuffer + gUV16DataLengthOffset) = htons(wDataLength);

   if ( wCommandOptionsSize > 0 )
   {   
      if ( lpCommandOptions )
      {
         // copy command options to the buffer
         memcpy(( lpBuffer + gUV8CommandOptionsOffset ), lpCommandOptions, wCommandOptionsSize);
      }
      else
      {
         // command option is null, fill the buffer with zeros
         memset(( lpBuffer + gUV8CommandOptionsOffset ), 0, wCommandOptionsSize );
      }
   }

   // calculate command length and padding if needed
   *dPacketSize = gnRequiredSize + wCommandOptionsSize;
   lNumPaddingNeeded = gnMinCommandSize - *dPacketSize;

   if ( lNumPaddingNeeded > 0 )
   {
      // move the pointer to the beginning of the padding
      lpTemp = lpBuffer + gUV8CommandOptionsOffset + wCommandOptionsSize;

      for (x = 0; x < lNumPaddingNeeded; x++, lpTemp++ )
      {
         *lpTemp = gnPadding;
      }
            
      *dPacketSize = gnMinCommandSize;
   }

   *(short *)(lpBuffer + gUV16CommandLengthOffset) = htons(*dPacketSize);
   *(short *)(lpBuffer + gUV16ReferenceNumberOffset) = htons(wRefNum ? wRefNum : 1);

   // add the trailing frame marker
   lpBuffer[ *dPacketSize - 1 ] = gcFrameMarker;

   if ( wDataLength )
   {            
      if ((*dPacketSize + wDataLength) > wBufferSize)
      {
          BUG("unable to fill data buffer EncodeCommand size=%d\n", wDataLength);
          return 1;
      }   

      if ( lpData )
      {
          // copy the data to the end of the command
          memcpy( lpBuffer + *dPacketSize, lpData, wDataLength );
      }
      else
      {
          // NULL data pointer, fill the buffer with zeros
          memset( lpBuffer + *dPacketSize, 0, wDataLength );
      }

      *dPacketSize += wDataLength;
   }

   return 0;
}

int Synch(int hd, int chan)
{
    int bRet = 0;
    int dPacketSize = 0;
    unsigned char buf[4096];

    // create the Synch command, send it to the device, 
    // and retrieve absolute credit data from the device.
    EncodeCommand(buf, sizeof(buf)
                     , 0            
                     , eSynch
                     , eCommandUnknown
                     , NULL
                     , gnMaxDataSize
                     , NULL
                     , gnMaxCmdOptionSize
                     , &dPacketSize
                     , gwSynchRefNum
                     );

    hpmud_write_channel(hd, chan, buf, dPacketSize, EXCEPTION_TIMEOUT, &bRet);

    return( bRet );
}

int SynchComplete(int hd, int chan)
{
    int bRet = 0;
    int dPacketSize = 0;
    unsigned char buf[32];

        // create the SynchComplete command, send it to the device, 
        // and retrieve absolute credit data from the device.
        EncodeCommand(buf, sizeof(buf)
                     , 0            
                     , eSynchComplete
                     , eCommandUnknown
                     , NULL
                     , 0
                     , NULL
                     , 0
                     , &dPacketSize
                     , gwSynchCompleteRefNum
                     );

    hpmud_write_channel(hd, chan, buf, dPacketSize, EXCEPTION_TIMEOUT, &bRet);

    return( bRet );
}

int Reset(int hd, int chan)
{
    int bRet = 0;
    int dPacketSize = 0;
    unsigned char buf[32];

        // create the Reset command, send it to the device, 
        // and retrieve absolute credit data from the device.
        //
        EncodeCommand(buf, sizeof(buf)
                     , 0            
                     , eResetLidil
                     , eCommandUnknown
                     , NULL
                     , 0
                     , NULL
                     , 0
                     , &dPacketSize
                     , gwResetRefNum
                     );

    hpmud_write_channel(hd, chan, buf, dPacketSize, EXCEPTION_TIMEOUT, &bRet);

    return( bRet );
}

int RetrieveAlignmentValues038(int hd, int chan, LDLGenAlign *pG)
{
   int n;
   int dPacketSize = 0;
   unsigned char buf[256];
   LDLResponseAlign038 *pA;

   /* Enable responses. */
   EncodeCommand(buf, sizeof(buf)
                     , 0
                     , eEnableResponses
                     , eCommandUnknown
                     , NULL
                     , 0
                     , NULL
                     , 0
                     , &dPacketSize
                     , 0
                     );
   hpmud_write_channel(hd, chan, buf, dPacketSize, EXCEPTION_TIMEOUT, &n);

   /* Write alignment query. */
   EncodeCommand(buf, sizeof(buf)
                     , 0             // device 0
                     , eCommand
                     , eQuery
                     , NULL
                     , 0
                     , gpAlignmentQuery
                     , gnPrinterQueryOptionsSize  
                     , &dPacketSize
                     , gwAlignmentQueryRefNum
                     );
   hpmud_write_channel(hd, chan, buf, dPacketSize, EXCEPTION_TIMEOUT, &n);

   /* Disable responses. */
   EncodeCommand(buf, sizeof(buf)
                     , 0
                     , eDiableResponses
                     , eCommandUnknown
                     , NULL
                     , 0
                     , NULL
                     , 0
                     , &dPacketSize
                     , 0
                     );
   hpmud_write_channel(hd, chan, buf, dPacketSize, EXCEPTION_TIMEOUT, &n);
 
   /* Read query response. */
   hpmud_read_channel(hd, chan, buf, sizeof(buf), EXCEPTION_TIMEOUT, &n);
   pA = (LDLResponseAlign038 *)buf;
   memset(pG, 0, sizeof(LDLGenAlign));
   if (pA->h.packet_type == 16)
   {
      pG->nPens = 2;
      /* Except for bi, convert values from relative to black pen to relative to color. */
      pG->pen[0].color = 0;
      pG->pen[0].vert = -pA->c[0];
      pG->pen[0].horz = -pA->c[1];
      pG->pen[0].bi = pA->k[2];
      pG->pen[1].color = 1;
      pG->pen[1].vert = pA->k[0];
      pG->pen[1].horz = pA->k[1];
      pG->pen[1].bi = pA->c[2];
   }

   return 0;
}

int RetrieveAlignmentValues043(int hd, int chan, LDLGenAlign *pG)
{
   int n=0;
   int dPacketSize = 0;
   unsigned char buf[256];
   LDLResponseAlign043 *pA;

   /* Enable responses. */
   EncodeCommand(buf, sizeof(buf)
                     , 0
                     , eEnableResponses
                     , eCommandUnknown
                     , NULL
                     , 0
                     , NULL
                     , 0
                     , &dPacketSize
                     , 0
                     );
   hpmud_write_channel(hd, chan, buf, dPacketSize, EXCEPTION_TIMEOUT, &n);

   /* Write alignment query. */
   EncodeCommand(buf, sizeof(buf)
                     , 0             // device 0
                     , eCommand
                     , eQuery
                     , NULL
                     , 0
                     , gpPenAlignmentQuery 
                     , gnPrinterQueryOptionsSize  
                     , &dPacketSize
                     , gwAlignmentQueryRefNum
                     );
   hpmud_write_channel(hd, chan, buf, dPacketSize, EXCEPTION_TIMEOUT, &n);

   /* Disable responses. */
   EncodeCommand(buf, sizeof(buf)
                     , 0
                     , eDiableResponses
                     , eCommandUnknown
                     , NULL
                     , 0
                     , NULL
                     , 0
                     , &dPacketSize
                     , 0
                     );
   hpmud_write_channel(hd, chan, buf, dPacketSize, EXCEPTION_TIMEOUT, &n);

   hpmud_read_channel(hd, chan, buf, sizeof(buf), EXCEPTION_TIMEOUT, &n);
   pA = (LDLResponseAlign043 *)buf;
   memset(pG, 0, sizeof(LDLGenAlign));
   if (pA->h.packet_type == 16)
   {
      memcpy(pG, &pA->g, sizeof(LDLGenAlign));
   }

   return 0;
}

uint32_t RetrieveVersion(int hd, int chan)
{
   int n, version=0;
   int dPacketSize = 0;
   unsigned char buf[256];
   LDLResponseVersion *pV;

   /* Enable responses. */
   EncodeCommand(buf, sizeof(buf)
                     , 0
                     , eEnableResponses
                     , eCommandUnknown
                     , NULL
                     , 0
                     , NULL
                     , 0
                     , &dPacketSize
                     , 0
                     );
   hpmud_write_channel(hd, chan, buf, dPacketSize, EXCEPTION_TIMEOUT, &n);

   /* Write lidil version query. */
   EncodeCommand(buf, sizeof(buf)
                     , 0             // device 0
                     , eCommand
                     , eQuery
                     , NULL
                     , 0
                     , gpPrinterVersionQuery
                     , gnPrinterQueryOptionsSize  
                     , &dPacketSize
                     , gwAlignmentQueryRefNum
                     );
   hpmud_write_channel(hd, chan, buf, dPacketSize, EXCEPTION_TIMEOUT, &n);

   /* Disable responses. */
   EncodeCommand(buf, sizeof(buf)
                     , 0
                     , eDiableResponses
                     , eCommandUnknown
                     , NULL
                     , 0
                     , NULL
                     , 0
                     , &dPacketSize
                     , 0
                     );
   hpmud_write_channel(hd, chan, buf, dPacketSize, EXCEPTION_TIMEOUT, &n);

   hpmud_read_channel(hd, chan, buf, sizeof(buf), EXCEPTION_TIMEOUT, &n);
   pV = (LDLResponseVersion *)buf;
   if (pV->h.packet_type == 16)
   {
      version = ntohl(pV->ldlversion);
      fprintf(stdout, "lidil version = %x\n", version);
   }

   return(version);
}

/*
 * Return value = (black | photo) to color vertical alignment offset, error = -1.
 *
 * All alignment values may be zero if pen(s) were never aligned. Valid values
 * may range from -30 to +30.
 */
int ReadHPVertAlign(int hd)
{
   int channel, n, i, x2colorVert=-1;
   uint32_t ver;
   LDLGenAlign ga;

   if (hpmud_open_channel(hd, "PRINT", &channel) != HPMUD_R_OK)
   {
      BUG("unable to open print channel ReadHPVertAlign\n");
      goto bugout;
   }

   if (Synch(hd, channel)==0)
   {  
      BUG("unable to write sync ReadHPVertAlign\n");  
      goto bugout;  
   }  

   if (SynchComplete(hd, channel)==0)
   {  
      BUG("unable to write sync complete ReadHPVertAlign\n");  
      goto bugout;  
   }  

   if (Reset(hd, channel)==0)
   {  
      BUG("unable to write reset ReadHPVertAlign\n");  
      goto bugout;  
   }  

   if ((ver = RetrieveVersion(hd, channel))==0)
   {  
      BUG("unable to read version ReadHPVertAlign\n");  
      goto bugout;  
   }  

   if (ver > 0x308)
      RetrieveAlignmentValues043(hd, channel, &ga);
   else 
      RetrieveAlignmentValues038(hd, channel, &ga);

   if (!(n = ga.nPens))
      goto bugout;

   for (i=0; i<n; i++)
   {
      if (ga.pen[i].color == 0 || ga.pen[i].color == 2)
      {
         x2colorVert = ga.pen[i].vert;  /* (black | photo) to color offset */
         BUG("%s alignment: vert=%d horz=%d bi=%d x2c=%d\n", (ga.pen[i].color==0) ? "black" : "photo", ga.pen[i].vert, ga.pen[i].horz, ga.pen[i].bi, x2colorVert);
      }
   }

   Reset(hd, channel);

bugout: 
   if (channel >= 0)
      hpmud_close_channel(hd, channel);

   return x2colorVert;
}

#endif // HAVE_LIBHPIP


