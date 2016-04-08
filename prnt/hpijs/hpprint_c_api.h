/*****************************************************************************\
  hpprint_c_api.h : Interface for C access to APDK

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


// hpprint_c_api.h
// 'C' interface functions to APDK external C++ interfaces
// For use when calling environment is written in 'C' not C++
// (a C++ compiler is still required, however; and there must
// be a derived SystemServices class defined for the host environment)

#ifndef APDK_HPPRINT_C_API_H
#define APDK_HPPRINT_C_API_H

#include "global_types.h"
#include "printerfactory.h"

typedef void * JobHandle;
typedef void * PrintContextHandle;
typedef void * FontHandle;
typedef void * ReferenceFontHandle;
typedef void * SystemServicesHandle;

APDK_USING_NAMESPACE

//////////////////////////////////////////
// 'C' interface to Job class
//

#ifdef __cplusplus
extern "C" {
#endif

extern DRIVER_ERROR C_Job_Create(JobHandle *phNewJob, PrintContextHandle hPrintContext);

extern void C_Job_Destroy(JobHandle hJob);

extern DRIVER_ERROR C_Job_SendRasters(JobHandle hJob, BYTE* ImageData);

extern DRIVER_ERROR C_Job_SupportSeparateBlack(JobHandle hJob, BOOL* bSeparateBlack);

extern DRIVER_ERROR C_Job_SendMultiPlaneRasters(JobHandle hJob, BYTE* BlackImageData, BYTE* ColorImageData);

#if defined(APDK_FONTS_NEEDED)
extern DRIVER_ERROR C_Job_TextOut(JobHandle hJob, const char* pTextString,
                                  unsigned int iLenString, const FontHandle hFont,
                                  int iAbsX, int iAbsY );
#endif

extern DRIVER_ERROR C_Job_NewPage(JobHandle hJob);


//////////////////////////////////////////
// 'C' interface to PrintContext class
//

extern DRIVER_ERROR C_PrintContext_Create(PrintContextHandle *phNewPrintContext,
                                          SystemServicesHandle hSysServ,
                                          unsigned int InputPixelsPerRow,
                                          unsigned int OutputPixelsPerRow,
                                          PAPER_SIZE ps);

extern void C_PrintContext_Destroy(PrintContextHandle hPrintContext);

extern void C_PrintContext_Flush(PrintContextHandle hPrintContext, int FlushSize);

extern DRIVER_ERROR C_PrintContext_SelectDevice(PrintContextHandle hPrintContext, const PRINTER_TYPE Model);

extern unsigned int C_PrintContext_GetModeCount(PrintContextHandle hPrintContext);

extern DRIVER_ERROR C_PrintContext_OldSelectPrintMode(PrintContextHandle hPrintContext, const unsigned int index);

extern DRIVER_ERROR C_PrintContext_SelectPrintMode(PrintContextHandle hPrintContext,
                const QUALITY_MODE quality_mode,
                const MEDIATYPE media_type,
                const COLORMODE color_mode,
                const BOOL device_text);

extern unsigned int C_PrintContext_OldCurrentPrintMode(PrintContextHandle hPrintContext);

extern DRIVER_ERROR C_PrintContext_CurrentPrintMode(PrintContextHandle hPrintContext,
                QUALITY_MODE quality_mode,
                MEDIATYPE media_type,
                COLORMODE color_mode,
                BOOL device_text);

extern const char* C_PrintContext_GetModeName(PrintContextHandle hPrintContext);

extern PRINTER_TYPE C_PrintContext_SelectedDevice(PrintContextHandle hPrintContext);

#if defined(APDK_FONTS_NEEDED)
extern ReferenceFontHandle C_PrintContext_EnumFont(PrintContextHandle hPrintContext, int * iCurrIdx);

extern FontHandle C_PrintContext_RealizeFont(PrintContextHandle hPrintContext,
                                             const int index, const BYTE bSize, const TEXTCOLOR eColor,
                                             const BOOL bBold, const BOOL bItalic, const BOOL bUnderline);
#endif

extern PRINTER_TYPE C_PrintContext_EnumDevices(const PrintContextHandle hPrintContext, FAMILY_HANDLE&  familyHandle);

extern DRIVER_ERROR C_PrintContext_PerformPrinterFunction(PrintContextHandle hPrintContext, PRINTER_FUNC eFunc);

extern DRIVER_ERROR C_PrintContext_SetPaperSize(PrintContextHandle hPrintContext, PAPER_SIZE ps, BOOL bFullBleed);

extern DRIVER_ERROR C_PrintContext_SetPixelsPerRow(PrintContextHandle hPrintContext,
                                                   unsigned int InputPixelsPerRow,
                                                   unsigned int OutputPixelsPerRow);

extern BOOL C_PrintContext_PrinterSelected(PrintContextHandle hPrintContext);

extern BOOL C_PrintContext_PrinterFontsAvailable(PrintContextHandle hPrintContext);

extern unsigned int C_PrintContext_InputPixelsPerRow(PrintContextHandle hPrintContext);

extern unsigned int C_PrintContext_OutputPixelsPerRow(PrintContextHandle hPrintContext);

extern PAPER_SIZE C_PrintContext_GetPaperSize(PrintContextHandle hPrintContext);

extern const char* C_PrintContext_PrinterModel(PrintContextHandle hPrintContext);

extern const char* C_PrintContext_PrintertypeToString(PrintContextHandle hPrintContext, PRINTER_TYPE pt);

extern unsigned int C_PrintContext_InputResolution(PrintContextHandle hPrintContext);

extern DRIVER_ERROR C_PrintContext_SetInputResolution(PrintContextHandle hPrintContext, unsigned int Res);

extern unsigned int C_PrintContext_EffectiveResolutionX(PrintContextHandle hPrintContext);

extern unsigned int C_PrintContext_EffectiveResolutionY(PrintContextHandle hPrintContext);

extern float C_PrintContext_PrintableWidth(PrintContextHandle hPrintContext);

extern float C_PrintContext_PrintableHeight(PrintContextHandle hPrintContext);

extern float C_PrintContext_PhysicalPageSizeX(PrintContextHandle hPrintContext);

extern float C_PrintContext_PhysicalPageSizeY(PrintContextHandle hPrintContext);

extern float C_PrintContext_PrintableStartX(PrintContextHandle hPrintContext);

extern float C_PrintContext_PrintableStartY(PrintContextHandle hPrintContext);

extern DRIVER_ERROR C_PrintContext_SendPrinterReadyData(PrintContextHandle hPrintContext, BYTE* stream, unsigned int size);


//////////////////////////////////////////
// 'C' interface to Font class
//
#if defined(APDK_FONTS_NEEDED)

extern void C_Font_Destroy(FontHandle hFont);

extern DRIVER_ERROR C_Font_GetTextExtent(PrintContextHandle hPrintContext,FontHandle hFont,
                                         const char* pTextString, const int iLenString,
                                         int * iHeight, int * iWidth);

extern const char* C_Font_GetName(const FontHandle hFont);

extern BOOL C_Font_IsBoldAllowed(const FontHandle hFont);

extern BOOL C_Font_IsItalicAllowed(const FontHandle hFont);

extern BOOL C_Font_IsUnderlineAllowed(const FontHandle hFont);

extern BOOL C_Font_IsColorAllowed(const FontHandle hFont);

extern BOOL C_Font_IsProportional(const FontHandle hFont);

extern BOOL C_Font_HasSerif(const FontHandle hFont);

extern BYTE C_Font_GetPitch(const FontHandle hFont, const BYTE pointsize);

extern int C_Font_Get_iPointsize(const FontHandle hFont);

extern void C_Font_Set_iPointsize(const FontHandle hFont, int iPointsize);

extern BOOL C_Font_Get_bBold(const FontHandle hFont);

extern void C_Font_Set_bBold(const FontHandle hFont, BOOL bBold);

extern BOOL C_Font_Get_bItalic(const FontHandle hFont);

extern void C_Font_Set_bItalic(const FontHandle hFont, BOOL bItalic);

extern BOOL C_Font_Get_bUnderline(const FontHandle hFont);

extern void C_Font_Set_bUnderline(const FontHandle hFont, BOOL bUnderline);

extern TEXTCOLOR C_Font_Get_eColor(const FontHandle hFont);

extern void C_Font_Set_eColor(const FontHandle hFont, TEXTCOLOR eColor);

extern int C_Font_Get_iPitch(const FontHandle hFont);

extern void C_Font_Set_iPitch(const FontHandle hFont, int iPitch);

extern int C_Font_Index(FontHandle hFont);

#endif  // defined(APDK_FONTS_NEEDED)

#ifdef __cplusplus
}
#endif

#endif  // APDK_HPPRINT_C_API_H
