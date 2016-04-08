PrintMode dj690PrintModes[] =
{
    {
        "PlainDraftColor",
        {300, 300, 300, 300, 300, 300},
        {300, 300, 300, 300, 300, 300},
        {1, 1, 1, 1, 1, 1}, 4, MATRIX,
        {ulMapDJ660_CCM_KCMY, NULL, NULL},
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
        {300, 300, 300, 300, 300, 300},
        {300, 300, 300, 300, 300, 300},
        {1, 1, 1, 1, 1, 1}, 4, FED,
        {ulMapDJ660_CCM_KCMY, NULL, NULL},
        300, 300, false, HTBinary_open, HTBinary_open
    },
    {
        "PlainNormalGrayK",
        {300, 300, 300, 300, 300, 300},
        {300, 300, 300, 300, 300, 300},
        {1, 1, 1, 1, 1, 1}, 1, FED,
        {ulMapDJ600_CCM_K, NULL, NULL},
        300, 300, false, HTBinary_open, HTBinary_open
    },
    {
        "PlainBestGrayK",
        {600, 600, 600, 600, 600, 600},
        {600, 600, 600, 600, 600, 600},
        {1, 1, 1, 1, 1, 1}, 1, FED,
        {ulMapDJ600_CCM_K, NULL, NULL},
        600, 600, false, HTBinary_open, HTBinary_open
    },
    {
        "PhotoBestColor",
        {600, 600, 600, 600, 600, 600},
        {600, 600, 600, 600, 600, 600},
        {1, 1, 1, 1, 1, 1}, 6, FED,
        {ulMapDJ690_CMYK, ulMapDJ690_ClMlxx, NULL},
        600, 600, false, HTBinary_open, HTBinary_open
    }
};

