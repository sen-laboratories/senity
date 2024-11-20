/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "MarkdownStyler.h"

#include <String.h>
#include <stdio.h>

static const char *markup_class_name[] = {"block", "span", "text"};
static const char *block_type_name[] = {"doc", "block q", "UL", "OL", "LI", "HR", "H", "Code", "HTML",
                                        "para", "table", "THEAD", "TBODY", "TR", "TH", "TD"};
static const char *span_type_name[] = {"em", "strong", "hyperlink", "image", "code", "strike", "LaTeX math",
                                       "latex math disp", "wiki link", "underline"};
static const char *text_type_name[] = {"normal", "NULL char", "hard break", "soft break", "entity",
                                       "code", "HTML", "LaTeX math"};

const char* MarkdownStyler::GetBlockTypeName(MD_BLOCKTYPE type) { return block_type_name[type]; }
const char* MarkdownStyler::GetSpanTypeName(MD_SPANTYPE type)   { return span_type_name[type];  }
const char* MarkdownStyler::GetTextTypeName(MD_TEXTTYPE type)   { return text_type_name[type];  }
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

    int result = md_parse(text, (uint) size, fParser, userdata);
    printf("Markdown parser returned status %d with %zu text elements.\n", result, userdata->text_map->size());

    return result;
}

// callback functions

int MarkdownStyler::EnterBlock(MD_BLOCKTYPE type, MD_OFFSET offset, void* detail, void* userdata)
{
    printf("EnterBlock type %s, offset: %u, detail:\n", block_type_name[type], offset);
    BMessage *detailMsg = GetDetailForBlockType(type, detail);

    AddMarkupMetadata(type, offset, detailMsg, userdata);
    return 0;
}

int MarkdownStyler::LeaveBlock(MD_BLOCKTYPE type, MD_OFFSET offset, void* detail, void* userdata)
{
    printf("LeaveBlock type %s, offset: %u, detail:\n", block_type_name[type], offset);
    BMessage *detailMsg = GetDetailForBlockType(type, detail);
    detailMsg->PrintToStream();
    //AddMarkupMetadata(type, offset, detailMsg, userdata);
    return 0;
}

int MarkdownStyler::EnterSpan(MD_SPANTYPE type, MD_OFFSET offset, void* detail, void* userdata)
{
    printf("EnterSpan type %s, offset: %u, detail:\n", span_type_name[type], offset);
    BMessage *detailMsg = GetDetailForSpanType(type, detail);

    AddMarkupMetadata(type, offset, detailMsg, userdata);
    return 0;
}

int MarkdownStyler::LeaveSpan(MD_SPANTYPE type, MD_OFFSET offset, void* detail, void* userdata)
{
    printf("LeaveSpan type %s, offset: %u, detail:\n", span_type_name[type], offset);
    BMessage *detailMsg = GetDetailForSpanType(type, detail);
    detailMsg->PrintToStream();
    //AddMarkupMetadata(type, offset, detailMsg, userdata);
    return 0;
}

int MarkdownStyler::Text(MD_TEXTTYPE type, const MD_CHAR* text, MD_OFFSET offset, MD_SIZE size, void* userdata)
{
    printf("MD_TEXT with offset %u, size %u.\n", offset, size);
    text_data* data = new text_data;
    data->markup_class = MARKUP_TEXT;
    data->markup_type.text_type = type;
    data->offset = offset;
    data->length = size;
    data->detail = NULL;    // text is in document and will be rendered according to markup stack and length given here

    AddTextMetadata(data, userdata);

    return 0;
}

void MarkdownStyler::AddMarkupMetadata(MD_BLOCKTYPE blocktype, MD_OFFSET offset, BMessage* detail, void* userdata)
{
    text_data* data = new text_data;
    data->markup_class = MARKUP_BLOCK;
    data->markup_type.block_type = blocktype;
    data->offset = offset;
    data->detail = detail;
    data->length = 0;   // meta, length is only used/relevant for TEXT

    AddToMarkupStack(data, userdata);
}

void MarkdownStyler::AddMarkupMetadata(MD_SPANTYPE spantype, MD_OFFSET offset, BMessage* detail, void* userdata)
{
    text_data* data = new text_data;
    data->markup_class = MARKUP_SPAN;
    data->markup_type.span_type = spantype;
    data->offset = offset;
    data->detail = detail;
    data->length = 0;   // meta, length is only used for TEXT

    AddToMarkupStack(data, userdata);
}

void MarkdownStyler::AddToMarkupStack(text_data *data, void *userdata) {
    text_info* user_data = reinterpret_cast<text_info*>(userdata);
    user_data->markup_stack->push_back(*data);
}

void MarkdownStyler::AddTextMetadata(text_data *data, void* userdata)
{
    printf("AddTextMetaData: offset = %u, length = %u bytes\n", data->offset, data->length);
    text_info* user_data = reinterpret_cast<text_info*>(userdata);

    // copy over markup stack to text map at offset pos for interpretation (e.g. outline, styling) later
    data->markup_stack = new std::vector<text_data>(user_data->markup_stack->size());
    *data->markup_stack = *user_data->markup_stack;
    printf("stored markup stack with %zu elements:\n", data->markup_stack->size());

    // clear stack from block/span parsing for next round
    user_data->markup_stack->clear();
    // store text bounds with markup stack for later reference (styling, semantics)
    user_data->text_map->insert({data->offset, *data});
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
            printf("skipping unsupported block type %s.\n", block_type_name[type]);
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

BMessage* MarkdownStyler::GetOutline(text_data* info, bool names) {
    BMessage *outlineMsg = new BMessage('Tout');

    for (auto item : *info->markup_stack) {
        switch (item.markup_class) {
            case MARKUP_BLOCK: {
                outlineMsg->AddUInt8("block:type", item.markup_type.block_type);
                outlineMsg->AddMessage("block:detail", item.detail != NULL ? item.detail : new BMessage());
                if (names) {
                    outlineMsg->AddString("block:name", block_type_name[item.markup_type.block_type]);
                }
                break;
            }
            case MARKUP_SPAN: {
                outlineMsg->AddUInt8("span:type", item.markup_type.span_type);
                outlineMsg->AddMessage("span:detail", item.detail != NULL ? item.detail : new BMessage());
                if (names) {
                    outlineMsg->AddString("span:name", span_type_name[item.markup_type.span_type]);
                }
                break;
            }
            case MARKUP_TEXT: {
                outlineMsg->AddUInt8("text:type", item.markup_type.text_type);
                outlineMsg->AddMessage("text:detail", item.detail != NULL ? item.detail : new BMessage());
                if (names) {
                    outlineMsg->AddString("text:name", text_type_name[item.markup_type.text_type]);
                }
                break;
            }
            default: {
                printf("unexpexted markup type %d!\n", item.markup_class);
                continue;
            }
        }
    }
    outlineMsg->PrintToStream();
    return outlineMsg;
}

/*
 * Debug callback for MD4C
 */
void MarkdownStyler::LogDebug(const char* msg, void* userdata)
{
    printf("\e[32m[\e[34mmd4c\e[32m]\e[0m %s\n", msg);
}
