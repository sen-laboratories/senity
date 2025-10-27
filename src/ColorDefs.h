/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

#include <GraphicsDefs.h>
#include <SupportDefs.h>

#define NUM_COLORS 10

// Semantic color names for highlights
enum COLOR_NAME {
    COLOR_BLACK = 0,
    COLOR_WHITE,
    COLOR_GOLD,          // For topics/concepts
    COLOR_ORANGE,        // For actions/events
    COLOR_RED,           // For important/urgent
    COLOR_MAGENTA,       // For people/persons
    COLOR_PURPLE,        // For locations/places
    COLOR_BLUE,          // For references/links
    COLOR_CYAN,          // For context/background
    COLOR_GREEN          // For positive/success
};

class ColorDefs {
    public:
                            ColorDefs();
        virtual             ~ColorDefs();

        rgb_color*          GetColor(COLOR_NAME color);
        rgb_color*          HexToRgb(const char* hexString);

        rgb_color*          text_color[NUM_COLORS];
};
