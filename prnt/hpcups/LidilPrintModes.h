#include "CommonDefinitions.h"
#include "dj3320PrintModes.h"
#include "dj4100PrintModes.h"


/*PQ_Cartridge_Map contains the PrintMode Index value present in PrintMode Array for all combinations of PQ and Installed Cartridges*/
/*-1 means the combination is not possible*/
int PQ_Cartridge_Map[][5]={
                                /*BlackColor*/       /*TriColor*/      /*PhotoColor*/   /*Black+TriColor*/   /*Photo+TriColor*/
/*Normal Color*/           {           1,                  4,                1,                 1,                   9},  
						
/*Normal Grey*/            {           6,                  6,                6,                 6,                   7},  

/*Draft Color*/            {           0,                  3,                0,                 0,                   8},  

/*Draft Grey*/             {           5,                  5,                5,                 5,                   5}, 
						
/*High Resolution Photo*/  {           2,                  2,                2,                 2,                  10},  
};

int PQ_Cartridge_Map_ViperTrim[][5]={
                                /*BlackColor*/       /*TriColor*/      /*PhotoColor*/   /*Black+TriColor*/   /*Photo+TriColor*/
/*Normal Color*/           {           4,                  4,                4,                 4,                   4},  
						
/*Normal Grey*/            {           6,                  6,                6,                 6,                   7},  

/*Draft Color*/            {           3,                  3,                3,                 3,                   3},  

/*Draft Grey*/             {           5,                  5,                5,                 5,                   5}, 
						
/*High Resolution Photo*/  {           2,                  2,                2,                 2,                  10},  
};


#if 0

/*PQ_Cartridge_Map contains the PrintMode Index value present in PrintMode Array for all combinations of PQ and Installed Cartridges*/
/*-1 means the combination is not possible*/
int PQ_Cartridge_Map[][5]={
                                /*BlackColor*/       /*TriColor*/      /*PhotoColor*/   /*Black+TriColor*/   /*Photo+TriColor*/
/*Normal Color*/           {          -1,                  4,               -1,                 1,                   9},  
						
/*Normal Grey*/            {           6,                 -1,                6,                 6,                   7},  

/*Draft Color*/            {          -1,                  3,               -1,                 0,                   8},  

/*Draft Grey*/             {           5,                 -1,               -1,                 5,                  -1}, 
						
/*High Resolution Photo*/  {          -1,                  2,               -1,                 2,                  10},  
};

#endif


PrintModeTable lidil_print_modes_table [] =
{
    {"dj3320", dj3320PrintModes, sizeof(dj3320PrintModes)/sizeof(PrintMode)},
    {"dj3600", dj3320PrintModes, sizeof(dj3320PrintModes)/sizeof(PrintMode)},
    {"dj4100", dj4100PrintModes, sizeof(dj4100PrintModes)/sizeof(PrintMode)},
    {"dj2600", dj4100PrintModes, sizeof(dj4100PrintModes)/sizeof(PrintMode)}
};

