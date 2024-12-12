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
    fParser->flags = MD_DIALECT_GITHUB; //MD_DIALECT_COMMONMARK
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
            mapItem.second->clear();                        // first clear stack
            fTextLookup->markupMap->erase(mapItem.first);   // then remove map item
        }
    }
}

int MarkdownParser::Parse(char* text, int32 size) {
    printf("Markdown parser parsing text of size %d chars\n", size);
    fTextSize = size;
    return md_parse(text, (uint) size, fParser, fTextLookup);
}

void MarkdownParser::InsertTextShiftAt(int32 start, int32 delta) {
    // TODO
}

int32 MarkdownParser::GetTextShiftAt(int32 offset) {
    // TODO
    return 0;
}

markup_map_iter MarkdownParser::GetNearestMarkupMapIter(int32 offset) {
    markup_map_iter lowIter;
    lowIter = fTextLookup->markupMap->lower_bound(offset);

    // not first offset but found element
    if (lowIter != fTextLookup->markupMap->begin() && lowIter != fTextLookup->markupMap->end()) {
        lowIter = std::prev(lowIter);
    }
    return lowIter;
}

markup_stack* MarkdownParser::GetMarkupStackAt(int32 offset, int32* mapOffsetFound) {
    // search markup stack for nearest offset in search direction
    printf("searching nearest markup info stack for offset %d...\n", offset);

    auto low = GetNearestMarkupMapIter(offset);
    printf("found stack at nearest lower offset %d for offset %d\n", low->first, offset);

    if (mapOffsetFound != NULL) {
        *mapOffsetFound = low->first;
    }

    return low->second;
}

outline_map* MarkdownParser::GetOutlineAt(int32 offset) {
    outline_map* outlineElements = new outline_map();

    // final stack holding all outline items
    markup_stack* resultStack = new markup_stack;
    auto mapIter = GetNearestMarkupMapIter(offset);
    if (mapIter == fTextLookup->markupMap->cend()) {
        printf("no text info found for outline!\n");
        return outlineElements;
    }
    bool search = true;

    while (search && mapIter != fTextLookup->markupMap->begin()) {
        auto stack = mapIter->second;
        printf("GetOutlineAt @%d\n", mapIter->first);

        for (auto item : *stack) {
            const char* outlineElement = GetOutlineItemName(item);
            if (outlineElement == NULL) {
                continue;
            }
            if (outlineElements->find(outlineElement) == outlineElements->cend()) {
                if (item->markup_class == MD_BLOCK_BEGIN) {
                    outlineElements->insert({outlineElement, item});
                }
                if (strncmp(outlineElement, "H1", 2) == 0) {
                    printf("top level H1 reached, done.\n");
                    search = false;
                    break;
                }
            } else {
                // check for closed blocks and remove from hierarchical outline
                if (item->markup_class == MD_BLOCK_END) {
                    // remove closed block from outline
                    printf("removing closed block element %s\n", outlineElement);
                    outlineElements->erase(outlineElement);
                }
            }
        }
        mapIter--;
    }
    if (search) {
        printf("Warning: reached start of document without finding proper outline root!\n");
    }
    printf("GetOutlineAt %d: found %zu outline items.\n", offset, resultStack->size());

    return outlineElements;
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
                if (unique) {
                    if (boundaryType == BLOCK) {
                        if (stackItem->markup_class == MD_BLOCK_BEGIN) {
                            blocks.insert({stackItem->markup_type.block_type, stackItem});
                        }
                    } else {
                        if (stackItem->markup_class == MD_SPAN_BEGIN) {
                            spans.insert({stackItem->markup_type.span_type, stackItem});
                        }
                    }
                }
            }
            // not found at stack at this map offset (or followToRoot is true), continue
            mapIter--;
        }
    }
    if (start != NULL)
        *start = startPos;

    int32 endPos = offset;
    blocks.clear();
    spans.clear();

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
                if (unique) {
                    if (boundaryType == BLOCK) {
                        if (stackItem->markup_class == MD_BLOCK_END) {
                            blocks.insert({stackItem->markup_type.block_type, stackItem});
                        }
                    } else {
                        if (stackItem->markup_class == MD_SPAN_END) {
                            spans.insert({stackItem->markup_type.span_type, stackItem});
                        }
                    }
                }

            }
        }
    }
    if (end != NULL)
        *end = endPos;

    printf("GetMarkupBoundary: found %s with offset %d from %d to %d.\n", boundaryType == BLOCK ? "BLOCK" : "SPAN",
        offset, startPos, endPos);

    return resultStack;
}

bool MarkdownParser::FindTextData(const text_data* data, map<MD_BLOCKTYPE, text_data*> blocks, map<MD_SPANTYPE, text_data*> spans) {
    if (data->markup_class == MD_BLOCK_BEGIN || data->markup_class == MD_BLOCK_END) {
        for (auto block : blocks) {
            MD_BLOCKTYPE type = block.first;
            text_data*   item = block.second;

            if (type == data->markup_type.block_type) {
                if ((item->detail != NULL) && (data->detail != NULL)) {
                    if (item->detail->HasSameData(*(data->detail), true, true)) {
                        printf("found already captured block %s.\n", GetBlockTypeName(type));
                        return true;
                    }
                }
            }
        }
    } else if (data->markup_class == MD_SPAN_BEGIN || data->markup_class == MD_SPAN_END) {
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
            BString fence;
            fence << detailData->fence_char;
            detailMsg->AddString("fenceChar", fence.String());
            break;
        }
        case MD_BLOCK_H: {
            auto detailData = reinterpret_cast<MD_BLOCK_H_DETAIL*>(detail);
            detailMsg->AddUInt8("level", detailData->level);
            break;
        }
        case MD_BLOCK_TABLE: {
            auto detailData = reinterpret_cast<MD_BLOCK_TABLE_DETAIL*>(detail);
            detailMsg->AddUInt8("headRowCount", detailData->head_row_count);
            detailMsg->AddUInt8("bodyRowCount", detailData->body_row_count);
            detailMsg->AddUInt8("colCount", detailData->col_count);
            break;
        }
        case MD_BLOCK_TD: {
            auto detailData = reinterpret_cast<MD_BLOCK_TD_DETAIL*>(detail);
            const char* alignment;
            switch(detailData->align) {
                case MD_ALIGN_LEFT:
                    alignment = "left";
                case MD_ALIGN_CENTER:
                    alignment = "center";
                case MD_ALIGN_RIGHT:
                    alignment = "right";
                default:
                    alignment = "default";
            }
            detailMsg->AddString("align", alignment);
            break;
        }
        case MD_BLOCK_OL: {
            auto detailData = reinterpret_cast<MD_BLOCK_OL_DETAIL*>(detail);
            detailMsg->AddUInt8("start", detailData->start);
            detailMsg->AddBool("tight", detailData->is_tight);
            BString delimiter;
            delimiter << detailData->mark_delimiter;
            detailMsg->AddString("delimiter", delimiter.String());
            break;
        }
        case MD_BLOCK_UL: {
            auto detailData = reinterpret_cast<MD_BLOCK_UL_DETAIL*>(detail);
            detailMsg->AddBool("tight", detailData->is_tight);
            BString mark;
            mark << detailData->mark;
            detailMsg->AddString("mark", mark.String());
            break;
        }
        case MD_BLOCK_LI: {
            auto detailData = reinterpret_cast<MD_BLOCK_LI_DETAIL*>(detail);
            detailMsg->AddBool("task", detailData->is_task);
            BString taskMark;
            taskMark << detailData->task_mark;
            detailMsg->AddString("taskMark", taskMark.String());
            detailMsg->AddUInt8("taskMarkOffset", detailData->task_mark_offset);
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
            printf("skipping span type w/o detail: %s.\n", span_type_name[type]);
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

const char* MarkdownParser::GetOutlineItemName(text_data *data){
    if (data->markup_class != MD_BLOCK_BEGIN && data->markup_class != MD_BLOCK_END) {
        return NULL;
    }
    switch (data->markup_type.block_type) {
        case MD_BLOCK_DOC: return "DOC";
        case MD_BLOCK_H: {
            if (data->detail != NULL) {
                uint8 level = data->detail->GetUInt8("level", 1);   // default if level is bogus, should always be there
                BString name("H");
                name << level;

                return (new BString(name))->String();
            }
            return "H?";    // see above, just a failsafe fallback to indicate a bogus header
        }
        case MD_BLOCK_CODE: return "CODE";
        case MD_BLOCK_HTML: return "HTML";
        case MD_BLOCK_P: return "P";
        case MD_BLOCK_QUOTE: return "Q";
        case MD_BLOCK_UL: return "UL";
        case MD_BLOCK_OL: return "OL";
        case MD_BLOCK_TABLE: return "TABLE";
        case MD_BLOCK_THEAD: return "THEAD";
        case MD_BLOCK_TBODY: return "TBODY";
        case MD_BLOCK_TR: return "TR";
        case MD_BLOCK_TH: return "TH";
        case MD_BLOCK_TD: return "TD";
        default:
            return NULL;
    }
}
