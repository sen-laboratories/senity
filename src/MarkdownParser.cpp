/*
 * Copyright 2025, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "MarkdownParser.h"
#include <Font.h>
#include <StringView.h>
#include <cstring>

MarkdownParser::MarkdownParser()
    : fDocument(nullptr)
{
}

MarkdownParser::~MarkdownParser()
{
    if (fDocument) {
        cmark_node_free(fDocument);
    }
}

bool MarkdownParser::ParseMarkdown(const char* text)
{
    // Clean up existing document
    if (fDocument) {
        cmark_node_free(fDocument);
        fDocument = nullptr;
    }
    fLineToNode.clear();

    // Parse with GitHub Flavored Markdown extensions
    cmark_parser* parser = cmark_parser_new(CMARK_OPT_DEFAULT);

    // Enable extensions
    cmark_gfm_core_extensions_ensure_registered();
    cmark_syntax_extension* table_ext = cmark_find_syntax_extension("table");
    cmark_syntax_extension* strikethrough_ext = cmark_find_syntax_extension("strikethrough");
    cmark_syntax_extension* tasklist_ext = cmark_find_syntax_extension("tasklist");

    if (table_ext)
        cmark_parser_attach_syntax_extension(parser, table_ext);
    if (strikethrough_ext)
        cmark_parser_attach_syntax_extension(parser, strikethrough_ext);
    if (tasklist_ext)
        cmark_parser_attach_syntax_extension(parser, tasklist_ext);

    // Parse the document
    cmark_parser_feed(parser, text, strlen(text));
    fDocument = cmark_parser_finish(parser);
    cmark_parser_free(parser);

    // Build line index for efficient lookup
    BuildLineIndex();
}

void MarkdownParser::BuildLineIndex()
{
    if (!fDocument) return;

    fLineToNode.clear();

    // Only index block-level nodes (the ones we care about for editing)
    cmark_iter* iter = cmark_iter_new(fDocument);
    cmark_event_type ev_type;

    while ((ev_type = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
        if (ev_type == CMARK_EVENT_ENTER) {
            cmark_node* node = cmark_iter_get_node(iter);

            // Only index block nodes (paragraphs, headings, code blocks, etc.)
            if (cmark_node_is_block(node)) {
                int start_line = cmark_node_get_start_line(node);

                // Only store the start line of each block
                // This is much more memory efficient
                fLineToNode[start_line] = node;
            }
        }
    }

    cmark_iter_free(iter);
}

void MarkdownParser::ParseIncrementalUpdate(int startLine, int endLine, const char* text)
{
    // Find the node(s) that need updating
    cmark_node* affectedNode = GetNodeAtLine(startLine);

    if (!affectedNode) {
        // Fall back to full parse
        ParseMarkdown(text);
        return;
    }

    // Find the parent block node (paragraph, heading, code block, etc.)
    cmark_node* blockNode = affectedNode;
    while (blockNode && !cmark_node_is_block(blockNode)) {
        blockNode = cmark_node_parent(blockNode);
    }

    if (!blockNode) {
        ParseMarkdown(text);
        return;
    }

    // Parse the new fragment
    cmark_parser* parser = cmark_parser_new(CMARK_OPT_DEFAULT);
    cmark_parser_feed(parser, text, strlen(text));
    cmark_node* newFragment = cmark_parser_finish(parser);
    cmark_parser_free(parser);

    // Replace the old node with new content
    cmark_node* parent = cmark_node_parent(blockNode);
    if (parent) {
        cmark_node* newChild = cmark_node_first_child(newFragment);
        if (newChild) {
            cmark_node_insert_before(blockNode, newChild);
            cmark_node_unlink(blockNode);
            cmark_node_free(blockNode);
        }
    }

    cmark_node_free(newFragment);

    // Rebuild line index
    BuildLineIndex();
}

BString MarkdownParser::ExtractTextContent(cmark_node* node) const
{
    BString result;

    cmark_iter* iter = cmark_iter_new(node);
    cmark_event_type ev_type;

    while ((ev_type = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
        cmark_node* cur = cmark_iter_get_node(iter);
        if (cmark_node_get_type(cur) == CMARK_NODE_TEXT) {
            const char* text = cmark_node_get_literal(cur);
            if (text) result << text;
        }
    }

    cmark_iter_free(iter);
    return result;
}

cmark_node* MarkdownParser::GetNodeAtLine(int line) const
{
    if (fLineToNode.empty()) return nullptr;

    // Find the block node that contains this line
    // Since we only indexed start lines, find the closest start line <= target line
    auto it = fLineToNode.upper_bound(line);

    if (it == fLineToNode.begin()) {
        return nullptr;
    }

    --it; // Go to the last node that starts at or before this line
    cmark_node* node = it->second;

    // Verify the line is actually within this node's range
    int start_line = cmark_node_get_start_line(node);
    int end_line = cmark_node_get_end_line(node);

    if (line >= start_line && line <= end_line) {
        return node;
    }

    return nullptr;
}

std::vector<MarkdownParser::OutlineItem> MarkdownParser::GetOutline() const
{
    std::vector<OutlineItem> outline;

    if (!fDocument) return outline;

    cmark_iter* iter = cmark_iter_new(fDocument);
    cmark_event_type ev_type;

    while ((ev_type = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
        if (ev_type == CMARK_EVENT_ENTER) {
            cmark_node* node = cmark_iter_get_node(iter);
            if (cmark_node_get_type(node) == CMARK_NODE_HEADING) {
                OutlineItem item;
                item.level = cmark_node_get_heading_level(node);
                item.text = ExtractTextContent(node);
                item.line = cmark_node_get_start_line(node);
                item.node = node;
                outline.push_back(item);
            }
        }
    }

    cmark_iter_free(iter);
    return outline;
}

