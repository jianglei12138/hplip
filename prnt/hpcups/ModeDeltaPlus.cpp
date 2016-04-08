/*****************************************************************************\
  ModeDeltaPlus.cpp : Implementation of ModeDeltaPlus class

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

#include "ModeDeltaPlus.h"

ModeDeltaPlus::ModeDeltaPlus 
(    
    unsigned int PlaneSize
) :
    Compressor(PlaneSize, true),
    pbyInputImageBuffer (NULL),
    pbySeedRow (NULL),
    m_bLastBand(false)
{
    inputsize = PlaneSize;
    inputsize = ((inputsize + 7) / 8) * 8;
    m_lCurrCDRasterRow  = 0;
    m_lPrinterRasterRow = 0;
    iRastersReady = 0;
    m_bCompressed = false;
    m_compressedsize = 0;
    m_fRatio = 0;
} // ModeDeltaPlus::ModeDeltaPlus

DRIVER_ERROR ModeDeltaPlus::Init()
{
    // allocate a 2X compression buffer..
    compressBuf = new BYTE[2 * INDY_STRIP_HEIGHT * inputsize];
    if (compressBuf == NULL)
    {
        return ALLOCMEM_ERROR;
    }
    memset (compressBuf, 0x00, 2 * INDY_STRIP_HEIGHT * inputsize);

    pbyInputImageBuffer = new BYTE[INDY_STRIP_HEIGHT * inputsize];
    if (pbyInputImageBuffer == NULL)
        return ALLOCMEM_ERROR;
    memset(pbyInputImageBuffer, 0x00, INDY_STRIP_HEIGHT * inputsize);

    pbySeedRow = new BYTE[inputsize];
    if (pbySeedRow == NULL)
    {
        return ALLOCMEM_ERROR;
    }
    memset (pbySeedRow, 0, inputsize * sizeof (BYTE));
    return NO_ERROR;
}

ModeDeltaPlus::~ModeDeltaPlus ()
{ 
    if (pbyInputImageBuffer)
    {
        delete [] pbyInputImageBuffer;
        pbyInputImageBuffer = NULL;
    }
    if (pbySeedRow)
    {
        delete [] pbySeedRow;
        pbySeedRow = NULL;
    }

} // ModeDeltaPlus::~ModeDeltaPlus

/*
 * The maximum width of a line, which is limited by the amount of hardware
 * buffer space allocated to storing the seedrow.
 */
#define ROW_LIMIT 7040
/*
 * The maximum number of literals in a single command, not counting the first
 * pixel.  This is limited by the hardware buffer used to store a literal
 * string.  For real images, I expect a value of 64 would be a suitable
 * minimum.  The minimum compression ratio will be bounded by this.  Note also
 * that the software does not need any buffer for this, so there need be no
 * limit at all on a purely software implementation.  For the sake of enabling
 * a hardware implementation, I would strongly recommend leaving it in and set
 * to some reasonable value (say 1023 or 255).
 */
#define LITERAL_LIMIT 511

/* These are set up this way to make it easy to change the predictions. */
#define LTEST_W            col > 0
#define LVAL_W(col)        cur_row[col-1]
#define LTEST_NW           col > 0
#define LVAL_NW(col)       seedrow[col-1]
#define LTEST_WW           col > 1
#define LVAL_WW(col)       cur_row[col-2]
#define LTEST_NWW          col > 1
#define LVAL_NWW(col)      seedrow[col-2]
#define LTEST_NE           (col+1) < row_width
#define LVAL_NE(col)       seedrow[col+1]
#define LTEST_NEWCOL       1
#define LVAL_NEWCOL(col)   new_color
#define LTEST_CACHE        1
#define LVAL_CACHE(col)    cache

#define LOC1TEST      LTEST_NE
#define LOC1VAL(col)  LVAL_NE(col)
#define LOC2TEST      LTEST_NW
#define LOC2VAL(col)  LVAL_NW(col)
#define LOC3TEST      LTEST_NEWCOL
#define LOC3VAL(col)  LVAL_NEWCOL(col)

#define check(condition) if (!(condition)) return 0

#define write_comp_byte(val)           \
   check(outptr < pastoutmem);         \
   *outptr++ = (BYTE) val;

#define read_byte(val)                 \
   check(inmem < pastinmem);           \
   val = *inmem++;

#define encode_count(count, over, mem)         \
   if (count >= over)                          \
   {                                           \
      count -= over;                           \
      if (count <= (uint32_t) 253)             \
      {                                        \
         check(mem < pastoutmem);              \
         *mem++ = (BYTE) count;               \
      }                                        \
      else if (count <= (uint32_t) (254 + 255) ) \
      {                                        \
         check((mem+1) < pastoutmem);          \
         check( count >= 254 );                \
         check( (count - 254) <= 255 );        \
         *mem++ = (BYTE) 0xFE;                \
         *mem++ = (BYTE) (count - 254);       \
      }                                        \
      else                                     \
      {                                        \
         check((mem+2) < pastoutmem);          \
         check( count >= 255 );                \
         check( (count - 255) <= 65535 );      \
         count -= 255;                         \
         *mem++ = (BYTE) 0xFF;                \
         *mem++ = (BYTE) (count >> 8);        \
         *mem++ = (BYTE) (count & 0xFF);      \
      }                                        \
   }
#define decode_count(count, over)              \
   if (count >= over)                          \
   {                                           \
      read_byte(inval);                        \
      count += (uint32_t) inval;                 \
      if (inval == (BYTE) 0xFE)               \
      {                                        \
         read_byte(inval);                     \
         count += (uint32_t) inval;              \
      }                                        \
      else if (inval == (BYTE) 0xFF)          \
      {                                        \
         read_byte(inval);                     \
         count += (((uint32_t) inval) << 8);     \
         read_byte(inval);                     \
         count += (uint32_t) inval;              \
      }                                        \
   }

#define bytes_for_count(count, over) \
  ( (count >= 255) ? 3 : (count >= over) ? 1 : 0 )


/* The number of bytes we should be greater than to call memset/memcpy */
#define memutil_thresh 15

BYTE* ModeDeltaPlus::encode_header(BYTE* outptr, const BYTE* pastoutmem, uint32_t isrun, uint32_t location, uint32_t seedrow_count, uint32_t run_count, const BYTE new_color)
{
    uint32_t byte;

    check (location < 4);
    check( (isrun == 0) || (isrun == 1) );

    /* encode "literal" in command byte */
    byte = (isrun << 7) | (location << 5);

    /* write out number of seedrow bytes to copy */
    if (seedrow_count > 2)
        byte |= (0x03 << 3);
    else
        byte |= (seedrow_count << 3);

    if (run_count > 6)
        byte |= 0x07;
    else
        byte |= run_count;

    /* write out command byte */
    write_comp_byte(byte);

    /* macro to write count if it's 3 or more */
    encode_count( seedrow_count, 3, outptr );

    /* if required, write out color of first pixel */
    if (location == 0)
    {
        write_comp_byte( new_color );
    }

    /* macro to write count if it's 7 or more */
    encode_count( run_count, 7, outptr );

    return outptr;
}

/******************************************************************************/
/*                                COMPRESSION                                 */
/******************************************************************************/
bool ModeDeltaPlus::compress (BYTE   *outmem,
                              uint32_t  *outlen,
                              const     BYTE     *inmem,
                              const     uint32_t    row_width,
                              const     uint32_t    inheight,
                              uint32_t  horz_ht_dist)
{
    register    BYTE     *outptr = outmem;
    register    uint32_t    col;
    const       BYTE     *seedrow;
    uint32_t                seedrow_count = 0;
    uint32_t                location = 0;
    BYTE                 new_color = (BYTE) 0xFF;
    const       BYTE     *cur_row;
    uint32_t                row;
    const       BYTE     *pastoutmem = outmem + *outlen;
    uint32_t                do_word_copies;
    /* Halftone distance must be 1-32 (but allow 0 == 1) */
    if (horz_ht_dist > 32)
    {
        return false;
    }
    if (horz_ht_dist < 1)
    {
       horz_ht_dist = 1;
    }

    seedrow = pbySeedRow;
    do_word_copies = ((row_width % 4) == 0);

    for (row = 0; row < inheight; row++)
    {
        cur_row = inmem + (row * row_width);

        col = 0;
        while (col < row_width)
        {
            /* First look for seedrow copy */
            seedrow_count = 0;
            if (do_word_copies)
            {
                /* Try a fast word-based search */
                while (((col & 3) != 0) &&
                        (col < row_width) &&
                        (cur_row[col] == seedrow[col]))
                {
                    seedrow_count++;
                    col++;
                }
                if ( (col & 3) == 0)
                {
                    while (((col+3) < row_width) &&
                            *((const uint32_t*) (cur_row + col)) == *((const uint32_t*) (seedrow + col)))
                    {
                        seedrow_count += 4;
                        col += 4;
                    }
                }
            }
            while ((col < row_width) && (cur_row[col] == seedrow[col]))
            {
               seedrow_count++;
               col++;
            }

            /* It is possible that we have hit the end of the line already. */
            if (col == row_width)
            {
                /* encode pure seed run as fake run */
                outptr = encode_header(outptr, pastoutmem, 1 /*run*/, 1 /*location*/, seedrow_count, 0 /*runcount*/, (BYTE) 0 /*color*/);
                /* exit the while loop for this row */
                break;
            }
            check(col < row_width);

            /* determine the prediction for the current pixel */
            if ( (LOC1TEST) && (cur_row[col] == LOC1VAL(col)) )
                location = 1;
            else if ( (LOC2TEST) && (cur_row[col] == LOC2VAL(col)) )
                location = 2;
            else if ( (LOC3TEST) && (cur_row[col] == LOC3VAL(col)) )
                location = 3;
            else
            {
                location = 0;
                new_color = cur_row[col];
            }


            /* Look for a run */
            if (
                 ((col+1) < row_width)
                 &&
                 ((col+1) >= horz_ht_dist)
                 &&
                 (cur_row[col+1-horz_ht_dist] == cur_row[col+1])
               )
            {
                /* We found a run.  Determine the length. */
                uint32_t run_count = 0;   /* Actually 2 */
                col++;
                while ( ((col+1) < row_width) && (cur_row[col+1-horz_ht_dist] == cur_row[col+1]) )
                {
                   run_count++;
                   col++;
                }
                col++;
                outptr = encode_header(outptr, pastoutmem, 1 /*run*/, location, seedrow_count, run_count, new_color);
            }

            else

            /* We didn't find a run.  Encode literal(s). */
            {
                uint32_t replacement_count = 0;   /* Actually 1 */
                const BYTE* byte_array = cur_row + col + 1;
                uint32_t i;
                col++;
                /*
                 * The (col+1) in this test is used because there is no need to
                 * check for literal breaks if this is the last byte of the row.
                 * Instead we just tack it on to our literal count at the end.
                 */
                while ( (col+1) < row_width )
                {
                    /*
                     * All cases that will break with 1 unit saved.  This
                     * should be the best breaking spots, since we will always
                     * gain with the break, but never break for no gain.  This
                     * leads to longer strings which is good for decomp speed.
                     */
                    if (
                         /* Seedrow ... */
                         (
                           (cur_row[col] == seedrow[col])
                           &&
                           (
                             /* 2 seedrows */
                             (
                               (cur_row[col+1] == seedrow[col+1])
                             )
                             ||
                             /* seedrow and predict */
                             (
                               (cur_row[col+1] == LVAL_NW(col+1))
                               ||
                               (cur_row[col+1] == LVAL_NEWCOL(col+1))
                             )
                             ||
                             (
                               ((col+2) < row_width)
                               &&
                               (
                                 /* seedrow and run */
                                 (
                                   ((col + 2) >= horz_ht_dist) &&
                                   (cur_row[col+2-horz_ht_dist] == cur_row[col+2])
                                 )
                                 ||
                                 /* seedrow and northeast predict */
                                 (cur_row[col+1] == LVAL_NE(col+1))
                               )
                             )
                           )
                         )
                         ||
                         /* Run ... */
                         (
                           (cur_row[col] != seedrow[col])
                           &&
                           ((col + 1) >= horz_ht_dist)
                           &&
                           (cur_row[col+1-horz_ht_dist] == cur_row[col+1])
                           &&
                           (
                             /* Run of 3 or more */
                             (
                               ((col+2) < row_width)
                               &&
                               ((col + 2) >= horz_ht_dist)
                               &&
                               (cur_row[col+2-horz_ht_dist] == cur_row[col+2])
                             )
                             ||
                             /* Predict first unit of run */
                             (cur_row[col] == LVAL_NE(col))
                             ||
                             (cur_row[col] == LVAL_NW(col))
                             ||
                             (cur_row[col] == LVAL_NEWCOL(col))
                           )
                         )
                       )
                        break;

                    /* limited hardware buffer */
                    if (replacement_count >= LITERAL_LIMIT)
                        break;

                    /* add another literal to the list */
                    replacement_count++;
                    col++;
                }

                /* If almost at end of block, just extend the literal by one */
                if ( (col+1) == row_width ) {
                   replacement_count++;
                   col++;
                }

                outptr = encode_header(outptr, pastoutmem, 0 /*not run*/, location, seedrow_count, replacement_count, new_color);

                /* Copy bytes from the byte array.  If rc was 1, then we will
                 * have encoded a zero in the command byte, so nothing will be
                 * copied here (the 1 indicates the first pixel, which was
                 * written above or was predicted.  If rc is between 2 and 7,
                 * then a value between 1 and 6 will have been written in the
                 * command byte, and we will copy it directly.  If 8 or more,
                 * then we encode more counts, then finally copy all the values
                 * from byte_array.
                 */

                if (replacement_count > 0)
                {
                    /* Now insert rc bytes of data from byte_array */
                    if (replacement_count > memutil_thresh)
                    {
                        check( (outptr + replacement_count) <= pastoutmem );
                        memcpy(outptr, byte_array, (size_t) replacement_count);
                        outptr += replacement_count;
                    }
                    else
                    {
                        for (i = 0; i < replacement_count; i++)
                        {
                            write_comp_byte( byte_array[i] );
                        }
                    }
                }
            }

        } /* end of column */

        /* save current row as next row's seed row */
        seedrow = cur_row;

    } /* end of row */

    check( outptr <= pastoutmem );
    if (outptr > pastoutmem)
    {
       /* We're in big trouble -- we wrote PAST the end of their memory! */
       *outlen = 0;
       return 0;
    }

    *outlen = (uint32_t) (outptr - outmem);

    return 1;
} /* end of deltaplus_compress2 */


bool ModeDeltaPlus::Process
(
    RASTERDATA* input
)
{
    if (input==NULL || 
        (input->rasterdata[COLORTYPE_COLOR]==NULL && input->rasterdata[COLORTYPE_BLACK]==NULL))    // flushing pipeline
    {
        return false;
    }
    if (input->rasterdata[COLORTYPE_COLOR])
    {
        if (m_lCurrCDRasterRow < INDY_STRIP_HEIGHT )
        {
            //Copy the data to m_SourceBitmap
            memcpy(pbyInputImageBuffer + m_lCurrCDRasterRow * inputsize, input->rasterdata[COLORTYPE_COLOR], input->rastersize[COLORTYPE_COLOR]);
            m_lCurrCDRasterRow ++;
        }
        if (m_lCurrCDRasterRow == INDY_STRIP_HEIGHT || m_bLastBand)
        {
            m_compressedsize = 2 * inputsize * INDY_STRIP_HEIGHT;
            bool bRet = compress (compressBuf, 
                                  &m_compressedsize,
                                  pbyInputImageBuffer,
                                  inputsize,
                                  m_lCurrCDRasterRow,
                                  16
                                  );
            if (!bRet)
            {
                memcpy (compressBuf, pbyInputImageBuffer, inputsize * INDY_STRIP_HEIGHT);
                m_compressedsize = inputsize * INDY_STRIP_HEIGHT;
            }
            else
            {
                m_bCompressed = true;
            }

            memset(pbyInputImageBuffer, 0x00, inputsize * INDY_STRIP_HEIGHT);

            m_lPrinterRasterRow += m_lCurrCDRasterRow;
            m_lCurrBlockHeight = m_lCurrCDRasterRow;
            m_lCurrCDRasterRow = 0;
            iRastersReady = 1;
            m_bLastBand = false;
        }
        else
        {
            return false;
        }
    }
    return true;
} //Process

bool ModeDeltaPlus::NextOutputRaster(RASTERDATA &next_raster)
{
    if (iRastersReady==0)
        return false;

    next_raster.rastersize[COLORTYPE_COLOR] =  m_compressedsize;
    if (m_compressedsize > 0)
    {
        next_raster.rasterdata[COLORTYPE_COLOR] =  compressBuf;
    }
    next_raster.rastersize[COLORTYPE_BLACK] = 0;
    next_raster.rasterdata[COLORTYPE_BLACK] = NULL;
    iRastersReady=0;
    return true;
}

