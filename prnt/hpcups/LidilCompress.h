/*****************************************************************************\
  LidilCompress.h : Defnition of LidilCompress class

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

#ifndef LIDIL_COMPRESS_H
#define LIDIL_COMPRESS_H

#define ALLOW_FILL_NEXT_CMD 1

#define FILL_0000_CMD  0x1000
#define FILL_FFFF_CMD  0x2000
#define FILL_NEXT_CMD  0x3000
#define FILL_IMAGE_CMD 0x4000
#define FILL_ODD_CMD   0x5000
#define FILL_EVEN_CMD  0x6000

#define MAX_HEADER_FRAME_SIZE 512
#define MAX_DATA_FRAME_SIZE   6048
#define MAX_RUNLENGTH         5

typedef enum
{
  IN_NOT,
  IN_FIRST,
  IN_IMAGE,
  IN_COPY
} LDLCOMPMODE;

class LidilCompress
{
public:
    LidilCompress (bool bLittleEndian) : m_bLittleEndian(bLittleEndian) {}
    ~LidilCompress () {}
    bool      Init(UInt16 *data, UInt16 datasize);
    bool      GetFrameInfo(BYTE **outdata, UInt16 *datasize);
    void      CompressData(Int16 compressmode = 1);
    UInt16    FlushCopy(UInt16 value);
    UInt16    FlushImage();
private:
    UInt16  *m_out_ptr;
    UInt16  *m_image_ptr; /* image ptr points into the raw data */
    UInt16  m_out_cnt;
    UInt16  m_image_cnt;
    UInt16  m_copy_cnt;
    UInt16  m_out_array[2048+16];
    UInt16  *m_raw_data;
    UInt16  m_run_length; /* minimun run length */
    UInt16  m_data_length;  /* actual data size in record */
    bool    m_bLittleEndian;
};

#endif //  LIDIL_COMPRESS_H

