/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "MarkdownStyler.h"

#include <stdio.h>

static const char *markup_class_name[] = {"block", "span", "text"};
static const char *block_type_name[] = {"doc", "block q", "UL", "OL", "LI", "HR", "H", "Code", "HTML",
                                        "para", "table", "THEAD", "TBODY", "TR", "TH", "TD"};
static const char *span_type_name[] = {"em", "strong", "hyperlink", "image", "code", "strike", "LaTeX math",
                                       "latex math disp", "wiki link", "underline"};
static const char *text_type_name[] = {"normal", "NULL char", "hard break", "soft break", "entity",
                                       "code", "HTML", "LaTeX math"};

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

    int result = md_parse(text, size, fParser, userdata);
    printf("Markdown parser returned status %d with %zu text elements.\n", result, userdata->text_map->size());

    return result;
}

// callback functions

int MarkdownStyler::EnterBlock(MD_BLOCKTYPE type, void* detail, void* userdata)
{
    printf("EnterBlock type %s, detail:\n", block_type_name[type]);
    GetDetailForBlockType(type, detail)->PrintToStream();

    AddMarkupMetadata(type, detail, userdata);
    return 0;
}

int MarkdownStyler::LeaveBlock(MD_BLOCKTYPE type, void* detail, void* userdata)
{
    printf("LeaveBlock type %s, detail:\n", block_type_name[type]);
    GetDetailForBlockType(type, detail)->PrintToStream();

    AddMarkupMetadata(type, detail, userdata);
    return 0;
}

int MarkdownStyler::EnterSpan(MD_SPANTYPE type, void* detail, void* userdata)
{
    printf("EnterSpan type %s, detail:\n", span_type_name[type]);
    GetDetailForSpanType(type, detail)->PrintToStream();

    AddMarkupMetadata(type, detail, userdata);
    return 0;
}

int MarkdownStyler::LeaveSpan(MD_SPANTYPE type, void* detail, void* userdata)
{
    printf("LeaveSpan type %s, detail:\n", span_type_name[type]);
    GetDetailForSpanType(type, detail)->PrintToStream();

    AddMarkupMetadata(type, detail, userdata);
    return 0;
}

int MarkdownStyler::Text(MD_TEXTTYPE type, const MD_CHAR* text, MD_OFFSET offset, MD_SIZE size, void* userdata)
{
    text_data* data = new text_data;
    data->markup_class = MARKUP_TEXT;
    data->markup_type.text_type = type;
    data->offset = offset;
    data->length = size;
    data->detail = new char[33];
    MD_SIZE len = min_c(size, 32);
    memcpy(data->detail, text, len);
    ((char*)data->detail)[len] = '\0';

    printf("Text type: %s, offset: %d, length: %d (%d), text: %s\n", text_type_name[type], offset, size, len,
            (char *) data->detail);

    AddTextMetadata(data, userdata);

    return 0;
}

void MarkdownStyler::AddMarkupMetadata(MD_BLOCKTYPE blocktype, void* detail, void* userdata)
{
    text_data* data = new text_data;
    data->markup_class = MARKUP_BLOCK;
    data->markup_type.block_type = blocktype;
    data->detail = detail;
    data->length = 0;   // meta, length is only used/relevant for TEXT

    AddToMarkupStack(data, userdata);
}

void MarkdownStyler::AddMarkupMetadata(MD_SPANTYPE spantype,  void* detail, void* userdata)
{
    text_data* data = new text_data;
    data->markup_class = MARKUP_SPAN;
    data->markup_type.span_type = spantype;
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
    text_info* user_data = reinterpret_cast<text_info*>(userdata);
    // copy over markup stack to text map at offset pos for interpretation (e.g. outline, styling) later
    printf("got markup stack with %zu elements:\n", user_data->markup_stack->size());
    data->markup_stack = new std::vector<text_data>(); //(user_data->markup_stack->size());
    *data->markup_stack = *user_data->markup_stack;
    printf("storing markup stack with %zu elements:\n", data->markup_stack->size());

    user_data->markup_stack->clear();
    user_data->text_map->insert({data->offset, *data});

    // TEST DEBUG
    BMessage *tmp = GetMarkupStack(& user_data->text_map->find(data->offset)->second);
    printf("stored markup stack details:\n");
    tmp->PrintToStream();
    delete tmp;
}

const char* MarkdownStyler::attr_to_str(MD_ATTRIBUTE *data) {
    ulong len = sizeof(data->text);
    if (data->text == NULL || len == 0) return "";

    char *str = new char[len + 1];
    strlcpy(str, data->text, len);
    str[len] = '\0';

    return str;
}

const char* MarkdownStyler::GetBlockTypeName(MD_BLOCKTYPE type) { return block_type_name[type]; }
const char* MarkdownStyler::GetSpanTypeName(MD_SPANTYPE type)   { return span_type_name[type];  }
const char* MarkdownStyler::GetTextTypeName(MD_TEXTTYPE type)   { return text_type_name[type];  }

BMessage* MarkdownStyler::GetDetailForBlockType(MD_BLOCKTYPE type, void* detail) {
    BMessage *detailMsg = new BMessage('Todt');
    if (detail == NULL) return detailMsg;

    switch (type) {
        case MD_BLOCK_CODE: {
            auto detailData = reinterpret_cast<MD_BLOCK_CODE_DETAIL*>(detail);
            detailMsg->AddString("info", attr_to_str(& (detailData->info)));
            detailMsg->AddString("lang", attr_to_str(& (detailData->lang)));
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
    BMessage *detailMsg = new BMessage('Todt');
    if (detail == NULL) return detailMsg;

    switch (type) {
        case MD_SPAN_A: {
            auto detailData = reinterpret_cast<MD_SPAN_A_DETAIL*>(detail);
            detailMsg->AddString("title", attr_to_str(& (detailData->title)));
            detailMsg->AddString("href", attr_to_str(& (detailData->href)));
            detailMsg->AddBool("autoLink", detailData->is_autolink);
            break;
        }
        case MD_SPAN_IMG: {
            auto detailData = reinterpret_cast<MD_SPAN_IMG_DETAIL*>(detail);
            detailMsg->AddString("title", attr_to_str(& (detailData->title)));
            detailMsg->AddString("src", attr_to_str(& (detailData->src)));
            break;
        }
        case MD_SPAN_WIKILINK: {
            auto detailData = reinterpret_cast<MD_SPAN_WIKILINK_DETAIL*>(detail);
            detailMsg->AddString("target", attr_to_str(& (detailData->target)));
            break;
        }
        default: {
            printf("skipping unsupported span type %s.\n", span_type_name[type]);
        }
    }

    return detailMsg;
}

BMessage* MarkdownStyler::GetMarkupStack(text_data* info) {
    printf("got markup stack with %zu items\n", info->markup_stack->size());
    BMessage *outlineMsg = new BMessage('Tout');
    const char *type, *detail;

    for (auto item : *info->markup_stack) {
        switch (item.markup_class) {
            case MARKUP_BLOCK: {
                outlineMsg->AddString("block:type", block_type_name[item.markup_type.block_type]);
                outlineMsg->AddMessage("block:detail", GetDetailForBlockType(item.markup_type.block_type, item.detail));
                break;
            }
            case MARKUP_SPAN: {
                outlineMsg->AddString("span:type", span_type_name[item.markup_type.span_type]);
                outlineMsg->AddMessage("span:detail", GetDetailForSpanType(item.markup_type.span_type, item.detail));
                break;
            }
            case MARKUP_TEXT: {
                outlineMsg->AddString("text:type", text_type_name[item.markup_type.text_type]);
                outlineMsg->AddMessage("text:detail", new BMessage());
                break;
            }
            default: {
                printf("unexpexted markup type %d!\n", item.markup_class);
                continue;
            }
        }
    }
    return outlineMsg;
}

/* Debug callback. Optional (may be NULL).
 *
 * If provided and something goes wrong, this function gets called.
 * This is intended for debugging and problem diagnosis for developers;
 * it is not intended to provide any errors suitable for displaying to an
 * end user.
 */
void MarkdownStyler::LogDebug(const char* msg, void* userdata)
{
    printf("parser error: %s\n", msg);
}
