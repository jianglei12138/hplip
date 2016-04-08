/*****************************************************************************\
  SystemServices.cpp : Implementation of SystemServices class

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
#include "SystemServices.h"
#include "utils.h"

SystemServices::SystemServices(int iLogLevel, int job_id, char* user_name) : m_iLogLevel(iLogLevel)
{
    m_fp = NULL;
    if (iLogLevel & SAVE_OUT_FILE)
    {
        char    fname[MAX_FILE_PATH_LEN];
        sprintf(fname, "%s/hpcups_%s_out_job%d_XXXXXX",CUPS_TMP_DIR, user_name, job_id);
        createTempFile(fname, &m_fp);
        if (m_fp)
        {
            chmod(fname, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
        }
    }
}

SystemServices::~SystemServices()
{
    if (m_fp)
    {
        fclose (m_fp);
    }
}

DRIVER_ERROR SystemServices::Send(const BYTE *pData, int iLength)
{
    if (m_fp)
    {
        fwrite (pData, 1, iLength, m_fp);
    }

    if ( !(m_iLogLevel & DONT_SEND_TO_BACKEND) )
    {
        write (STDOUT_FILENO, pData, iLength);
    }

    return NO_ERROR;
}

