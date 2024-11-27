/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "MarkdownParser.h"

#include <String.h>
#include <cassert>
#include <iostream>
#include <stdio.h>

static const char *markup_class_name[] = {"block_begin", "block_end", "span_begin", "span_end", "TEXT"};
static const char *block_type_name[] = {"doc", "bq", "ul", "ol", "li", "hr", "h", "code", "HTML",
                                        "para", "table", "thead", "tbody", "tr", "th", "td"};
static const char *span_type_name[] = {"em", "strong", "hyperlink", "image", "code", "strike", "LaTeX math",
                                       "latex math disp", "wiki link", "underline"};
static const char *text_type_name[] = {"normal", "NULL char", "hard break", "soft break", "entity",
                                       "code", "HTML", "LaTeX math"};

const char* MarkdownParser::GetBlockTypeName(MD_BLOCKTYPE type) { return block_type_name[type]; }
const char* MarkdownParser::GetSpanTypeName(MD_SPANTYPE type)   { return span_type_name[type];  }
const char* MarkdownParser::GetTextTypeName(MD_TEXTTYPE type)   { return text_type_name[type];  }
const char* MarkdownParser::GetMarkupClassName(MD_CLASS type)   { return markup_class_name[type];  }

const char* MarkdownParser::attr_to_str(MD_ATTRIBUTE data) {
    if (data.text == NULL || data.size < 2) return "";
    printf("attr_to_str got text %s with length %u\n", data.text, data.size);
    return (new BString(data.text, data.size))->String();
}

MarkdownParser::MarkdownParser()
    : fParser(new MD_PARSER) {

    fTextLookupMap = new std::map<int32, markup_stack*>;
}

MarkdownParser::~MarkdownParser() {
    ClearTextInfo();
    delete fParser;
}

text_lookup* MarkdownParser::GetTextLookupMap() {
    return fTextLookupMap;
}

void MarkdownParser::Init() {
    fParser->abi_version = 0;
    fParser->syntax = 0;
    fParser->flags = MD_DIALECT_COMMONMARK;
    fParser->enter_block = &MarkdownParser::EnterBlock;
    fParser->leave_block = &MarkdownParser::LeaveBlock;
    fParser->enter_span  = &MarkdownParser::EnterSpan;
    fParser->leave_span  = &MarkdownParser::LeaveSpan;
    fParser->text        = &MarkdownParser::Text;
    fParser->debug_log   = &MarkdownParser::LogDebug;
}

void MarkdownParser::ClearTextInfo(int32 start, int32 end) {
    if (fTextLookupMap->empty()) {
        return;
    }

    for (auto mapItem : *fTextLookupMap) {
        int32 offset = mapItem.first;
        if (offset >= start && offset <= end) {
            mapItem.second->clear();                // first clear stack
            fTextLookupMap->erase(mapItem.first);   // then remove map item
        }
    }
}

int MarkdownParser::Parse(char* text, int32 size) {
    printf("Markdown parser parsing text of size %d chars\n", size);

    return md_parse(text, (uint) size, fParser, fTextLookupMap);
}

markup_stack* MarkdownParser::GetMarkupStackAt(int32 offset, MD_CLASS markupType, int32* mapOffsetFound) {
    // search markup stack for nearest offset in search direction
    printf("searching nearest markup info stack for offset %d...\n", offset);
    markup_stack* closestStack;

    std::map<int32, markup_stack*>::iterator low;
    low = fTextLookupMap->lower_bound(offset);

    // not first offset but found element
    if (low != fTextLookupMap->begin() && low != fTextLookupMap->end()) {
        low = std::prev(low);
    }
    if (mapOffsetFound != NULL) {
        *mapOffsetFound = low->first;
        printf("found stack at nearest lower offset %d for offset %d\n", *mapOffsetFound, offset);
    }
    closestStack = low->second;

    return closestStack;
}

void MarkdownParser::GetMarkupBoundariesAt(int32 offset, BOUNDARY_TYPE boundaryType, int32* start, int32* end) {
    *start = -1;
    *end = -1;
    int32 offsetStart;
    MD_CLASS classToSearch = (boundaryType == BLOCK ? MD_BLOCK_BEGIN : MD_SPAN_BEGIN);
    auto stack = GetMarkupStackAt(offset, classToSearch, &offsetStart);
    if (stack == NULL) {
        return;
    }
    auto mapIter = fTextLookupMap->find(offsetStart);
    assert(mapIter != fTextLookupMap->cend());
    *start = mapIter->first;

    bool found = false;
    // now search foward to matching BLOCK/SPAN_END
    while (!found) {
        auto nextIter = std::next(mapIter);
        if (nextIter == fTextLookupMap->cend()) {
            break;
        }
        // process markup stack at next map position
        markup_stack* markupStack = nextIter->second;
        for (auto item : *markupStack) {
            auto markupClass = item->markup_class;
            switch (boundaryType) {
                case BLOCK:
                    if (markupClass == MD_BLOCK_END) {
                        *end = nextIter->first;
                        found = true;
                    }
                    break;
                case SPAN:
                    if (markupClass == MD_BLOCK_END) {
                        *end = nextIter->first;
                        found = true;
                    }
                    break;
            }
        }
    }
    if (found) {
        printf("found %s around offset %d from %d to %d.\n", boundaryType == BLOCK ? "BLOCK" : "SPAN", offset, *start, *end);
    } else {
        printf("no %s found at offset %d.\n", boundaryType == BLOCK ? "BLOCK" : "SPAN", offset);
    }
}

// callback functions

int MarkdownParser::EnterBlock(MD_BLOCKTYPE type, MD_OFFSET offset, void* detail, void* userdata)
{
    printf("EnterBlock type %s, offset: %u, detail:\n", block_type_name[type], offset);
    BMessage *detailMsg = GetDetailForBlockType(type, detail);

    AddMarkupMetadata(MD_BLOCK_BEGIN, type, offset, detailMsg, userdata);
    return 0;
}

int MarkdownParser::LeaveBlock(MD_BLOCKTYPE type, MD_OFFSET offset, void* detail, void* userdata)
{
    // ignore document boundary, not needed and may only cause problems with offset map, esp. on block leave (para+doc)
    if (type == MD_BLOCK_DOC) {
        printf("LeaveBlock ignoring type %s, offset: %u\n", block_type_name[type], offset);
        return 0;
    }
    printf("LeaveBlock type %s, offset: %u, detail:\n", block_type_name[type], offset);
    BMessage *detailMsg = GetDetailForBlockType(type, detail);

    AddMarkupMetadata(MD_BLOCK_END, type, offset, detailMsg, userdata);
    return 0;
}

int MarkdownParser::EnterSpan(MD_SPANTYPE type, MD_OFFSET offset, void* detail, void* userdata)
{
    printf("EnterSpan type %s, offset: %u, detail:\n", span_type_name[type], offset);
    BMessage *detailMsg = GetDetailForSpanType(type, detail);

    AddMarkupMetadata(MD_SPAN_BEGIN, type, offset, detailMsg, userdata);
    return 0;
}

int MarkdownParser::LeaveSpan(MD_SPANTYPE type, MD_OFFSET offset, void* detail, void* userdata)
{
    printf("LeaveSpan type %s, offset: %u, detail:\n", span_type_name[type], offset);
    BMessage *detailMsg = GetDetailForSpanType(type, detail);

    AddMarkupMetadata(MD_SPAN_END, type, offset, detailMsg, userdata);
    return 0;
}

int MarkdownParser::Text(MD_TEXTTYPE type, const MD_CHAR* text, MD_OFFSET offset, MD_SIZE size, void* userdata)
{
    printf("TEXT @ %d with len %d\n", offset, size);
    text_data* data = new text_data;

    // text is already stored in the document and will be rendered according to block/span markup and MD_TEXTTYPE
    data->markup_class = MD_TEXT;
    data->markup_type.text_type = type;
    data->length = size;
    data->detail = NULL;

    AddMarkupMetadata(data, offset, userdata);

    return 0;
}

void MarkdownParser::AddMarkupMetadata(MD_CLASS markupClass, MD_BLOCKTYPE blockType, MD_OFFSET offset, BMessage* detail, void* userdata)
{
    text_data* data = new text_data;
    data->markup_class = markupClass;
    data->markup_type.block_type = blockType;
    data->detail = detail;

    AddMarkupMetadata(data, offset, userdata);
}

void MarkdownParser::AddMarkupMetadata(MD_CLASS markupClass, MD_SPANTYPE spanType, MD_OFFSET offset, BMessage* detail, void* userdata)
{
    text_data* data = new text_data;
    data->markup_class = markupClass;
    data->markup_type.span_type = spanType;
    data->detail = detail;

    AddMarkupMetadata(data, offset, userdata);
}

/**
 * store text offset with markup info for later reference (styling, semantics).
 */
void MarkdownParser::AddMarkupMetadata(text_data *data, MD_OFFSET offset, void* userdata)
{
    data->offset = offset;

    auto lookupMap = reinterpret_cast<text_lookup*>(userdata);
    auto lookupMapIter = lookupMap->find(offset);

    if (lookupMapIter == lookupMap->end()) {
        // add stack elemet to new map
        markup_stack* stack = new markup_stack;
        stack->push_back(data);
        lookupMap->insert({offset, stack});
    } else {
        lookupMapIter->second->push_back(data);
    }
}

BMessage* MarkdownParser::GetDetailForBlockType(MD_BLOCKTYPE type, void* detail) {
    BMessage *detailMsg = new BMessage('Tbdt');
    if (detail == NULL) return detailMsg;

    switch (type) {
        case MD_BLOCK_CODE: {
            auto detailData = reinterpret_cast<MD_BLOCK_CODE_DETAIL*>(detail);
            detailMsg->AddString("info", attr_to_str(detailData->info));
            detailMsg->AddString("lang", attr_to_str(detailData->lang));
            break;
        }
        case MD_BLOCK_H: {
            auto detailData = reinterpret_cast<MD_BLOCK_H_DETAIL*>(detail);
            detailMsg->AddUInt8("level", detailData->level);
            break;
        }
        default: {
            printf("get detail: skipping unsupported block type %s.\n", block_type_name[type]);
        }
    }

    return detailMsg;
}

BMessage* MarkdownParser::GetDetailForSpanType(MD_SPANTYPE type, void* detail) {
    BMessage *detailMsg = new BMessage('Tsdt');
    if (detail == NULL) return detailMsg;

    switch (type) {
        case MD_SPAN_A: {
            auto detailData = reinterpret_cast<MD_SPAN_A_DETAIL*>(detail);
            detailMsg->AddString("title", attr_to_str(detailData->title));
            detailMsg->AddString("href",  attr_to_str(detailData->href));
            detailMsg->AddBool("autoLink", detailData->is_autolink);
            break;
        }
        case MD_SPAN_WIKILINK: {
            auto detailData = reinterpret_cast<MD_SPAN_WIKILINK_DETAIL*>(detail);
            detailMsg->AddString("target", attr_to_str(detailData->target));
            break;
        }
        case MD_SPAN_IMG: {
            auto detailData = reinterpret_cast<MD_SPAN_IMG_DETAIL*>(detail);
            detailMsg->AddString("title", attr_to_str(detailData->title));
            detailMsg->AddString("src", attr_to_str(detailData->src));
            break;
        }
        default: {
            printf("skipping unsupported/empty span type %s.\n", span_type_name[type]);
        }
    }

    return detailMsg;
}

/*
 * Debug callback for MD4C
 */
void MarkdownParser::LogDebug(const char* msg, void* userdata)
{
    printf("\e[32m[\e[34mmd4c\e[32m]\e[0m %s\n", msg);
}
