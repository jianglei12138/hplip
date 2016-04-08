/*****************************************************************************\
  resources.h : externs for open source imaging

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

extern unsigned char HTBinary_open[1024];
extern unsigned char HT300x3004level_open[1024];
extern unsigned char HT600x6004level895_open[1024];

extern unsigned char HTBinary_prop[1024];
extern unsigned char HT300x3004level_prop[1024];
extern unsigned char HT600x6004level895_prop[1024];

extern unsigned char HT600x6004level970_open[1024];
extern unsigned char HT600x6004level970_prop[1024];

extern unsigned char HT600x6004level3600_open[1024];

extern unsigned char HT1200x1200x1_PhotoPres970_open[1024];
extern unsigned char HT600x600x4_Pres970_open[1024];

extern unsigned char HT300x3004level970_open[1024];
extern unsigned char HT1200x1200x1PhotoBest_open[1024];

extern BYTE *GetHT12x12x1_4100_Photo_Best ();

