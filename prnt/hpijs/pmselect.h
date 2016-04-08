/*****************************************************************************\
  pmselect.h : Interface for the ModeSet class

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


#ifndef APDK_PMSELECT_H
#define APDK_PMSELECT_H

APDK_BEGIN_NAMESPACE

//MSNode
//!
/*!
\internal
******************************************************************************/
struct MSNode
{
    //public constructor
    MSNode() : m_pNext(NULL), m_pPrintMode(NULL) { }

    // public attribs
    MSNode* m_pNext;
    PrintMode* m_pPrintMode;
};

//ModeSet
//!
/*!
\internal
******************************************************************************/
class ModeSet
{
public:
    ModeSet() : m_ListHead(NULL), m_Current(NULL) { }
    ModeSet(PrintMode* pPM);
    ModeSet(ModeSet* pMS);
    ~ModeSet();

    // generic container properties and manipulations
    ModeSet* Head();
    PrintMode* HeadPrintMode() const { return m_ListHead->m_pPrintMode; }
    PrintMode* CurrPrintMode() const { return m_Current->m_pPrintMode; }
    MSNode* Next()  { m_Current = m_Current->m_pNext; return m_Current; }
    void Reset() { m_Current = m_ListHead; }
    BOOL Append(PrintMode* pPrM);   // TRUE if err
    BOOL IsEmpty() const { return (m_ListHead == NULL); }
    unsigned int Size() const;

    // subset producers
    ModeSet* FontCapableSubset();
    ModeSet* PenCompatibleSubset(PEN_TYPE pens);
    ModeSet* ColorCompatibleSubset(COLORMODE color);
    ModeSet* QualitySubset(QUALITY_MODE eQuality);
    ModeSet* MediaSubset(MEDIATYPE eMedia);

    // algorithmic chunks to improve readability and save space
//    DRIVER_ERROR QualitySieve(ModeSet*& Modes, ModeSet*& tempModeSet, QUALITY_MODE& eQuality);

private:
    MSNode* m_ListHead;
    MSNode* m_Current;

//    PrintMode* m_pPM;
//    ModeSet* m_nextMS;
}; //ModeSet

APDK_END_NAMESPACE

#endif //APDK_PMSELECT_H
