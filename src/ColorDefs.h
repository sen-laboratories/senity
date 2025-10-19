/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

#include <GraphicsDefs.h>
#include <SupportDefs.h>

#define NUM_COLORS 10

enum COLOR_NAME{
    BLACK = 0,
    WHITE
};

class ColorDefs {
    public:
                            ColorDefs();
        virtual             ~ColorDefs();

        rgb_color*          GetColor(COLOR_NAME color);
        rgb_color*          HexToRgb(const char* hexString);

        rgb_color*          text_color[NUM_COLORS];
};
