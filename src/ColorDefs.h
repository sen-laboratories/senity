/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

#include <GraphicsDefs.h>
#include <SupportDefs.h>

#define NUM_COLORS 16

enum COLOR_NAME{
    BLACK = 0,
    WHITE,
    LIGHT_RED,
    LIGHT_GREEN,
    LIGHT_BLUE,
    LIGHT_YELLOW,
    LIGHT_PURPLE,
    LIGHT_CYAN,
    LIGHT_GREY,
    DARK_RED,
    DARK_GREEN,
    DARK_BLUE,
    DARK_YELLOW,
    DARK_PURPLE,
    DARK_CYAN,
    DARK_GREY,
};

class ColorDefs {
    public:
                            ColorDefs();
        virtual             ~ColorDefs();

        rgb_color*          GetColor(COLOR_NAME color);
        rgb_color*          HexToRgb(const char* hexString);

        rgb_color*          text_color[NUM_COLORS];
};
