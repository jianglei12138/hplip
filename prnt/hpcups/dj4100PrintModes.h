PrintMode dj4100PrintModes[] =
{
	// Both pens - Plain/Draft
    {
        "PlainDraftKColor",
        {600, 600, 600, 600, 600, 600},
        {600, 600, 600, 600, 600, 600},
        {1, 1, 1, 1, 1, 1}, 4, FED,
        {ulMapDJ3320_KCMY_3x3x1, NULL, NULL},
        600, 600, false, HTBinary_open, HTBinary_open
    },
	//  Both Pens - Plain/Normal Color
    {
        "PlainNormalKColor",
        {600, 600, 600, 600, 600, 600},
        {600, 600, 600, 600, 600, 600},
        {1, 1, 1, 1, 1, 1}, 4, FED,
        {NULL, NULL, ucMapDJ4100_KCMY_6x6x1},
        600, 600, false, HTBinary_open, HTBinary_open
    },
	//  Both pens or Color pen only- photo paper/best
    {
        "PhotoBest",
        {600, 600, 600, 600, 600, 600},
        {600, 600, 600, 600, 600, 600},
        {1, 2, 2, 2, 2, 2}, 4, FED,
        {NULL, NULL, ucMapDJ4100_KCMY_Photo_BestA_12x12x1},
        600, 600, false, HTBinary_open, HT1200x1200x1PhotoBest_open
    },
	// color pen only - Plain/Draft
    {
        "PlainDraftColor",
        {600, 600, 600, 600, 600, 600},
        {600, 600, 600, 600, 600, 600},
        {1, 1, 1, 1, 1, 1}, 3, FED,
        {ulMapDJ3320_CMY_3x3x1, NULL, NULL},
        600, 600, false, HTBinary_open, HTBinary_open
    },
	//  Color Pen only - Plain/Normal Color
    {
        "PlainNormalColor",
        {600, 600, 600, 600, 600, 600},
        {600, 600, 600, 600, 600, 600},
        {1, 1, 1, 1, 1, 1}, 3, FED,
        {ulMapDJ3320_CMY_6x6x1, NULL, NULL},
        600, 600, false, HTBinary_open, HTBinary_open
    },
	//  Black pen only or both pens - Plain/Draft
    {
        "PlainDraftGrayK",
        {600, 600, 600, 600, 600, 600},
        {600, 600, 600, 600, 600, 600},
        {1, 1, 1, 1, 1, 1}, 1, FED,
        {ulMapDJ3320_K_3x3x1, NULL, NULL},
        600, 600, false, HTBinary_open, HTBinary_open
    },
	//  Black pen only or Photo Pen only or both pens - Plain/Normal Gray
    {
        "PlainNormalGrayK",
        {600, 600, 600, 600, 600, 600},
        {600, 600, 600, 600, 600, 600},
        {1, 1, 1, 1, 1, 1}, 1, FED,
        {ulMapDJ3320_K_6x6x1, NULL, NULL},
        600, 600, false, HTBinary_open, HTBinary_open
    },
	//  Color and Photo pen - Plain/Normal - Gray
    {
        "PlainNormalPhotoGrayK",
        {600, 600, 600, 600, 600, 600},
        {600, 600, 600, 600, 600, 600},
        {1, 1, 1, 1, 1, 1}, 1, FED,
        {ulMapDJ3320_K_6x6x1, ulMapDJ3600_ClMlxx_6x6x1, NULL},
        600, 600, false, HTBinary_open, HTBinary_open
    },
	//  Color and Photo pen - Plain/Draft - color
    {
        "PlainDraftPhotoColor",
        {600, 600, 600, 600, 600, 600},
        {600, 600, 600, 600, 600, 600},
        {1, 1, 1, 1, 1, 1}, 6, FED,
        {ulMapDJ3320_KCMY_3x3x1, ulMapDJ3600_ClMlxx_3x3x1, NULL},
        600, 600, false, HTBinary_open, HTBinary_open
    },
	//  Color and Photo pen - Plain/Normal - color
    {
        "PlainNormalPhotoColor",
        {600, 600, 600, 600, 600, 600},
        {600, 600, 600, 600, 600, 600},
        {1, 1, 1, 1, 1, 1}, 6, FED,
        {ulMapDJ3320_K_6x6x1, ulMapDJ3600_ClMlxx_6x6x1, NULL},
        600, 600, false, HTBinary_open, HTBinary_open
    },
	//  Color and Photo pen - photo/best - color
    {
        "PhotoBestPhotoColor",
        {600, 600, 600, 600, 600, 600},
        {600, 600, 600, 600, 600, 600},
        {2, 2, 2, 2, 2, 2}, 6, FED,
        {ulMapDJ3600_KCMY_6x6x2, ulMapDJ3600_ClMlxx_6x6x2, NULL},
        600, 600, false, HTBinary_open, HTBinary_open
    }
};

