/*****************************************************************************\
  Copyright (c) 2002 - 2015, HP Co.
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
// PrinterProxy.cpp - Morpheus component code base

#include "header.h"
#include <string.h>
#include "printerfactory.h"
#include "printerproxy.h"

APDK_BEGIN_NAMESPACE

//PrinterProxy
//! Constructor for PrinterProxy
/*!
******************************************************************************/
PrinterProxy::PrinterProxy
(
    const char* szFamilyName,
    const char* szModelNames
) : m_szFamilyName(szFamilyName),
    m_szModelNames(szModelNames)
{
    unsigned int uCount = 0;
    char* tPtr = const_cast<char*>(m_szModelNames);
    while (*tPtr)
    {
        tPtr += strlen(tPtr)+1;
        uCount++;
    }
    m_uModelCount = uCount;

    PrinterFactory::Register(this);
    TRACE("PP::PP family is %s and supports %d models\n", GetFamilyName(), GetModelCount());
} //PrinterProxy


//~PrinterProxy
//! Destructor for PrinterProxy
/*!
******************************************************************************/
PrinterProxy::~PrinterProxy()
{
    TRACE("PP::~PP<%s> destructing\n", GetFamilyName());
    PrinterFactory::UnRegister(this);
} //~PrinterProxy


//GetNextModelName
//! Return the next model the family supports
/*!
\return
pointer to the next model string<br>
NULL if there are no more models in the list
******************************************************************************/
const char* PrinterProxy::GetNextModelName
(
    MODEL_HANDLE& theModelHandle
) const
{
    TRACE("PP::GetNextModelName()\n");
    const char* szModelName = GetModelName(theModelHandle);
    if (szModelName == NULL)
    {
        TRACE("PP:GetNextModelName - reset to being of list\n");
        szModelName = m_szModelNames;
    }
    else
    {
        szModelName += strlen(szModelName)+1;
    }
    if (*szModelName == '\0')
    {
        TRACE("PP:GetNextModelName - hit end of list\n");
        szModelName = NULL;
    }
    theModelHandle = szModelName;
    return szModelName;
} //GetNextModelName


//ModelMatchQuery
//! Indicate if there is a match in this proxy for the model string
/*!
\return
true - there was a match<br>
false - there was no match
******************************************************************************/
bool PrinterProxy::ModelMatchQuery
(
    const char* szModelString                   //!< model or family string
) const
{
    TRACE("PP::ModelMatchQuery(%s)\n", szModelString);
    bool bFound = false;
    // check to see if they passed in the family name
    TRACE("PP::MMQ - searching for %s(family) in %s\n",GetFamilyName(), szModelString);
    if (!strcmp(szModelString, GetFamilyName()))
    {
        TRACE("PP::MMQ - found family name match (%s)!\n", GetFamilyName());
        bFound = true;
    }
    else
    {
        const char* szNextModel = m_szModelNames;
        while(*szNextModel)
        {
            TRACE("PP::MMQ - searching for %s in %s\n", szNextModel, szModelString);
            // do a simple substring on what was passed in with what is in the model list
            if (strstr(szModelString, szNextModel))
            {
                // we found a match
                TRACE("PP:MMQ - found a model name match (%s)!\n", szNextModel);
                bFound = true;
                break;
            }
            szNextModel += strlen(szNextModel)+1;
        }
    }
    return bFound;
} //DeviceQueryMatch


//DeviceMatchQuery
//! Indicate to what level this printer class could support the device
/*!
\return
PROXY_VOTE - confidance vote from 0 - 100 indicating how well the proxy
belives that this printer class could support the device indicated. 0
indicating "not at all" and 100 indicating "exact match".
\sa PROXY_VOTE
******************************************************************************/
PROXY_VOTE PrinterProxy::DeviceMatchQuery
(
    const char* szDeviceString          //!< valid device id string
) const
{
    TRACE("PP::DeviceMatchQuery(%s)\n", szDeviceString);
    return VOTE_NO_MATCH;
} //DeviceQueryMatch

APDK_END_NAMESPACE
