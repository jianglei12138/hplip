/************************************************************************************\

  common.h - common code for scl, pml and soap backends

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

  Contributing Authors: David Paschal, Don Welch, David Suffield, Sarbeswar Meher 

\************************************************************************************/

#ifndef _COMMON_H
#define _COMMON_H

#include <syslog.h>

// Uncomment the following line to get verbose debugging output
//#define HPAIO_DEBUG

#define _STRINGIZE(x) #x
#define STRINGIZE(x) _STRINGIZE(x)

#define BUG(args...) {syslog(LOG_ERR, __FILE__ " " STRINGIZE(__LINE__) ": " args); DBG(2, __FILE__ " " STRINGIZE(__LINE__) ": " args);}
#define BUG_DUMP(data, size) bugdump((data), (size))
#define BUG_SZ(args...) {syslog(LOG_ERR, args); DBG(2, args);}

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

#define BACKEND_NAME hpaio

#define BREAKPOINT __asm( "int3" )

#define OK 1
#define ERROR 0
#define MAX_LIST_SIZE 32
#define EXCEPTION_TIMEOUT 45 /* seconds */

#define STR_COMPRESSION_NONE  SANE_I18N("None")
#define STR_COMPRESSION_MH  SANE_I18N("MH")
#define STR_COMPRESSION_MR  SANE_I18N("MR")
#define STR_COMPRESSION_MMR SANE_I18N("MMR")
#define STR_COMPRESSION_JPEG  SANE_I18N("JPEG")

#define STR_ADF_MODE_AUTO SANE_I18N("Auto")
#define STR_ADF_MODE_FLATBED  SANE_I18N("Flatbed")
#define STR_ADF_MODE_ADF  SANE_I18N("ADF")
#define STR_ADF_MODE_CAMERA  SANE_I18N("Camera")

#define STR_TITLE_ADVANCED SANE_I18N("Advanced")

#define STR_NAME_COMPRESSION "compression"
#define STR_TITLE_COMPRESSION SANE_I18N("Compression")
#define STR_DESC_COMPRESSION SANE_I18N("Selects the scanner compression method for faster scans, possibly at the expense of image quality.")

#define STR_NAME_JPEG_QUALITY "jpeg-quality"
#define STR_TITLE_JPEG_QUALITY SANE_I18N("JPEG compression factor")
#define STR_DESC_JPEG_QUALITY SANE_I18N("Sets the scanner JPEG compression factor. Larger numbers mean better compression, " \
                                                           "and smaller numbers mean better image quality.")

#define STR_NAME_BATCH_SCAN "batch-scan"
#define STR_TITLE_BATCH_SCAN SANE_I18N("Batch scan")
#define STR_DESC_BATCH_SCAN SANE_I18N("Enables continuous scanning with automatic document feeder (ADF).")

#define STR_NAME_DUPLEX "duplex"
#define STR_TITLE_DUPLEX SANE_I18N("Duplex")
#define STR_DESC_DUPLEX SANE_I18N("Enables scanning on both sides of the page.")

#define STR_TITLE_GEOMETRY SANE_I18N("Geometry")

#define STR_NAME_LENGTH_MEASUREMENT "length-measurement"
#define STR_TITLE_LENGTH_MEASUREMENT SANE_I18N("Length measurement")
#define STR_DESC_LENGTH_MEASUREMENT SANE_I18N("Selects how the scanned image length is measured and " \
                                   "reported, which is impossible to know in advance for scrollfed scans.")

#define STR_LENGTH_MEASUREMENT_UNKNOWN    SANE_I18N("Unknown")
#define STR_LENGTH_MEASUREMENT_UNLIMITED  SANE_I18N("Unlimited")
#define STR_LENGTH_MEASUREMENT_APPROXIMATE  SANE_I18N("Approximate")
#define STR_LENGTH_MEASUREMENT_PADDED   SANE_I18N("Padded")
#define STR_LENGTH_MEASUREMENT_EXACT    SANE_I18N("Exact")
#define STR_UNKNOWN  SANE_I18N("???")

#define MIN_JPEG_COMPRESSION_FACTOR 0
#define MAX_JPEG_COMPRESSION_FACTOR 100
/* To prevent "2252" asserts on OfficeJet 600 series: */
#define SAFER_JPEG_COMPRESSION_FACTOR 10

#define BEND_GET_SHORT(s) (((s)[0]<<8)|((s)[1]))
#define BEND_GET_LONG(s) (((s)[0]<<24)|((s)[1]<<16)|((s)[2]<<8)|((s)[3]))
#define BEND_SET_SHORT(s,x) ((s)[0]=((x)>>8)&0xFF,(s)[1]=(x)&0xFF)
#define BEND_SET_LONG(s,x) ((s)[0]=((x)>>24)&0xFF,(s)[1]=((x)>>16)&0xFF,(s)[2]=((x)>>8)&0xFF,(s)[3]=(x)&0xFF)
#define LEND_GET_SHORT(s) (((s)[1]<<8)|((s)[0]))
#define LEND_GET_LONG(s) (((s)[3]<<24)|((s)[2]<<16)|((s)[1]<<8)|((s)[0]))
#define LEND_SET_SHORT(s,x) ((s)[1]=((x)>>8)&0xFF,(s)[0]=(x)&0xFF)
#define LEND_SET_LONG(s,x) ((s)[3]=((x)>>24)&0xFF,(s)[2]=((x)>>16)&0xFF,(s)[1]=((x)>>8)&0xFF,(s)[0]=(x)&0xFF)

#define GEOMETRY_OPTION_TYPE    SANE_TYPE_FIXED
#define MILLIMETER_SHIFT_FACTOR   SANE_FIXED_SCALE_SHIFT

#define DECIPOINTS_PER_INCH     720
#define DEVPIXELS_PER_INCH      300
#define MILLIMETERS_PER_10_INCHES   254
#define INCHES_PER_254_MILLIMETERS  10

#define BYTES_PER_LINE(pixelsPerLine,bitsPerPixel) \
    ((((pixelsPerLine)*(bitsPerPixel))+7)/8)

#define INCHES_TO_MILLIMETERS(inches) \
    DivideAndShift(__LINE__, \
    (inches), \
    MILLIMETERS_PER_10_INCHES, \
    INCHES_PER_254_MILLIMETERS, \
    MILLIMETER_SHIFT_FACTOR)

#define DECIPIXELS_TO_MILLIMETERS(decipixels) \
    DivideAndShift(__LINE__, \
    (decipixels), \
    MILLIMETERS_PER_10_INCHES, \
    INCHES_PER_254_MILLIMETERS*hpaio->decipixelsPerInch, \
    MILLIMETER_SHIFT_FACTOR)

#define MILLIMETERS_TO_DECIPIXELS(millimeters) \
    DivideAndShift(__LINE__, \
    (millimeters), \
    INCHES_PER_254_MILLIMETERS*hpaio->decipixelsPerInch, \
    MILLIMETERS_PER_10_INCHES, \
    -MILLIMETER_SHIFT_FACTOR)

#define PIXELS_TO_MILLIMETERS(pixels,pixelsPerInch) \
    DivideAndShift(__LINE__, \
    (pixels), \
    MILLIMETERS_PER_10_INCHES, \
    (pixelsPerInch)*INCHES_PER_254_MILLIMETERS, \
    MILLIMETER_SHIFT_FACTOR)

#define MILLIMETERS_TO_PIXELS(millimeters,pixelsPerInch) \
    DivideAndShift(__LINE__, \
    (millimeters), \
    INCHES_PER_254_MILLIMETERS*(pixelsPerInch), \
    MILLIMETERS_PER_10_INCHES, \
    -MILLIMETER_SHIFT_FACTOR)

#define ADD_XFORM(x) \
  do { \
    pXform->eXform=x; \
    pXform++; \
  } while(0)

int __attribute__ ((visibility ("hidden"))) bug(const char *fmt, ...);
void __attribute__ ((visibility ("hidden"))) sysdump(const void *data, int size);
void __attribute__ ((visibility ("hidden"))) bugdump(const void *data, int size);
char __attribute__ ((visibility ("hidden"))) *psnprintf(char *buf, int bufSize, const char *fmt, ...);
unsigned long __attribute__ ((visibility ("hidden"))) DivideAndShift(int line, unsigned long numerator1, unsigned long numerator2,
                              unsigned long denominator, int shift);
void __attribute__ ((visibility ("hidden"))) NumListClear( int * list );
int __attribute__ ((visibility ("hidden"))) NumListIsInList( int * list, int n );
int __attribute__ ((visibility ("hidden"))) NumListAdd( int * list, int n );
int __attribute__ ((visibility ("hidden"))) NumListGetCount( int * list );
int __attribute__ ((visibility ("hidden"))) NumListGetFirst( int * list );
void __attribute__ ((visibility ("hidden"))) StrListClear( const char ** list );
int __attribute__ ((visibility ("hidden"))) StrListIsInList( const char ** list, char * s );
int __attribute__ ((visibility ("hidden"))) StrListAdd( const char ** list, char * s );
char* __attribute__ ((visibility ("hidden"))) itoa(int value, char* str, int radix);
#endif
