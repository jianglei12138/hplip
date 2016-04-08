#include "CommonDefinitions.h"
#include "dj400PrintModes.h"
#include "dj540PrintModes.h"
#include "dj600PrintModes.h"
#include "dj630PrintModes.h"
#include "dj690PrintModes.h"
#include "apPrintModes.h"
#include "dj850PrintModes.h"

PrintModeTable pcl3_print_modes_table [] =
{
    {"dj400", dj400PrintModes, sizeof(dj400PrintModes)/sizeof(PrintMode)},
    {"dj540", dj540PrintModes, sizeof(dj540PrintModes)/sizeof(PrintMode)},
    {"dj600", dj600PrintModes, sizeof(dj600PrintModes)/sizeof(PrintMode)},
    {"dj630", dj630PrintModes, sizeof(dj630PrintModes)/sizeof(PrintMode)},
    {"dj690", dj690PrintModes, sizeof(dj690PrintModes)/sizeof(PrintMode)},
    {"apollo2xxx", apPrintModes, sizeof(apPrintModes)/sizeof(PrintMode)},
    {"dj850", dj850PrintModes, sizeof(dj850PrintModes)/sizeof(PrintMode)}
};

