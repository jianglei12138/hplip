PrintMode dj895PrintModes[] =
{
    {
        "PlainDraftColor",
        {300, 300, 300, 300, 300, 300},
        {300, 300, 300, 300, 300, 300},
        {1, 1, 1, 1, 1, 1}, 4, FED,
        {ulMapDJ895_Binary_KCMY, NULL, NULL},
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
        {1, 1, 1, 1, 1, 1}, 4, FED,
        {ulMapDJ895_KCMY, NULL, NULL},
        300, 300, true, HTBinary_open, HT300x3004level_open
    },
    {
        "PlainNormalGrayK",
        {600, 300, 300, 300, 300, 300},
        {600, 300, 300, 300, 300, 300},
        {1, 1, 1, 1, 1, 1}, 1, FED,
        {ulMapGRAY_K_6x6x1, NULL, NULL},
        300, 300, true, HTBinary_open, HTBinary_open
    },
    {
        "PhotoBestColor",
        {600, 600, 600, 600, 600, 600},
        {600, 600, 600, 600, 600, 600},
        {1, 2, 2, 2, 2, 2}, 4, FED,
        {ulMapDJ895_HB_KCMY, NULL, NULL},
        600, 600, false, HTBinary_open, HT600x6004level895_open
    }
};

