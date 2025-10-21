/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * defines some extensions for integrating MD4C into a simple Markdown editor.
 * see also https://spec.commonmark.org/0.31.2/#appendix-a-parsing-strategy
 */
#pragma once

#include <String.h>
#include <Font.h>
#include <vector>
#include <map>

extern "C" {
    #include <cmark/cmark-gfm.h>
    #include <cmark/cmark-gfm-core-extensions.h>
}

// Text style run for BTextView
struct TextRun {
    int32 offset;      // Character offset in text
    int32 length;      // Length of this run
    BFont font;
    rgb_color color;

    enum Type {
        NORMAL,
        HEADING_1,
        HEADING_2,
        HEADING_3,
        HEADING_4,
        HEADING_5,
        HEADING_6,
        CODE_INLINE,
        CODE_BLOCK,
        EMPHASIS,
        STRONG,
        LINK,
        LIST_ITEM
    } type;

    // Additional metadata
    BString language;  // For code blocks
    int32 listLevel;   // For nested lists
};

class MarkdownParser {
public:
    MarkdownParser();
    ~MarkdownParser();

    // Main parsing interface
    bool ParseMarkdown(const char* text);
    bool ParseIncrementalUpdate(int startLine, int endLine, const char* text);

    // Get parsed content for rendering
    const std::vector<TextRun>& GetTextRuns() const { return fTextRuns; }

    // Outline structure as BMessage hierarchy
    // Each heading is a BMessage with fields:
    //   "level" (int32) - heading level 1-6
    //   "text" (string) - heading text
    //   "offset" (int32) - text offset
    //   "line" (int32) - source line number
    //   "children" (message) - nested sub-headings
    const BMessage& GetOutline() const { return fOutline; }

    // Position mapping for editor integration
    cmark_node* GetNodeAtLine(int line) const;
    cmark_node* GetNodeAtOffset(int32 textOffset) const;
    void GetNodeBounds(cmark_node* node, int& startLine, int& endLine) const;
    int32 GetTextOffsetForLine(int line) const;

    // Style configuration
    void SetHeadingFont(int level, const BFont& font);
    void SetCodeFont(const BFont& font);
    void SetTextFont(const BFont& font);
    void SetColor(TextRun::Type type, rgb_color color);

private:
    // AST processing
    void BuildPlainTextAndRuns();
    void ProcessNode(cmark_node* node, int32& textOffset);
    void ExtractTextFromNode(cmark_node* node, BString& output, int32& offset);

    // Indexing
    void BuildLineIndex();
    void BuildOutline();

    // Internal state
    cmark_node* fDocument;

    // Output for rendering
    std::vector<TextRun> fTextRuns;
    std::vector<OutlineItem> fOutline;

    // Position mapping
    std::map<int, cmark_node*>   fLineToNode;      // Line -> block node
    std::map<cmark_node*, int32> fNodeToOffset;    // Node -> text offset
    std::map<int32, cmark_node*> fOffsetToNode;    // Text offset -> node

    // Style configuration
    BFont fHeadingFonts[6];
    BFont fCodeFont;
    BFont fTextFont;
    std::map<TextRun::Type, rgb_color> fColors;
};
