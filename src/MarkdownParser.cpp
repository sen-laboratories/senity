/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "MarkdownParser.h"

#include <String.h>
#include <cassert>
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

MarkdownParser::MarkdownParser()
    : fParser(new MD_PARSER) {

    fTextLookup = new text_lookup;
    fTextLookup->markupMap = new std::map<int32, markup_stack*>;
    fTextLookup->shiftMap = new std::map<int32, int32>;
}

MarkdownParser::~MarkdownParser() {
    ClearTextInfo();
    delete fParser;
}

std::map<int32, markup_stack*>* MarkdownParser::GetMarkupMap() {
    return fTextLookup->markupMap;
}

/*
 * we need a separate Init() function since these methods are not yet
 * available for wiring when the class is being constructed.
 */
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
    if (fTextLookup->markupMap->empty()) {
        return;
    }

    for (auto mapItem : *fTextLookup->markupMap) {
        int32 offset = mapItem.first;
        if (offset >= start && offset <= end) {
            mapItem.second->clear();                // first clear stack
            fTextLookup->markupMap->erase(mapItem.first);   // then remove map item
        }
    }
}

int MarkdownParser::Parse(char* text, int32 size) {
    printf("Markdown parser parsing text of size %d chars\n", size);
    fTextSize = size;
    return md_parse(text, (uint) size, fParser, fTextLookup);
}

void MarkdownParser::InsertTextLookupShiftAt(int32 start, int32 delta) {
    // TODO
}

int32 MarkdownParser::GetTextLookupShiftAt(int32 offset) {
    // TODO
    return 0;
}

markup_stack* MarkdownParser::GetMarkupStackAt(int32 offset, int32* mapOffsetFound) {
    // search markup stack for nearest offset in search direction
    printf("searching nearest markup info stack for offset %d...\n", offset);
    markup_stack* closestStack;

    std::map<int32, markup_stack*>::iterator low;
    low = fTextLookup->markupMap->lower_bound(offset);

    // not first offset but found element
    if (low != fTextLookup->markupMap->begin() && low != fTextLookup->markupMap->end()) {
        low = std::prev(low);
    }
    printf("found stack at nearest lower offset %d for offset %d\n", low->first, offset);

    if (mapOffsetFound != NULL) {
        *mapOffsetFound = low->first;
    }
    closestStack = low->second;

    return closestStack;
}

markup_stack* MarkdownParser::GetOutlineAt(int32 offset) {
    int32 searchOffset = offset, foundOffset;
    // first, trace begin of current span
    markup_stack* stack = GetMarkupBoundariesAt(searchOffset, &foundOffset, NULL, SPAN, BEGIN, true, true);
    // final stack holding all outline items
    markup_stack* resultStack(stack);

    bool topLevelReached = false;

    while (!topLevelReached && foundOffset > 0) {
        searchOffset = foundOffset;
        stack = GetMarkupBoundariesAt(searchOffset, &foundOffset, NULL, BLOCK, BEGIN, true, true);
        // add all outline items found to result stack
        for (auto item : *stack) {
            if (IsOutlineItem(item)) {
                resultStack->push_back(item);
                printf("GetOutline: added %zu items to result which now has %zu items.\n", stack->size(), resultStack->size());
                if (item->markup_type.block_type == MD_BLOCK_H) {
                    if (item->detail != NULL && ! item->detail->IsEmpty()) {
                        uint8 level = item->detail->GetUInt8("level", 99);
                        if (level == 1) {
                            topLevelReached = true;
                            printf("    found top level H1 @%d.\n", foundOffset);
                            break;
                        }
                    }
                }
            }
        }
    }

    return resultStack;
}

// TODO: this is a demi-God method, maybe split search-to-begin and search-to-end
markup_stack* MarkdownParser::GetMarkupBoundariesAt(int32 offset, int32* start, int32* end,
                                              BOUNDARY_TYPE boundaryType,
                                              SEARCH_DIRECTION searchType,
                                              bool returnStack, bool unique) {

    MD_CLASS classToSearch = (boundaryType == BLOCK ? MD_BLOCK_BEGIN : MD_SPAN_BEGIN);
    int32 searchOffset;
    // we just need the offset for starting the search here
    GetMarkupStackAt(offset, &searchOffset);

    markup_stack* resultStack;
    if (returnStack) {
        resultStack = new markup_stack;     // only initialize when needed, caller has to free
    }

    auto mapIter = fTextLookup->markupMap->find(searchOffset);
    if (mapIter == fTextLookup->markupMap->cend()) {
        if (start != NULL)
            *start = -1;
        if (end != NULL)
            *end = -1;
        printf("error: could not find offset %d in lookupMap!\n", searchOffset);
        return NULL;
    }

    int32 startPos = 0;
    bool  search = true;
    // if unique is set, we need to filter elements with the same
    // markup class and type, e.g. sibling spans or similar blocks.
    map<MD_BLOCKTYPE, text_data*> blocks;
    if (unique)
        blocks = map<MD_BLOCKTYPE, text_data*>();

    map<MD_SPANTYPE, text_data*>  spans;
    if (unique)
        spans = map<MD_SPANTYPE, text_data*>();

    if (searchType == BEGIN) {
        // go back through markup map until the stack containing the desired markup class is found
        while (search && mapIter != fTextLookup->markupMap->begin()) {
            auto stack = mapIter->second;
            // now search for desired markup class type in the stack found
            for (auto stackItem : *mapIter->second) {
                if (stackItem->markup_class == classToSearch) {
                    if (returnStack) {
                        if (! unique) {      // add all stack elements unfiltered
                            resultStack->push_back(stackItem);
                        } else {
                            // search for already captured element with similar class, type and detail (e.g. H1 != H2)
                            bool found = FindTextData(stackItem, blocks, spans);
                            if (! found) {   // add if no similar element was found
                                resultStack->push_back(stackItem);
                            }
                        }
                    }
                    startPos = mapIter->first;

                    printf("    markup START boundary search: found markup class %s [%s] at offset %d\n",
                            GetMarkupClassName(classToSearch),
                            (classToSearch == MD_BLOCK_BEGIN ? GetBlockTypeName(stackItem->markup_type.block_type)
                                                             : GetSpanTypeName(stackItem->markup_type.span_type)),
                            startPos);

                    search = false;
                    break;
                }
            }
            // not found at stack at this map offset (or followToRoot is true), continue
            mapIter--;
        }
    }
    if (start != NULL)
        *start = startPos;

    int32 endPos = fTextSize - 1;

    if (searchType == END || searchType == BOTH) {
        printf("GetMarkupRange: searching to the END.\n");
        // now search forward to matching BLOCK_END/SPAN_END from original text offset on
        mapIter = fTextLookup->markupMap->find(searchOffset);
        if (mapIter == fTextLookup->markupMap->cend()) {
            if (end != NULL)
                *end = -1;

            printf("error: could not find offset %d in lookupMap!\n", searchOffset);
            return NULL;
        }
        classToSearch = (boundaryType == BLOCK ? MD_BLOCK_END : MD_SPAN_END);

        search = true;
        while (search && mapIter != fTextLookup->markupMap->end()) {
            mapIter++;

            // process markup stack at next map position
            markup_stack* markupStack = mapIter->second;
            for (auto stackItem : *markupStack) {
                auto markupClass = stackItem->markup_class;
                if (stackItem->markup_class == classToSearch) {
                    if (returnStack) {
                        if (! unique) {      // add all stack elements unfiltered
                            resultStack->push_back(stackItem);
                        } else {
                            // search for already captured element with similar class, type and detail
                            bool found = FindTextData(stackItem, blocks, spans);
                            if (! found) {   // add if no similar element was found
                                printf("    add %s item to stack.\n", GetMarkupClassName(stackItem->markup_class));
                                resultStack->push_back(stackItem);
                            }
                        }
                    }
                    endPos = mapIter->first;
                    printf("    markup END boundary search: found markup class %s at offset %d\n",
                            GetMarkupClassName(classToSearch), endPos);

                    search = false;
                    break;
                }
            }
        }
    }
    if (end != NULL)
        *end = endPos;

    printf("found %s around offset %d from %d to %d.\n", boundaryType == BLOCK ? "BLOCK" : "SPAN",
        offset, startPos, endPos);

    return resultStack;
}

bool MarkdownParser::FindTextData(const text_data* data, map<MD_BLOCKTYPE, text_data*> blocks, map<MD_SPANTYPE, text_data*>  spans) {
    for (auto block : blocks) {
        MD_BLOCKTYPE type = block.first;
        text_data*   item = block.second;
        if (type == data->markup_type.block_type) {
            if ((item->detail != NULL) && (data->detail != NULL)) {
                if (item->detail->HasSameData(*(data->detail), true, true)) {
                    printf("found already captured block %s.\n", GetBlockTypeName(type));
                    return true;
                }
                // HasSameData seems to be buggy for Header H items (level not checked although deep is set)
                if (item->markup_type.block_type == MD_BLOCK_H && data->markup_type.block_type == MD_BLOCK_H) {
                    uint8 levelItem = item->detail->GetUInt8("level", 99);
                    uint8 levelData = data->detail->GetUInt8("level", 99);
                    printf("    H level %d vs %d.\n", levelItem, levelData);
                    if (levelItem == levelData) {
                        printf("    gotcha, bad BMessage.HasSameData() bug!\n");
                        return true;
                    }
                }
            }
        }
    }

    for (auto span : spans) {
        MD_SPANTYPE  type = span.first;
        text_data*   item = span.second;
        if (type == data->markup_type.span_type) {
            if ((item->detail != NULL) && (data->detail != NULL) && item->detail->HasSameData(*(data->detail), true, true)) {
                printf("found already captured span %s.\n", GetSpanTypeName(type));
                return true;
            }
        }
    }
    return false;
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

    auto lookup = reinterpret_cast<text_lookup*>(userdata);
    auto lookupMapIter = lookup->markupMap->find(offset);

    if (lookupMapIter == lookup->markupMap->end()) {
        // add stack elemet to new map
        markup_stack* stack = new markup_stack;
        stack->push_back(data);
        lookup->markupMap->insert({offset, stack});
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

/*
 * helper function to null-terminate strings from parser
 */
const char* MarkdownParser::attr_to_str(MD_ATTRIBUTE data) {
    if (data.text == NULL || data.size == 0) return "";
    printf("attr_to_str got text %s with length %u\n", data.text, data.size);
    return (new BString(data.text, data.size))->String();
}

bool MarkdownParser::IsOutlineItem(text_data *data){
    if (data->markup_class != MD_BLOCK_BEGIN) {
        return false;
    }
    auto type = data->markup_type.block_type;

    return (type == MD_BLOCK_DOC ||
            type == MD_BLOCK_H ||
            type == MD_BLOCK_UL ||
            type == MD_BLOCK_OL ||
            type == MD_BLOCK_TABLE );
}
