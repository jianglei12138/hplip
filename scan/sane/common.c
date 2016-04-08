/********************************************************************************************\

  common.c - common code for scl, pml, and soap backends

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

  Contributing Authors: David Paschal, Don Welch, David Suffield, Naga Samrat Chowdary Narla
						Sarbeswar Meher

\********************************************************************************************/

#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>
#include <string.h>
#include <ctype.h>
#include "common.h"

#define DEBUG_NOT_STATIC
#include "sanei_debug.h"

int __attribute__ ((visibility ("hidden"))) bug(const char *fmt, ...)
{
   char buf[256];
   va_list args;
   int n;

   va_start(args, fmt);
   if ((n = vsnprintf(buf, 256, fmt, args)) == -1)
      buf[255] = 0;     /* output was truncated */
   syslog(LOG_WARNING, "%s", buf);
   DBG(2, "%s", buf);
   va_end(args);
   return n;
}

void __attribute__ ((visibility ("hidden"))) sysdump(const void *data, int size)
{
    /* Dump size bytes of *data. Output looks like:
     * [0000] 75 6E 6B 6E 6F 77 6E 20 30 FF 00 00 00 00 39 00 unknown 0.....9.
     */

    unsigned char *p = (unsigned char *)data;
    unsigned char c;
    int n;
    char bytestr[4] = {0};
    char addrstr[10] = {0};
    char hexstr[16*3 + 5] = {0};
    char charstr[16*1 + 5] = {0};
    for(n=1;n<=size;n++) {
        if (n%16 == 1) {
            /* store address for this line */
            snprintf(addrstr, sizeof(addrstr), "%.4d", (int)((p-(unsigned char *)data) & 0xffff));
        }
            
        c = *p;
        if (isprint(c) == 0) {
            c = '.';
        }

        /* store hex str (for left side) */
        snprintf(bytestr, sizeof(bytestr), "%02X ", *p);
        strncat(hexstr, bytestr, sizeof(hexstr)-strlen(hexstr)-1);

        /* store char str (for right side) */
        snprintf(bytestr, sizeof(bytestr), "%c", c);
        strncat(charstr, bytestr, sizeof(charstr)-strlen(charstr)-1);

        if(n%16 == 0) { 
            /* line completed */
            DBG_SZ("[%4.4s]   %-50.50s  %s\n", addrstr, hexstr, charstr);
            hexstr[0] = 0;
            charstr[0] = 0;
        }
        p++; /* next byte */
    }

    if (strlen(hexstr) > 0) {
        /* print rest of buffer if not empty */
        DBG_SZ("[%4.4s]   %-50.50s  %s\n", addrstr, hexstr, charstr);
    }
}

void __attribute__ ((visibility ("hidden"))) bugdump(const void *data, int size)
{
    /* Dump size bytes of *data. Output looks like:
     * [0000] 75 6E 6B 6E 6F 77 6E 20 30 FF 00 00 00 00 39 00 unknown 0.....9.
     */

    unsigned char *p = (unsigned char *)data;
    unsigned char c;
    int n;
    char bytestr[4] = {0};
    char addrstr[10] = {0};
    char hexstr[16*3 + 5] = {0};
    char charstr[16*1 + 5] = {0};
    for(n=1;n<=size;n++) {
        if (n%16 == 1) {
            /* store address for this line */
            snprintf(addrstr, sizeof(addrstr), "%.4d", (int)((p-(unsigned char *)data) & 0xffff));
        }
            
        c = *p;
        if (isprint(c) == 0) {
            c = '.';
        }

        /* store hex str (for left side) */
        snprintf(bytestr, sizeof(bytestr), "%02X ", *p);
        strncat(hexstr, bytestr, sizeof(hexstr)-strlen(hexstr)-1);

        /* store char str (for right side) */
        snprintf(bytestr, sizeof(bytestr), "%c", c);
        strncat(charstr, bytestr, sizeof(charstr)-strlen(charstr)-1);

        if(n%16 == 0) { 
            /* line completed */
            BUG_SZ("[%4.4s]   %-50.50s  %s\n", addrstr, hexstr, charstr);
            hexstr[0] = 0;
            charstr[0] = 0;
        }
        p++; /* next byte */
    }

    if (strlen(hexstr) > 0) {
        /* print rest of buffer if not empty */
        BUG_SZ("[%4.4s]   %-50.50s  %s\n", addrstr, hexstr, charstr);
    }
}

char __attribute__ ((visibility ("hidden"))) *psnprintf(char *buf, int bufSize, const char *fmt, ...)
{
   va_list args;
   int n;

   buf[0] = 0;

   va_start(args, fmt);
   if ((n = vsnprintf(buf, bufSize, fmt, args)) == -1)
      buf[bufSize] = 0;     /* output was truncated */
   va_end(args);

   return buf;
}

unsigned long __attribute__ ((visibility ("hidden"))) DivideAndShift( int line,
                              unsigned long numerator1,
                              unsigned long numerator2,
                              unsigned long denominator,
                              int shift )
{
    unsigned long remainder, shiftLoss = 0;
    unsigned long long result = numerator1;
    result *= numerator2;
    if( shift > 0 )
    {
        result <<= shift;
    }
    remainder = result % denominator;
    result /= denominator;
    if( shift < 0 )
    {
        shiftLoss = result & ( ( 1 << ( -shift ) ) - 1 );
        result >>= ( -shift );
    }
    return result;
}

void __attribute__ ((visibility ("hidden"))) NumListClear( int * list )
{
    memset( list, 0, sizeof( int ) * MAX_LIST_SIZE );
}

int __attribute__ ((visibility ("hidden"))) NumListIsInList( int * list, int n )
{
    int i;
    for( i = 1; i < MAX_LIST_SIZE; i++ )
    {
        if( list[i] == n )
        {
            return 1;
        }
    }
    return 0;
}

int __attribute__ ((visibility ("hidden"))) NumListAdd( int * list, int n )
{
    if( NumListIsInList( list, n ) )
    {
        return 1;
    }
    if( list[0] >= ( MAX_LIST_SIZE - 1 ) )
    {
        return 0;
    }
    list[0]++;
    list[list[0]] = n;
    return 1;
}

int __attribute__ ((visibility ("hidden"))) NumListGetCount( int * list )
{
    return list[0];
}

int __attribute__ ((visibility ("hidden"))) NumListGetFirst( int * list )
{
    int n = list[0];
    if( n > 0 )
    {
        n = list[1];
    }
    return n;
}

void __attribute__ ((visibility ("hidden"))) StrListClear( const char ** list )
{
    memset( list, 0, sizeof( char * ) * MAX_LIST_SIZE );
}

int __attribute__ ((visibility ("hidden"))) StrListIsInList( const char ** list, char * s )
{
    while( *list )
    {
        if( !strcasecmp( *list, s ) )
        {
            return 1;
        }
        list++;
    }
    return 0;
}

int __attribute__ ((visibility ("hidden"))) StrListAdd( const char ** list, char * s )
{
    int i;
    for( i = 0; i < MAX_LIST_SIZE - 1; i++ )
    {
        if( !list[i] )
        {
            list[i] = s;
            return 1;
        }
        if( !strcasecmp( list[i], s ) )
        {
            return 1;
        }
    }
    return 0;
}

char* __attribute__ ((visibility ("hidden"))) itoa(int value, char* str, int radix)
{
  static char dig[] = "0123456789""abcdefghijklmnopqrstuvwxyz";
  int n = 0, neg = 0;
  unsigned int v;
  char* p, *q;
  char c;
 
  if (radix == 10 && value < 0)
  {
    value = -value;
    neg = 1;
   }
  v = value;
  do {
    str[n++] = dig[v%radix];
    v /= radix;
  } while (v);
  if (neg)
    str[n++] = '-';
    str[n] = '\0';
 
  for (p = str, q = p + (n-1); p < q; ++p, --q)
    c = *p, *p = *q, *q = c;
  return str;
}




