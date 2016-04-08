/*****************************************************************************\
  debug.h : Interface for debuging support

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


#ifndef DEBUG_H
#define DEBUG_H

#include <assert.h>

#define NO_DEBUG    0x00
#define DBG_ASSERT  0x01                    // 0001
#define DBG_LVL1    0x02                    // 0010
#define DBG_DFLT    (DBG_ASSERT | DBG_LVL1) // 0011
#define PUMP        0x04                    // 0100
#define HARNESS     0x08                    // 1000

#ifndef ASSERT  // System may already have ASSERT handling
    // ANSI C/C++ defines assert(...) as a macro that will check the expression in the
    // debugging version and compile to (void)0 in the release version so we will just
    // define the ALL CAPS version of ASSERT for ourselves.
    #define ASSERT(EXP) assert(EXP)
#endif

#ifndef DBG1 // this allows for a different implementation elsewhere (ie platform.h)
    #if defined(DEBUG) && (DBG_MASK & DBG_LVL1)
        #define DBG1(STUFF) printf(STUFF)
    #else
        #define DBG1(_STUFF) (void(0))
    #endif
#endif

#ifndef TRACE
    #if defined(DEBUG) && (DBG_MASK & DBF_LVL1)
        #define TRACE       printf
    #else
		#if defined(__GNUC__)
				 #define TRACE(args...)    ((void)0)
		#else
				 #define TRACE       1 ? 0 : printf
		#endif
    #endif
#endif

#if defined(DEBUG) && (DBG_MASK & PUMP)
    #define USAGE_LOG
    #define NULL_IO
#endif

#if defined(DEBUG) && (DBG_MASK & HARNESS)
    #define APDK_CAPTURE
#endif

#endif //DEBUG_H


