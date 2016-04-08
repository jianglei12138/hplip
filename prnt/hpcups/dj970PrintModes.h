PrintMode dj970PrintModes[] =
{
    {
        "PlainDraftColor",
        {300, 300, 300, 300, 300, 300},
        {300, 300, 300, 300, 300, 300},
        {1, 1, 1, 1, 1, 1}, 4, FED,
        {ulMapDJ970_Draft_KCMY, NULL, NULL},
        300, 300, false, HTBinary_open, HTBinary_open
    },
    {
        "PlainDraftGrayK",
        {300, 300, 300, 300, 300, 300},
        {300, 300, 300, 300, 300, 300},
        {1, 1, 1, 1, 1, 1}, 1, FED,
        {ulMapDJ600_CCM_K, NULL, NULL},
        300, 300, false, HTBinary_open, HTBinary_open
    },
    {
        "PlainNormalColor",
        {600, 300, 300, 300, 300, 300},
        {600, 300, 300, 300, 300, 300},
        {1, 2, 2, 2, 2, 2}, 4, FED,
        {ulMapDJ970_KCMY_3x3x2, NULL, NULL},
        300, 300, true, HTBinary_open, HT300x3004level970_open
    },
    {
        "PlainNormalGrayK",
        {600, 300, 300, 300, 300, 300},
        {600, 300, 300, 300, 300, 300},
        {1, 1, 1, 1, 1, 1}, 1, FED,
        {ulMapGRAY_K_6x6x1, NULL, NULL},
        600, 600, false, HTBinary_open, HTBinary_open
    },
    {
        "PlainBestColor",
        {600, 600, 600, 600, 600, 600},
        {600, 600, 600, 600, 600, 600},
        {1, 2, 2, 2, 2, 2}, 4, FED,
        {ulMapDJ970_KCMY, NULL, NULL},
        600, 600, false, HTBinary_open, HT600x600x4_Pres970_open
    },
    {
        "PhotoBestColor",
        {600, 600, 600, 600, 600, 600},
        {600, 600, 600, 600, 600, 600},
        {1, 2, 2, 2, 2, 2}, 4, FED,
        {ulMapDJ970_Gossimer_Normal_KCMY, NULL, NULL},
        600, 600, false, HTBinary_open, HT600x6004level970_open
    },
    {
        "PhotoMaxDpiColor",
        {1200, 1200, 1200, 1200, 1200, 1200},
        {1200, 1200, 1200, 1200, 1200, 1200},
        {1, 1, 1, 1, 1, 1}, 4, FED,
        {ulMapDJ970_Gossimer_Normal_KCMY, NULL, NULL},
        1200, 1200, false, HTBinary_open, HT1200x1200x1_PhotoPres970_open
    }
};

