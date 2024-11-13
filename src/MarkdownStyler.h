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

typedef enum MARKUP_CLASS {
    MARKUP_BLOCK = 0,
    MARKUP_SPAN,
    MARKUP_TEXT
} markup_class;

union markup_type {
    MD_BLOCKTYPE    block_type;
    MD_SPANTYPE     span_type;
    MD_TEXTTYPE     text_type;
};

typedef struct text_data {
    MARKUP_CLASS            markup_class;
    union markup_type       markup_type;
    std::vector<text_data> *markup_stack;           // persistent stack for later calculation of style etc.
    void*                   detail;
    int32                   offset;
    int32                   length;
} text_data;

typedef struct text_info {
    std::vector<text_data>      *markup_stack;      // temp stack for keeping track of nested block/span types before TEXT
    std::map<int32, text_data>  *text_map;          // the actual markup info that is visible as text
} text_info;

class MarkdownStyler {

public:
                        MarkdownStyler();
    virtual             ~MarkdownStyler();
    void                Init();

    int                 MarkupText(char* text, int32 size, text_info* userdata);

    static BMessage*    GetMarkupStack(text_data* info);
    static BMessage*    GetDetailForBlockType(MD_BLOCKTYPE type, void* detail);
    static BMessage*    GetDetailForSpanType(MD_SPANTYPE type, void* detail);

    static const char*  GetBlockTypeName(MD_BLOCKTYPE type);
    static const char*  GetSpanTypeName(MD_SPANTYPE type) ;
    static const char*  GetTextTypeName(MD_TEXTTYPE type);

private:
    MD_PARSER*          fParser;
    // callback functions
    static int          EnterBlock(MD_BLOCKTYPE type, void* detail, void* userdata);
    static int          LeaveBlock(MD_BLOCKTYPE type, void* detail, void* userdata);
    static int          EnterSpan(MD_SPANTYPE type, void* detail, void* userdata);
    static int          LeaveSpan(MD_SPANTYPE type, void* detail, void* userdata);
    static int          Text(MD_TEXTTYPE type, const MD_CHAR* text, MD_OFFSET offset, MD_SIZE size, void* userdata);
    static void         LogDebug(const char* msg, void* userdata);
    // parsing
    static void         AddMarkupMetadata(MD_BLOCKTYPE blocktype, void* detail, void* userdata);
    static void         AddMarkupMetadata(MD_SPANTYPE spantype, void* detail, void* userdata);
    static void         AddToMarkupStack(text_data *data, void *userdata);
    static void         AddTextMetadata(text_data* data, void* userdata);
    // helper
    static const char*  attr_to_str(MD_ATTRIBUTE *data);
};
