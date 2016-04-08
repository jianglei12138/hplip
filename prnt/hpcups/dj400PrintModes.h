PrintMode dj400PrintModes[] =
{
    {
        "PlainNormalColor",
        {300, 300, 300, 300, 300, 300},
        {300, 300, 300, 300, 300, 300},
        {1, 1, 1, 1, 1, 1}, 3, FED,
        {ulMapDJ400_CMY, NULL, NULL},
        300, 300, false, HTBinary_open, HTBinary_open
    },
    {
        "PlainNormalGray",
        {300, 300, 300, 300, 300, 300},
        {300, 300, 300, 300, 300, 300},
        {1, 1, 1, 1, 1, 1}, 1, FED,
        {ulMapDJ400_K, NULL, NULL},
        300, 300, false, HTBinary_open, HTBinary_open
    }
};

