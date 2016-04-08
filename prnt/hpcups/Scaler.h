
#ifndef SCALER_H
#define SCALER_H

#include "CommonDefinitions.h"
#include "Processor.h"

class Scaler : public Processor
{
public:
    Scaler(unsigned int inputwidth, unsigned int numerator,
           unsigned int denominator, bool bVIP, unsigned int BytesPerPixel,
           unsigned int iNumInks);
    virtual ~Scaler();
    bool Process(RASTERDATA* InputRaster);
    virtual void Flush() { Process(NULL); }

    float ScaleFactor;
    unsigned int remainder;

    unsigned int  GetMaxOutputWidth();
    bool  NextOutputRaster(RASTERDATA &next_raster);

private:

    bool scaling;       // false iff ScaleFactor==1.0
    bool ReplicateOnly; // true iff 1<ScaleFactor<2

    unsigned int iOutputWidth;
    unsigned int iInputWidth;
    BYTE     *pOutputBuffer[MAX_COLORTYPE];
    bool     fractional;
    unsigned int rowremainder;
    unsigned int NumInks;
    bool     vip;
    DRIVER_ERROR constructor_error;
};
#endif // SCALER_H

