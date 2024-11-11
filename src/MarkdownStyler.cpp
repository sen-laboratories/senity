/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "MarkdownStyler.h"

#include <stdio.h>
#include <String.h>

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
    printf("EnterBlock type %d, detail: %s\n", type, (char*) detail);
    AddMarkupMetadata(type, detail, userdata);
    return 0;
}

int MarkdownStyler::LeaveBlock(MD_BLOCKTYPE type, void* detail, void* userdata)
{
    printf("LeaveBlock type %d, detail: %s\n", type, (char*) detail);
    AddMarkupMetadata(type, detail, userdata);
    return 0;
}

int MarkdownStyler::EnterSpan(MD_SPANTYPE type, void* detail, void* userdata)
{
    printf("EnterSpan type %d, detail: %s\n", type, (char*) detail);
    AddMarkupMetadata(type, detail, userdata);
    return 0;
}

int MarkdownStyler::LeaveSpan(MD_SPANTYPE type, void* detail, void* userdata)
{
    printf("LeaveSpan type %d, detail: %s\n", type, (char*) detail);
    AddMarkupMetadata(type, detail, userdata);
    return 0;
}

int MarkdownStyler::Text(MD_TEXTTYPE type, const MD_CHAR* text, MD_OFFSET offset, MD_SIZE size, void* userdata)
{
    printf("Text type: %d, offset: %d, length: %d, text: %s\n", type, offset, size,
            BString(text).TruncateChars(32).String());

    text_data* data = new text_data;
    data->markup_class = MARKUP_TEXT;
    data->markup_type.text_type = type;
    data->offset = offset;
    data->length = size;
    data->detail = new char[32];
    strlcpy((char*) data->detail, text, 32);

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
    // copy over markup stack to text offset pos for interpretation (e.g. styling) later
    data->markup_stack = new std::vector<text_data>;
    data->markup_stack = user_data->markup_stack;
    printf("storing markup stack with %zu elements.\n", data->markup_stack->size());

    user_data->markup_stack->clear();
    user_data->text_map->insert({data->offset, *data});
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
