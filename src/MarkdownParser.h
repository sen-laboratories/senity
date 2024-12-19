/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * defines some extensions for integrating MD4C into a simple Markdown editor.
 * see also https://spec.commonmark.org/0.31.2/#appendix-a-parsing-strategy
 */
#pragma once

#include "include/md4c.h"
#include <map>
#include <Message.h>
#include <SupportDefs.h>
#include <vector>

using namespace std;

typedef enum MD_CLASS {
    MD_BLOCK_BEGIN = 0,
    MD_BLOCK_END,
    MD_SPAN_BEGIN,
    MD_SPAN_END,
    MD_TEXT
} markup_class;

typedef struct MD_TYPE {
    MD_BLOCKTYPE    block_type;
    MD_SPANTYPE     span_type;
    MD_TEXTTYPE     text_type;
} markup_type;

/**
 * searches for block boundaries
 */
enum BOUNDARY_TYPE {
    BLOCK = 0,
    SPAN
};

/**
 * search direction for block boundaries
 */
enum SEARCH_DIRECTION {
    BEGIN = 0,  // used for hierarchy outline
    END,        // currently not used
    BOTH        // default
};

typedef struct text_data {
    MD_CLASS        markup_class;
    MD_TYPE         markup_type;
    BMessage        *detail;
    uint            offset;
    uint            length;
} text_data;

// used as temporary processing buffer for styling
typedef struct vector<text_data*>               markup_stack;
typedef map<int32, markup_stack*>               markup_map;
typedef map<int32, markup_stack*>::iterator     markup_map_iter;
typedef map<const char*, text_data*>            outline_map;

/**
 * main structure for integrating markdown parser.
 */
typedef struct text_lookup {
    /**
     * holds markup stacks keyed by text offset, both received from parsing
     */
    map<int32, markup_stack*>   *markupMap;
    /**
     * holds the delta from specific offsets onwards to all subsequent offsets
     * as caused by editing (insert -> shift back, delete -> shift forward).
     * used for efficient recalculation of markup at existing offsets without
     * causing the need to always do a full re-parse.
     */
    map<int32, int32>   *shiftMap;
} text_lookup;

class MarkdownParser {

public:
                        MarkdownParser();
    virtual             ~MarkdownParser();
    void                Init();
    void                ClearTextInfo(int32 start = -1, int32 end = INT32_MAX);

    int                 Parse(char* text, int32 size);
    markup_map*         GetMarkupMap();

    /**
     * looks up nearest position in the text markup map
     */
    markup_map_iter     GetNearestMarkupMapIter(int32 offset);
    /**
     * returns the text metadata stack at or near the given offset and optionally returns the effective offset.
     */
    markup_stack*       GetMarkupStackAt(int32 offset, int32* mapOffsetFound = NULL);
    /**
    * search for block or span boundaries to capture block/span markup info and collect them into text_data stack.
    */
    status_t            GetMarkupBoundariesAt(int32 offset, int32* start, int32* end,
                                         BOUNDARY_TYPE boundaryType = BLOCK,
                                         SEARCH_DIRECTION searchType = BOTH,
                                         markup_stack *resultStack = NULL,
                                         bool unique = false);

    outline_map*        GetOutlineAt(int32 offset);

    static BMessage*    GetDetailForBlockType(MD_BLOCKTYPE type, void* detail);
    static BMessage*    GetDetailForSpanType(MD_SPANTYPE type, void* detail);

    // helper functions for mapping parsing info to human readable form
    static const char*  GetBlockTypeName(MD_BLOCKTYPE type);
    static const char*  GetSpanTypeName(MD_SPANTYPE type) ;
    static const char*  GetTextTypeName(MD_TEXTTYPE type);
    static const char*  GetMarkupClassName(MD_CLASS type);

private:
    MD_PARSER*   fParser;
    /**
     * markup map/stack for quick lookup of markup info at any given offset.
     *
     * Markdown text has block/span markers overlapping with each other andd with text.
     * We want to use the text offset as a top-level key, hence the nested structure:
     *
     * * we keep a map for the offsets (keys are reused for nested blocks/spans/text as they are kept in a stack stored
     * under the offset key).
     * * then, with map::lower_bound and map::upper_bound we can search for the nearest markup info at
     * a given index
     * * we can then simply iterate over the returned stack for styling.
     */
    text_lookup*        fTextLookup;
    int32               fTextSize;
    void                InsertTextShiftAt(int32 start, int32 delta);
    int32               GetTextShiftAt(int32 offset);
    bool                FindTextData(const text_data* data, map<MD_BLOCKTYPE, text_data*> blocks, map<MD_SPANTYPE, text_data*>  spans);

    // callback functions
    static int          EnterBlock(MD_BLOCKTYPE type, MD_OFFSET offset, void* detail, void* userdata);
    static int          LeaveBlock(MD_BLOCKTYPE type, MD_OFFSET offset, void* detail, void* userdata);
    static int          EnterSpan(MD_SPANTYPE type, MD_OFFSET offset, void* detail, void* userdata);
    static int          LeaveSpan(MD_SPANTYPE type, MD_OFFSET offset, void* detail, void* userdata);
    static int          Text(MD_TEXTTYPE type, const MD_CHAR* text, MD_OFFSET offset, MD_SIZE size, void* userdata);
    static void         LogDebug(const char* msg, void* userdata);

    // parsing
    static void         AddMarkupMetadata(MD_CLASS markupClass, MD_BLOCKTYPE blockType, MD_OFFSET offset, BMessage* detail, void* userdata);
    static void         AddMarkupMetadata(MD_CLASS markupClass, MD_SPANTYPE spanType, MD_OFFSET offset, BMessage* detail, void* userdata);
    static void         AddMarkupMetadata(text_data *data, MD_OFFSET offset,void* userdata);

    // helper
    static const char*  attr_to_str(MD_ATTRIBUTE data);
    static const char*  GetOutlineItemName(text_data *data);
};
