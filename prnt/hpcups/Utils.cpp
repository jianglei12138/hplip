/*****************************************************************************\
  Utils.cpp : implementaiton of utility functions

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

#include "CommonDefinitions.h"
#include "utils.h"

int SendChunkHeader (BYTE *szStr, DWORD dwSize, DWORD dwChunkType, DWORD dwNumItems)
{
    for (int j = 3, i = 0; j >= 0; j--)
    {
        szStr[i] = (BYTE) ((dwSize >> (8 * (j))) & 0xFF);
        szStr[4+i] = (BYTE) ((dwChunkType >> (8 * (j))) & 0xFF);
        szStr[8+i] = (BYTE) ((dwNumItems >> (8 * (j))) & 0xFF);
        i++;
    }

    szStr[12] = (BYTE) (((dwNumItems * 12) & 0xFF00) >> 8);
    szStr[13] = (BYTE) (((dwNumItems * 12) & 0x00FF));

    szStr[14] = 'Z';
    szStr[15] = 'Z';
    return 16;
}

int SendItemExtra (BYTE *szStr, BYTE cType, WORD wItem, DWORD dwValue, DWORD dwExtra)
{
    int        i, j;
    dwExtra += 12;
    for (j = 3, i = 0; j >= 0; j--)
    {
        szStr[i++] = (BYTE) ((dwExtra >> (8 * (j))) & 0xFF);
    }
    szStr[i++] = (BYTE) ((wItem & 0xFF00) >> 8);
    szStr[i++] = (BYTE) ((wItem & 0x00FF));
    szStr[i++] = (BYTE) cType;
    szStr[i++] = 0;
    for (j = 3; j >= 0; j--)
    {
        szStr[i++] = (BYTE) ((dwValue >> (8 * (j))) & 0xFF);
    }
    return i;
}

int SendItem (BYTE *szStr, BYTE cType, WORD wItem, DWORD dwValue)
{
    return SendItemExtra (szStr, cType, wItem, dwValue, 0);
}

int SendIntItem (BYTE *szStr, int iItem, int iItemType, int iItemValue)
{
    int        i = 0;
    int        j;
    for (j = 3; j >= 0; j--)
    {
        szStr[i++] = (BYTE) ((iItem >> (8 * (j))) & 0xFF);
    }
    for (j = 3; j >= 0; j--)
    {
        szStr[i++] = (BYTE) ((iItemType >> (8 * (j))) & 0xFF);
    }
    for (j = 3; j >= 0; j--)
    {
        szStr[i++] = (BYTE) ((iItemValue >> (8 * (j))) & 0xFF);
    }
    return i;
}

