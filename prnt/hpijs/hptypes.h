/*****************************************************************************\
  hptypes.h : HP types defined used by imaging modeules

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


#ifndef HPTYPES_H
#define HPTYPES_H
//===========================================================================
//
//  Filename     :  hptypes.h
//
//  Module       :  Open Source Imaging
//
//  Description  :  This file contains HP Types used by imaging modules.
//
//============================================================================

//=============================================================================
//  Header file dependencies
//=============================================================================

#define HPFAR

#define ENVPTR(typeName)                    HPFAR *typeName

typedef unsigned char   HPUInt8;

typedef const unsigned char   HPCUInt8;

typedef signed short    HPInt16, ENVPTR(HPInt16Ptr);

typedef const signed short  HPCInt16, ENVPTR(HPCInt16Ptr);

typedef unsigned short  HPUInt16;

typedef uint32_t        HPUInt32, ENVPTR(HPUInt32Ptr);

typedef int             HPBool;

typedef HPUInt8         HPByte, ENVPTR(HPBytePtr);

typedef HPCUInt8        HPCByte, ENVPTR(HPCBytePtr);

typedef HPUInt32    HPUIntPtrSize, ENVPTR(HPUIntPtrSizePtr);

#define kSpringsErrorType       HPInt16
#define kSpringsErrorTypePtr    HPInt16Ptr

#define HPTRUE    1
#define HPFALSE   0

#endif // HPTYPES_H
