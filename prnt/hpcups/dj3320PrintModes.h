PrintMode dj3320PrintModes[] =
{
	// Both pens - Plain/Draft
    {
        "PlainDraftKColor",
        {300, 300, 300, 300, 300, 300},
        {300, 300, 300, 300, 300, 300},
        {1, 1, 1, 1, 1, 1}, 4, FED,
        {ulMapDJ3320_KCMY_3x3x1, NULL, NULL},
        300, 300, false, HTBinary_open, HTBinary_open
    },
	//  Both Pens - Plain/Normal Color
    {
        "PlainNormalKColor",
        {600, 600, 600, 600, 600, 600},
        {600, 600, 600, 600, 600, 600},
        {1, 1, 1, 1, 1, 1}, 4, FED,
        {ulMapDJ3320_KCMY_6x6x1, NULL, NULL},
        600, 600, false, HTBinary_open, HTBinary_open
    },
	//  Both pens or Color pen only- photo paper/best
    {
        "PhotoBest",
        {600, 600, 600, 600, 600, 600},
        {600, 600, 600, 600, 600, 600},
        {1, 2, 2, 2, 2, 2}, 4, FED,
        {ulMapDJ970_Gossimer_Normal_KCMY, NULL, NULL},
        600, 600, false, HTBinary_open, HT600x6004level970_open
    },
	// color pen only - Plain/Draft
    {
        "PlainDraftColor",
        {300, 300, 300, 300, 300, 300},
        {300, 300, 300, 300, 300, 300},
        {1, 1, 1, 1, 1, 1}, 3, FED,
        {ulMapDJ3320_CMY_3x3x1, NULL, NULL},
        300, 300, false, HTBinary_open, HTBinary_open
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
        {300, 300, 300, 300, 300, 300},
        {300, 300, 300, 300, 300, 300},
        {1, 1, 1, 1, 1, 1}, 1, FED,
        {ulMapDJ3320_K_3x3x1, NULL, NULL},
        300, 300, false, HTBinary_open, HTBinary_open
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
        "PlainGrayPhotoColor",
        {600, 600, 600, 600, 600, 600},
        {600, 600, 600, 600, 600, 600},
        {1, 1, 1, 1, 1, 1}, 1, FED,
        {ulMapDJ3320_K_6x6x1, ulMapDJ3600_ClMlxx_6x6x1, NULL},
        600, 600, false, HTBinary_open, HTBinary_open
    },
	//  Color and Photo pen - Plain/Draft - color
    {
        "PlainDraftColorPhoto",
        {300, 300, 300, 300, 300, 300},
        {300, 300, 300, 300, 300, 300},
        {1, 1, 1, 1, 1, 1}, 6, FED,
        {ulMapDJ3320_KCMY_3x3x1, ulMapDJ3600_ClMlxx_3x3x1, NULL},
        300, 300, false, HTBinary_open, HTBinary_open
    },
	//  Color and Photo pen - Plain/Normal - color
    {
        "PlainNormalColorPhoto",
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

