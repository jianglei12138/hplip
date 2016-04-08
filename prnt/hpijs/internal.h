/*****************************************************************************\
  internal.h : Interface for internal classes

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


#ifndef APDK_INTERNAL_H
#define APDK_INTERNAL_H

APDK_BEGIN_NAMESPACE

/////////////////////////////////////////////////////////////////////////
// contains all class declarations
// for Slimhost++ driver
//
// merged in from file "Objects.h" 5/18/98
/////////////////////////////////////////////////////////////////////////
// these correspond to PCL codes
typedef int TYPEFACE;
#define COURIER 3
#define LETTERGOTHIC 6
#define CGTIMES 4101
#define UNIVERS 52

// character set names for PCL
#define LATIN1 "0N"     // aka ECMA94
#define PC8 "10U"
#define HP_LEGAL "1U"


// used to encourage consistent ordering of color planes
#define K   0
#define C   1
#define M   2
#define Y   3
#define Clight  4
#define Mlight  5

#define RANDSEED 77


#define DEFAULT_SLOW_POLL_COUNT 30
#define DEFAULT_SLOW_POLL_BIDI 3

//////////////////////////////////////////

enum STYLE_TYPE { UPRIGHT, ITALIC };

enum WEIGHT_TYPE { NORMAL, BOLD };

enum DATA_FORMAT { RASTER_LINE, RASTER_STRIP };


///////////////////////////////////////////////////////////////////

#define MAX_ESC_SEQ 40
#define MAX_RASTERSIZE  10000   // REVISIT

#define MAX_Y_OFFSET    32767
// very frequently used fragments made into macros for readability
#define CERRCHECK if (constructor_error != NO_ERROR) {DBG1("CERRCHECK fired\n"); return;}
#define ERRCHECK if (err != NO_ERROR) {DBG1("ERRCHECK fired\n"); return err;}
#define NEWCHECK(x) if (x==NULL) return ALLOCMEM_ERROR;
#define CNEWCHECK(x) if (x==NULL) { constructor_error=ALLOCMEM_ERROR; return; }



//////// STATIC DATA ////////////////////////////////////////////////////////////////
// escape sequences -- see PCL Implementor's Guide or Software Developer's PCL Guides
// for documentation
#define ESC 0x1b

const char UEL[] = {ESC, '%', '-','1','2','3','4','5','X' };
const char EnterLanguage[] = {'@','P','J','L',' ','E','N','T','E','R',' ',
                        'L','A','N','G','U','A','G','E','=' };
const char PCL3[] = {'P','C','L','3' };
const char PCLGUI[] = {'P','C','L','3','G','U','I' };
const char JobName[] = {'@','P','J','L',' ','J','O','B',' ','N','A','M','E',' ','=',' '};
const char Reset[] = {ESC,'E'};
const char crdStart[] = {ESC, '*', 'g'};            // configure raster data command
const char cidStart[] = {ESC, '*', 'v'};            // configure image data command
const char crdFormat = 2; // only format for 600
const char grafStart[] = {ESC, '*', 'r', '1', 'A'}; // raster graphics mode
const char grafMode0[] = {ESC, '*', 'b', '0', 'M'}; // compression methods
const char grafMode9[] =    {ESC, '*', 'b', '9', 'M'};
const char grafMode2[] =    {ESC, '*', 'b', '2', 'M'};
const char SeedSame[] = {ESC, '*', 'b', '0', 'S'};
//const char EjectPage[] = {ESC, '&', 'l', '0', 'H'};   // not needed by us; will pick if no page already picked
const char BlackExtractOff[] = {ESC, '*', 'o', '5', 'W', 0x04, 0xC, 0, 0, 0 };
const char LF = '\012';
const char Quote = '\042';
const BYTE DJ895_Power_On[] = {ESC, '%','P','u','i','f','p','.',
        'p','o','w','e','r',' ','1',';',
        'u','d','w','.','q','u','i','t',';',ESC,'%','-','1','2','3','4','5','X' };
/*const BYTE DJ895_Pre_Pick[] = {ESC, '&', 'l', -2, 'H'};
{ESC, '%','P','m','e','c','h','.',
        'l','o','a','d','_','p','a','p','e','r',';',
        'u','d','w','.','q','u','i','t',';' };//,ESC,'%','-','1','2','3','4','5','X' };
*/
const char EnableDuplex[] = { ESC,'&','l', '2', 'S'};
const char NoDepletion[] = {ESC, '*', 'o', '1', 'D'};
const char NoGrayBalance[] = {ESC, '*', 'b', '2', 'B'};
const char EnableBufferFlushing[] =  { ESC,'&','b','1','5','W','P','M','L',32,4,0,5,1,2,1,1,5,4,1,1 };
const char DisableBufferFlushing[] = { ESC,'&','b','1','5','W','P','M','L',32,4,0,5,1,2,1,1,5,4,1,2 };
const char DriverwareJobName[] = { ESC,'*','o','5','W',0x0d,0x06,0x00,0x00,0x01 };
const BYTE PEN_CLEAN_PML[]={0x1B,0x45,0x1B,0x26,0x62,0x31,0x36,0x57,
                0x50,0x4D,0x4C,0x20,  // EscE Esc&b16WPML{space}
                0x04,0x00,0x06,0x01,0x04,0x01,0x05,0x01,
                0x01,0x04,0x01,0x64}; // PML Marking-Agent-Maintenance=100

//
// ** move these to intenal.h

struct fOptSubSig
{
    float pi;
    const float *means;
};


struct fOptClassSig
{
    int nsubclasses;
    float variance;
    float inv_variance;
    float cnst;
    struct fOptSubSig *OptSubSig;
};


struct fOptSigSet
{
    int nbands;
    struct fOptClassSig *OptClassSig;
};


struct RESSYNSTRUCT
{
        int             Width;
        int             ScaleFactorMultiplier;
        int             ScaleFactorDivisor;
//      int             CallerAlloc;         // Who does the memory alloc.
        int             Remainder;           // For use in non integer scaling cases
        int             Repeat;              // When to send an extra output raster
        int             RastersinBuffer;     // # of currently buffered rasters
        unsigned char*  Bufferpt[NUMBER_RASTERS];
        int             BufferSize;
        unsigned char*  Buffer;
        struct fOptSigSet OS;
        struct fOptSubSig rsOptSubSigPtr1[45];
        struct fOptClassSig OCS;
        float **joint_means;
        float ***filter;
        float filterPtr1[45];
        float filterPtr2[45][9];
        float joint_meansPtr1[45];

};

typedef enum
{
	COLORTYPE_COLOR,       // 0
	COLORTYPE_BLACK,       // 1
	COLORTYPE_BOTH
} COLORTYPE;

#define  MAX_COLORTYPE     2

struct RASTERDATA
{
    int     rastersize[MAX_COLORTYPE];
    BYTE*	rasterdata[MAX_COLORTYPE];
};

// Bitmap structure definition
struct HPRGBQUAD {
  BYTE    rgbBlue; 
  BYTE    rgbGreen; 
  BYTE    rgbRed; 
  BYTE    rgbReserved; 
};

struct HPBITMAPINFOHEADER{
  DWORD  biSize; 
  long   biWidth; 
  long   biHeight; 
  WORD   biPlanes; 
  WORD   biBitCount; 
  DWORD  biCompression; 
  DWORD  biSizeImage; 
  long   biXPelsPerMeter; 
  long   biYPelsPerMeter; 
  DWORD  biClrUsed; 
  DWORD  biClrImportant; 
}; 

struct HPBITMAPINFO { 
  HPBITMAPINFOHEADER bmiHeader; 
  HPRGBQUAD          bmiColors[1]; 
}; 


struct HPLJBITMAP {

    HPBITMAPINFO  bitmapInfo;
    unsigned long cjBits;
    BYTE          *pvBits;	
	BYTE          *pvAlignedBits;
};


//////////////////////////////////////////
class Pipeline;
//Scaler, Halftoner, ColorMatcher, ErnieFilter, PixelReplicator,
//(FRE object, ...) are subclasses of Processor.


//Processor
//!\internal
//! Executes the "Process" method in it's containee.
/*!
Enter a full description of the class here. This will be the API doc.

\sa Scaler Halftoner ColorMatcher ErnieFilter PixelReplicator
******************************************************************************/
class Processor
{
public:
    Processor();
    virtual ~Processor();

    //virtual BOOL Process(BYTE* InputRaster=NULL, unsigned int size=0)=0;    // returns TRUE iff output ready
    virtual BOOL Process(RASTERDATA  *InputRaster=NULL)=0;    // returns TRUE iff output ready
	virtual void Flush()=0;     // take any concluding actions based on internal state
    virtual BYTE* NextOutputRaster(COLORTYPE  rastercolor)=0;
    virtual unsigned int GetOutputWidth(COLORTYPE  rastercolor)=0;        // in bytes, not pixels
    virtual unsigned int GetMaxOutputWidth(COLORTYPE  rastercolor) { return GetOutputWidth(rastercolor); }

    unsigned int iRastersReady, iRastersDelivered;
    Pipeline* myphase;
	COLORTYPE myplane;
	RASTERDATA  raster;
}; //Processor


//Pipeline
//! Manages the processes used in a pipeline
/*!
\internal
Enter a full description of the class here. This will be the API doc.

******************************************************************************/
class Pipeline
{
public:
    Pipeline(Processor* E);
    virtual ~Pipeline();

    void AddPhase(Pipeline* p);             // add p at end

    //DRIVER_ERROR Execute(BYTE* InputRaster=NULL, unsigned int size=0);   // run pipeline
	DRIVER_ERROR Execute(RASTERDATA* InputRaster=NULL);   // run pipeline

    DRIVER_ERROR Flush();

    //BOOL Process(BYTE* InputRaster=NULL, unsigned int size=0);   // call processor for this phase
    BOOL Process(RASTERDATA*  InputRaster=NULL);   // call processor for this phase

    BYTE* NextOutputRaster(COLORTYPE  rastercolor)      { return Exec->NextOutputRaster(rastercolor); }
    unsigned int GetOutputWidth(COLORTYPE  rastercolor) { return Exec->GetOutputWidth(rastercolor); }
    unsigned int GetMaxOutputWidth(COLORTYPE  rastercolor) { return Exec->GetMaxOutputWidth(rastercolor); }

    Pipeline* next;
    Pipeline* prev;

    Processor* Exec;

    DRIVER_ERROR err;

}; //Pipeline

struct PipeConfigTable
{
    BOOL bResSynth;

#if defined(APDK_VIP_COLORFILTERING)
    BOOL bErnie;
#endif

    BOOL bPixelReplicate;
    BOOL bColorImage;
    BOOL bCompress;
    HALFTONING_ALGORITHM eHT;
};


//Scaler
//! Scales input by a factor
/*!
\internal
Enter a full description of the class here. This will be the API doc.

******************************************************************************/
class Scaler : public Processor
{
public:
    // constructor protected -- use Create_Scaler()
    virtual ~Scaler();
    //virtual BOOL Process(BYTE* InputRaster=NULL, unsigned int size=0)=0;
    BOOL Process(RASTERDATA* InputRaster=NULL)=0;
    virtual void Flush() { Process(); }

    DRIVER_ERROR constructor_error;

    float ScaleFactor;
    unsigned int remainder;

    virtual unsigned int GetOutputWidth(COLORTYPE  rastercolor);
	virtual unsigned int GetMaxOutputWidth(COLORTYPE  rastercolor);
    virtual BYTE* NextOutputRaster(COLORTYPE  rastercolor)=0;

protected:
    Scaler(SystemServices* pSys,unsigned int inputwidth,
            unsigned int numerator,unsigned int denominator, BOOL vip, unsigned int BytesPerPixel);
    SystemServices* pSS;

    BOOL scaling;       // false iff ScaleFactor==1.0
    BOOL ReplicateOnly; // true iff 1<ScaleFactor<2


    unsigned int iOutputWidth;
    unsigned int iInputWidth;
    BYTE* pOutputBuffer[MAX_COLORTYPE];
    BOOL fractional;
}; //Scaler

#ifdef APDK_PSCRIPT
class PScriptProxy;
#endif

#ifdef APDK_LJMONO
class LJMonoProxy;
#endif

#ifdef APDK_LJCOLOR
class LJColorProxy;
#endif

#ifdef APDK_LJJETREADY
class LJJetReadyProxy;
#endif

#ifdef APDK_LJFASTRASTER
class LJFastRasterProxy;
#endif

#ifdef APDK_LJZJS_MONO
class LJZjsMonoProxy;
#endif

#ifdef APDK_LJZJS_COLOR
class LJZjsColorProxy;
#endif

#ifdef APDK_LJM1005
class LJM1005Proxy;
class LJP1XXXProxy;
#endif

#if defined(APDK_PSP100) && defined (APDK_DJ9xxVIP)
class PSP100Proxy;
class PSP470Proxy;
#endif

#if defined(APDK_DJGENERICVIP) && defined (APDK_DJ9xxVIP)
class DJGenericVIPProxy;
class DJ55xxProxy;
#endif

#ifdef APDK_DJ9xxVIP
class DJ9xxVIPProxy;
class OJProKx50Proxy;
#endif

#ifdef APDK_DJ9xx
class DJ9xxProxy;
#endif

#if defined(APDK_DJ8xx)|| defined(APDK_DJ9xx)
class DJ8xxProxy;
#endif

#if defined(APDK_DJ8xx)|| defined(APDK_DJ9xx)
#ifdef APDK_DJ8x5
class DJ8x5Proxy;
#endif
#endif

#if defined(APDK_DJ890)
class DJ890Proxy;
#endif

#if defined(APDK_DJ850)
class DJ850Proxy;
#endif

#ifdef APDK_DJ6xxPhoto
class DJ6xxPhotoProxy;
#endif

#ifdef APDK_DJ6xx
class DJ660Proxy;
#endif

#ifdef APDK_DJ630
class DJ630Proxy;
#endif

#ifdef APDK_DJ600
class DJ600Proxy;
#endif

#ifdef APDK_DJ540
class DJ540Proxy;
#endif

#ifdef APDK_DJ400
class DJ400Proxy;
#endif

#ifdef APDK_DJ350
class DJ350Proxy;
#endif

#if defined(APDK_DJ3600) && defined (APDK_DJ3320)
class DJ3600Proxy;
class DJ4100Proxy;
class DJD2600Proxy;
#endif

#if defined (APDK_DJ3320)
class DJ3320Proxy;
#endif

#ifdef APDK_APOLLO2560
class Apollo2560Proxy;
#endif

#ifdef APDK_APOLLO21XX
class Apollo21xxProxy;
#endif

#ifdef APDK_APOLLO2XXX
class Apollo2xxxProxy;
#endif

#ifdef APDK_QUICKCONNECT
class QuickConnectProxy;
#endif

//DeviceRegistry
//! Isolates device dependencies
/*!
\internal
DeviceRegistry, for isolating all device dependencies
The data is contained in Registry.cpp

This object encapsulates all model-specific data for a build.
Its features are presented to client through the PrintContext.

******************************************************************************/
class DeviceRegistry
{
public:
    DeviceRegistry();
    virtual ~DeviceRegistry();


    // get model string from DevID string
    DRIVER_ERROR ParseDevIDString(const char* sDevID, char* strModel, int *pVIPVersion, char* strPens);
    // set "device" to index of entry
    virtual DRIVER_ERROR SelectDevice(char* model, int* pVIPVersion, char* pens, SystemServices* pSS);

    virtual DRIVER_ERROR SelectDevice(const char* sDevID, SystemServices* pSS);

    virtual DRIVER_ERROR SelectDevice(const PRINTER_TYPE Model);

    virtual DRIVER_ERROR GetPrinterModel(char* strModel, int* pVIPVersion, char* strPens, SystemServices* pSS);


    // create a Printer object as pointee of p, using the given SystemServices
    // and the current value of device; still needs to be configured
    virtual DRIVER_ERROR InstantiatePrinter(Printer*& p,SystemServices* pSS);


    int device;                         // ordinal of device from list (or UNSUPPORTED=-1)

#ifdef APDK_PSCRIPT
    static PScriptProxy s_PScriptProxy;
#endif

#ifdef APDK_LJMONO
    static LJMonoProxy s_LJMonoProxy;
#endif

#ifdef APDK_LJCOLOR
    static LJColorProxy s_LJColorProxy;
#endif

#ifdef APDK_LJJETREADY
	static LJJetReadyProxy s_LJJetReadyProxy;
#endif

#ifdef APDK_LJFASTRASTER
    static LJFastRasterProxy s_LJFastRasterProxy;
#endif

#ifdef APDK_LJZJS_MONO
    static LJZjsMonoProxy s_LJZjsMonoProxy;
#endif

#ifdef APDK_LJZJS_COLOR
    static LJZjsColorProxy s_LJZjsColorProxy;
#endif

#ifdef APDK_LJM1005
    static LJM1005Proxy s_LJM1005Proxy;
    static LJP1XXXProxy s_LJP1XXXProxy;
#endif

#if defined(APDK_PSP100) && defined (APDK_DJ9xxVIP)
    static PSP100Proxy s_PSP100Proxy;
    static PSP470Proxy s_PSP470Proxy;
#endif

#if defined(APDK_DJGENERICVIP) && defined (APDK_DJ9xxVIP)
    static DJGenericVIPProxy s_DJGenericVIPProxy;
    static DJ55xxProxy s_DJ55xxProxy;
#endif

#ifdef APDK_DJ9xxVIP
    static DJ9xxVIPProxy s_DJ9xxVIPProxy;
    static OJProKx50Proxy s_OJProKx50Proxy;
#endif

#ifdef APDK_DJ9xx
    static DJ9xxProxy s_DJ9xxProxy;
#endif

#if defined(APDK_DJ8xx)|| defined(APDK_DJ9xx)
    static DJ8xxProxy s_DJ8xxProxy;
#endif

#if defined(APDK_DJ8xx)|| defined(APDK_DJ9xx)
#ifdef APDK_DJ8x5
    static DJ8x5Proxy s_DJ8x5Proxy;
#endif
#endif

#if defined(APDK_DJ890)
    static DJ890Proxy s_DJ890Proxy;
#endif

#if defined(APDK_DJ850)
    static DJ850Proxy s_DJ850Proxy;
#endif

#ifdef APDK_DJ6xxPhoto
    static DJ6xxPhotoProxy s_DJ6xxPhotoProxy;
#endif

#ifdef APDK_DJ6xx
    static DJ660Proxy s_DJ660Proxy;
#endif

#ifdef APDK_DJ630
    static DJ630Proxy s_DJ630Proxy;
#endif

#ifdef APDK_DJ600
    static DJ600Proxy s_DJ600Proxy;
#endif

#ifdef APDK_DJ540
    static DJ540Proxy s_DJ540Proxy;
#endif

#ifdef APDK_DJ400
    static DJ400Proxy s_DJ400Proxy;
#endif

#ifdef APDK_DJ350
    static DJ350Proxy s_DJ350Proxy;
#endif

#if defined(APDK_DJ3600) && defined (APDK_DJ3320)
    static DJ3600Proxy s_DJ3600Proxy;
    static DJ4100Proxy s_DJ4100Proxy;
    static DJD2600Proxy s_DJD2600Proxy;
#endif

#if defined (APDK_DJ3320)
    static DJ3320Proxy s_DJ3320Proxy;
#endif

#ifdef APDK_APOLLO2560
    static Apollo2560Proxy s_Apollo2560Proxy;
#endif

#ifdef APDK_APOLLO21XX
    static Apollo21xxProxy s_Apollo21xxProxy;
#endif

#ifdef APDK_APOLLO2XXX
    static Apollo2xxxProxy s_Apollo2xxxProxy;
#endif

#ifdef APDK_QUICKCONNECT
    static QuickConnectProxy s_QuickConnectProxy;
#endif

}; //DeviceRegistry


////////////////////////////////////////////////
typedef struct
{
    const uint32_t *ulMap1;
    const uint32_t *ulMap2;
    const unsigned char *ulMap3;
} ColorMap;


//Compressor
//!\internal
//! Base for data compression methods
/*!
Impliment compression

******************************************************************************/
class Compressor : public Processor
{
public:
    Compressor(SystemServices* pSys, unsigned int RasterSize, BOOL useseed);
    virtual ~Compressor();

    virtual BOOL Process(RASTERDATA* InputRaster=NULL)=0;

    virtual void Flush() { }    // no pending output

    unsigned int GetOutputWidth(COLORTYPE color);
    virtual BYTE* NextOutputRaster(COLORTYPE color);

    void SetSeedRow(BYTE* seed) { SeedRow=seed; }

    DRIVER_ERROR constructor_error;

    SystemServices* pSS;
    // buffer is public for use by GraphicsTranslator
    BYTE* compressBuf;      // output buffer
    BYTE* SeedRow;
    BOOL UseSeedRow;
	BYTE* originalKData;

    unsigned int compressedsize;
    unsigned int inputsize;
    BOOL seeded;
}; //Compressor


//ClassName
//!
/*
\internal
*/
class Mode9 : public Compressor
{
public:
    Mode9(SystemServices* pSys,unsigned int RasterSize, BOOL bVIPPrinter = FALSE);
    virtual ~Mode9();
    BOOL Process(RASTERDATA* input);
    void Flush();
	BOOL ResetSeedRow;
    BOOL m_bVIPPrinter;
}; //Mode9

//ClassName
//!
/*
\internal
*/
class Mode2 : public Compressor
{
public:
    Mode2(SystemServices* pSys,unsigned int RasterSize);
    virtual ~Mode2();
    BOOL Process(RASTERDATA* input);
}; //Mode2

//ClassName
//!
/*
\internal
*/
class Mode3 : public Compressor
{
public:
    Mode3 (SystemServices* pSys, Printer *pPrinter, unsigned int RasterSize);
    virtual ~Mode3 ();
    BOOL Process (RASTERDATA* input);
    void    Flush ();
private:
    Printer *m_pPrinter;
}; //Mode3

////////////////////////////////////////////////////////////////////////////
#if defined(APDK_FONTS_NEEDED)


//TextTranslator
//! ASCII data support
/*!
\internal
Does encapsulation work specific to ascii data, including handling of fonts
and treatments.

******************************************************************************/
class TextTranslator
{
public:
    TextTranslator(Printer* p,int quality,unsigned int colorplanes);
    virtual ~TextTranslator();

    DRIVER_ERROR TextOut(const char* pTextString, unsigned int LenString,
                const Font& font, BOOL sendfont=FALSE,
                int iAbsX=-1, int iAbsY=-1);

    DRIVER_ERROR SendCAP(unsigned int iAbsX,unsigned int iAbsY);
    const BYTE* ColorSequence(TEXTCOLOR eColor);
    BYTE ColorCode(TEXTCOLOR eColor);
    int TypefaceCode(const char* FontName);
    const BYTE* PointsizeSequence(unsigned int iPointsize);
    const BYTE* PitchSequence(unsigned int iPitch);
    const BYTE* StyleSequence(BOOL bItalic);
    const BYTE* WeightSequence(BOOL bBold);
    const BYTE* CompleteSequence(const Font& font);
    const BYTE* UnderlineSequence();
    const BYTE* DisableUnderlineSequence();

    // "transparent mode" escape to treat control code (BYTE b) as normal char
    int TransparentChar(unsigned int iMaxLen, BYTE b, BYTE* outbuff);

    DRIVER_ERROR constructor_error;

    DRIVER_ERROR SendFont(const Font& font);
    DRIVER_ERROR SendColorSequence(const TEXTCOLOR eColor);
    DRIVER_ERROR SendPointsize(const unsigned int iPointsize);
    DRIVER_ERROR SendPitch(const unsigned int iPitch);
    DRIVER_ERROR SendStyle(const BOOL bItalic);
    DRIVER_ERROR SendWeight(const BOOL bBold);
    DRIVER_ERROR SendUnderline();
    DRIVER_ERROR SendCompleteSequence(const Font& font);
    DRIVER_ERROR DisableUnderline();


private:
    Printer* thePrinter;
    int qualcode;                       // pcl code for text quality
    BYTE EscSeq[MAX_ESC_SEQ];           // storage for the command
    unsigned int iNumPlanes;                        // color planes, based on pen
    BYTE ColorCode1(TEXTCOLOR eColor);  // if iNumPlanes==1 (black)
    BYTE ColorCode3(TEXTCOLOR eColor);  // if iNumPlanes==3 (CMY)
    BYTE ColorCode4(TEXTCOLOR eColor);  // if iNumPlanes==4 (KCMY)

    // items for avoiding redundant font resets
    // (cheaper than copying whole font)
    TEXTCOLOR lastcolor;
    char lastname[20];
    char lastcharset[MAX_CHAR_SET];
    int lastpointsize;
    BOOL lastitalic;
    BOOL lastbold;
    void SetLast(const Font& font);

}; //TextTranslator

#endif //fonts needed


//Header
//! Composes a header stream
/*!
\internal
Composes a header stream, embodying specific requirements of the Printer.

******************************************************************************/
class Header
{
friend class Job;
friend class ModeJPEG;

public:
    Header(Printer* p,PrintContext* pc);
    virtual ~Header ();

    virtual DRIVER_ERROR Send()=0;

    virtual DRIVER_ERROR EndJob();

    virtual DRIVER_ERROR SendCAPy(unsigned int iAbsY);  // made virtual for crossbow
    virtual DRIVER_ERROR FormFeed();                    // made virtual for crossbow

	BOOL	IsLastBand() { return bLastBand; }
	void	SetLastBand(BOOL lastband) { bLastBand = lastband; }

    unsigned int CAPy;  // may be moved during header; retrieved by Job

protected:
    Printer* thePrinter;
    PrintContext* thePrintContext;
    PrintMode* thePrintMode;
    /// routines to set values of internal variables
    void SetMediaType(MediaType mtype);
    void SetMediaSize(PAPER_SIZE papersize);
    void SetMediaSource(MediaSource msource);
    void SetQuality(Quality qual);
    void SetSimpleColor();

    // components of a header
    DRIVER_ERROR Margins();
    virtual DRIVER_ERROR Graphics();
    DRIVER_ERROR Simple();
    DRIVER_ERROR Modes();
    DRIVER_ERROR ConfigureRasterData();

    // common escapes, plus mode and margin setting
    virtual DRIVER_ERROR StartSend();

////// data members /////////////////////////////////
    unsigned int ResolutionX[MAXCOLORPLANES];
    unsigned int ResolutionY[MAXCOLORPLANES];
    unsigned int dyeCount;

    // utilities

    unsigned int ColorLevels(unsigned int ColorPlane);

    // escape sequence constants
    char SimpleColor[6]; BYTE sccount;      // color command string, and its size
    char mediatype[6]; BYTE mtcount;        // mediatype string, and its size
    char mediasize[8]; BYTE mscount;        // mediasize string, and its size
    char mediasource[6]; BYTE msrccount;    // mediasource string, and its size
    char quality[6]; BYTE qualcount;        // quality string, and its size
    BYTE QualityCode();         // returns just the variable byte of quality
	BOOL bLastBand;
}; //Header


//ClassName
/*
******************************************************************************/
class Header350 : public Header
{
public:
    Header350(Printer* p,PrintContext* pc);
    DRIVER_ERROR Send();
}; //Header350


//ClassName
/*
******************************************************************************/
class Header400 : public Header
{
public:
    Header400(Printer* p,PrintContext* pc);
    DRIVER_ERROR Send();

}; //Header400


//ClassName
/*
******************************************************************************/
class Header6XX : public Header
{
public:
    Header6XX(Printer* p,PrintContext* pc);
    virtual DRIVER_ERROR Send();
protected:

}; //Header6XX


//ClassName
/*
******************************************************************************/
class Header600 : public Header6XX
{
public:
    Header600(Printer* p,PrintContext* pc);
    DRIVER_ERROR Send();

}; //Header600


//ClassName
/*
******************************************************************************/
class Header690 : public Header
{
public:
    Header690(Printer* p,PrintContext* pc);
    DRIVER_ERROR Send();
}; //Header690


//ClassName
/*
******************************************************************************/
class Header540 : public Header
{
public:
    Header540(Printer* p,PrintContext* pc);
    DRIVER_ERROR Send();

}; //Header540


//ClassName
/*
******************************************************************************/
class Header895 : public Header
{
public:
    Header895(Printer* p,PrintContext* pc);
    virtual DRIVER_ERROR Send();

protected:
    DRIVER_ERROR Graphics();
    DRIVER_ERROR StartSend();
}; //Header895


//ClassName
/*
******************************************************************************/
class Header850 : public Header895
{
public:
    Header850(Printer* p,PrintContext* pc);

protected:
    DRIVER_ERROR StartSend();

}; //Header850


//ClassName
/*
******************************************************************************/
class Header900 : public Header895
{
public:
    Header900(Printer* p,PrintContext* pc);
    virtual DRIVER_ERROR Send();

protected:
    BOOL DuplexEnabled(BYTE* bDevIDBuff);
}; //Header900


//ClassName
/*
******************************************************************************/
class HeaderDJ990 : public Header
{
public:
    HeaderDJ990(Printer* p,PrintContext* pc);
    DRIVER_ERROR ConfigureRasterData();
    DRIVER_ERROR ConfigureImageData();
    DRIVER_ERROR Send();
    DRIVER_ERROR StartSend();
    void SetMediaSource(MediaSource msource);
}; //HeaderDJ990

//ClassName
/*
******************************************************************************/
class HeaderDJGenericVIP : public HeaderDJ990
{
public:
    HeaderDJGenericVIP (Printer *p, PrintContext *pc);
protected:
    unsigned int m_uiCAPy;
    DRIVER_ERROR SendCAPy (unsigned int iAbsY);
    DRIVER_ERROR FormFeed ();
}; // HeaderDJGenericVIP

//ClassName
/*
******************************************************************************/
class Header630 : public Header
{
public:
    Header630(Printer* p,PrintContext* pc);
    DRIVER_ERROR Send();
}; //Header630


//ClassName
/*
******************************************************************************/
class Header2xxx : public Header
{
public:
    Header2xxx(Printer* p,PrintContext* pc);
    DRIVER_ERROR Send();
}; //Header2xxx


//ClassName
/*
******************************************************************************/
class Header3320 : public Header
{
public:
    Header3320 (Printer *p, PrintContext *pc);
    DRIVER_ERROR Send ();
protected:
    DRIVER_ERROR EndJob ();
    DRIVER_ERROR FormFeed ();
    DRIVER_ERROR SendCAPy (unsigned int iAbsY);
};
//ClassName
/*
******************************************************************************/
class Header21xx : public Header
{
public:
    Header21xx(Printer* p,PrintContext* pc);
    DRIVER_ERROR Send();
};  //Header21xx

//ClassName
/*
******************************************************************************/
class HeaderLJMono : public Header
{
public:
    HeaderLJMono (Printer* p,PrintContext* pc);
    virtual DRIVER_ERROR Send();

protected:
    DRIVER_ERROR EndJob ();
    DRIVER_ERROR Graphics ();
    DRIVER_ERROR StartSend ();
}; //HeaderLJMono

//ClassName
/*
******************************************************************************/
class HeaderLJColor : public Header
{
public:
    HeaderLJColor (Printer* p,PrintContext* pc);
    virtual DRIVER_ERROR Send();
	virtual DRIVER_ERROR FormFeed ();

protected:
    DRIVER_ERROR EndJob ();
    DRIVER_ERROR Graphics ();
    DRIVER_ERROR StartSend ();
    DRIVER_ERROR SendCAPy (unsigned int iAbsY);
}; //HeaderLJColor

//ClassName
/*
******************************************************************************/
class HeaderPScript : public Header
{
public:
    HeaderPScript (Printer *p, PrintContext *pc);
    DRIVER_ERROR Send ();
protected:
    DRIVER_ERROR EndJob ();
    DRIVER_ERROR FormFeed ();
    DRIVER_ERROR SendCAPy (unsigned int iAbsY);
    DRIVER_ERROR StartSend ();
};

//ClassName
/*
******************************************************************************/
class HeaderLJJetReady : public Header
{
	friend class LJJetReady;
public:
    HeaderLJJetReady (Printer* p,PrintContext* pc);
    virtual DRIVER_ERROR Send();
	virtual DRIVER_ERROR FormFeed ();
	virtual DRIVER_ERROR StartPage();
	virtual DRIVER_ERROR EndPage();
protected:
    DRIVER_ERROR EndJob ();
    DRIVER_ERROR StartSend ();
    DRIVER_ERROR SendCAPy (unsigned int iAbsY);
	DRIVER_ERROR MapPCLMediaTypeToString (MEDIATYPE eM);
	int			 JRPaperToMediaSize(PAPER_SIZE ps);
}; //HeaderLJJetReady

//ClassName
/*
******************************************************************************/
class HeaderLJFastRaster : public Header
{
	friend class LJFastRaster;
public:
    HeaderLJFastRaster (Printer* p,PrintContext* pc);
    virtual DRIVER_ERROR Send();
	virtual DRIVER_ERROR FormFeed ();
	virtual DRIVER_ERROR StartPage();
	virtual DRIVER_ERROR EndPage();
protected:
    DRIVER_ERROR EndJob ();
    DRIVER_ERROR StartSend ();
    DRIVER_ERROR SendCAPy (unsigned int iAbsY);
	DRIVER_ERROR MapPCLMediaTypeToString (MEDIATYPE eM);
	int			 FrPaperToMediaSize(PAPER_SIZE ps);
}; //HeaderLJFastRaster

//ClassName
/*
******************************************************************************/
class HeaderLJZjs : public Header
{
    friend class LJZjs;
public:
    HeaderLJZjs (Printer *p, PrintContext *pc);
    virtual DRIVER_ERROR Send ();
    virtual DRIVER_ERROR FormFeed ();
protected:
    DRIVER_ERROR EndJob ();
    DRIVER_ERROR SendCAPy (unsigned int iAbsY);
}; // HeaderLJZjs

//ClassName
/*
******************************************************************************/
class HeaderQuickConnect : public Header
{
public:
    HeaderQuickConnect (Printer *p, PrintContext *pc);
    virtual DRIVER_ERROR Send ();
    virtual DRIVER_ERROR FormFeed ();
protected:
    DRIVER_ERROR EndJob ();
    DRIVER_ERROR SendCAPy (unsigned int iAbsY);
}; // HeaderQuickConnect

//ClassName
/*
******************************************************************************/

class RasterSender : public Processor
{
friend class Header;
friend class Header895;
friend class Header900;
public:
   // installs Header and Connection
    RasterSender(Printer* pP, PrintContext* pPC,
                Job* pJob,Halftoner* pHalftoner);

    virtual ~RasterSender();

    // processor interface ////////////////////////////////////
    //BOOL Process(BYTE* InputRaster=NULL, unsigned int size=0);
	BOOL Process(RASTERDATA* InputRaster=NULL);
    void Flush() { };
    BYTE* NextOutputRaster(COLORTYPE  rastercolor) { return NULL; }   // end of pipeline
    unsigned int GetOutputWidth(COLORTYPE  rastercolor) { return 0; } // never called


    DRIVER_ERROR constructor_error;

    DRIVER_ERROR SendRaster(RASTERDATA* InputRaster=NULL);

private:
    Printer* thePrinter;

    PrintContext* thePrintContext;
    Job* theJob;
    Halftoner* theHalftoner;
    long    m_lNBlankRasters;	/* XXX unused? */
}; //RasterSender

// end of RasterSender section ///////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

#if defined(APDK_FONTS_NEEDED)

//TextMapper
//! Encapsulate mappings
/*!
\internal
Component of TextManager
This class encapsulates mappings that may be peculiar to different partners
or data sources. The use is as follows:
 1. invoke Map
 2. now access SubstLen and charset

Currently sets charset to LATIN1 at construction.

******************************************************************************/
class TextMapper
{
public:
    TextMapper(TextTranslator* t);

    // main function -- puts alternate string in buffer
    virtual void Map(BYTE b,BYTE* bSubst);

    // public members for access after call to Map()
    unsigned int SubstLen;
    char charset[MAX_CHAR_SET];

protected:
    TextTranslator* theTranslator;
}; //TextMapper

//GenericMapper
//!
/*
******************************************************************************/
class GenericMapper : public TextMapper
{
public:
    GenericMapper(TextTranslator* t);
    void Map(BYTE b,BYTE* bSubst);
}; //GenericMapper


//TextManager
//!
/*!
\internal
******************************************************************************/
class TextManager
// Component of TextJob
{
public:
    TextManager(TextTranslator* t,unsigned int PrintableX, unsigned int PrintableY);
    virtual ~TextManager();

    virtual DRIVER_ERROR TextOut(const char* pTextString, unsigned int iLenString,
                const Font& font, int iAbsX=-1, int iAbsY=-1);
    TextTranslator* theTranslator;

    DRIVER_ERROR constructor_error;

protected:

    unsigned int PrintableRegionX;
    unsigned int PrintableRegionY;

    DRIVER_ERROR CheckCoords(unsigned int iAbsX, unsigned int iAbsY );

    TextMapper* theMapper;

}; //TextManager

#endif     // FONTS

APDK_END_NAMESPACE

#endif //APDK_INTERNAL_H
