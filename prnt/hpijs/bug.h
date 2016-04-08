/************************************************************************************\

    bug.h - debug support

    (c) 2009 Copyright HP Development Company, LP

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:
    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
    3. Neither the name of the Hewlett-Packard nor the names of its
       contributors may be used to endorse or promote products derived
       from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
    IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
    OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
    IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
    NOT LIMITED TO, PATENT INFRINGEMENT; PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
    STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
    IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.

\************************************************************************************/

#ifndef _BUG_H
#define _BUG_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#define ADDITIONAL_LOG     1
#define SAVE_PCL_FILE      2
#define SAVE_INPUT_RASTERS 4
#define SEND_TO_PRINTER    8

#include <syslog.h>
#include <stdio.h>

#define _STRINGIZE(x) #x
#define STRINGIZE(x) _STRINGIZE(x)

#define BUG(args...) {syslog(LOG_ERR, __FILE__ " " STRINGIZE(__LINE__) ": " args); \
fprintf(stderr, __FILE__ " " STRINGIZE(__LINE__) ": " args);}

#define DBG_DUMP(data, size) sysdump((data), (size))
#if 1
   #define DBG6(args...) DBG(6, __FILE__ " " STRINGIZE(__LINE__) ": " args)
   #define DBG8(args...) DBG(8, __FILE__ " " STRINGIZE(__LINE__) ": " args)
   #define DBG_SZ(args...) DBG(6, args)
#else
   #define DBG6(args...) syslog(LOG_INFO, __FILE__ " " STRINGIZE(__LINE__) ": " args)
   #define DBG8(args...) syslog(LOG_INFO, __FILE__ " " STRINGIZE(__LINE__) ": " args)
   #define DBG_SZ(args...) syslog(LOG_INFO, args)
#endif

#endif /* _BUG_H */
