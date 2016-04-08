/************************************************************************************\

  mfpdtf.h - HP Multi-Function Peripheral Data Transfer Format filter.

  (c) 2001-2005 Copyright HP Development Company, LP

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

  Contributing Author(s): David Paschal, Don Welch, David Suffield

\************************************************************************************/

#if !defined(_MFPDTF_H )
#define _MFPDTF_H

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>


enum MfpdtfImageRecordID_e { MFPDTF_ID_START_PAGE = 0,
                             MFPDTF_ID_RASTER_DATA = 1,
                             MFPDTF_ID_END_PAGE = 2 };

struct MfpdtfFixedHeader_s
{
        unsigned char   blockLength[4]; /* Includes header(s). */
        unsigned char   headerLength[2];    /* Just header(s). */
        unsigned char   dataType;
        unsigned char   pageFlags;
}               __attribute__(( packed) );

struct MfpdtfImageRasterDataHeader_s
{
        unsigned char   traits;         /* Unused. */
        unsigned char   byteCount[2];
}               __attribute__(( packed) );

struct MfpdtfImageStartPageRecord_s
{
        unsigned char   encoding;
        unsigned char   pageNumber[2];
        struct
        {
                unsigned char   pixelsPerRow[2];
                unsigned char   bitsPerPixel[2];
                unsigned char   rowsThisPage[4];
                unsigned char   xres[4];
                unsigned char   yres[4];
        } black, color;
}               __attribute__(( packed) );

struct MfpdtfImageEndPageRecord_s
{
        unsigned char   unused[3];
        struct
        {
                unsigned char   numberOfRows[4];
        } black, color;
}               __attribute__(( packed) );

struct Mfpdtf_s
{
        //ptalChannel_t   chan;
        int deviceid;
        int channelid;
        int fdLog;              /* <0 means not (yet) open. */
        int logOffset;

        struct
        {
                struct timeval                          timeout;
                int                                     simulateImageHeaders;

                int                                     lastServiceResult;
                int                                     dataType;           /* <0 means not (yet) valid. */
                int                                     arrayRecordCount;
                int                                     arrayRecordSize;
                int                                     fixedBlockBytesRemaining;   /* Also generic data. */
                int                                     innerBlockBytesRemaining;   /* Image or array data. */
                int                                     dontDecrementInnerBlock;

                struct MfpdtfFixedHeader_s              fixedHeader;
                int                                     lenVariantHeader;
                union  MfpdtfVariantHeader_u *          pVariantHeader;
                struct MfpdtfImageStartPageRecord_s     imageStartPageRecord;
                struct MfpdtfImageRasterDataHeader_s    imageRasterDataHeader;
                struct MfpdtfImageEndPageRecord_s       imageEndPageRecord;
        } read;
};

typedef struct Mfpdtf_s * Mfpdtf_t;

#define MFPDTF_RESULT_NEW_PAGE                 0x00000001
#define MFPDTF_RESULT_END_PAGE                 0x00000002
#define MFPDTF_RESULT_NEW_DOCUMENT             0x00000004
#define MFPDTF_RESULT_END_DOCUMENT             0x00000008

#define MFPDTF_RESULT_END_STREAM               0x00000010
#define MFPDTF_RESULT_RESERVED_20              0x00000020
#define MFPDTF_RESULT_RESERVED_40              0x00000040
#define MFPDTF_RESULT_RESERVED_80              0x00000080

#define MFPDTF_RESULT_00000100                 0x00000100
#define MFPDTF_RESULT_READ_TIMEOUT             0x00000200
#define MFPDTF_RESULT_READ_ERROR               0x00000400
#define MFPDTF_RESULT_OTHER_ERROR              0x00000800

#define MFPDTF_RESULT_ERROR_MASK               0x00000E00

#define MFPDTF_RESULT_NEW_DATA_TYPE            0x00001000
#define MFPDTF_RESULT_NEW_VARIANT_HEADER       0x00002000
#define MFPDTF_RESULT_GENERIC_DATA_PENDING     0x00004000
#define MFPDTF_RESULT_ARRAY_DATA_PENDING       0x00008000

#define MFPDTF_RESULT_NEW_START_OF_PAGE_RECORD 0x00010000
#define MFPDTF_RESULT_IMAGE_DATA_PENDING       0x00020000
#define MFPDTF_RESULT_NEW_END_OF_PAGE_RECORD   0x00040000
#define MFPDTF_RESULT_00080000                 0x00080000

#define MFPDTF_RESULT_INNER_DATA_PENDING       0x00028000

enum MfpdtfDataType_e { MFPDTF_DT_UNKNOWN = 0,
                        MFPDTF_DT_FAX_IMAGES = 1,
                        MFPDTF_DT_SCANNED_IMAGES = 2,
                        MFPDTF_DT_DIAL_STRINGS = 3,
                        MFPDTF_DT_DEMO_PAGES = 4,
                        MFPDTF_DT_SPEED_DIALS = 5,
                        MFPDTF_DT_FAX_LOGS = 6,
                        MFPDTF_DT_CFG_PARMS = 7,
                        MFPDTF_DT_LANG_STRS = 8,
                        MFPDTF_DT_JUNK_FAX_CSIDS = 9, /* MFPDTF_DT_DIAL_STRINGS */
                        MFPDTF_DT_REPORT_STRS = 10, /* MFPDTF_DT_LANG_STRS    */
                        MFPDTF_DT_FONTS = 11,
                        MFPDTF_DT_TTI_BITMAP = 12,
                        MFPDTF_DT_COUNTERS = 13,
                        MFPDTF_DT_DEF_PARMS = 14, /* MFPDTF_DT_CFG_PARMS    */
                        MFPDTF_DT_SCAN_OPTIONS = 15,
                        MFPDTF_DT_FW_JOB_TABLE = 17 };
#define MFPDTF_DT_MASK_IMAGE \
    ((1<<MFPDTF_DT_FAX_IMAGES) | \
     (1<<MFPDTF_DT_SCANNED_IMAGES) | \
     (1<<MFPDTF_DT_DEMO_PAGES))

enum MfpdtfImageEncoding_e { MFPDTF_RASTER_BITMAP                                  = 0,
                             MFPDTF_RASTER_GRAYMAP = 1,
                             MFPDTF_RASTER_MH = 2,
                             MFPDTF_RASTER_MR = 3,
                             MFPDTF_RASTER_MMR = 4,
                             MFPDTF_RASTER_RGB = 5,
                             MFPDTF_RASTER_YCC411 = 6,
                             MFPDTF_RASTER_JPEG = 7,
                             MFPDTF_RASTER_PCL = 8,
                             MFPDTF_RASTER_NOT = 9 };


union MfpdtfVariantHeader_u
{
        struct MfpdtfVariantHeaderFaxDataArtoo_s
        {
                unsigned char   majorVersion;   /* 1 */
                unsigned char   minorVersion;   /* 0 */
                unsigned char   dataSource; /* unknown=0,prev,host,fax,pollfax,scanner */
                unsigned char   dataFormat; /* unknown=0,ASCII=3,CCITT_G3=10 */
                unsigned char   dataCompression;    /* native=1,MH,MR,MMR */
                unsigned char   pageResolution; /* fine=0,std,CCITT300,CCITT400 */
                unsigned char   pageSize;       /* unknown=0 */
                unsigned char   pixelsPerRow[2];
                unsigned char   year;
                unsigned char   month;
                unsigned char   day;
                unsigned char   hour;
                unsigned char   minute;
                unsigned char   second;
                unsigned char   T30_CSI[20];
                unsigned char   T30_SUB[20];
        }               __attribute__(( packed) )                            faxArtoo;

        struct MfpdtfVariantHeaderFaxDataSolo_s
        {
                unsigned char   majorVersion;   /* 1 */
                unsigned char   minorVersion;   /* 1 */
                unsigned char   dataSource;
                unsigned char   dataFormat;
                unsigned char   dataCompression;
                unsigned char   pageResolution;
                unsigned char   pageSize;
                unsigned char   pixelsPerRow[2];
                unsigned char   year;
                unsigned char   month;
                unsigned char   day;
                unsigned char   hour;
                unsigned char   minute;
                unsigned char   second;
                unsigned char   suppressTTI;
                unsigned char   T30_CSI[20];
                unsigned char   T30_SUB[20];
                unsigned char   T30_PWD[20];
        }               __attribute__(( packed) )                            faxSolo;

        struct MfpdtfVariantHeaderFaxData_s
        {
                unsigned char   majorVersion;
                unsigned char   minorVersion;
                /* TODO: Finish. */
        }               __attribute__(( packed) )                            fax;

        struct MfpdtfVariantHeaderImageData_s
        {
                unsigned char   majorVersion;
                unsigned char   minorVersion;
                unsigned char   sourcePages[2];
                unsigned char   copiesPerPage[2];
                unsigned char   zoomFactor[2];
                unsigned char   jpegQualityFactor[2];
        }               __attribute__(( packed) ) image;

        struct MfpdtfVariantHeaderArrayData_s
        {
                unsigned char   majorVersion;
                unsigned char   minorVersion;
                unsigned char   recordCount[2];
                unsigned char   recordSize[2];
        } __attribute__(( packed) ) array;
} __attribute__(( packed) );

Mfpdtf_t __attribute__ ((visibility ("hidden"))) MfpdtfAllocate(int deviceid, int channelid);
int __attribute__ ((visibility ("hidden"))) MfpdtfDeallocate(Mfpdtf_t mfpdtf);
int __attribute__ ((visibility ("hidden"))) MfpdtfSetChannel(Mfpdtf_t mfpdtf, int channelid);
int __attribute__ ((visibility ("hidden"))) MfpdtfLogToFile(Mfpdtf_t mfpdtf, char * filename);
int __attribute__ ((visibility ("hidden"))) MfpdtfReadGetSimulateImageHeaders(Mfpdtf_t mfpdtf);
int __attribute__ ((visibility ("hidden"))) MfpdtfReadSetSimulateImageHeaders(Mfpdtf_t mfpdtf, int simulateImageHeaders);
int __attribute__ ((visibility ("hidden"))) MfpdtfReadStart(Mfpdtf_t mfpdtf);
int __attribute__ ((visibility ("hidden"))) MfpdtfReadService(Mfpdtf_t mfpdtf);
int __attribute__ ((visibility ("hidden"))) MfpdtfReadGetLastServiceResult(Mfpdtf_t mfpdtf);
int __attribute__ ((visibility ("hidden"))) MfpdtfReadGetVariantHeader(Mfpdtf_t mfpdtf, union MfpdtfVariantHeader_u * buffer, int maxlen);
int __attribute__ ((visibility ("hidden"))) MfpdtfReadGetStartPageRecord(Mfpdtf_t mfpdtf, struct MfpdtfImageStartPageRecord_s * buffer, int maxlen);
int __attribute__ ((visibility ("hidden"))) MfpdtfReadInnerBlock(Mfpdtf_t mfpdtf, unsigned char * buffer, int countdown);

/* 
 * Phase 2 rewrite. des
 */

enum MFPDTF_DATA_TYPE
{
   DT_FAX = 1,
   DT_SCAN = 2
};

/* MFPDTF page flag bit fields. */
#define PF_NEW_PAGE 1
#define PF_END_PAGE 2
#define PF_NEW_DOC 4
#define PF_END_DOC 8

enum MFPDTF_RECORD_ID
{
   ID_START_PAGE = 0,
   ID_RASTER_DATA = 1,
   ID_END_PAGE = 2
};

/* All words are stored little endian. */

typedef struct
{
   uint32_t BlockLength;   /* includes header in bytes */
   uint16_t HeaderLength;  /* in bytes */
   uint8_t DataType;
   uint8_t PageFlag;      
} __attribute__((packed)) MFPDTF_FIXED_HEADER;

typedef struct
{
   uint8_t ID;
   uint8_t Code;
   uint16_t PageNumber;
   uint16_t BlackPixelsPerRow;
   uint16_t BlackBitsPerPixel;
   uint32_t BlackRows;
   uint32_t BlackHorzDPI;
   uint32_t BlackVertDPI;
   uint16_t CMYPixelsPerRow;
   uint16_t CMYBitsPerPixel;
   uint32_t CMYRows;
   uint32_t CMYHorzDPI;
   uint32_t CMYVertDPI;
} __attribute__((packed)) MFPDTF_START_PAGE;

typedef struct
{
   uint8_t ID;
   uint8_t dummy;
   uint16_t Size;    /* in bytes */
} __attribute__((packed)) MFPDTF_RASTER;

typedef struct
{
   uint8_t ID;
   char dummy[3];
   uint32_t BlackRows;
   uint32_t CMYRows;
} __attribute__((packed)) MFPDTF_END_PAGE;

/* Folloing macros are now in gcc 4.3.2 endian.h */
#if !defined(htole16) 
#if defined(WORDS_BIGENDIAN)
#define htole16(A) ((((uint16_t)(A) & 0xff00) >> 8) | (((uint16_t)(A) & 0x00ff) << 8))    /* host to little-endian 16-bit value */
#define le16toh htole16                         /* little-endian to host 16-bit value */
#define htole32(A) ((((uint32_t)(A) & (uint32_t)0x000000ff) << 24) | (((uint32_t)(A) & (uint32_t)0x0000ff00) << 8) | \
                  (((uint32_t)(A) & (uint32_t)0x00ff0000) >> 8) | (((uint32_t)(A) & (uint32_t)0xff000000) >> 24))
#define le32toh htole32
#else
#define htole16(A) (A)
#define le16toh(A) (A)
#define le32toh(A) (A)
#define htole32(A) (A)
#endif   // #if defined(WORDS_BIGENDIAN)
#endif   // #if !defined(htole16)

int __attribute__ ((visibility ("hidden"))) read_mfpdtf_block(int device, int channel, char *buf, int bufSize, int timeout);

#endif  // _MFPDTF_H
