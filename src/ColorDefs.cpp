/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "ColorDefs.h"
#include <cstring>
#include <string>

ColorDefs::ColorDefs()
{
    int color = 0;
    text_color[color++] = HexToRgb("100F0F");
    text_color[color++] = HexToRgb("FFFCF0");
    text_color[color++] = HexToRgb("AD8301");
    text_color[color++] = HexToRgb("BC5215");
    text_color[color++] = HexToRgb("AF3029");
    text_color[color++] = HexToRgb("A02F6F");
    text_color[color++] = HexToRgb("5E409D");
    text_color[color++] = HexToRgb("205EA6");
    text_color[color++] = HexToRgb("24837B");
    text_color[color++] = HexToRgb("66800B");
}

ColorDefs::~ColorDefs()
{
    for (int i = 0; i < NUM_COLORS; i++) {
        if (text_color[i] != NULL) delete text_color[i];
    }
}

rgb_color* ColorDefs::GetColor(COLOR_NAME color)
{
   return text_color[color];
}

rgb_color* ColorDefs::HexToRgb(const char* hexStr)
{
    int len = strlen(hexStr);
    if (len != 6 && len != 8) {   // only allow RGB or RGBA
        printf("illegal argument %s, resorting to default black.\n", hexStr);
        return text_color[BLACK];
    }

    int rgb[4];
    std::string hexString(hexStr);

    for (int pos = 0; pos < len; pos += 2) {
        auto colCompHex = hexString.substr(pos, 2);
        rgb[pos / 2] = std::stoul(colCompHex, nullptr, 16);
    }
    // add default alpha if not provided
    if (len == 6) rgb[3] = 255;

    printf("color for hex %s in RGB is %d, %d, %d\n", hexStr, rgb[0], rgb[1], rgb[2]);
    return new rgb_color(make_color(rgb[0], rgb[1], rgb[2], rgb[3]));
}
