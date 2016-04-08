/*****************************************************************************\

  hpiom.h - HP I/O message handler
 
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

#ifndef hpiom_INCLUDED
#define hpiom_INCLUDED

/*
 * LIDIL Definitions.
 */

enum ePACKETTYPE{ eCommand
                , eDisablePacing
                , eEnablePacing
                , eResumeNormalOperation
                , eDiableResponses
                , eEnableResponses
                , eResetLidil
                , eSynch
                , eSynchComplete
                , eResponseCommandExecuted = 16
                , eResponseCommandDiscarded
                , eResponseCommandNotSupported
                , eResponseUnitNumberNotSupported
                , eResponseCommandLengthNotSupported
                , eResponseDataLengthNotSupported
                , eResponseCommandError
                , eResponseProtocalError
                , eResponseAuto
                , eOperationComplete
                , eAbsoluteCredit = 32
                , eCredit 
                , ePacketUnknown
                };

enum eCOMMANDNUMBER{ eJobControl
                   , eLoadPage
                   , eEjectPage
                   , ePrintSweep
                   , eLoadSweepData
                   , eQuery
                   , eComment               = 7
                   , eHandlePen
                   , eUnderware             = 11
                   , eDeviceControlCommand
                   , eCommandUnknown
                   };

typedef struct
{
   unsigned char start_frame;
   unsigned short cmd_length;
   unsigned char unit_num;
   unsigned char packet_type;
   unsigned char cmd_num;
   unsigned short ref_num;
   unsigned short data_length;
   unsigned char response_cmp;
   unsigned char pad[4];
   unsigned char end_frame;
} __attribute__((packed)) LDLResponseHeader;

typedef struct
{
   LDLResponseHeader h;
   unsigned short colors;
   char k[3];     /* 0=vertical 1=horizontal 2=bi */
   char c[3];     /* c=m=y */
   char m[3];
   char y[3];
} __attribute__((packed)) LDLResponseAlign038;

typedef struct
{
  char color;   /* 0=black 1=color 2=photo */
  char vert;    /* vertical alignment offset */
  char horz;    /* horizontal alignment offset */
  char bi;      /* bi-directional offset */
} __attribute__((packed)) LDLPen;

typedef struct
{
   unsigned char nPens;   /* number of pens */
   LDLPen pen[3];
} __attribute__((packed)) LDLGenAlign;

typedef struct
{
   LDLResponseHeader h;
   LDLGenAlign g;
} __attribute__((packed)) LDLResponseAlign043;

typedef struct
{
   LDLResponseHeader h;
   uint32_t ldlversion;
   char firmversion[8];
} __attribute__((packed)) LDLResponseVersion;

#ifdef __cplusplus
extern "C" {
#endif

extern int ReadHPVertAlign(int hd);

#ifdef __cplusplus
}
#endif

#endif    /* hpiom_INCLUDED */


