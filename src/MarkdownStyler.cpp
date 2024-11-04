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
    fParser->enter_span = &MarkdownStyler::EnterSpan;
    fParser->leave_span = &MarkdownStyler::LeaveSpan;
    fParser->text = &MarkdownStyler::Text;
    fParser->debug_log = &MarkdownStyler::LogDebug;
}

int MarkdownStyler::MarkupText(char* text, int32 size) {
    printf("Markdown parser parsing text of size %d chars\n", size);
    int result = md_parse(text, size, fParser, NULL);   // TODO: pass in editor custom data table!
    printf("Markdown parser returned status %d \n", result);
    return result;
}

// callback functions

int MarkdownStyler::EnterBlock(MD_BLOCKTYPE type, void* detail, void* userdata)
{
    printf("EnterBlock type %d, detail: %s\n", type, (char*) detail);
    return 0;
}

int MarkdownStyler::LeaveBlock(MD_BLOCKTYPE type, void* detail, void* userdata)
{
    printf("LeaveBlock type %d, detail: %s\n", type, (char*) detail);
    return 0;
}

int MarkdownStyler::EnterSpan(MD_SPANTYPE type, void* detail, void* userdata)
{
    printf("EnterSpan type %d, detail: %s\n", type, (char*) detail);
    return 0;
}

int MarkdownStyler::LeaveSpan(MD_SPANTYPE type, void* detail, void* userdata)
{
    printf("LeaveSpan type %d, detail: %s\n", type, (char*) detail);
    return 0;
}

int MarkdownStyler::Text(MD_TEXTTYPE type, const MD_CHAR* text, MD_SIZE size, void* userdata)
{
    printf("Text type %d, text: %s\n", type, BString(text).TruncateChars(32).String());
    return 0;
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
