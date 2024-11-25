/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "MarkdownStyler.h"

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

const char* MarkdownStyler::GetBlockTypeName(MD_BLOCKTYPE type) { return block_type_name[type]; }
const char* MarkdownStyler::GetSpanTypeName(MD_SPANTYPE type)   { return span_type_name[type];  }
const char* MarkdownStyler::GetTextTypeName(MD_TEXTTYPE type)   { return text_type_name[type];  }
const char* MarkdownStyler::GetMarkupClassName(MD_CLASS type)   { return markup_class_name[type];  }

const char* MarkdownStyler::attr_to_str(MD_ATTRIBUTE data) {
    if (data.text == NULL || data.size < 2) return "";
    printf("attr_to_str got text %s with length %u\n", data.text, data.size);
    return (new BString(data.text, data.size))->String();
}

MarkdownStyler::MarkdownStyler()
    : fParser(new MD_PARSER) {
}

MarkdownStyler::~MarkdownStyler() {}

void MarkdownStyler::Init() {
    fParser->abi_version = 0;
    fParser->syntax = 0;
    fParser->flags = MD_DIALECT_GITHUB;
    fParser->enter_block = &MarkdownStyler::EnterBlock;
    fParser->leave_block = &MarkdownStyler::LeaveBlock;
    fParser->enter_span  = &MarkdownStyler::EnterSpan;
    fParser->leave_span  = &MarkdownStyler::LeaveSpan;
    fParser->text        = &MarkdownStyler::Text;
    fParser->debug_log   = &MarkdownStyler::LogDebug;
}

int MarkdownStyler::MarkupText(char* text, int32 size,  text_info* userdata) {
    printf("Markdown parser parsing text of size %d chars\n", size);

    return md_parse(text, (uint) size, fParser, userdata);
}

// callback functions

int MarkdownStyler::EnterBlock(MD_BLOCKTYPE type, MD_OFFSET offset, void* detail, void* userdata)
{
    // ignore document block, not needed and may only cause problems with offset map, esp. on block leave (para+doc)
    if (type == MD_BLOCK_DOC) {
        printf("EnterBlock ignoring type %s, offset: %u\n", block_type_name[type], offset);
        return 0;
    }
    printf("EnterBlock type %s, offset: %u, detail:\n", block_type_name[type], offset);
    BMessage *detailMsg = GetDetailForBlockType(type, detail);

    AddMarkupMetadata(MD_BLOCK_BEGIN, type, offset, detailMsg, userdata);
    return 0;
}

int MarkdownStyler::LeaveBlock(MD_BLOCKTYPE type, MD_OFFSET offset, void* detail, void* userdata)
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

int MarkdownStyler::EnterSpan(MD_SPANTYPE type, MD_OFFSET offset, void* detail, void* userdata)
{
    printf("EnterSpan type %s, offset: %u, detail:\n", span_type_name[type], offset);
    BMessage *detailMsg = GetDetailForSpanType(type, detail);

    AddMarkupMetadata(MD_SPAN_BEGIN, type, offset, detailMsg, userdata);
    return 0;
}

int MarkdownStyler::LeaveSpan(MD_SPANTYPE type, MD_OFFSET offset, void* detail, void* userdata)
{
    printf("LeaveSpan type %s, offset: %u, detail:\n", span_type_name[type], offset);
    BMessage *detailMsg = GetDetailForSpanType(type, detail);

    AddMarkupMetadata(MD_SPAN_END, type, offset, detailMsg, userdata);
    return 0;
}

int MarkdownStyler::Text(MD_TEXTTYPE type, const MD_CHAR* text, MD_OFFSET offset, MD_SIZE size, void* userdata)
{
    text_data* data = new text_data;
    // text is already stored in the document and will be rendered according to block/span markup and MD_TEXTTYPE
    data->markup_class = MD_TEXT;
    data->markup_type.text_type = type;
    data->offset = offset;
    data->length = size;
    data->detail = new BMessage();

    printf("adding TEXT @ %d with len %d\n", data->offset, data->length);
    AddTextMetadata(data, userdata);

    return 0;
}

void MarkdownStyler::AddMarkupMetadata(MD_CLASS markupClass, MD_BLOCKTYPE blocktype, MD_OFFSET offset, BMessage* detail, void* userdata)
{
    text_data* data = new text_data;
    data->markup_class = markupClass;
    data->markup_type.block_type = blocktype;

    AddMarkupMetadata(data, offset, detail, userdata);
}

void MarkdownStyler::AddMarkupMetadata(MD_CLASS markupClass, MD_SPANTYPE spantype, MD_OFFSET offset, BMessage* detail, void* userdata)
{
    text_data* data = new text_data;
    data->markup_class = markupClass;
    data->markup_type.span_type = spantype;

    AddMarkupMetadata(data, offset, detail, userdata);
}

/**
 * store text offset with markup info for later reference (styling, semantics).
 */
void MarkdownStyler::AddMarkupMetadata(text_data *data, MD_OFFSET offset, BMessage* detail, void* userdata)
{
    text_info* text_data = reinterpret_cast<text_info*>(userdata);
    data->offset = offset;
    data->detail = detail;
    data->length = 0;   // meta, length is only used for TEXT

    // offsets must not overlap, markup offset must be unique!
    assert(text_data->markup_map->find(data->offset) == text_data->markup_map->cend());

    text_data->markup_map->insert({data->offset, data});
}

/**
 * store text offset with markup info for later reference (styling, semantics).
 */
void MarkdownStyler::AddTextMetadata(text_data *data, void* userdata)
{
    text_info* text_data = reinterpret_cast<text_info*>(userdata);

    // offsets must not overlap, text offset must be unique!
    assert(text_data->text_map->find(data->offset) == text_data->text_map->cend());

    text_data->text_map->insert({data->offset, data});
}

BMessage* MarkdownStyler::GetDetailForBlockType(MD_BLOCKTYPE type, void* detail) {
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

BMessage* MarkdownStyler::GetDetailForSpanType(MD_SPANTYPE type, void* detail) {
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
void MarkdownStyler::LogDebug(const char* msg, void* userdata)
{
    printf("\e[32m[\e[34mmd4c\e[32m]\e[0m %s\n", msg);
}
