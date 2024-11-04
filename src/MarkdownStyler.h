/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

#include "include/md4c.h"
#include <SupportDefs.h>

class MarkdownStyler {

public:
                        MarkdownStyler();
    virtual             ~MarkdownStyler();
    void                Init();
    int                 MarkupText(char* text, int32 size);

private:
    MD_PARSER*          fParser;
    // callback functions
    static int                 EnterBlock(MD_BLOCKTYPE type, void* detail, void* userdata);
    static int                 LeaveBlock(MD_BLOCKTYPE type, void* detail, void* userdata);
    static int                 EnterSpan(MD_SPANTYPE type, void* detail, void* userdata);
    static int                 LeaveSpan(MD_SPANTYPE type, void* detail, void* userdata);
    static int                 Text(MD_TEXTTYPE type, const MD_CHAR* text, MD_SIZE size, void* userdata);
    static void                LogDebug(const char* msg, void* userdata);
};
