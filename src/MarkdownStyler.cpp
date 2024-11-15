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
    printf("MD_TEXT call got offset %u, size %u and text %s\n", offset, size,
        BString(text, size).String());
    text_data* data = new text_data;
    data->markup_class = MARKUP_TEXT;
    data->markup_type.text_type = type;
    data->offset = offset;
    data->length = size;
    data->detail = NULL;

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
    printf("AddTextMetaData: offset = %u, length = %u bytes\n", data->offset, data->length);
    text_info* user_data = reinterpret_cast<text_info*>(userdata);

    // copy over markup stack to text map at offset pos for interpretation (e.g. outline, styling) later
    data->markup_stack = new std::vector<text_data>(user_data->markup_stack->size());
    *data->markup_stack = *user_data->markup_stack;
    printf("stored markup stack with %zu elements:\n", data->markup_stack->size());

    user_data->markup_stack->clear();
    user_data->text_map->insert({data->offset, *data});

    // TEST DEBUG
    BMessage *tmp = GetMarkupStack(& user_data->text_map->find(data->offset)->second);
    printf("stored markup stack details:\n");
    tmp->PrintToStream();
    delete tmp;
}

BMessage* MarkdownStyler::GetDetailForBlockType(MD_BLOCKTYPE type, void* detail) {
    BMessage *detailMsg = new BMessage('Tbdt');
    if (detail == NULL) return detailMsg;

    switch (type) {
        case MD_BLOCK_CODE: {
            auto detailData = reinterpret_cast<MD_BLOCK_CODE_DETAIL*>(detail);
            detailMsg->AddString("info", detailData->info.text);
            detailMsg->AddString("lang", detailData->lang.text);
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
            detailMsg->AddString("title", detailData->title.text);
            detailMsg->AddString("href",  detailData->href.text);
            detailMsg->AddBool("autoLink", detailData->is_autolink);
            break;
        }
        case MD_SPAN_IMG: {
            auto detailData = reinterpret_cast<MD_SPAN_IMG_DETAIL*>(detail);
            printf("adding image detail of size %u\n", detailData->src.size);
            detailMsg->AddString("title", detailData->title.text);
            detailMsg->AddString("src", detailData->src.text);
            break;
        }
        case MD_SPAN_WIKILINK: {
            auto detailData = reinterpret_cast<MD_SPAN_WIKILINK_DETAIL*>(detail);
            printf("adding wiki link detail of size %u\n", detailData->target.size);
            detailMsg->AddString("target", detailData->target.text);
            break;
        }
        default: {
            printf("skipping unsupported/empty span type %s of size %lu: %s.\n",
                    span_type_name[type], sizeof(detail), (char*) detail);
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
