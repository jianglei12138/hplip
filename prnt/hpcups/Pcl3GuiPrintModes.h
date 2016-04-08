#include "CommonDefinitions.h"
#include "dj890PrintModes.h"
#include "dj895PrintModes.h"
#include "dj8x5PrintModes.h"
#include "dj970PrintModes.h"

PrintModeTable pcl3_gui_print_modes_table [] =
{
    {"dj890", dj890PrintModes, sizeof(dj890PrintModes)/sizeof(PrintMode)},
    {"dj895", dj895PrintModes, sizeof(dj895PrintModes)/sizeof(PrintMode)},
    {"dj8x5", dj8x5PrintModes, sizeof(dj8x5PrintModes)/sizeof(PrintMode)},
    {"dj970", dj970PrintModes, sizeof(dj970PrintModes)/sizeof(PrintMode)}
};

