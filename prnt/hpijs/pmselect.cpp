/*****************************************************************************\
  pmselect.cpp : Implimentation for the ModeSet class

  Copyright (c) 2001 - 2015, HP Co.
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


#include "header.h"
#include "pmselect.h"

APDK_BEGIN_NAMESPACE

ModeSet::ModeSet
(
    PrintMode* pPM
)
{
    ASSERT(pPM);
    m_ListHead = new MSNode;

    if (m_ListHead != NULL)
    {
        m_ListHead->m_pNext = NULL;
        m_ListHead->m_pPrintMode = pPM;
    }
    Reset();                //   m_Current = m_ListHead;
} //ModeSet


ModeSet::~ModeSet()
{
    if (m_ListHead != NULL)
    {
        Reset();
        while (m_Current != NULL)
        {
            MSNode* pDeleteable = m_Current;
            Next();             //m_Current = m_Current->m_pNext;
            delete pDeleteable;
        }
        m_ListHead = NULL;
    }
} //~ModeSet

// note that copy constructor, Head() and all "Subset" methods make NEW COPIES that need deleting

// copy constructor
ModeSet::ModeSet
(
    ModeSet* pSource
) :
    m_ListHead(NULL),
    m_Current(NULL)
{
    ASSERT(pSource);

    MSNode* t_SrcNode = pSource->m_ListHead;

    while (t_SrcNode)
    {
        Append(t_SrcNode->m_pPrintMode);
        t_SrcNode = t_SrcNode->m_pNext;
    }
} //ModeSet


BOOL ModeSet::Append(PrintMode* pPrM)   // return TRUE for memerr
{
    ASSERT(pPrM);
    if (m_ListHead == NULL)
    {
        m_ListHead = new MSNode;
        if (m_ListHead == NULL)
        {
            return TRUE;            // memory error
        }
        m_ListHead->m_pPrintMode = pPrM;
    }
    else
    {
        MSNode* t_walk;
        if (m_Current != NULL)      // try to be effiecient and not walk the whole list
        {
            t_walk = m_Current;
        }
        else
        {
            t_walk = m_ListHead;        // we know m_ListHead != NULL from above
        }

        while(t_walk->m_pNext != NULL)
        {
            t_walk = t_walk->m_pNext;
        }

        t_walk->m_pNext = new MSNode;
        if (t_walk->m_pNext == NULL)
        {
            return TRUE;                // memory error
        }

        // update current pointer (possible to shorten next append
        // i.e. current will point to the last item in the list after an append
        m_Current = t_walk->m_pNext;
        m_Current->m_pPrintMode = pPrM;       // finally!!
    }

    return FALSE;       // no error
} //Append


unsigned int ModeSet::Size() const
{
    unsigned int uCount = 0;

    MSNode* t_walk = m_ListHead;
    while (t_walk != NULL)
    {
        uCount++;
        t_walk = t_walk->m_pNext;
    }

    return uCount;
} //Size

// strange name...?  Create a new one entry list with the first entry of this list
ModeSet* ModeSet::Head()
{
    ASSERT(m_ListHead);
    ModeSet* pNew = new ModeSet(HeadPrintMode());
    return pNew;
} //Head


ModeSet* ModeSet::FontCapableSubset()
{
    ModeSet* resMS = new ModeSet;
    if (resMS == NULL)
    {
        return NULL;
    }

    Reset();
    while(m_Current)
    {
        // if we have set up the list properly we never have to worry about m_pPrintMode
        // being NULL.  If it is NULL then we shouldn't even have a node.
        ASSERT(CurrPrintMode());

        if(CurrPrintMode()->bFontCapable)
        {
            if(resMS->Append(CurrPrintMode()))
            {
                delete resMS;               // there was a memory error appending
                return NULL;
            }
        }
        Next();             //m_Current = m_Current->m_pNext;
    }
    return resMS;
} //FontCapableSubset


ModeSet* ModeSet::PenCompatibleSubset(PEN_TYPE pens)
{
    ModeSet* resMS = new ModeSet;
    if (resMS == NULL)
    {
        return NULL;
    }

    Reset();
    while(m_Current)
    {
        ASSERT(CurrPrintMode());            // see comments above

        if(CurrPrintMode()->Compatible(pens))
        {
            if(resMS->Append(CurrPrintMode()))
            {
                delete resMS;                       // memory error
                return NULL;
            }
        }
        Next();
    }
    return resMS;
} //PenCompatibleSubset


ModeSet* ModeSet::ColorCompatibleSubset(COLORMODE color)
{
    ModeSet* resMS = new ModeSet;
    if (resMS == NULL)
    {
        return NULL;
    }

    Reset();
    while(m_Current)
    {
        ASSERT(CurrPrintMode());            // see comments above

        if (CurrPrintMode()->ColorCompatible(color))
        {
            if(resMS->Append(CurrPrintMode()))
            {
                delete resMS;                       // memory error
                return NULL;
            }
        }
        Next();
    }
    return resMS;
} //ColorCompatibleSubset


ModeSet* ModeSet::QualitySubset(QUALITY_MODE eQuality)
{
    ModeSet* resMS = new ModeSet;
    if (resMS == NULL)
    {
        return NULL;
    }

    Reset();
    while(m_Current)
    {
        ASSERT(CurrPrintMode());            // see comments above

        if (CurrPrintMode()->QualityCompatible(eQuality))
        {
            if(resMS->Append(CurrPrintMode()))
            {
                delete resMS;                       // memory error
                return NULL;
            }
        }
        Next();
    }
    return resMS;
} //QualitySubset


ModeSet* ModeSet::MediaSubset(MEDIATYPE eMedia)
{
    ModeSet* resMS = new ModeSet;
    if (resMS == NULL)
    {
        return NULL;
    }

    Reset();
    while(m_Current)
    {
        ASSERT(CurrPrintMode());            // see comments above

        if (CurrPrintMode()->MediaCompatible(eMedia))
        {
            if(resMS->Append(CurrPrintMode()))
            {
                delete resMS;                       // memory error
                return NULL;
            }
        }
        Next();
    }
    return resMS;
} //MediaSubset

APDK_END_NAMESPACE
