/*****************************************************************************\
  global_types.h : global types, enums, and #defines for APDK

  Copyright (c) 1996 - 2008, HP Co.
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


/*! \addtogroup globals
Definitions for global variables, types, and #defines
@{
*/

//
// Definitions and structures needed by applications
//
// This file does not include C++ class definitions etc. so it can
// be included by calling C or C++ source files

#ifndef APDK_GLOBAL_TYPES_H
#define APDK_GLOBAL_TYPES_H

#include "models.h"
#include "modes.h"

#define APDK_INVALID_VALUE -9999

#if APDK_AUTO_INCLUDE
    #include "auto-include.h"
#else
    typedef unsigned long uint32_t;
#endif

// ** Defines

#ifdef APDK_NOTHROW
    #include <new>
    //#define NOTHROW (std::nothrow)
    #define new new(std::nothrow)
#endif

// Camera compiler warns about NULL at different locations each time.
// So, force a definition here.

#ifdef NULL
    #undef NULL
#endif

#ifndef NULL
    #define NULL 0
#endif

#ifndef BOOL
    typedef int BOOL;
    #define TRUE 1
    #define FALSE 0
#endif

/*
#ifndef BOOL
   #define BOOL bool
#endif
#ifndef TRUE
   #define TRUE true
#endif
#ifndef FALSE
   #define FALSE false
#endif
*/

typedef unsigned char BYTE;

typedef unsigned short WORD;

typedef uint32_t DWORD;

#ifndef LOWORD
    #define LOWORD(l)   ((WORD) (l))
#endif

#ifndef HIWORD
    #define HIWORD(l)   ((WORD) (((DWORD) (l) >> 16) & 0xFFFF))
#endif

#ifndef ABS
    #define ABS(x)      ( ((x)<0) ? -(x) : (x) )
#endif

#ifdef BLACK_PEN
    #undef BLACK_PEN
#endif

#ifdef NO_ERROR
    #undef NO_ERROR
#endif

APDK_BEGIN_NAMESPACE


//! \internal For use in connection with PCL media-type command.  Values are PCL codes.
enum MediaType
{
    mediaAuto = -1,
    mediaPlain = 0,
    mediaBond = 0,
    mediaSpecial = 2,
    mediaGlossy = 3,
    mediaTransparency = 4,
    mediaHighresPhoto = 3,       // used by vip printers for 2400 mode
    mediaCDDVD = 7,
    mediaBrochure = 8
};


//! \internal For use in connection with PCL media-size command. Values are PCL codes.
enum MediaSize
{
    sizeUSLetter = 2,
    sizeUSLegal = 3,
    sizeA4 = 26,
    sizeNum10Env = 81,
	sizeA2Env = 109,
	sizeC6Env = 92,
	sizeDLEnv = 90,
	size3JPNEnv = 110,
	size4JPNEnv = 111,
    sizePhoto = 74,             // 4x6 Index Card / Photo paper
    sizeA6 = 73,                // used to be 24, full-bleed support is for 73 only. Is same size though
    sizeB4 = 46,
    sizeB5 = 45,
    sizeOUFUKU = 72,
    sizeHAGAKI = 71,
    sizeA3 = 27,
    sizeA5 = 25,
    sizeSuperB = 16,
    sizeLedger = 6,
    sizeFLSA = 10,
    sizeExecutive = 1,
    sizeCustom = 101,
    size5x7 = 122,
    sizeCDDVD80 = 98,
    sizeCDDVD120 = 99
};

//! \internal For use in connection with PCL media-source command.  Values are PCL codes.
enum MediaSource
{
    sourceTrayMin = -2,
    sourceBanner = -1,
    sourceTray1 = 1,
    sourceManual = 2,
    sourceManualEnv = 3,
    sourceTray2 = 4,
	sourceDuplexerNHagakiFeed = 5,
    sourceOptionalEnv = 6,
    sourceTrayPhoto = 6,
    sourceTrayAuto = 7,
    sourceTrayCDDVD = 14,
    sourceTrayMax = 50
};


//! \internal For use in connection with PCL quality-mode command.  Values are PCL codes.
enum Quality
{
    qualityAuto         = -3,
    qualityFastDraft    = -2,
    qualityDraft        = -1,
    qualityNormal       =  0,
    qualityPresentation =  1,
    qualityMarvellous   =  2,
    qualityFastNormal   =  3
};

//! \internal For use in connection with fullbleed support.  Values are type of fullbleed.
enum FullbleedType
{
    fullbleedNotSupported = 0,
    fullbleed3EdgeAllMedia = 1,
    fullbleed3EdgePhotoMedia = 2,
	fullbleed3EdgeNonPhotoMedia = 3,
    fullbleed4EdgeAllMedia = 4,
    fullbleed4EdgePhotoMedia = 5,
	fullbleed4EdgeNonPhotoMedia = 6
};

enum FontIndex
{
    COURIER_INDEX = 1,
    CGTIMES_INDEX = 2,
    LETTERGOTHIC_INDEX = 3,
    UNIVERS_INDEX = 4
};

const int MAX_CHAR_SET = 5;

const int MAX_POINTSIZES = 5;

const int MAXCOLORDEPTH = 3;

const int MAXCOLORPLANES = 6;   // current max anticipated, 6 for 690 photopen

const int MAXCOLORROWS = 2;     // multiple of high-to-low for mixed-resolution cases

// ** JOB related structures/enums

//! Possible pen combinations
enum PEN_TYPE
{
    BLACK_PEN,                  //!< Only BLACK pen in the printer
    COLOR_PEN,                  //!< Only COLOR pen in the printer
    BOTH_PENS,                  //!< BLACK & COLOR pens in printer
    MDL_PEN,                    //!< Photo pen in the printer
    MDL_BOTH,                   //!< COLOR and Photo pen in printer
    MDL_AND_BLACK_PENS,         //!< BLACK and Photo pen in printer
    MDL_BLACK_AND_COLOR_PENS,   //!< BLACK, COLOR and Photo pen in printer
	GREY_PEN,                   //!< Only GREY pen in the printer
	GREY_BOTH,                  //!< COLOR and GREY pen in the printer
	MDL_AND_GREY_PENS,          //!< GREY and Photo pen in printer
	MDL_GREY_AND_COLOR_PENS,    //!< GREY, COLOR, and Photo pen in the printer
	UNKNOWN_PEN,                //!< New Pen type that we have no knowledge yet
    // if more pen or pen combos are added then add them here and point MAX_PEN_TYPE to the last one
    NO_PEN,                     //!< No pens in the printer
    DUMMY_PEN,                  //!< Not a possible value - used for initialization
    MAX_PEN_TYPE = UNKNOWN_PEN  //!< base 0, ending with MDL_BOTH (NOT NO_PEN)
};
//const int MAX_PEN_TYPE = 4;


//! Supported Paper sizes
/*
The PAPER_SIZE enum is directly supported by PSM in PrintContext
do not change the order of the PAPER_SIZE enum.  The static array in
PrintContext depends on this order.  Any changes to this enum may require
changes to the PSM array.
*/
typedef enum              // typedef'ed for C interface
{
    UNSUPPORTED_SIZE =-1,           //!< Not supported paper size (also used as mandatory flag)
    LETTER = 0,                     //!< 8.5 x 11 in.
    A4 = 1,                         //!< 210 x 297 mm.
    LEGAL = 2,                      //!< 8.25 x 14 in.
    PHOTO_SIZE = 3,                 //!< 4x6 Photo with tear-off tab
    A6 = 4,                         //!< 105 x 148 mm.
    CARD_4x6 = 5,                   //!< 4x6 photo/card without tear-off tab
    B4 = 6,                         //!< 250 x 353 mm.
    B5 = 7,                         //!< 176 x 250 mm.
    OUFUKU = 8,                     //!< 148 x 200 mm.
    OFUKU = 8,                      //!< Misspelled - here for backwards compatibility
    HAGAKI = 9,                     //!< 100 x 148 mm.
    A6_WITH_TEAR_OFF_TAB = 10,      //!< A6 with tear-off tab
#ifdef APDK_EXTENDED_MEDIASIZE
    A3 = 11,                        //!< 294 x 419.8 mm.
    A5 = 12,                        //!< 148 x 210 mm.
    LEDGER = 13,                    //!< 11 x 17 in.
    SUPERB_SIZE = 14,               //!< 13 x 19 in.
    EXECUTIVE = 15,                 //!< 7.25 x 10.5 in.
    FLSA = 16,                      //!< 8.5 x 13 in.
    CUSTOM_SIZE = 17,               //!< Custom
	ENVELOPE_NO_10 = 18,            //!< No. 10 Envelope (4.12 x 9.5 in.)
	ENVELOPE_A2 = 19,               //!< A2 Envelope (4.37 x 5.75 in.)
	ENVELOPE_C6 = 20,               //!< C6 Envelope (114 x 162 mm)
	ENVELOPE_DL = 21,               //!< DL Envelope (110 x 220 mm)
	ENVELOPE_JPN3 = 22,             //!< Japanese Envelope #3 (120 x 235 mm)
	ENVELOPE_JPN4 = 23,             //!< Japanese Envelope #4 (90 x 205 mm)
#endif
    PHOTO_5x7,                      //!< 5x7 Photo
    CDDVD_80,                       //!< 3 in. CD or DVD
    CDDVD_120,                      //!< 5 in. CD or DVD
#ifdef APDK_EXTENDED_MEDIASIZE
    PHOTO_4x8,                       //!< Panorama 4 in. x 8 in.
    PHOTO_4x12,                      //!< Panorama 4 in. x 12 in.
    L,                              //!< Japanese card (3.5 in. x 5 in.)
#endif
    MAX_PAPER_SIZE                  //!< Only for array size and loops
} PAPER_SIZE;


typedef enum            // typedef'ed for C interface
{
    CLEAN_PEN = 0
} PRINTER_FUNC;


// ** TEXT related structures/enums

//! Supported Text colors for device text
typedef enum           // typedef'ed for C interface
{
    WHITE_TEXT,                    //!< White
    CYAN_TEXT,                     //!< Cyan
    MAGENTA_TEXT,                  //!< Magenta
    BLUE_TEXT,                     //!< Blue
    YELLOW_TEXT,                   //!< Yellow
    GREEN_TEXT,                    //!< Green
    RED_TEXT,                      //!< Red
    BLACK_TEXT                     //!< Black
} TEXTCOLOR;


// currently only portrait fonts are supported
enum TEXTORIENT
{
    PORTRAIT,
    LANDSCAPE,
    BOTH
};

const int MAX_FONT_SIZES = 10;      // max # of fonts to be realized at one time


// ** I/O related stuff

const int TIMEOUTVAL = 500;         // in msec, ie 0.5 sec

typedef WORD PORTID;
typedef void * PORTHANDLE;

enum MODE1284
{
    COMPATIBILITY,
    NIBBLE,
    ECP
};


enum HALFTONING_ALGORITHM
{
    FED,
    MATRIX
};


//! Color modes for SelectPrintMode
typedef enum               // typedef'ed for C interface
{
    GREY_K,                 //!< Use the BLACK pen to print B&W only
    GREY_CMY,               //!< Use the COLOR pen to print grey scale
    COLOR,                  //!< Use the COLOR pen to print color
    MAX_COLORMODE
} COLORMODE;


//! Quality modes for SelectPrintMode
typedef enum                // typedef'ed for C interface
{
    QUALITY_NORMAL,         //!< Normal quality print mode (probably 300x300)
    QUALITY_DRAFT,          //!< Draft print mode - the same or faster than normal
    QUALITY_BEST,           //!< Probably slower and possible higher resolution
    QUALITY_HIGHRES_PHOTO,  //!< 1200 dpi - currently 9xxvip, linux only
    QUALITY_FASTDRAFT,      //!< True draft, 300 dpi - newer VIP printers only
    QUALITY_AUTO,           //!< Printer selects optimum resolution - 05 and later VIP printers only
    QUALITY_FASTNORMAL,     //!< Normal quality print mode - faster than Normal
    MAX_QUALITY_MODE
} QUALITY_MODE;


//! Media types for SelectPrintMode
typedef enum               // typedef'ed for C interface
{
    MEDIA_PLAIN,            //!< Plain paper
    MEDIA_PREMIUM,          //!< Premium paper - for use with 6xx series
    MEDIA_PHOTO,            //!< Photo paper - for use with photo quality printers
    MEDIA_TRANSPARENCY,     //!< Transparency film
    MEDIA_HIGHRES_PHOTO,    //!< Premium photo paper
    MEDIA_AUTO,             //!< Printer uses media sense to determine media type
    MEDIA_ADVANCED_PHOTO,   //!< Advanced photo paper
    MEDIA_CDDVD = 7,        //!< CD or DVD media
    MEDIA_BROCHURE = 8,     //!< Glossy brochure paper
    MAX_MEDIATYPE
} MEDIATYPE;

//! PhotoTray status
typedef enum               // typedef'ed for C interface
{
    UNKNOWN = -1,           //!< Unknown State
    DISENGAGED = 0,         //!< Photo Tray is nor engaged
    ENGAGED = 1             //!< Photo Tray is engaged
} PHOTOTRAY_STATE;

//////////////////////////////////////////////////////////////////////////////////////
//  values of DRIVER_ERROR
// first of 2 hex digits indicates category

typedef enum                // typedef'ed for C interface
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
    TEXT_UNSUPPORTED     =  0x12,    //!< no text allowed in current build, UsePageWidth is false
    GRAPHICS_UNSUPPORTED =  0x13,    //!< no graphics allowed in current build
    UNSUPPORTED_FONT     =  0x14,    //!< font selection failed
    ILLEGAL_COORDS       =  0x15,    //!< bad (x,y) passed to TextOut
    UNSUPPORTED_FUNCTION =  0x16,    //!< bad selection for PerformPrinterFunction
    BAD_INPUT_WIDTH      =  0x18,    //!< inputwidth is 0 and
    OUTPUTWIDTH_EXCEEDS_PAGEWIDTH = 0x19, //!< inputwidth exceeds printable width
    UNSUPPORTED_SCALING  =  0x1a,    //!< inputwidth exceeds outputwidth, can't shrink output

// I/O related
    IO_ERROR             =  0x20,    //!< I/O error communicating with printer
    BAD_DEVICE_ID        =  0x21,    //!< bad or garbled device id from printer
    CONTINUE_FROM_BLOCK  =  0x22,    //!< continue from blocked state for printers with no buttons

//  Run time related
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
    WARN_FULL_BLEED_UNSUPPORTED = -6,//!< device does not support full-bleed printing
    WARN_FULL_BLEED_3SIDES = -7,     //!< full bleed on only 3 sides
	WARN_FULL_BLEED_PHOTOPAPER_ONLY = -30, //!< device only support full-bleed on photo paper
	WARN_FULL_BLEED_3SIDES_PHOTOPAPER_ONLY = -31, //!< device only support 3 sided full-bleed on photo paper
    WARN_ILLEGAL_PAPERSIZE = -8,     //!< papersize illegal for given hardware
    ILLEGAL_PAPERSIZE      = -8, 
    WARN_INVALID_MEDIA_SOURCE = -9   //!< media source tray is invalid
} DRIVER_ERROR; //DRIVER_ERROR


///////////////////////////////////////////////////////////////////////////////////////

// ** Printer Status return values


enum DISPLAY_STATUS
{    // used for DisplayPrinterStatus
    NODISPLAYSTATUS = -1,
    DISPLAY_PRINTING,
    DISPLAY_PRINTING_COMPLETE,
    DISPLAY_PRINTING_CANCELED,
    DISPLAY_OFFLINE,
    DISPLAY_BUSY,
    DISPLAY_OUT_OF_PAPER,
    DISPLAY_TOP_COVER_OPEN,
    DISPLAY_ERROR_TRAP,
    DISPLAY_NO_PRINTER_FOUND,
    DISPLAY_NO_PEN_DJ400,
    DISPLAY_NO_PEN_DJ600,
    DISPLAY_NO_COLOR_PEN,
    DISPLAY_NO_BLACK_PEN,
    DISPLAY_NO_PENS,
    DISPLAY_PHOTO_PEN_WARN,
    DISPLAY_PRINTER_NOT_SUPPORTED,
    DISPLAY_COMM_PROBLEM,
    DISPLAY_CANT_ID_PRINTER,
    DISPLAY_OUT_OF_PAPER_NEED_CONTINUE,
    DISPLAY_PAPER_JAMMED,
    DISPLAY_PHOTOTRAY_MISMATCH,

    // internal driver use only

    ACCEPT_DEFAULT,
    DISPLAY_PRINTCONTEXT_WARN,
    DISPLAY_PRINTMODE_WARN,
    DISPLAY_JOB_WARN
};


// ** move these to internal.h
// items from wtv_interp.h

const int NUMBER_PLANES = 3;

// must be #define instead of const int for C interface
#define NUMBER_RASTERS  3            // The number of Rasters to Buffer

#ifdef APDK_AUTODUPLEX
enum DUPLEXMODE
{
    DUPLEXMODE_NONE,
    DUPLEXMODE_TABLET,
    DUPLEXMODE_BOOK
};
#endif

typedef enum
{
    SPEED_MECH_HINT,
    PAGES_IN_DOC_HINT,
    EXTRA_DRYTIME_HINT,
    MAX_FILE_SIZE_HINT,
    RED_EYE_REMOVAL_HINT,
    PHOTO_FIX_HINT,
    LEFT_OVERSPRAY_HINT,
    RIGHT_OVERSPRAY_HINT,
    TOP_OVERSPRAY_HINT,
    BOTTOM_OVERSPRAY_HINT
} PRINTER_HINT;

APDK_END_NAMESPACE

/*! @} */ // end globals group (documentation)

#endif //APDK_GLOBAL_TYPES_H
