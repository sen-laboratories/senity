/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

#include "include/md4c.h"
#include <map>
#include <Message.h>
#include <SupportDefs.h>
#include <vector>

typedef enum MD_CLASS {
    MD_BLOCK_BEGIN = 0,
    MD_BLOCK_END,
    MD_SPAN_BEGIN,
    MD_SPAN_END,
    MD_TEXT
} markup_class;

union markup_type {
    MD_BLOCKTYPE    block_type;
    MD_SPANTYPE     span_type;
    MD_TEXTTYPE     text_type;
};

typedef struct text_data {
    MD_CLASS                markup_class;
    union markup_type       markup_type;
    BMessage               *detail;
    uint                    offset;
    uint                    length;
} text_data;

typedef struct text_info {
    std::map<uint, text_data>   *text_map;          // holds metadata from parsing like blocks and spans
} text_info;

typedef struct markup_stack {
    std::vector<text_data>      *text_stack;
} markup_stack;

class MarkdownStyler {

public:
                        MarkdownStyler();
    virtual             ~MarkdownStyler();
    void                Init();

    int                 MarkupText(char* text, int32 size, text_info* userdata);

    static BMessage*    GetDetailForBlockType(MD_BLOCKTYPE type, void* detail);
    static BMessage*    GetDetailForSpanType(MD_SPANTYPE type, void* detail);

    static const char*  GetBlockTypeName(MD_BLOCKTYPE type);
    static const char*  GetSpanTypeName(MD_SPANTYPE type) ;
    static const char*  GetTextTypeName(MD_TEXTTYPE type);
    static const char*  GetMarkupClassName(MD_CLASS type);

private:
    MD_PARSER*          fParser;
    // callback functions
    static int          EnterBlock(MD_BLOCKTYPE type, MD_OFFSET offset, void* detail, void* userdata);
    static int          LeaveBlock(MD_BLOCKTYPE type, MD_OFFSET offset, void* detail, void* userdata);
    static int          EnterSpan(MD_SPANTYPE type, MD_OFFSET offset, void* detail, void* userdata);
    static int          LeaveSpan(MD_SPANTYPE type, MD_OFFSET offset, void* detail, void* userdata);
    static int          Text(MD_TEXTTYPE type, const MD_CHAR* text, MD_OFFSET offset, MD_SIZE size, void* userdata);
    static void         LogDebug(const char* msg, void* userdata);
    // parsing
    static void         AddMarkupMetadata(MD_CLASS markupClass, MD_BLOCKTYPE blocktype, MD_OFFSET offset, BMessage* detail, void* userdata);
    static void         AddMarkupMetadata(MD_CLASS markupClass, MD_SPANTYPE spantype, MD_OFFSET offset, BMessage* detail, void* userdata);
    static void         AddTextMetadata(text_data* data, void* userdata);
    // helper
    static const char*  attr_to_str(MD_ATTRIBUTE data);
};
