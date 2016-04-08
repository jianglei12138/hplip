/*****************************************************************************\
  font.h : Interface for the font classes

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


#ifndef APDK_FONT_H
#define APDK_FONT_H
#if defined(APDK_FONTS_NEEDED)

APDK_BEGIN_NAMESPACE

// font names
const char sCourier[]="Courier";
const char sCGTimes[]="CGTimes";
const char sLetterGothic[]="LetterGothic";
const char sUnivers[]="Univers";
const char sBad[]="Bad";

class PrintContext;

//Font
//! Base class for font support
/*! \class Font font.h "hpprintapi.h"
This object is used to control printer fonts when sending ASCII data.
(For systems that lack a reasonable font engine or for some reason can
benefit from using printer fonts.)

It is not abstract, so that clients can request a font generically,
but its constructor is not public -- Fonts are rather created
through RealizeFont -- thus it is effectively "abstract" in this sense.
Example:
         Font* myFont = myJob->RealizeFont(FIXED_SERIF,12,...);

Note that Printer initially constructs a dummy font (with default values)
for each of its typefaces, so that the Is<x>Allowed functions can be
invoked (for EnumFonts) prior to choosing specific instances.
Then the Clone function is invoked by Printer::RealizeFont to provide
instances to client.

\sa ReferenceFont
******************************************************************************/
class Font
{
friend class Printer;
friend class TextManager;
public:

    // constructors are protected -- clients use Job::RealizeFont()
    virtual ~Font();

    // public functions

    // the base class version is really for printer fonts
    virtual DRIVER_ERROR GetTextExtent(PrintContext* pPC, const char* pTextString,const int iLenString,
                                int& iHeight, int& iWidth);

////// these functions allow access to properties of derived classes

    //! Returns the typeface name.
    virtual const char* GetName() const { return sBad; }

    //! Tells whether text bolding is available.
    virtual BOOL IsBoldAllowed() const { return FALSE; }

    //! Tells whether text italicizing is available.
    virtual BOOL IsItalicAllowed() const { return FALSE; }

    //! Tells whether text underlining is available.
    virtual BOOL IsUnderlineAllowed() const { return FALSE; }

    //! Tells whether text coloring is available.
    virtual BOOL IsColorAllowed() const { return FALSE; }

    //! Tells whether this font is proportionally spaced, as opposed to fixed.
    virtual BOOL IsProportional() const { return FALSE; }

    //! Tells whether this typeface has serifs.
    virtual BOOL HasSerif() const { return FALSE; }


    /*!
    For fixed fonts, returns the pitch for given point-size.
    (Returns zero for proportional fonts.)
    */
    virtual BYTE GetPitch(const BYTE pointsize) const
        { return 0; }   // default for proportionals

////// these data members give the properties of the actual instance
    // as set by the user
    BYTE        iPointsize;
    BOOL        bBold;      // boolean TRUE to request bold
    BOOL        bItalic;    // boolean TRUE to request italic
    BOOL        bUnderline; // boolean TRUE to request underline
    TEXTCOLOR   eColor;     // enum
    int         iPitch;

    // string designating character set (as recognized by firmware)
    //
    //!\todo is this comment still valid?
    // REVISIT: shouldn't really have Translator data here; we
    // should have an enum here, which is interpreted by Translator
    char charset[MAX_CHAR_SET];

    BOOL PrinterBased;

    //! Index of point-size from available list for this font.
    virtual int Index() { return -1; };

    // items for spooling
//  virtual BOOL Equal(Font* f);
//  virtual DRIVER_ERROR Store(FILE* sp, int& size);
//  virtual int SpoolSize();
#ifdef APDK_CAPTURE
    SystemServices* pSS;
    void Capture_dFont(const unsigned int ptr);
#endif
protected:
    // constructor, invoked by derivative constructors
    Font(int SizesAvailable,BYTE size=0,
            BOOL bold=FALSE, BOOL italic=FALSE, BOOL underline=FALSE,
            TEXTCOLOR color=BLACK_TEXT,BOOL printer=TRUE,
            unsigned int pvres=300,unsigned int phres=300);

    // copy constructor used by RealizeFont
    Font(const Font& f,const BYTE bSize,
         const TEXTCOLOR color, const BOOL bold,
         const BOOL italic, const BOOL underline);

    // return a clone with a different character set
    // base class version should not be called -- this should be pure virtual!
    virtual Font* CharSetClone(char* NewCharSet) const;

    int numsizes;   // number of available pointsizes
    // return array of sizes allowed
    virtual BYTE* GetSizes() const { return (BYTE*)NULL; }
    // return index of pointsize from array of available pointsizes
    virtual int Ordinal(unsigned int /* pointsize */) const
        { return 0; }

    // match arbitrary input size to one we have
    BYTE AssignSize(BYTE Size);
    void Subst_Char(int& bCurrChar)const;

    // pointers to the arrays containing widths for a given font
    //  separated into Lo (32..127) & Hi (160..255)
    const BYTE *pWidthLo[MAX_POINTSIZES];
    const BYTE *pWidthHi[MAX_POINTSIZES];

    unsigned int PrinterVRes;
    unsigned int PrinterHRes;

    BOOL internal;  // true iff font belongs to printer

}; //Font

//ReferenceFont
//! Used by Job to realize a font
/*! \class ReferenceFont font.h "hpprintapi.h"
Subclass ReferenceFont (EnumFont) is used to query available font properties
prior to instantiating the font. Whereas Font objects created upon request are
to be deleted by caller, ReferenceFonts live with the core structures and
cannot be deleted.

The main purpose of this class is to hide the destructor, since
the fonts that live with the Printer and are returned by EnumFont
are meant to remain alive for the life of the Printer.

\sa Font Printer
******************************************************************************/
class ReferenceFont : public Font
{
friend class Printer;           // deletes from its fontarray
friend class DJ400;        // replaces fontarray from base class
public:
    ReferenceFont(int SizesAvailable,BYTE size=0,
            BOOL bold=FALSE, BOOL italic=FALSE, BOOL underline=FALSE,
            TEXTCOLOR color=BLACK_TEXT,BOOL printer=TRUE,
            unsigned int pvres=300,unsigned int phres=300);
protected:
    ~ReferenceFont();

    // copy constructor used by RealizeFont
    ReferenceFont(const ReferenceFont& f,const BYTE bSize,
         const TEXTCOLOR color, const BOOL bold,
         const BOOL italic, const BOOL underline);

}; //ReferenceFont


#ifdef APDK_COURIER
// fixed-pitch, serif

extern BYTE CourierSizes[];

class Courier : public ReferenceFont
{
friend class Printer;
public:
    Courier(BYTE size=0,
            BOOL bold=FALSE, BOOL italic=FALSE, BOOL underline=FALSE,
            TEXTCOLOR=BLACK_TEXT, unsigned int SizesAvailable=3);
    virtual ~Courier();


    BYTE GetPitch(const BYTE pointsize)const;
    const char* GetName() const { return sCourier; }
    BOOL IsBoldAllowed() const { return TRUE; }
    BOOL IsItalicAllowed() const { return TRUE; }
    BOOL IsUnderlineAllowed() const { return TRUE; }
    virtual BOOL IsColorAllowed() const { return TRUE; }
    BOOL IsProportional() const { return FALSE; }
    BOOL HasSerif() const { return TRUE; }

    int Index() { return COURIER_INDEX; }

    BYTE unused;    // left for future use by clients

protected:
    Courier(const Courier& f,const BYTE bSize,
         const TEXTCOLOR color, const BOOL bold,
         const BOOL italic, const BOOL underline);
    int Ordinal(unsigned int pointsize)const;
    virtual BYTE* GetSizes() const {  return CourierSizes; }
    virtual Font* CharSetClone(char* NewCharSet) const
     { Courier* c = new Courier(*this,iPointsize,eColor,bBold,bItalic,bUnderline);
        if (c==NULL) return (Font*)NULL;
        strcpy(c->charset, NewCharSet);
        return c;
     }
}; //Courier

#ifdef APDK_DJ400

extern BYTE Courier400Sizes[];

class Courier400 : public Courier
{
friend class DJ400;
public:
    Courier400(BYTE size=0,
            BOOL bold=FALSE, BOOL italic=FALSE, BOOL underline=FALSE);

    BOOL IsColorAllowed() const { return FALSE; }


protected:
    Courier400(const Courier400& f,const BYTE bSize,
         const TEXTCOLOR color, const BOOL bold,
         const BOOL italic, const BOOL underline);
    BYTE* GetSizes() const { return Courier400Sizes; }
    Font* CharSetClone(char* NewCharSet) const
     { Courier400* c = new Courier400(*this,iPointsize,eColor,bBold,bItalic,bUnderline);
        if (c==NULL) return (Font*)NULL;
        strcpy(c->charset, NewCharSet);
        return c;
     }
}; //Courier400

#endif // APDK_DJ400

#endif // APDK_COURIER


#ifdef APDK_CGTIMES
// proportional, serif

extern BYTE CGTimesSizes[];

class CGTimes : public ReferenceFont
{
friend class Printer;
public:
    CGTimes(BYTE size=0,
            BOOL bold=FALSE, BOOL italic=FALSE, BOOL underline=FALSE,
            TEXTCOLOR=BLACK_TEXT, unsigned int SizesAvailable=5);

    const char* GetName() const { return sCGTimes; }
    BOOL IsBoldAllowed() const { return TRUE; }
    BOOL IsItalicAllowed() const { return TRUE; }
    BOOL IsUnderlineAllowed() const { return TRUE; }
    virtual BOOL IsColorAllowed() const { return TRUE; }
    BOOL IsProportional() const { return TRUE; }
    BOOL HasSerif() const { return TRUE; }

    int Index() { return CGTIMES_INDEX; }

protected:
    CGTimes(const CGTimes& f,const BYTE bSize,
         const TEXTCOLOR color, const BOOL bold,
         const BOOL italic, const BOOL underline);
    int Ordinal(unsigned int pointsize)const;
    virtual BYTE* GetSizes() const { return CGTimesSizes; }
    virtual Font* CharSetClone(char* NewCharSet) const
     { CGTimes* c = new CGTimes(*this,iPointsize,eColor,bBold,bItalic,bUnderline);
        if (c==NULL) return (Font*)NULL;
        strcpy(c->charset, NewCharSet);
        return c;
     }
}; //CGTimes


#ifdef APDK_DJ400

extern BYTE CGTimes400Sizes[];

class CGTimes400 : public CGTimes
{
friend class DJ400;
public:
    CGTimes400(BYTE size=0,
            BOOL bold=FALSE, BOOL italic=FALSE, BOOL underline=FALSE);

    BOOL IsColorAllowed() const { return FALSE; }


protected:
    CGTimes400(const CGTimes400& f,const BYTE bSize,
         const TEXTCOLOR color, const BOOL bold,
         const BOOL italic, const BOOL underline);
    int Ordinal(unsigned int pointsize)const;
    BYTE* GetSizes() const { return CGTimes400Sizes; }
    Font* CharSetClone(char* NewCharSet) const
     { CGTimes400* c = new CGTimes400(*this,iPointsize,eColor,bBold,bItalic,bUnderline);
        if (c==NULL) return (Font*)NULL;
        strcpy(c->charset, NewCharSet);
        return c;
     }
}; //CGTimes400


#endif  // ifdef APDK_DJ400

#endif  // ifdef APDK_CGTIMES


#ifdef APDK_LTRGOTHIC
// fixed-pitch, sans-serif

extern BYTE LetterGothicSizes[];


class LetterGothic : public ReferenceFont
{
friend class Printer;
public:
    LetterGothic(BYTE size=0,
            BOOL bold=FALSE, BOOL italic=FALSE, BOOL underline=FALSE,
            TEXTCOLOR=BLACK_TEXT, unsigned int SizesAvailable=3);
    virtual ~LetterGothic();


    BYTE GetPitch(const BYTE pointsize)const;
    const char* GetName() const { return sLetterGothic; }
    BOOL IsBoldAllowed() const { return TRUE; }
    BOOL IsItalicAllowed() const { return TRUE; }
    BOOL IsUnderlineAllowed() const { return TRUE; }
    virtual BOOL IsColorAllowed() const { return TRUE; }
    BOOL IsProportional() const { return FALSE; }
    BOOL HasSerif() const { return FALSE; }

    int Index() { return LETTERGOTHIC_INDEX; }

    BYTE unused;    // left for future use by clients

protected:
    LetterGothic(const LetterGothic& f,const BYTE bSize,
         const TEXTCOLOR color, const BOOL bold,
         const BOOL italic, const BOOL underline);
    int Ordinal(unsigned int pointsize)const;
    virtual BYTE* GetSizes() const {  return LetterGothicSizes; }
    virtual Font* CharSetClone(char* NewCharSet) const
     { LetterGothic* c = new LetterGothic(*this,iPointsize,eColor,bBold,bItalic,bUnderline);
        if (c==NULL) return (Font*)NULL;
        strcpy(c->charset, NewCharSet);
        return c;
     }
}; //LettrerGothic

#ifdef APDK_DJ400

extern BYTE LetterGothic400Sizes[];

class LetterGothic400 : public LetterGothic
{
friend class DJ400;
public:
    LetterGothic400(BYTE size=0,
            BOOL bold=FALSE, BOOL italic=FALSE, BOOL underline=FALSE);

    BOOL IsColorAllowed() const { return FALSE; }


protected:
    LetterGothic400(const LetterGothic400& f,const BYTE bSize,
         const TEXTCOLOR color, const BOOL bold,
         const BOOL italic, const BOOL underline);
    BYTE* GetSizes() const { return LetterGothic400Sizes; }
    Font* CharSetClone(char* NewCharSet) const
     { LetterGothic400* c = new LetterGothic400(*this,iPointsize,eColor,bBold,bItalic,bUnderline);
        if (c==NULL) return (Font*)NULL;
        strcpy(c->charset, NewCharSet);
        return c;
     }
}; //LetterGothic400

#endif // APDK_DJ400

#endif  // APDK_LTRGOTHIC


#ifdef APDK_UNIVERS
// proportional, sans-serif

extern BYTE UniversSizes[];

class Univers : public ReferenceFont
{
friend class Printer;
public:
    Univers(BYTE size=0,
            BOOL bold=FALSE, BOOL italic=FALSE, BOOL underline=FALSE,
            TEXTCOLOR=BLACK_TEXT, unsigned int SizesAvailable=3);

    const char* GetName() const { return sUnivers; }
    BOOL IsBoldAllowed() const { return TRUE; }
    BOOL IsItalicAllowed() const { return TRUE; }
    BOOL IsUnderlineAllowed() const { return TRUE; }
    virtual BOOL IsColorAllowed() const { return TRUE; }
    BOOL IsProportional() const { return TRUE; }
    BOOL HasSerif() const { return FALSE; }

    int Index() { return UNIVERS_INDEX; }

protected:
    Univers(const Univers& f,const BYTE bSize,
         const TEXTCOLOR color, const BOOL bold,
         const BOOL italic, const BOOL underline);
    int Ordinal(unsigned int pointsize)const;
    virtual BYTE* GetSizes() const { return UniversSizes; }
    virtual Font* CharSetClone(char* NewCharSet) const
     { Univers* c = new Univers(*this,iPointsize,eColor,bBold,bItalic,bUnderline);
        if (c==NULL) return (Font*)NULL;
        strcpy(c->charset, NewCharSet);
        return c;
     }
}; //Univers

#endif  // APDK_UNIVERS

APDK_END_NAMESPACE

#endif  //APDK_FONTS_NEEDED
#endif  //APDK_FONT_H

