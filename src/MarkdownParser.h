/*
 * Copyright 2024-2025, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

#include <Message.h>
#include <String.h>
#include <Font.h>
#include <vector>
#include <map>

extern "C" {
    #include <cmark/cmark-gfm.h>
    #include <cmark/cmark-gfm-core-extensions.h>
}

class SyntaxHighlighter;

// Represents a styled region of text
// NOTE: We keep the markdown syntax IN the text, we only add styling
struct StyleRun {
    int32 offset;      // Character offset in original text
    int32 length;      // Length of this styled region

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
        LINK_URL,
        LIST_BULLET,
        LIST_NUMBER,
        BLOCKQUOTE,
        // Syntax highlighting types
        SYNTAX_KEYWORD,
        SYNTAX_TYPE,
        SYNTAX_FUNCTION,
        SYNTAX_STRING,
        SYNTAX_NUMBER,
        SYNTAX_COMMENT,
        SYNTAX_OPERATOR
    } type;

    // Colors for rendering (BFont doesn't hold colors)
    rgb_color foreground;
    rgb_color background;

    // Font information
    BFont font;

    // Metadata
    BString language;  // For code blocks
    int32 listLevel;   // For nested lists
    BString url;       // For links
};

class MarkdownParser {
public:
    MarkdownParser();
    ~MarkdownParser();

    // Main parsing - scans markdown and creates style runs
    // The original text is NOT modified
    bool Parse(const char* markdownText);

    // Get style runs for rendering
    const std::vector<StyleRun>& GetStyleRuns() const { return fStyleRuns; }

    // Get hierarchical outline as BMessage
    const BMessage& GetOutline() const { return fOutline; }

    // Position mapping
    cmark_node* GetNodeAtLine(int line) const;
    cmark_node* GetNodeAtOffset(int32 textOffset) const;
    int32 GetLineForOffset(int32 textOffset) const;

    // Style configuration
    void SetFont(StyleRun::Type type, const BFont& font);
    void SetColor(StyleRun::Type type, rgb_color foreground, rgb_color background = {255, 255, 255, 255});

    // Syntax highlighting
    void SetSyntaxHighlighter(SyntaxHighlighter* highlighter);
    bool IsSyntaxHighlightingEnabled() const { return fSyntaxHighlighter != nullptr; }

    // Clear all state
    void Clear();

private:
    // AST and indices
    cmark_node* fDocument;
    std::map<int, cmark_node*> fLineToNode;
    std::map<int32, cmark_node*> fOffsetToNode;
    std::map<cmark_node*, int32> fNodeToOffset;

    // Output
    std::vector<StyleRun> fStyleRuns;
    BMessage fOutline;

    // Style configuration
    std::map<StyleRun::Type, BFont> fFonts;
    std::map<StyleRun::Type, rgb_color> fForegroundColors;
    std::map<StyleRun::Type, rgb_color> fBackgroundColors;

    // Syntax highlighter (not owned)
    SyntaxHighlighter* fSyntaxHighlighter;

    // Processing methods
    void ProcessNode(cmark_node* node, const char* sourceText);
    void CreateStyleRun(int32 offset, int32 length, StyleRun::Type type,
                       const BString& language = "", const BString& url = "");
    void ApplySyntaxHighlighting(int32 codeOffset, int32 codeLength,
                                 const char* code, const char* language);

    // Indexing
    void BuildLineIndex();
    void BuildOutline();
    void BuildOutlineRecursive(cmark_node* startNode, BMessage& parent, int minLevel);

    // Utilities
    void InitializeDefaultStyles();
    int32 GetNodeStartOffset(cmark_node* node, const char* sourceText) const;
    int32 GetNodeEndOffset(cmark_node* node, const char* sourceText) const;
};
