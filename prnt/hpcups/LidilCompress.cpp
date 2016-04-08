/*****************************************************************************\
  LidilCompress.cpp : Implementation of LidilCompress class

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

#include "CommonDefinitions.h"
#include "LidilCompress.h"

UInt16 LidilCompress::FlushImage ()
{
    UInt16    command;
    UInt16    wsize;
    UInt16    bsize;
    UInt16    *from_ptr;
    int       index;

    wsize = m_image_cnt;
    bsize = 0;

    if (wsize)
    {
        from_ptr = m_image_ptr;

        command = FILL_IMAGE_CMD | (wsize-1);

        index = 0;
        *m_out_ptr++ = command;
        for (UInt16 i = 0; i < wsize; i++)
        {
            *m_out_ptr++ = *from_ptr++;
        }
        bsize = ((m_image_cnt+1) * 2);
        m_out_cnt += bsize;

        m_image_cnt = 0;
    }

    return bsize;
}

UInt16 LidilCompress::FlushCopy (UInt16 value)
{
    UInt16    command;
    UInt16    size;

    size = m_copy_cnt;
    if (size)
    {
        UInt16  *uP = m_out_ptr++;
        size = 2;
        if (value == 0)
        {
            command = FILL_0000_CMD | (m_copy_cnt-1);
        }
        else if (value == 0xFFFF)
        {
            command = FILL_FFFF_CMD | (m_copy_cnt-1);
        }
        else
        {
            command = FILL_NEXT_CMD | (m_copy_cnt-1);
            *m_out_ptr++ = value;
            size = 4;
        }

        *uP = command;
        m_out_cnt += size;
        m_copy_cnt = 0;
    }
    return size;
}

void LidilCompress::CompressData (Int16 compressionmode)
{
    Int16   i;
    UInt16  *in_ptr;
    UInt16  in;
    UInt16  last=0;
    UInt16  copy_item;
    UInt16  data_length;

    LDLCOMPMODE mode = IN_NOT;


    m_out_cnt = 0;
    m_image_cnt = 0;
    m_copy_cnt = 0;

    m_out_ptr = &m_out_array[8];
    data_length = m_data_length;

    if ((data_length & 1) != 0)
    {
    //    ErrorTrap((char *)"Data length is odd.");
    }

    copy_item = 0;
    in_ptr = &m_raw_data[0];

    for (i=0; i<data_length; i+=2)
    {
        in = *in_ptr;

        switch(mode)
        {
            case IN_NOT:
            {
                /* default the first entry to 'image' */
                last = in;
                m_image_ptr = in_ptr;
                m_image_cnt = 1;
                mode = IN_FIRST;
                break;
            }

            case IN_FIRST:
            {
#if ALLOW_FILL_NEXT_CMD
                if (last == in)
#else
                if ((last == in) && ((in==0xFFFF) || (in == 0)) )
#endif
                {
                    mode = IN_COPY;
                    m_copy_cnt = 2;
                    m_image_cnt = 0;
                    copy_item = in;
                }
                else
                {
                    mode = IN_IMAGE;
                    m_image_cnt++;
                    last = in;
                }
                break;
            }

            case IN_COPY:
            {
                if (last == in)
                {
                    m_copy_cnt++;
                }
                else
                {
                    /* revisit - could allow 2 words of copy if the data is
                    0000 or FFFF */

                    /* convert a copy cnt of 2 to an image */
                    UInt16 copy_count = m_copy_cnt;

                    if (copy_count <= m_run_length)
                    {
                        if (m_image_cnt == 0)
                        {
                            /* point the pointer to the first element */
                            m_image_ptr = in_ptr - copy_count;
                        }
                        m_image_cnt += (1+copy_count);
                     m_copy_cnt = 0;
                    }
                    else
                    {
                        /* have enough to be a legal copy */

                        (void) FlushImage ();

                        (void) FlushCopy (copy_item);

                        m_image_ptr = in_ptr;
                        m_image_cnt = 1;
                    }
                    mode = IN_IMAGE;
                    last = in;
                }
                break;
            }

            case IN_IMAGE:
            {
#if ALLOW_FILL_NEXT_CMD
                if (last == in)
#else
                if ((last == in) && ((in==0xFFFF) || (in == 0)) )
#endif
                {
                    m_image_cnt--;

                    mode = IN_COPY;
                    copy_item = in;
                    m_copy_cnt = 2;
                }
                else /* different */
                {
                    last = in;
                    m_image_cnt++;
                }
                break;
            }

            default:
            {
                break;
            }
        }
        in_ptr++;
    } /* next data - end of processing */

    /* flush out the remainder */

    switch(mode)
    {
        case IN_COPY:
        {
            /* have enough to be a legal copy */
            (void) FlushImage ();

            (void) FlushCopy (copy_item);
            break;
        }
        case IN_IMAGE:
        case IN_FIRST:
        {
            (void) FlushImage ();
            break;
        }
        default:
            break;
    }

    if (m_out_cnt > 2048+16)
    {
    //    ErrorTrap("out cnt too big");
    //    exit (-7);
    }
}

bool LidilCompress::GetFrameInfo (BYTE **outdata, UInt16 *data_size)
{
    *outdata = (unsigned char *) &m_out_array[0];
    *data_size = m_out_cnt;
    return(true);
}

bool LidilCompress::Init (UInt16 *data, UInt16 datasize)
{
	m_image_ptr   = data;
	m_raw_data    = data;
	m_data_length = datasize;

	m_run_length = MAX_RUNLENGTH;

	return(true);
}

