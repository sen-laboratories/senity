/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

#include "include/md4c.h"
#include <ObjectList.h>
#include <SupportDefs.h>

typedef enum MARKUP_TYPE {
    MARKUP_BLOCK = 0,
    MARKUP_SPAN,
    MARKUP_TEXT
} markup_type;

typedef struct text_data {
    MARKUP_TYPE     markup_type;
    MD_BLOCKTYPE    block_type;
    MD_SPANTYPE     span_type;
    MD_TEXTTYPE     text_type;
    void*           detail;
    int32           offset;
    int32           length;
} text_data;

class MarkdownStyler {

public:
                        MarkdownStyler();
    virtual             ~MarkdownStyler();
    void                Init();
    int                 MarkupText(char* text, int32 size, BObjectList<text_data>* userdata);

private:
    MD_PARSER*          fParser;
    // callback functions
    static int          EnterBlock(MD_BLOCKTYPE type, void* detail, void* userdata);
    static int          LeaveBlock(MD_BLOCKTYPE type, void* detail, void* userdata);
    static int          EnterSpan(MD_SPANTYPE type, void* detail, void* userdata);
    static int          LeaveSpan(MD_SPANTYPE type, void* detail, void* userdata);
    static int          Text(MD_TEXTTYPE type, const MD_CHAR* text, MD_OFFSET offset, MD_SIZE size, void* userdata);
    static void         LogDebug(const char* msg, void* userdata);

    static void         AddMetadata(MD_BLOCKTYPE blocktype, void* detail, void* userdata);
    static void         AddMetadata(MD_SPANTYPE spantype, void* detail, void* userdata);
    static void         AddMetadata(text_data* data, void* userdata);
};
