/*****************************************************************************\
  PrinterCommands.h : Printer command sequences

  Copyright (c) 1996 - 2015, HP Co.
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
\*****************************************************************************/

#ifndef PRINTER_COMMANDS_H
#define PRINTER_COMMANDS_H

const BYTE UEL[]            = {0x1b, '%', '-','1','2','3','4','5','X' };
const BYTE EnterLanguage[]  = {'@','P','J','L',' ','E','N','T','E','R',' ',
                               'L','A','N','G','U','A','G','E','=','P','C','L','3','G','U','I',0x0A};
const BYTE Reset[]          = {0x1b,'E'};
const char PJLExit[]        = "\x1b%-12345X@PJL EOJ\012\x1b%-12345X";
const BYTE grafStart[5]     = {0x1b, '*', 'r', '1', 'A'}; // raster graphics mode
const BYTE grafMode2[5]     = {0x1b, '*', 'b', '2', 'M'}; // Mode2 Compression
const BYTE grafMode3[5]     = {0x1b, '*', 'b', '3', 'M'}; // Mode3 Compression
const BYTE grafMode9[5]     = {0x1b, '*', 'b', '9', 'M'}; // Mode9 Compression
const BYTE seedSame[5]      = {0x1b, '*', 'b', '0', 'S'}; // Reset seed row
const BYTE GrayscaleSeq[10]        = {0x1b, '*', 'o', '5', 'W', 0x0B, 0x01, 0x00, 0x00, 0x02};
const BYTE MediaSubtypeSeq[8]      = {0x1b, 0x2A, 0x6F, 0x35, 0x57, 0x0D, 0x03, 0x00};
                                     // "Esc*o5W 0D 03 00 00 00" Media Type Index
const BYTE EnableDuplex[5]         = {0x1b,'&','l', '2', 'S'};
const BYTE ExtraDryTime[21]        = {0x1b, '&', 'b', '1', '6', 'W', 'P', 'M', 'L', ' ',
                                     0x04, 0x00, 0x06, 0x01, 0x04, 0x01, 0x04, 0x01, 0x06,
                                     0x08, 0x01};
const BYTE crd_sequence_k[18]      = {0x1b, 0x2a, 0x67, 0x31, 0x32, 0x57, 0x06, 0x1F, 0x00, 0x01,
                                     //Esc   *    |g    |# of bytes |W    |frmt |SP   |# of cmpnts 
                                     0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x01, 0x01};
                                     //|Horz Res |Vert Rez   |compr|orien|bits |planes
      
const BYTE crd_sequence_color[18]  = {0x1b, 0x2a, 0x67, 0x31, 0x32, 0x57, 0x06, 0x07, 0x00, 0x01, 
                                     // Esc   *    |g  |# of bytes  |W    |frmt |SP   |# of cmpnts
                                     0x00, 0x00, 0x00, 0x00, 0x0a, 0x01, 0x20, 0x01};
                                     //|Horz Res |Vert Rez   |compr|orien|bits |planes
      
const BYTE crd_sequence_both[26]   = {0x1b, 0x2a, 0x67, 0x32, 0x30, 0x57, 0x06, 0x1F, 0x00, 0x02,
                                     // Esc  *    |g   |# of bytes |W    |frmt |SP   |# of cmpnts
      /* K */                        0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x01, 0x01,
                                     //|Horz Res |Vert Rez   |compr|orien|bits |planes
      /* RGB */                      0x00, 0x00, 0x00, 0x00, 0x0a, 0x01, 0x20, 0x01};
      
const BYTE speed_mech_cmd[8]       = {0x1B, '*', 'o', '5', 'W', 0x0D, 0x02, 0x00};
const BYTE speed_mech_end[10]      = {0x1B, '*', 'o', '5', 'W', 0x0D, 0x05, 0x00, 0x00, 0x01};
const BYTE speed_mech_continue[10] = {0x1B, '*', 'o', '5', 'W', 0x0D, 0x05, 0x00, 0x00, 0x00};
const BYTE black_extract_off[10]   = {0x1B, '*', 'o', '5', 'W', 0x04, 0xC, 0, 0, 0};
const BYTE FRBeginSession[]        = {0xC0, 0x00, 0xF8, 0x86, 0xC0, 0x03, 0xF8, 0x8F, 0xD1, 0x58, 
                                      0x02, 0x58, 0x02, 0xF8, 0x89, 0x41};
const BYTE FRFeedOrientation[]     = {0xC0, 0x00  , 0xF8, 0x28 };
//                                    |fd ori enum|       |ori cmd|                                            
const BYTE FRPaperSize[]           = {0xC0,  0x00      ,0xF8, 0x25};
//                                   |pap siz enum|     |pap sz cmd|
const BYTE FRCustomMediaSize[]     = {0xC0, 0x00, 0xF8, 0x30};
const BYTE FRMedSource[]           = {0xC0,  0x00      ,0xF8, 0x26};
//                                    |Med src enum|     |Med src cmd|
const BYTE FRMedDestination[]      = {0xC0,  0x00        ,0xF8 , 0x24};
//                                    |Med Dest enum|      |Med src cmd|
const BYTE FRBeginPage[]           = {0x43, 0xD3, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x2A, 0x75, 0xC0, 0x07,
                                      0xF8, 0x03, 0x6A, 0xC0, 0xCC, 0xF8, 0x2C, 0x7B,
                                      0xD3, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x4C, 0x6B};
const BYTE FRBeginImage[]          = {0xC2, 0x00, 0x40, 0x70, 0x68, 0xF8, 0x91, 0xC1};

const BYTE FRVUverTag[]            = {0xC2, 0x00, 0x00, 0x04, 0x00 , 0xF8, 0x95};
//                                    |endian alignd         |  |FR_ver_ tag|
const BYTE FRDataLength[]          = {0xC2, 0x86, 0x0A, 0x00, 0x00, 0xF8, 0x92};
//                                    | VU data length|
const BYTE FRVendorUniq[]          = {0x46};
const BYTE FRVUExtn3[]             = {0xC2, 0x11, 0x20, 0x70, 0x68            ,0xF8, 0x91};
//                                    |endian alignd FR rd img tag|      |VU extensn|
const BYTE FROpenDataSource[]      = {0xC0, 0x00, 0xF8, 0x88, 0xC0, 0x01, 0xF8, 0x82, 0x48};
const BYTE FREnterFRMode[]         = {0xC2, 0x06, 0x20, 0x70,0x68, 0xF8, 0x91, 0xC2};
const BYTE FREndPage[]             =  {0x44};
const BYTE FREndSession[]          =  {0x42};
const BYTE FRCloseDataSource[]     =  {0x49};

const BYTE JRBeginSessionSeq[]     = {0xC0, 0x00, 0xF8, 0x86, 0xC0, 0x03, 0xF8, 0x8F, 0xD1, 0x58, 
                                      0x02, 0x58, 0x02, 0xF8, 0x89, 0x41};
const BYTE JRFeedOrientationSeq[]  = {0xC0, 0x00, 0xF8, 0x28 };
//                                    |fd ori enum|       |ori cmd|           
const BYTE JRPaperSizeSeq[]        = {0xC0,  0x00, 0xF8, 0x25};
//                                    |pap siz enum|     |pap sz cmd|
const BYTE JRCustomPaperSizeSeq[]  = {0xF8, 0x2F, 0xC0, 0x00, 0xF8, 0x30};
const BYTE JRMediaSourceSeq[]      = {0xC0,  0x00, 0xF8, 0x26};
//                                    |Med src enum|     |Med src cmd|
const BYTE JRMediaDestinationSeq[] = {0xC0,  0x00, 0xF8, 0x24};
//                                    |Med Dest enum|      |Med src cmd|
const BYTE JRBeginPageSeq[]        = {0x43, 0xD3, 0x64, 0x00, 0x64, 0x00, 0xF8, 0x2A, 0x75, 0xD3, 
                                      0x00, 0x00, 0x00, 0x00, 0xF8, 0x4C, 0x6B};
const BYTE JRBeginImageSeq[]       = {0xC2, 0x00, 0x40, 0x70, 0x68, 0xF8, 0x91, 0xC1};
const BYTE JRReadImageSeq[]        = {0xC2, 0x01, 0x40, 0x70, 0x68, 0xF8, 0x91, 0xC1};
const BYTE JRStripHeightSeq[]      = {0xF8, 0x6D, 0xC1, 0x80, 0x00, 0xF8, 0x63};
const BYTE JRTextObjectTypeSeq[]   = {0xC0, 0x00, 0xF8, 0x96};
// Interleaved Color Enumeration for Mojave
const BYTE JRICESeq[]              = {0xC0, 0x00, 0xF8, 0x98};

const BYTE JRVueVersionTagSeq[]    = {0xC2, 0x00, 0x00, 0x04, 0x00 , 0xF8, 0x95};
//                                    |endian alignd         |  |JR_ver_ tag|
const BYTE JRDataLengthSeq[]       = {0xC2, 0x86, 0x0A, 0x00, 0x00, 0xF8, 0x92};
//                                    | VU data length|
const BYTE JRVendorUniqueSeq[]     = {0x46};
const BYTE JRVueExtn3Seq[]         = {0xC2, 0x02, 0x40, 0x70, 0x68, 0xF8, 0x91 };
//                                    |endian alignd JR rd img tag|   |VU extensn|
const BYTE JREndPageSeq[]          =  {0x44};
const BYTE JREndSessionSeq[]       =  {0x42};

const BYTE JRQTSeq[]               = {0x00, 0x80, 0x00, 0x03, 0x00, 0x00};
const BYTE JRCRSeq[]               = {0x01, 0x80, 0x2C, 0x00, 0x00, 0x00};
const BYTE JRCR1GSeq[]             = {0x05, 0xE0, 0x00, 0x00};
const BYTE JRCR1CSeq[]             = {0x01, 0xE0, 0x14, 0x66};

const BYTE JRSCSeq[][4] =
{
    {
        0x00, 0x20, 0x00, 0x00
    },
    {
        0x00, 0x00, 0x00, 0x00
    },
    {
        0x00, 0x00, 0x00, 0x00
    },
    {
        0x00, 0x20, 0x00, 0x00
    },
    {
        0x00, 0xE0, 0x00, 0x00
    },
    {
        0x00, 0x00, 0x00, 0x00
    },
    {
        0x00, 0x20, 0x00, 0x00
    },
    {
        0x00, 0x00, 0x00, 0x00
    },
    {
        0x00, 0xE0, 0x00, 0x00
    }
};

const BYTE LdlReset[] =             {0x24, 0x00, 0x10, 0x00, 0x06, 0x00, 0x00, 0x00,
                                     0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x24};
const BYTE LdlSync[] =              {0x24, 0x01, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00,
                                     0x08,  0x00};
const BYTE LdlSyncComplete[] =      {0x24, 0x00, 0x10, 0x00, 0x08, 0x00, 0x00, 0x00,
                                     0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x24};
const BYTE LdlContinue[] =          {0x24, 0x00, 0x10, 0x00, 0x00, 0x0C, 0x00, 0x00,
                                     0x00, 0x00, 0x02, 0xFF, 0xFF, 0xFF, 0xFF, 0x24};
const BYTE LdlPrepareToCancel[] =   {0x24, 0x00, 0x10, 0x00, 0x00, 0x0C, 0x00, 0x00,
                                     0x00, 0x00, 0x0A, 0xFF, 0xFF, 0xFF, 0xFF, 0x24};


#endif // PRINTER_COMMANDS_H

