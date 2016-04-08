/*****************************************************************************\
  CommonDefinitions.h : common header

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

  Author: Naga Samrat Chowdary Narla,
\*****************************************************************************/

#ifndef COMMON_DEFINITIONS_H
#define COMMON_DEFINITIONS_H

#include <stdio.h>
#include <inttypes.h>
#include <stdint.h>
//#include <machine/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <cups/cups.h>
#include <cups/raster.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <assert.h>
#include <time.h>
#include <sys/timeb.h>
#include <string.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#define BASIC_LOG          1
#define SAVE_OUT_FILE      2
#define SAVE_INPUT_RASTERS 4
#define SAVE_OUT_FILE_IN_BACKEND    8
#define DONT_SEND_TO_BACKEND   16
#define DONT_SEND_TO_PRINTER   32

#define MAX_COLORTYPE 2
#define NUMBER_PLANES 3

#define ASSERT assert

#define _STRINGIZE(x) #x
#define STRINGIZE(x) _STRINGIZE(x)

#define dbglog(args...) {syslog(LOG_DEBUG, __FILE__ " " STRINGIZE(__LINE__) ": " args); \
fprintf(stderr, __FILE__ " " STRINGIZE(__LINE__) ": " args);}


typedef unsigned char     BYTE;

#ifndef ABS
    #define ABS(x)      ( ((x)<0) ? -(x) : (x) )
#endif

#ifndef MIN
    #define MIN(a,b)    (((a)>=(b))?(b):(a))
#endif

#ifndef MAX
    #define MAX(a,b)    (((a)<=(b))?(b):(a))
#endif

#ifdef APDK_LITTLE_ENDIAN
    #define GetRed(x) (((x >> 16) & 0x0FF))
    #define GetGreen(x) (((x >> 8) & 0x0FF))
    #define GetBlue(x) ((x & 0x0FF))
 #else
    #define GetRed(x) (((x >> 24) & 0x0FF))
    #define GetGreen(x) (((x >> 16) & 0x0FF))
    #define GetBlue(x) (((x >> 8) & 0x0FF))
 #endif

#ifdef BLACK_PEN
    #undef BLACK_PEN
#endif

#ifdef NO_ERROR
    #undef NO_ERROR
#endif

#define HIBYTE(sVar) (BYTE) ((sVar & 0xFF00) >> 8)
#define LOBYTE(sVar) (BYTE) ((sVar & 0x00FF))
#ifndef LOWORD
    #define LOWORD(l)   ((unsigned short) (l))
#endif

#ifndef HIWORD
    #define HIWORD(l)   ((unsigned short) (((uint32_t) (l) >> 16) & 0xFFFF))
#endif

#define PCL_BUFFER_SIZE    10000

typedef unsigned short  UInt16;
typedef unsigned long   UInt32;
typedef unsigned char   UChar;
typedef unsigned int    Int16;
typedef long     Int32;

typedef struct RASTERDATA
{
    int      rastersize[MAX_COLORTYPE];
    BYTE     *rasterdata[MAX_COLORTYPE];
} RASTERDATA;

const int MAXCOLORDEPTH = 3;

const int MAXCOLORPLANES = 6;   // current max anticipated, 6 for 690 photopen

const int MAXCOLORROWS = 2;     // multiple of high-to-low for mixed-resolution cases

enum HALFTONING_ALGORITHM
{
    FED,
    MATRIX
};

/*
 *  values of DRIVER_ERROR
 *  Values < 0 are warnings
 */

typedef enum
{

// general or system errors
    NO_ERROR             =  0x00,    //!< everything okay
    JOB_CANCELED         =  0x01,    //!< CANCEL chosen by user
    SYSTEM_ERROR         =  0x02,    //!< something bad that should not have happened
    ALLOCMEM_ERROR       =  0x03,    //!< failed to allocate memory
    NO_PRINTER_SELECTED  =  0x04,    //!< indicates improper calling sequence or unidi
    INDEX_OUT_OF_RANGE   =  0x05,    //!< what it says
    ILLEGAL_RESOLUTION   =  0x06,    //!< tried to set resolution at unacceptable value
    NULL_POINTER         =  0x07,    //!< supplied ptr was null
    MISSING_PENS         =  0x08,    //!< one or more printhead/pen missing

// build-related
// (items either absent from current build, or just bad index from client code)
    UNSUPPORTED_PRINTER  =  0x10,    //!< selected printer-type unsupported in build
    UNSUPPORTED_PEN      =  0x11,    //!< selected pen-type unsupported
    GRAPHICS_UNSUPPORTED =  0x13,    //!< no graphics allowed in current build
    ILLEGAL_COORDS       =  0x15,    //!< bad (x,y) passed to TextOut
    BAD_INPUT_WIDTH      =  0x18,    //!< inputwidth is 0 and
    OUTPUTWIDTH_EXCEEDS_PAGEWIDTH = 0x19, //!< inputwidth exceeds printable width
    UNSUPPORTED_PRINTMODE = 0x19,    //!< requested printmode not available

// I/O related
    IO_ERROR             =  0x20,    //!< I/O error communicating with printer
    BAD_DEVICE_ID        =  0x21,    //!< bad or garbled device id from printer
    CONTINUE_FROM_BLOCK  =  0x22,    //!< continue from blocked state for printers with no buttons

//  Runtime related
    PLUGIN_LIBRARY_MISSING = 0x30,   //!< a required plugin (dynamic) library is missing

// WARNINGS
// convention is that values < 0 can be ignored (at user's peril)
    WARN_MODE_MISMATCH    =  -1,     //!< printmode selection incompatible with pen, tray, etc.
    WARN_DUPLEX           =  -2,     //!< duplexer installed; our driver can't use it
    WARN_LOW_INK_BOTH_PENS=  -3,     //!< sensor says pens below threshold
    WARN_LOW_INK_BLACK    =  -4,     //!< sensor says black pen below threshold
    WARN_LOW_INK_COLOR    =  -5,     //!< sensor says color pen below threshold

    WARN_LOW_INK_PHOTO    =  -10,    //!< sensor says photo pen below threshold
    WARN_LOW_INK_GREY     =  -11,    //!< sensor says grey pen below threshold
    WARN_LOW_INK_BLACK_PHOTO =  -12,     //!< sensor says black and photo pens below threshold
    WARN_LOW_INK_COLOR_PHOTO =  -13,     //!< sensor says color and photo pens below threshold
    WARN_LOW_INK_GREY_PHOTO  =  -14,     //!< sensor says grey and photo pens below threshold
    WARN_LOW_INK_COLOR_GREY  =  -15,     //!< sensor says color and grey pens below threshold
    WARN_LOW_INK_COLOR_GREY_PHOTO  =  -16,     //!< sensor says color, photo, and grey pens below threshold
    WARN_LOW_INK_COLOR_BLACK_PHOTO  =  -17,     //!< sensor says color, photo, and black pens below threshold
    WARN_LOW_INK_CYAN               = -18,      //!< sensor says cyan ink below threshold
    WARN_LOW_INK_MAGENTA            = -19,      //!< sensor says magenta ink below threshold
    WARN_LOW_INK_YELLOW             = -20,      //!< sensor says yellow ink below threshold
    WARN_LOW_INK_MULTIPLE_PENS      = - 21,     //!< sensor says more than one pen below threshold
    WARN_ILLEGAL_PAPERSIZE = -8,     //!< papersize illegal for given hardware
    ILLEGAL_PAPERSIZE      = -8, 
    WARN_INVALID_MEDIA_SOURCE = -9,   //!< media source tray is invalid
    
    eCreate_Thread_Error = 128   //! Thread error creating the error....
} DRIVER_ERROR; //DRIVER_ERROR

enum DUPLEXMODE
{
    DUPLEXMODE_NONE,
    DUPLEXMODE_BOOK,
    DUPLEXMODE_TABLET
};

enum ENDIAN_TYPE
{
    LITTLEENDIAN,
    BIGENDIAN
};

// used to encourage consistent ordering of color planes
#define PLANE_K   0
#define PLANE_C   1
#define PLANE_M   2
#define PLANE_Y   3
#define Clight    4
#define Mlight    5
#define kWhite    0x00FFFFFE

#define K         0
#define C         1
#define M         2
#define Y         3
#define Clight    4
#define Mlight    5
#define RANDSEED  77

/*
 *  ZJStream related definitions
 */

typedef uint32_t          DWORD;
typedef unsigned short    WORD;
typedef enum
{
    ZJT_START_DOC,
    ZJT_END_DOC,
    ZJT_START_PAGE,
    ZJT_END_PAGE,
    ZJT_JBIG_BIH,
    ZJT_JBIG_HID,
    ZJT_END_JBIG,
    ZJT_SIGNATURE,
    ZJT_RAW_IMAGE,
    ZJT_START_PLANE,
    ZJT_END_PLANE,
    ZJT_PAUSE,
    ZJT_BITMAP
} CHUNK_TYPE;

typedef enum
{
/* 0x00*/    ZJI_PAGECOUNT,
/* 0x01*/    ZJI_DMCOLLATE,
/* 0x02*/    ZJI_DMDUPLEX,

/* 0x03*/    ZJI_DMPAPER,
/* 0x04*/    ZJI_DMCOPIES,
/* 0x05*/    ZJI_DMDEFAULTSOURCE,
/* 0x06*/    ZJI_DMMEDIATYPE,
/* 0x07*/    ZJI_NBIE,
/* 0x08*/    ZJI_RESOLUTION_X,
/* 0x09*/    ZJI_RESOLUTION_Y,
/* 0x0A */    ZJI_OFFSET_X,
/* 0x0B */    ZJI_OFFSET_Y,
/* 0x0C */    ZJI_RASTER_X,
/* 0x0D */    ZJI_RASTER_Y,

/* 0x0E */    ZJI_COLLATE,
/* 0x0F */    ZJI_QUANTITY,

/* 0x10 */    ZJI_VIDEO_BPP,
/* 0x11 */    ZJI_VIDEO_X,
/* 0x12 */    ZJI_VIDEO_Y,
/* 0x13 */    ZJI_INTERLACE,
/* 0x14 */    ZJI_PLANE,
/* 0x15 */    ZJI_PALETTE,

/* 0x16 */    ZJI_RET,
/* 0x17 */    ZJI_TONER_SAVE,

/* 0x18 */    ZJI_MEDIA_SIZE_X,
/* 0x19 */    ZJI_MEDIA_SIZE_Y,
/* 0x1A */    ZJI_MEDIA_SIZE_UNITS,

/* 0x1B */    ZJI_CHROMATIC,

/* 0x63 */    ZJI_PAD = 99,

/* 0x64 */    ZJI_PROMPT,

/* 0x65 */    ZJI_BITMAP_TYPE,
/* 0x66 */    ZJI_ENCODING_DATA,
/* 0x67 */    ZJI_END_PLANE,

/* 0x68 */    ZJI_BITMAP_PIXELS,
/* 0x69 */    ZJI_BITMAP_LINES,
/* 0x6A */    ZJI_BITMAP_BPP,
/* 0x6B */    ZJI_BITMAP_STRIDE,

} ZJ_ITEM;

typedef enum
{
    RET_OFF = 0,
    RET_ON,
    RET_AUTO,
    RET_LIGHT,
    RET_MEDIUM,
    RET_DARK
} RET_VALUE;

typedef enum
{
    ZJIT_UINT32 = 1,
    ZJIT_INT32,
    ZJIT_STRING,
    ZJIT_BYTELUT
} CHUNK_ITEM_TYPE;

// very frequently used fragments made into macros for readability
#define CERRCHECK if (constructor_error != NO_ERROR) {dbglog("CERRCHECK fired\n"); return;}
#define ERRCHECK if (err != NO_ERROR) {dbglog("ERRCHECK fired\n"); return err;}
#define NEWCHECK(x) if (x==NULL) return ALLOCMEM_ERROR;
#define CNEWCHECK(x) if (x==NULL) { constructor_error=ALLOCMEM_ERROR; return; }

#define CUSTOM_MEDIA_SIZE 101

#define EVENT_PRINT_FAILED_MISSING_PLUGIN 502
enum COLORTYPE
{
    COLORTYPE_COLOR     = 0,       
    COLORTYPE_BLACK     = 1,    
    COLORTYPE_BOTH      = 2
};

typedef struct ColorMap_s
{
    uint32_t *ulMap1;
    uint32_t *ulMap2;
    unsigned char *ulMap3;
} ColorMap;

enum COMPRESS_MODE
{
    COMPRESS_MODE0       = 0,
    COMPRESS_MODE2       = 2, 
    COMPRESS_MODE9       = 9,
    COMPRESS_MODE_AUTO   = 10,
    COMPRESS_MODE_JPEG   = 11,
    COMPRESS_MODE_LJ     = 12,
    COMPRESS_MODE_GRAFIT = 16
};

enum COMPRESSOR_TYPE
{
    COMPRESSOR_JPEG_QUICKCONNECT,
    COMPRESSOR_JPEG_JETREADY,
    COMPRESSOR_TAOS
};

const int QTABLE_SIZE =  64;
//  jpeglib.h declares these as UINT16, which is defined as unsigned short in jmorecfg.h
typedef struct QTableInfo_s
{
    DWORD qtable0[QTABLE_SIZE];
    DWORD qtable1[QTABLE_SIZE];
    DWORD qtable2[QTABLE_SIZE];
    unsigned int   qFactor;
} QTableInfo;

typedef struct PrintMode_s
{
    const char    *name;
// The resolutions can be different for different planes
    unsigned int ResolutionX[MAXCOLORPLANES];
    unsigned int ResolutionY[MAXCOLORPLANES];

    unsigned int ColorDepth[MAXCOLORPLANES];
    unsigned int dyeCount;      // number of inks in the pen(s)

    HALFTONING_ALGORITHM eHT;

    ColorMap cmap;

    unsigned int BaseResX,BaseResY;
    bool     MixedRes;

    const unsigned char* BlackFEDTable;
    const unsigned char* ColorFEDTable;
} PrintMode;

typedef struct
{
    const char   *printer_platform_name;
    PrintMode    *print_modes;
    int          count;
} PrintModeTable;

typedef struct QualityAttributes_s
{
    int             media_type;
    int             media_subtype;
    int             print_quality;
    char            hbpl1_print_quality[32];
    unsigned int    horizontal_resolution;
    unsigned int    vertical_resolution;
    unsigned int    actual_vertical_resolution;
    char            print_mode_name[32];
} QualityAttributes;

typedef struct MediaAttributes_s
{
    int        pcl_id;
    int        physical_width;
    int        physical_height;
    int        printable_width;
    int        printable_height;
    int        printable_start_x;
    int        printable_start_y;
    int        horizontal_overspray;
    int        vertical_overspray;
    int        left_overspray; 
    int        top_overspray;
    char       PageSizeName[64];
    char       MediaTypeName[64];
} MediaAttributes;

typedef struct JobAttributes_s
{
    int                media_source;
    int                color_mode;
    DUPLEXMODE         e_duplex_mode;
    int                print_borderless;
    int                krgb_mode;
    int                mech_offset;
    QualityAttributes  quality_attributes;
    MediaAttributes    media_attributes;
    int                job_id;
    int                page_order;
    int                total_pages;
    char               job_title[128];
    char               user_name[32];
    char               host_name[32];
    char               domain_name[32];
    char               os_name[160];
    char               driver_version[32];
    char               driver_name[128];
    char               printer_name[160];
    char               job_start_time[32];
    char               uuid[64];
    char               printer_platform[32];
    char               printer_language[32];
    int                integer_values[16];
    int                printer_platform_version;
    int                pre_process_raster;
    int                HPSPDClass;
} JobAttributes;

#endif // COMMON_DEFINITIONS_H

