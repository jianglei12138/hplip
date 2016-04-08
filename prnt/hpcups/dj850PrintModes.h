PrintMode dj850PrintModes[] =
{
    {
        "PlainDraftColor",
        {300, 300, 300, 300, 300, 300},
        {300, 300, 300, 300, 300, 300},
        {1, 1, 1, 1, 1, 1}, 4, FED,
        {ulMapDJ850_Normal_KCMY, NULL, NULL},
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
        {ulMapDJ850_Normal_KCMY, NULL, NULL},
        300, 300, true, HTBinary_open, HT300x3004level_open
    },
    {
        "PlainNormalGrayK",
        {600, 300, 300, 300, 300, 300},
        {600, 300, 300, 300, 300, 300},
        {1, 1, 1, 1, 1, 1}, 1, FED,
        {ulMapGRAY_K_6x6x1, NULL, NULL},
        300, 300, true, HTBinary_open, HTBinary_open
    }
};

