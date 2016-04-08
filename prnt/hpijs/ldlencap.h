/*****************************************************************************\
  ldlencap.h : definitions for the lidil encapsulation

  Copyright (c) 2001 - 2015, HP Co.
  All rights reserved.

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
  \***************************************************************************/
#ifndef _LDLENCAP
#define _LDLECNAP

// Wait times
#define PACKET_WAIT                 5
#define IO_WAIT                     5
#define CREDIT_WAIT                 5
#define ERROR_WAIT                  20

// Bytes to retrieve data from packet are 0-based, i.e. FRAME_BYTE is 1st byte of packet,
// but 0th element of buffer.  Command packets and credit packets may have different elements
// with the same value
#define FRAME_BYTE                  0
#define COMMAND_LENGTH_BYTE         1
#define PACKET_TYPE_BYTE            4
#define COMMAND_NUMBER_BYTE         5
#define REFERENCE_NUMBER_BYTE       6
#define DATA_LENGTH_BYTE            8
#define NUMBER_OF_COMMANDS_BYTE     10
#define COMMAND_OPT_BYTE            10
#define CREDIT_BYTE                 11

// Packet types
#define RESPONSE_COMMAND_EXECUTED   16
#define RESPONSE_AUTO               24
#define ABSOLUTE_CREDIT             32
#define INCREMENTAL_CREDIT          33

// Command numbers
#define COMMAND_CANCEL              0
#define COMMAND_QUERY               5
#define COMMAND_CONTINUE            12
#define COMMAND_PREPARE_TO_CANCEL   12

// Auto response numbers 1-5 are reserved, but we'll probably only ever use number 1
#define AUTO_RESPONSE_STATUS        1

// MAX_PACKET_SIZE has to be a multiple of 64, which is the packet size of the printer
// See comment at LDLEncap::GetPackets
#define MAX_PACKET_READ_SIZE        256

// Special packet types
const BYTE byEnablePacing[] =       { 0x24, 0x00, 0x10, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x24 };
const BYTE byResetLIDIL[] =         { 0x24, 0x00, 0x10, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x24 };

// The Sync packet has 245 bytes of 0's as command options and 2048 bytes of 0's as data following
// the trailing frame byte.  Rather than explicitly initialize the entire Sync packet with 0's here,
// we'll just initialize up to the COMMAND_OPT_BYTE and fill in the rest of the command in the
// LDLEncap constructor using memcpy
const BYTE bySync[] =               { 0x24, 0x01, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x08,
                                      0x00 };
const BYTE bySyncComplete[] =       { 0x24, 0x00, 0x10, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x24 };
// Commands
//const BYTE byDisableResponses[] = { 0x24, 0x00, 0x10, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00,
//                                    0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x24 };
//const BYTE byEnableResponses[] =  { 0x24, 0x00, 0x10, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00,
//                                    0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x24 };
//const BYTE byStatusQuery[] =      { 0x24, 0x00, 0x10, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00,
//                                    0x00, 0x01, 0x00, 0x00, 0x00, 0xFF, 0x24 };
const BYTE byEOCStatusQuery[] =     { 0x24, 0x00, 0x10, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00,
                                      0x00, 0x01, 0x02, 0x00, 0x01, 0xFF, 0x24 };
const BYTE byContinue[] =           { 0x24, 0x00, 0x10, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00,
                                      0x00, 0x02, 0xFF, 0xFF, 0xFF, 0xFF, 0x24 };
const BYTE byPrepareToCancel[] =    { 0x24, 0x00, 0x10, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00,
                                      0x00, 0x0A, 0xFF, 0xFF, 0xFF, 0xFF, 0x24 };

const int CANCELSIZE = 16;
const int SYNCSIZE = 2304;
const int SYNC_CMD_OPT_SIZE = 245;
//const int QUERYSIZE = sizeof(byEnableResponses) + sizeof(byStatusQuery) + sizeof(byDisableResponses);

typedef unsigned short  UInt16;
typedef unsigned long   UInt32;
typedef unsigned char   UChar;
typedef unsigned int    Int16;
typedef long     Int32;
typedef enum
{
    eLDLStartJob = 0,
    eLDLEndJob   = 0,
    eLDLLoadPage = 1,
    eLDLEjectPage =2,
    eLDLPrintSweep=3,
    eLDLLoadSweepData=4,
    eLDLQueryPrinter = 5,
    eLDLComments=7,
    eLDLHandlePen = 8,
    eLDLControl = 12,
    eLDLDataStreamVersion=12
}LDLCMD;

typedef enum
{
    eLDLUnknownColor=-1,
    eLDLBlack,
    eLDLCyan,
    eLDLMagenta,
    eLDLYellow,
    eLDLLoCyan,
    eLDLLoMagenta,
	eLDLLoBlack,
    eLDLMaxColor
} COLORENUM;


#define LDLPACKET_MINSIZE   16  // define the minimum packet size defined by the protocol
#define FRAME_SYN '$'           // defined the synchronization frame
#define OPTIMIZED_DELAYLIMIT 0  // defined how many PrintSweep will be sent before the
                                // first LoadSweepData command

#define LDL_MAX_IMAGE_SIZE  2048


#define SIZEOF_LDLHDR 10
#define SIZEOF_LDLTERM 1
#define SIZEOF_LDL_JOB_CMDOPT 5

// define possible operation field
#define OPERATION_STJOB     0
#define OPERATION_ENDJOB    1
#define OPERATION_CANCJOB   2

#define SIZEOF_LDL_LDPAGE_CMDOPT 17

// defn for possible option field settings
#define MEDIATYPE_PLAIN 0           // possible types for mediatype field
#define MEDIATYPE_PHOTO 3

#define MEDIASRC_MAINTRAY 0         // possible setting for mediasrc

#define MEDIADEST_MAINBIN 0         // possible setting for mediadest

#define QUALITYLEVEL_DRAFT -1       // possible setting for quality
#define QUALITYLEVEL_NORMAL 0
#define QUALITYLEVEL_BEST   1
#define QUALITYLEVEL_WONDER 2

#define SPECLOAD_NONE       0       // possible setting for specload
#define SPECLOAD_ENVELOPE   1

#define MEDIALD_SPEED           0x00000001  // bitfield defn for opt_fields
#define NEED_TO_SERVICE_PERIOD  0x00000002
#define MINTIME_BTW_SWEEP       0x00000004

#define DEVUNITS_XBOW      2400 // Crossbow device units is 2400 dots per inch

#define SIZEOF_LDL_LDPAGE_OPTFLDS 4

#define SIZEOF_LDL_COLROPT_ACTIVECOLR 2

#define NO_ACTIVE_COLORS 0x0000

#define SIZEOF_LDL_EJPAGE_CMDOPT 4

#define MEDIA_EJSPEED       0x00000001      // bitfield defn for opt_fields

#define SIZEOF_LDL_EJPAGE_OPTFLDS 1

#define SIZEOF_LDL_PRTSWP_CMDOPT 18

#define SWINGFMT_UNCOMPRSS      0 // define possible swing format

#define PRNDRN_LEFTTORIGHT      0 // define the possible print direction
#define PRNDRN_RIGHTTOLEFT      1

#define IPS_CARRSPEED           0x00000001 // bitfield defn for printsweep optional field
#define IPS_INIPRNSPEED         0x00000002
#define IPS_MEDIASPEED          0x00000004
#define PAPER_ACCURACY          0x00000008
#define ACCURATEPOSN_NEEDED     0x00000010
#define DRYTIME                 0x00000020 // bit 6-31 undefined

#define SIZEOF_LDL_PRTSWP_OPTFLDS 3

#define SIZEOF_LDL_PRTSWP_COLROPT 29

#define SIZEOF_LDL_LDSWPDATA_CMDOPT 2

#define NO_ECHO 0
#define ECHO_DATA 1

#define OPERATION_CONTINUE   2
#define DATASTREAMVERSION    3

#define OPERATION_SPIT_PEN   2

// possible values for ldlversion
#define CURR_LDLVERSION     0x00000302   // lidil version of 0.3.2

#endif
