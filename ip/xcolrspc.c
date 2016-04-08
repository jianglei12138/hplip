/* libhpojip -- HP OfficeJet image-processing library. */

/* Copyright (C) 1995-2002 HP Company
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * is provided AS IS, WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, and
 * NON-INFRINGEMENT.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA.
 *
 * In addition, as a special exception, HP Company
 * gives permission to link the code of this program with any
 * version of the OpenSSL library which is distributed under a
 * license identical to that listed in the included LICENSE.OpenSSL
 * file, and distribute linked combinations including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * this file, you may extend this exception to your version of the
 * file, but you are not obligated to do so.  If you do not wish to
 * do so, delete this exception statement from your version.
 */

/* Original author: Mark Overton and others.
 *
 * Ported to Linux by David Paschal.
 */

/******************************************************************************\
 *
 * xcolrspc.c - Converts between color spaces
 *
 ******************************************************************************
 *
 * Name of Global Jump-Table:
 *
 *    colorTbl
 *
 * Items in aXformInfo array passed into setXformSpec:
 *
 *    aXformInfo[IP_CNV_COLOR_SPACE_WHICH_CNV] = Which color-space conversion to do:
 *       IP_CNV_YCC_TO_CIELAB = ycc    -> cielab
 *       IP_CNV_CIELAB_TO_YCC = cielab -> ycc
 *       IP_CNV_YCC_TO_SRGB   = ycc    -> srgb
 *       IP_CNV_SRGB_TO_YCC   = srgb   -> ycc
 *       IP_CNV_LHS_TO_SRGB   = lhs    -> srgb
 *       IP_CNV_SRGB_TO_LHS   = srgb   -> lhs
 *       IP_CNV_BGR_SWAP      = rgb<->bgr swap
 *
 *    aXformInfo[IP_CNV_COLOR_SPACE_GAMMA] = Gamma value for gamma correction,
 *                    in 16.16 fixed-point.
 *                    In the YCC->CIELab conversion, the data will be
 *                    inverse-gamma corrected using 1/Gamma.
 *                    In the CIELab->YCC conversion, it will be gamma corrected
 *                    using Gamma.
 *                    This Gamma value is ignored in the other conversions.
 *                    A value of 0.0 makes us use a default Gamma of 2.2.
 *                    You must set this to 1.0 if you want to disable Gamma.
 *
 *                    Gamma is done here because the pixels are in RGB at one
 *                    point in the YCC<->CIELab conversions. Since you can't do
 *                    Gamma on YCC or CIELab data, it can't be done outside
 *                    this xform for those two conversions.
 *
 * Capabilities and Limitations:
 *
 *    Does the space conversions listed above.  Conversions will only be done on
 *    24-bit data; monochrome data are passed through unchanged.
 *
 * Default Input Traits, and Output Traits:
 *
 *          trait             default input             output
 *    -------------------  ---------------------  ------------------------
 *    iPixelsPerRow         * passed into output   same as default input
 *    iBitsPerPixel         * must be <= 24        same as default input
 *    iComponentsPerPixel     passed into output   same as default input
 *    lHorizDPI               passed into output   same as default input
 *    lVertDPI                passed into output   same as default input
 *    lNumRows                passed into output   same as default input
 *    iNumPages               passed into output   same as default input
 *    iPageNum                passed into output   same as default input
 *
 *    Above, a "*" by an item indicates it must be valid (not negative).
 *
 * Feb 1998 Mark Overton -- wrote original code, with dummy conversions
 * Apr 1998 Mark Overton -- added actual conversion code, adapted from
 *                          Cindy Samson's code
 *
\******************************************************************************/

#include "math.h"   // needed for pow and floor
#include "hpip.h"
#include "ipdefs.h"
#include "string.h"    /* for memset and memcpy */
#include "assert.h"

/* xsc_clean[Uu]p.h: */

int Send_yTable[100] = {
210,212,213,214,215,216,
217,218,219,220,221,223,
224,225,226,228,230,232,
233,234,235,237,238,239,
240,242,243,244,245,246,
247,248,249,250,251,252,
253,254,255,255,255,255,
255,255,255,255};

/* end xsc_clean[Uu]p.h */


#if 0
    #include "stdio.h"
    #include <tchar.h>
    #define PRINT(msg,arg1,arg2) \
        _ftprintf(stderr, msg, (int)arg1, (int)arg2)
#else
    #define PRINT(msg,arg1,arg2)
#endif

#define CHECK_VALUE 0x4ba1dace


typedef struct {
    IP_IMAGE_TRAITS traits;     /* traits of the input image */
    IP_WHICH_CNV    which_cnv;  /* which space-conversion to do */
    BYTE      bGammaTbl[256];   /* Gamma-correction table */
    DWORD     dwRowsDone;       /* number of rows converted so far */
    DWORD     dwInNextPos;      /* file pos for subsequent input */
    DWORD     dwOutNextPos;     /* file pos for subsequent output */
    DWORD     dwValidChk;       /* struct validity check value */
} COL_INST, *PCOL_INST;

static BOOL fInited = FALSE;

//Neutral Shift Data Definition
#define NEUTRAL_SHIFT_SEND   TRUE  


// constants to convert [r,g,b] into Y
#define RGBTOY_R_FAC      0.30078125f
#define RGBTOY_G_FAC      0.5859375f
#define RGBTOY_B_FAC      0.11328125f

// constants to convert [y,cb,cr] into [r,g,b]
#define YCCTORGB_CR_TO_R  1.39946f
#define YCCTORGB_CB_TO_G -0.344228f
#define YCCTORGB_CR_TO_G -0.717202f
#define YCCTORGB_CB_TO_B  1.77243f


// The above constants used for converting between RGB and YCC are based
// on the following conversion matrices.  The code assumes that the
// 0.003 and 0.006 below are zero.
#if 0
    static float RGBtoYCC[3][3]= {  // input is RGB
        0.30078125f,  0.5859375f,   0.11328125f,  // Y
       -0.16796875f, -0.33203125f,  0.5f,         // Cb
        0.5f,        -0.41796875f, -0.08203125f   // Cr
    };

    static float YCCtoRGB[3][3]= {  // input is YCbCr
        1.0f,  0.003f,     1.39946f,    // R
        1.0f, -0.344228f, -0.717202f,   // G
        1.0f,  1.77243f,   0.006f       // B
    };
#endif


#define X_SCALE  (255.0/244.0)
#define Y_SCALE  (255.0/255.0)
#define Z_SCALE  (255.0/210.0)

#define DEFAULT_GAMMA 2.2f
#define SLIGHT_BOOST  (255.0/253.0)   // todo - get rid of this


/****************************************************************************\
 ****************************************************************************
 **                                                                        **
 **                    W O R K E R   R O U T I N E S                       **
 **                                                                        **
 ****************************************************************************
\****************************************************************************/



#define CLIP(wilma) ((wilma>255) ? 255 : ((wilma<0) ? 0 : (wilma)))

#define TERM_FRAC_BITS   4    // # frac bits in temp x/y/z terms
#define CONV_FRAC_BITS   4    // # frac bits in "_fix" conversion tables
#define CONV_FRAC_ROUND  (1L<<(CONV_FRAC_BITS-1))
#define TBL33_FRAC_BITS  16   // # bits of fraction in 3x3 tables
#define TABLE_FRAC_SCALE (1L << TBL33_FRAC_BITS)


// these tables are computed by initTables

static BYTE  YtoL       [256];   // these are for to/from CIELab
static BYTE  LtoY       [256];
static short cubeRoot   [256];
static BYTE  cube       [256];

static short cb2b       [256];   // these are for [y,cb,cr]->[r,g,b]
static short cr2r       [256];
static short cr2g_fix   [256];
static short cb2g_fix   [256];

static short r2y_fix    [256];   // these are for [r,g,b]->Y
static short g2y_fix    [256];
static short b2y_fix    [256];

static BYTE  by2cb     [2*256]; // these are for [r,g,b]->CbCr
static BYTE  ry2cr     [2*256];



// VectMult - Multiplies a pixel by a 3x3 matrix, with fixed-point math
//
static void VectMult(
    int   inPixel[3],  // in:  input pixel
    int   outPixel[3], // out: output pixel
    long *pMat)        // in:  3x3 matrix with TBL33_FRAC_BITS of fraction
{
    int    i;

    for (i=0; i<3; i++) {
        outPixel[i] = (int)( pMat[0]*inPixel[0] + 
                             pMat[1]*inPixel[1] +
                             pMat[2]*inPixel[2] + (1L<<(TBL33_FRAC_BITS-1))
                      ) >> TBL33_FRAC_BITS;
        pMat += 3;
    }
}



// initTables - Computes look-up tables
//
// CIE Illuminant D50 white point Xn=96.422 Yn=100 Zn=82.521
// CIE Illuminant D65 white point Xn=95.04  Yn=100 Zn=108.89
// FAX gamut Range:
// L* = [0,100]
// a* = [-85, 85]
// b* = [-75, 125]
//
// NL = 255/100*l
// Na = 255/170 x a* + 128
// Nb = 255/200 x b* + 96
//

static void initTables (void)
{
    int   val;
    int   icr, icb;
    float fval;
    float t;

    for (val=0; val<=255; val++)
    {
        fval = (float)val / 255.0f;

        // compute Y->L array

        if (fval > 0.008856f) t = 116.0f*(float)pow(fval,1.0/3.0) - 16.0f;
        else                  t = 903.3f*fval;
        t = (255.0f/100.0f)*t;
        if (t < 0.0f)   t = 0.0f;
        if (t > 255.0f) t = 255.0f;
        YtoL[val] = (BYTE)(t + 0.5f);

        // compute L->Y array

        if (val <= 7)
            t = val * (100.0f/903.3f);
        else {
            t = (fval*100.0f+16.0f) / 116.0f;
            t = 255.0f*t*t*t;
        }
        LtoY[val] = (BYTE)(t + 0.5f);

        // compute cube-root array.
        // Input is 0..255. We divide it by 255 so it's 0..1, take cube root,
        // and scale result to 0..255.  Adjustment is for small numbers.

        if (fval > 0.008856f) t = (float)pow(fval, 1.0f/3.0f);
        else                  t = 7.7867f*fval + 16.0f/116.0f;
        cubeRoot[val] = (short)(t*255.0f*(1L<<TERM_FRAC_BITS) + 0.5f);

        cube[val] = (BYTE)(255.0f*fval*fval*fval);
        // above, we don't round because we won't reach full black in lab->ycc conv

        // compute ycc<->rgb conversion tables

        fval = (float)val;

        r2y_fix[val] = (short)(SLIGHT_BOOST*RGBTOY_R_FAC*fval*(1<<CONV_FRAC_BITS));
        g2y_fix[val] = (short)(SLIGHT_BOOST*RGBTOY_G_FAC*fval*(1<<CONV_FRAC_BITS));
        b2y_fix[val] = (short)(SLIGHT_BOOST*RGBTOY_B_FAC*fval*(1<<CONV_FRAC_BITS));

        fval -= 128.0f;

        cb2b    [val] = (short)(floor(YCCTORGB_CB_TO_B*fval + 0.5));
        cr2r    [val] = (short)(floor(YCCTORGB_CR_TO_R*fval + 0.5));
        cb2g_fix[val] = (short)(YCCTORGB_CB_TO_G*fval*(1<<CONV_FRAC_BITS));
        cr2g_fix[val] = (short)(YCCTORGB_CR_TO_G*fval*(1<<CONV_FRAC_BITS));
    }


    for (val=0; val<=2*255; val++)
    {
        fval = (float)val - 255.0f;

        icb = (int)floor(fval/YCCTORGB_CB_TO_B + 0.5);
        if (icb>=-4 && icb<=4)   // make sure white is white
           icb = 0;
        icb += 128;
        by2cb[val] = (BYTE)CLIP(icb);

        icr = (int)floor(fval/YCCTORGB_CR_TO_R + 0.5);
        if (icr>=-4 && icr<=4)   // make sure white is white.
            icr = 0;
        icr += 128;
        ry2cr[val] = (BYTE)CLIP(icr);
    }
}



static void calcGammaTable (
    PCOL_INST g,
    DWORD     dwGamma)   /* Gamma value in 16.16 fixed-point */
{
    #define MAX_GAMMA_SLOPE 4

    int   i;
    int   maxval;
    float fGamma, f, gamval;
    BYTE  bGamVal;

    fGamma = (dwGamma == 0) ? DEFAULT_GAMMA : (float)dwGamma/(1L<<16);

    switch (g->which_cnv) {
        case IP_CNV_YCC_TO_CIELAB:
        /* YCC is assumed to have been Gamma corrected. So we must do
         * inverse Gamma when converting to CIELab
         */
        fGamma = 1.0f / fGamma;
        break;

        case IP_CNV_CIELAB_TO_YCC:
        /* CIELab has not been Gamma corrected, so we must do forward
         * Gamma when going to YCC.
         */
        break;

        default:
        /* No Gamma for the other conversions */
        fGamma = 1.0f;
    }

    if (fGamma == 1.0f) {
        /* No gamma correction: use identity table */
        for (i=0; i<=255; i++)
            g->bGammaTbl[i] = i;
    } else {
        fGamma = 1.0f / fGamma;
        for (i=0; i<=255; i++) {
            f = (float)i / 255.0f;
            gamval = (float)pow(f, fGamma);
            bGamVal = (BYTE)(255.0f*gamval + 0.5f);
            maxval = (int)(i * MAX_GAMMA_SLOPE);
            if (fGamma<1.0f && bGamVal>maxval)
                bGamVal = maxval;
            g->bGammaTbl[i] = bGamVal;
        }
    }
}



/****************************************************************************\
 ****************************************************************************
 **                                                                        **
 **                  YCC -> sRGB   and   YCC -> CIELAB                     **
 **                                                                        **
 ****************************************************************************
\****************************************************************************/


/*  Table for Molokai (and Wizard, I think) */
#if 0
static long RGBtoXYZ50[9] = {
    (long)(0.4358530*X_SCALE*TABLE_FRAC_SCALE + 0.5),
    (long)(0.3840300*X_SCALE*TABLE_FRAC_SCALE + 0.5),
    (long)(0.1431260*X_SCALE*TABLE_FRAC_SCALE + 0.5),

    (long)(0.2225640*Y_SCALE*TABLE_FRAC_SCALE + 0.5),
    (long)(0.7200520*Y_SCALE*TABLE_FRAC_SCALE + 0.5),
    (long)(0.0607176*Y_SCALE*TABLE_FRAC_SCALE + 0.5),

    (long)(0.0139307*Z_SCALE*TABLE_FRAC_SCALE + 0.5),
    (long)(0.0973260*Z_SCALE*TABLE_FRAC_SCALE + 0.5),
    (long)(0.7142870*Z_SCALE*TABLE_FRAC_SCALE + 0.5)
};
#endif


/*  Table for Polaris and Avalon */
#if 1
static long RGBtoXYZ50[9] = {
    (long)(0.464700*X_SCALE*TABLE_FRAC_SCALE + 0.5),
    (long)(0.339211*X_SCALE*TABLE_FRAC_SCALE + 0.5),
    (long)(0.156961*X_SCALE*TABLE_FRAC_SCALE + 0.5),

    (long)(0.220615*Y_SCALE*TABLE_FRAC_SCALE + 0.5),
    (long)(0.700919*Y_SCALE*TABLE_FRAC_SCALE + 0.5),
    (long)(0.053199*Y_SCALE*TABLE_FRAC_SCALE + 0.5),

    (long)(0.005146*Z_SCALE*TABLE_FRAC_SCALE + 0.5),
    (long)(0.050405*Z_SCALE*TABLE_FRAC_SCALE + 0.5),
    (long)(0.774384*Z_SCALE*TABLE_FRAC_SCALE + 0.5)
};
#endif



// YCCToCIELab - Converts a pixel (3 unsigned bytes) from YCC to fax LAB
//
// Conversions done herein:
//    YCC -> sRGB -> inverse Gamma -> XYZ(d65) -> XYZ(d50) -> LAB
//    Some of the above steps are combined for speed.
//                           
static void YCCToCIELab (
    PBYTE pYCC,      // in:  YCC pixel (3 unsigned bytes)
    PBYTE pCIELab,   // out: LAB pixel (3 unsigned bytes)
    PBYTE pGamma)    // in:  inverse Gamma table
{
    int iy, icb,icr;
    int sR, sG, sB;
    int sRGBval[3];
    int XYZ50val[3];
    int x, y, z;
    int xterm, yterm, zterm;
    int L, a, b;
    int absCr, absCb; //Neutral Shift Purpose
   

    iy  = pYCC[0];
    icb = pYCC[1];
    icr = pYCC[2];

#ifdef NEUTRAL_SHIFT_SEND

      absCb = abs(icb-128);
      absCr = abs(icr-128);
    
      if (iy>=210)
    {
      iy = Send_yTable[iy-210];
    }

    if (iy ==255)
    {
        
      if ((absCb < 5) && (absCr <5))
      {
        icr = icb = 128;
      }
    }
    else if (iy>240)
    {
      if ((absCb < 4) && (absCr <4))
      {
        icr=icb=128;
        iy = 255;
      }
      else if ((absCb < 3) && (absCr <3))
      {
        icr=icb=128;
      }
    }
    else if (iy>230)
    {         
      if ((absCb < 3) && (absCr <3))
      {
        icr=icb=128;
        iy = 250;
      }
    }
    else if (iy>220)
    {
      if ((absCb < 3) && (absCr <3))
      {
        icr=icb=128;
        iy = 240;
      }
    }
    else if (iy >60)
    {
      if ((absCb <3) && (absCr <3))
      {
        icr=icb=128;
      }
    }
    
  #endif

/* END NEUTRAL SHIFT */
    // iy=77; icb=85; icr=255;    // solid red
    // ycc=29,255,107  is  solid blue


    /**** Convert YCC to sRGB ****/

    sR = iy + cr2r[icr];
    sR = CLIP(sR);

    sG = iy + ((cr2g_fix[icr] + cb2g_fix[icb] + CONV_FRAC_ROUND) >> CONV_FRAC_BITS);
    sG = CLIP(sG);

    sB = iy + cb2b[icb];
    sB = CLIP(sB);

    /**** Inverse Gamma correction, and Convert sRGB(d65) to XYZ(d50) ****/

    sRGBval[0] = (long)pGamma[sR];
    sRGBval[1] = (long)pGamma[sG];
    sRGBval[2] = (long)pGamma[sB];

    VectMult (sRGBval, XYZ50val, RGBtoXYZ50);

    x = CLIP(XYZ50val[0]);
    y = CLIP(XYZ50val[1]);
    z = CLIP(XYZ50val[2]);

    /**** Convert XYZ(d50) to fax LAB ****/

    pCIELab[0] = L = YtoL[y];
    xterm = cubeRoot[x];
    yterm = cubeRoot[y];
    zterm = cubeRoot[z];

    b = yterm - zterm;
    b += (96<<TERM_FRAC_BITS) + (1<<(TERM_FRAC_BITS-1));
    b >>= TERM_FRAC_BITS;
    pCIELab[2] = (BYTE)CLIP(b);

    if (b < 0) {  /* todo - adjustment for out-of-gamut bright blue */
        /* xterm and L factors below of 6,1 -> rgb of 32,31,229     */
        /* xterm and L factors below of 8,2 -> rgb of 84,80,255     */
        /* xterm and L factors below of 6,0.5 are visually best     */
        xterm += 6*b;
        L -= b>>1;
        pCIELab[0] = (BYTE)CLIP(L);
    }

    // a = (long)((1<<10)*500.0/170.0) * (xterm-yterm) >> 10;
    // the 3*a - a/16 below is close enough to (500.0/170.0)*a
    a = xterm - yterm;
    a = a + a + a - (a>>4);  // multiply by approx 500/170
    a += (128<<TERM_FRAC_BITS) + (1<<(TERM_FRAC_BITS-1));
    a >>= TERM_FRAC_BITS;
    pCIELab[1] = (BYTE)CLIP(a);
}



// YCCTosRGB - Converts a pixel (3 unsigned bytes) from YCC to sRGB
//
static void YCCTosRGB (
    PBYTE pYCC,    // in:  YCC  pixel (3 unsigned bytes)
    PBYTE psRGB)   // out: sRGB pixel (3 unsigned bytes)
{
    int iy, icb, icr;
    int sR, sG, sB;

    iy  = pYCC[0];
    icb = pYCC[1];
    icr = pYCC[2];

    sR = iy + cr2r[icr];
    psRGB[0] = (BYTE)CLIP(sR);

    sG = iy + ((cr2g_fix[icr] + cb2g_fix[icb] + CONV_FRAC_ROUND) >> CONV_FRAC_BITS);
    psRGB[1] = (BYTE)CLIP(sG);

    sB = iy + cb2b[icb];
    psRGB[2] = (BYTE)CLIP(sB);
}



/****************************************************************************\
 ****************************************************************************
 **                                                                        **
 **                  sRGB -> YCC   and   CIELAB -> YCC                     **
 **                                                                        **
 ****************************************************************************
\****************************************************************************/


static long XYZ50tosRGB[9] = {
   (long)( 3.1344500*TABLE_FRAC_SCALE  /* *(255.0/248.0) */  /X_SCALE + 0.5),
   (long)(-1.6177000*TABLE_FRAC_SCALE  /* *(255.0/248.0) */  /Y_SCALE - 0.5),
   (long)(-0.4905000*TABLE_FRAC_SCALE  /* *(255.0/248.0) */  /Z_SCALE - 0.5),

   (long)(-0.9788600*TABLE_FRAC_SCALE  /* *(255.0/258.0) */  /X_SCALE - 0.5),
   (long)( 1.9164800*TABLE_FRAC_SCALE  /* *(255.0/258.0) */  /Y_SCALE + 0.5),
   (long)( 0.0334962*TABLE_FRAC_SCALE  /* *(255.0/258.0) */  /Z_SCALE + 0.5),

   (long)( 0.0719813*TABLE_FRAC_SCALE  /* *(255.0/254.0) */  /X_SCALE + 0.5),
   (long)(-0.2290660*TABLE_FRAC_SCALE  /* *(255.0/254.0) */  /Y_SCALE - 0.5),
   (long)( 1.4050500*TABLE_FRAC_SCALE  /* *(255.0/254.0) */  /Z_SCALE + 0.5)
};



// CIELabToYCC - Converts a pixel (3 unsigned bytes) from fax LAB to YCC
//
// Conversions done herein:
//    LAB(d50) -> XYZ(d50) -> XYZ(d65) -> sRGB -> Gamma -> YCC
//    Some of the above steps are combined for speed.
//
static void CIELabToYCC (
    PBYTE pCIELab,   // in:  LAB pixel (3 unsigned bytes)
    PBYTE pYCC,      // out: YCC pixel (3 unsigned bytes)
    PBYTE pGamma)    // in:  Gamma table
{
    int   a, b, xterm, yterm, zterm;
    int   Y;
    long  factor;
    int   XYZ50[3];
    int   sRGB[3];
    int   sR,sG,sB;
    int   iy;
    int   icr, icb;

    /**** LAB -> XYZ, both in d50 ****/

    factor = (long)((1L<<16)*170.0/500.0);
    a = (int)(((((long)pCIELab[1]-128) * factor) + 0x8000L) >> 16);
    b = (int)pCIELab[2] - 96;

    XYZ50[1] = Y = LtoY[pCIELab[0]];
    yterm = (cubeRoot[Y] + (1<<(TERM_FRAC_BITS-1))) >> TERM_FRAC_BITS;

    xterm = a + yterm;
    XYZ50[0] = cube[CLIP(xterm)];

    zterm = yterm - b;
    XYZ50[2] = cube[CLIP(zterm)];

    /**** XYZ(d50)->XYZ(d65)->sRGB via 3x3 matrix, then Gamma correct ****/

    VectMult (XYZ50, sRGB, XYZ50tosRGB);

    sR = pGamma[CLIP(sRGB[0])];
    sG = pGamma[CLIP(sRGB[1])];
    sB = pGamma[CLIP(sRGB[2])];

    /**** sRGB -> YCC ****/

    iy = r2y_fix[sR] + g2y_fix[sG] + b2y_fix[sB];
    iy = (iy + CONV_FRAC_ROUND) >> CONV_FRAC_BITS;
    iy = CLIP(iy);

    //It is done inside the Neutral Shift area
    //pYCC[0] = (BYTE)iy;

    //pYCC[1] = (by2cb+255)[sB-iy];
   // pYCC[2] = (ry2cr+255)[sR-iy];
    icb = (int)((by2cb+255)[sB-iy]); //it is pYCC[1]
    icr = (int)((ry2cr+255)[sR-iy]); //it is pYCC[2]

  pYCC[0] = (BYTE)iy;
  pYCC[1] = (BYTE)icb;
  pYCC[2] = (BYTE)icr;



}



// sRGBToYCC - Converts a pixel (3 unsigned bytes) from sRGB to YCC
//
static void sRGBToYCC (
    PBYTE psRGB,   // in:  sRGB pixel (3 unsigned bytes)
    PBYTE pYCC)    // out: YCC  pixel (3 unsigned bytes)
{
    int sR, sG, sB;
    int iy;

    sR = psRGB[0];
    sG = psRGB[1];
    sB = psRGB[2];

    iy = r2y_fix[sR] + g2y_fix[sG] + b2y_fix[sB];
    iy = (iy + CONV_FRAC_ROUND) >> CONV_FRAC_BITS;
    iy = CLIP(iy);
    pYCC[0] = (BYTE)iy;

    pYCC[1] = (by2cb+255)[sB-iy];
    pYCC[2] = (ry2cr+255)[sR-iy];
}



/****************************************************************************\
 ****************************************************************************
 **                                                                        **
 **                  sRGB -> HLS   and   HLS -> YCC                        **
 **                                                                        **
 ****************************************************************************
\****************************************************************************/



// sRGBToLHS - Converts a pixel (3 unsigned bytes) from sRGB to LHS
//
static void sRGBToLHS (
    PBYTE psRGB,   // in:  sRGB pixel (3 unsigned bytes)
    PBYTE pLHS)    // out: LHS  pixel (3 unsigned bytes)
{
    int R, G, B;
    int L, H, S;   // these are in 0..255
    int    maxVal, minVal, diff, sum, numerator;

    R = (unsigned)psRGB[0];
    G = (unsigned)psRGB[1];
    B = (unsigned)psRGB[2];

    maxVal = IP_MAX (R, G);
    maxVal = IP_MAX (maxVal, B);
    minVal = IP_MIN (R, G);
    minVal = IP_MIN (minVal, B);
    diff   = maxVal - minVal;
    sum    = maxVal + minVal;
    L      = sum >> 1;

    if (diff <= 1) {
        S = 0;
        H = 0;
    } else {
        // below is really 255*diff / (...), but avoiding the multiply
        S = ((diff<<9) - diff - diff) / (L<=127 ? sum : 510-sum);
        S = (S+1) >> 1;   // round to 8 bits
            
        // determine the hue
        if (R == maxVal) { 
            numerator = (maxVal-B) - (maxVal-G);
            H = 0;         // red is at 0 degrees
        } else if (G == maxVal) { 
            numerator = (maxVal-R) - (maxVal-B);
            H = (1<<12)*1/3;   // green is at 120 degrees
        } else {  // blue-dominant
            numerator = (maxVal-G) - (maxVal-R);
            H = (1<<12)*2/3;   // blue is at 240 degrees
        }

        // The line below is same as:  hue += ((1<<12)/6) * numerator / diff;
        // but is faster and more accurate.
        H += (numerator<<11) / (diff+diff+diff);
        H = (H + (1<<3)) >> 4;   // rounds 12 bits down to 8 bits
    }

    pLHS[0] = (unsigned char)L;
    pLHS[1] = (unsigned char)H;
    pLHS[2] = (unsigned char)S;
}



// LHSTosRGB - Converts a pixel (3 unsigned bytes) from LHS to sRGB
//
static void LHSTosRGB (
    PBYTE pLHS,    // in:  LHS  pixel (3 unsigned bytes)
    PBYTE psRGB)   // out: sRGB pixel (3 unsigned bytes)
{
    int L, H, S;
    int R=0, G=0, B=0;
    int hbase, hfrac, product, maxVal, minVal, midVal;

    L = (unsigned)pLHS[0];   // 1.0 is at 255
    H = (unsigned)pLHS[1];   // 1.0 is at 255 (which is 360 degrees)
    S = (unsigned)pLHS[2];   // 1.0 is at 255

    // In RGB_to_HLS, L=sum/2, which truncates.  The average error from this
    // truncation is 0.25, which is the (1<<4) added below.
    L = (L<<6) + (1<<4);   // now 1.0 is at 255*(1<<6), 6 bits of frac
    S = S<<6;

    H *= 6;
    hbase = H >> 8;       // in 0..5, and is the basic hue
    hfrac = H & 0x00ff;   // fractional offset from basic hue to next basic hue

    product = L*S;
    product = (product + (product>>8)) >> (8+6);  // approx division by 255*(1<<6)
    if (L <= (127<<6)+(1<<4)) maxVal = L + product;
    else                      maxVal = L + S - product;

    minVal = L + L - maxVal;
    midVal = minVal + (((maxVal-minVal) * ((hbase&1) ? 256-hfrac : hfrac)) >> 8);

    // round the results to 8 bits by shifting out the 6 frac bits
    minVal = (minVal+(1<<5)) >> 6;
    midVal = (midVal+(1<<5)) >> 6;
    maxVal = (maxVal+(1<<5)) >> 6;

    // I ran this routine with all possible h-l-s values (2^24 of them!), and
    // none produced a value outside 0..255, so we don't do the checks below.
    // if (maxVal < 0) maxVal=0; else if (maxVal > 255) maxVal = 255;
    // if (midVal < 0) midVal=0; else if (midVal > 255) midVal = 255;
    // if (minVal < 0) minVal=0; else if (minVal > 255) minVal = 255;

    switch (hbase)
    {
        case 0:  R = maxVal;  G = midVal;  B = minVal;  break;
        case 1:  R = midVal;  G = maxVal;  B = minVal;  break;
        case 2:  R = minVal;  G = maxVal;  B = midVal;  break;
        case 3:  R = minVal;  G = midVal;  B = maxVal;  break;
        case 4:  R = midVal;  G = minVal;  B = maxVal;  break;
        case 5:  R = maxVal;  G = minVal;  B = midVal;  break;
    }

    psRGB[0] = (BYTE)R;
    psRGB[1] = (BYTE)G;
    psRGB[2] = (BYTE)B;
}



/****************************************************************************\
 ****************************************************************************
 **                                                                        **
 **                      E N T R Y   P O I N T S                           **
 **                                                                        **
 ****************************************************************************
\****************************************************************************/



/*****************************************************************************\
 *
 * color_openXform - Creates a new instance of the transformer
 *
 *****************************************************************************
 *
 * This returns a handle for the new instance to be passed into
 * all subsequent calls.
 *
 * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
 *
\*****************************************************************************/

static WORD color_openXform (
    IP_XFORM_HANDLE *pXform)   /* out: returned handle */
{
    PCOL_INST g;

    if (! fInited) {
        initTables ();
        fInited = TRUE;
    }

    INSURE (pXform != NULL);
    IP_MEM_ALLOC (sizeof(COL_INST), g);
    *pXform = g;
    memset (g, 0, sizeof(COL_INST));
    g->dwValidChk = CHECK_VALUE;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * color_setDefaultInputTraits - Specifies default input image traits
 *
 *****************************************************************************
 *
 * The header of the file-type handled by the transform probably does
 * not include *all* the image traits we'd like to know.  Those not
 * specified in the file-header are filled in from info provided by
 * this routine.
 *
 * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
 *
\*****************************************************************************/

static WORD color_setDefaultInputTraits (
    IP_XFORM_HANDLE  hXform,     /* in: handle for xform */
    PIP_IMAGE_TRAITS pTraits)    /* in: default image traits */
{
    PCOL_INST g;

    HANDLE_TO_PTR (hXform, g);

    /* Insure that values we care about are correct */
    INSURE (pTraits->iBitsPerPixel <= 24);
    INSURE (pTraits->iPixelsPerRow > 0);

    g->traits = *pTraits;   /* a structure copy */
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * color_setXformSpec - Provides xform-specific information
 *
\*****************************************************************************/

static WORD color_setXformSpec (
    IP_XFORM_HANDLE hXform,         /* in: handle for xform */
    DWORD_OR_PVOID  aXformInfo[])   /* in: xform information */
{
    PCOL_INST g;

    HANDLE_TO_PTR (hXform, g);
    g->which_cnv = (IP_WHICH_CNV)aXformInfo[IP_CNV_COLOR_SPACE_WHICH_CNV].dword;
    calcGammaTable (g, aXformInfo[IP_CNV_COLOR_SPACE_GAMMA].dword);

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * color_getHeaderBufSize- Returns size of input buf needed to hold header
 *
\*****************************************************************************/

static WORD color_getHeaderBufSize (
    IP_XFORM_HANDLE  hXform,          /* in:  handle for xform */
    DWORD           *pdwInBufLen)     /* out: buf size for parsing header */
{
    /* since input is raw pixels, there is no header, so set it to zero */
    *pdwInBufLen = 0;
    return IP_DONE;
}



/*****************************************************************************\
 *
 * color_getActualTraits - Parses header, and returns input & output traits
 *
\*****************************************************************************/

static WORD color_getActualTraits (
    IP_XFORM_HANDLE  hXform,          /* in:  handle for xform */
    DWORD            dwInputAvail,    /* in:  # avail bytes in input buf */
    PBYTE            pbInputBuf,      /* in:  ptr to input buffer */
    PDWORD           pdwInputUsed,    /* out: # bytes used from input buf */
    PDWORD           pdwInputNextPos, /* out: file-pos to read from next */
    PIP_IMAGE_TRAITS pIntraits,       /* out: input image traits */
    PIP_IMAGE_TRAITS pOutTraits)      /* out: output image traits */
{
    PCOL_INST g;

    HANDLE_TO_PTR (hXform, g);

    /* Since there is no header, we'll report no usage of input */
    *pdwInputUsed    = 0;
    *pdwInputNextPos = 0;

    *pIntraits  = g->traits;   /* structure copies */
    *pOutTraits = g->traits;

    return IP_DONE | IP_READY_FOR_DATA;

    fatal_error:
    return IP_FATAL_ERROR;
}



/****************************************************************************\
 *
 * color_getActualBufSizes - Returns buf sizes needed for remainder of job
 *
\****************************************************************************/

static WORD color_getActualBufSizes (
    IP_XFORM_HANDLE hXform,           /* in:  handle for xform */
    PDWORD          pdwMinInBufLen,   /* out: min input buf size */
    PDWORD          pdwMinOutBufLen)  /* out: min output buf size */
{
    PCOL_INST g;

    HANDLE_TO_PTR (hXform, g);
    *pdwMinInBufLen = *pdwMinOutBufLen =
        (g->traits.iPixelsPerRow*g->traits.iBitsPerPixel + 7) / 8;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * color_convert - Converts one row
 *
\*****************************************************************************/

static WORD color_convert (
    IP_XFORM_HANDLE hXform,
    DWORD           dwInputAvail,     /* in:  # avail bytes in in-buf */
    PBYTE           pbInputBuf,       /* in:  ptr to in-buffer */
    PDWORD          pdwInputUsed,     /* out: # bytes used from in-buf */
    PDWORD          pdwInputNextPos,  /* out: file-pos to read from next */
    DWORD           dwOutputAvail,    /* in:  # avail bytes in out-buf */
    PBYTE           pbOutputBuf,      /* in:  ptr to out-buffer */
    PDWORD          pdwOutputUsed,    /* out: # bytes written in out-buf */
    PDWORD          pdwOutputThisPos) /* out: file-pos to write the data */
{
    PCOL_INST g;
    int       nBytes;
    PBYTE     pIn, pOut, pOutAfter;

    HANDLE_TO_PTR (hXform, g);

    /**** Check if we were told to flush ****/

    if (pbInputBuf == NULL) {
        PRINT (_T("color_convert: Told to flush.\n"), 0, 0);
        *pdwInputUsed = *pdwOutputUsed = 0;
        *pdwInputNextPos  = g->dwInNextPos;
        *pdwOutputThisPos = g->dwOutNextPos;
        return IP_DONE;
    }

    /**** Output a Row ****/

    nBytes = (g->traits.iPixelsPerRow*g->traits.iBitsPerPixel + 7) / 8;
    INSURE (dwInputAvail  >= (DWORD)nBytes );
    INSURE (dwOutputAvail >= (DWORD)nBytes);

    pIn  = pbInputBuf;
    pOut = pbOutputBuf;
    pOutAfter = pOut + nBytes;

    if (g->traits.iBitsPerPixel < 24) {
        /* grayscale data; pass through unchanged */
        memcpy (pOut, pIn, nBytes);
    } else if (g->which_cnv == IP_CNV_BGR_SWAP) {
        while (pOut < pOutAfter) {
            pOut[0] = pIn[2];
            pOut[1] = pIn[1];
            pOut[2] = pIn[0];
            pIn  += 3;
            pOut += 3;
        }
    } else {
        while (pOut < pOutAfter) {
            switch (g->which_cnv) {
                case IP_CNV_YCC_TO_CIELAB: YCCToCIELab (pIn, pOut, g->bGammaTbl);  break;
                case IP_CNV_CIELAB_TO_YCC: CIELabToYCC (pIn, pOut, g->bGammaTbl);  break;
                case IP_CNV_YCC_TO_SRGB:   YCCTosRGB   (pIn, pOut);                break;
                case IP_CNV_SRGB_TO_YCC:   sRGBToYCC   (pIn, pOut);                break;
                case IP_CNV_LHS_TO_SRGB:   LHSTosRGB   (pIn, pOut);                break;
                case IP_CNV_SRGB_TO_LHS:   sRGBToLHS   (pIn, pOut);                break;
                default:                   goto fatal_error;
            }

            pIn  += 3;
            pOut += 3;
        }
    }

    *pdwInputUsed     = nBytes;
    g->dwInNextPos   += nBytes;
    *pdwInputNextPos  = g->dwInNextPos;

    *pdwOutputUsed    = nBytes;
    *pdwOutputThisPos = g->dwOutNextPos;
    g->dwOutNextPos  += nBytes;

    g->dwRowsDone += 1;

    return IP_CONSUMED_ROW | IP_PRODUCED_ROW | IP_READY_FOR_DATA;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * color_insertedData - client inserted into our output stream
 *
\*****************************************************************************/

static WORD color_insertedData (
    IP_XFORM_HANDLE hXform,
    DWORD           dwNumBytes)
{
    fatalBreakPoint ();
    return IP_FATAL_ERROR;   /* must never be called (can't insert data) */
}



/*****************************************************************************\
 *
 * color_newPage - Tells us to flush this page, and start a new page
 *
\*****************************************************************************/

static WORD color_newPage (
    IP_XFORM_HANDLE hXform)
{
    PCOL_INST g;

    HANDLE_TO_PTR (hXform, g);
    return IP_DONE;   /* can't insert page-breaks, so ignore this call */

    fatal_error:
    return IP_FATAL_ERROR;

}



/*****************************************************************************\
 *
 * color_closeXform - Destroys this instance
 *
\*****************************************************************************/

static WORD color_closeXform (IP_XFORM_HANDLE hXform)
{
    PCOL_INST g;

    HANDLE_TO_PTR (hXform, g);

    g->dwValidChk = 0;
    IP_MEM_FREE (g);       /* free memory for the instance */

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * colorTbl - Jump-table for transform driver
 *
\*****************************************************************************/

IP_XFORM_TBL colorTbl = {
    color_openXform,
    color_setDefaultInputTraits,
    color_setXformSpec,
    color_getHeaderBufSize,
    color_getActualTraits,
    color_getActualBufSizes,
    color_convert,
    color_newPage,
    color_insertedData,
    color_closeXform
};

/* End of File */
