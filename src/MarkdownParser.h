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

#include "StyleRun.h"

extern "C" {
#include <tree_sitter/api.h>
}

// Forward declare tree-sitter languages
extern "C" const TSLanguage *tree_sitter_markdown();
extern "C" const TSLanguage *tree_sitter_markdown_inline();

inline TSNode ts_null_node = {0}; // This is a valid null node.

class SyntaxHighlighter;

class MarkdownParser {
public:
    MarkdownParser();
    ~MarkdownParser();

    // Main parsing - creates AST with tree-sitter
    bool Parse(const char* markdownText);

    // TRUE incremental parsing - tell tree-sitter what changed
    bool ParseIncremental(const char* markdownText,
                         int32 editOffset, int32 oldLength, int32 newLength,
                         int32 startLine, int32 startColumn,
                         int32 oldEndLine, int32 oldEndColumn,
                         int32 newEndLine, int32 newEndColumn);

    // Get style runs for rendering
    const std::vector<StyleRun>& GetStyleRuns() const { return fStyleRuns; }

    // Get style runs for a specific range (more efficient for partial updates)
    std::vector<StyleRun> GetStyleRunsInRange(int32 startOffset, int32 endOffset) const;

    // Get hierarchical outline (full document)
    BMessage* GetOutline() { return &fOutline; }

    // Fast outline queries using TreeSitter API directly
    // Get heading at specific offset (returns null node if not a heading)
    TSNode GetHeadingAtOffset(int32 offset) const;
    
    // Find all headings in document (lightweight, returns TSNode array)
    std::vector<TSNode> FindAllHeadings() const;
    
    // Find parent heading for a given offset
    TSNode FindParentHeading(int32 offset) const;
    
    // Find sibling headings at the same level
    std::vector<TSNode> FindSiblingHeadings(TSNode heading) const;
    
    // Build a context trail (breadcrumb) from offset
    void GetHeadingContext(int32 offset, BMessage* context) const;
    
    // Extract heading info from TSNode into BMessage
    void ExtractHeadingInfo(TSNode node, BMessage* msg, bool withText = true) const;
    
    // Get heading level from node
    int GetHeadingLevelFromNode(TSNode node) const;

    // Position queries
    TSNode GetNodeAtOffset(int32 offset) const;
    int32 GetLineForOffset(int32 offset) const;

    // Style configuration
    void SetFont(StyleRun::Type type, const BFont& font);
    void SetColor(StyleRun::Type type, rgb_color foreground, rgb_color background = {255, 255, 255, 255});

    // Syntax highlighting
    void SetSyntaxHighlighter(SyntaxHighlighter* highlighter);

    // Debug output
    void SetDebugEnabled(bool enabled) { fDebugEnabled = enabled; }
    bool IsDebugEnabled() const { return fDebugEnabled; }
    void DumpTree() const;
    void DumpStyleRuns() const;
    void DumpOutline() const;

    // Unicode symbol replacement
    void SetUseUnicodeSymbols(bool use) { fUseUnicodeSymbols = use; }
    bool GetUseUnicodeSymbols() const { return fUseUnicodeSymbols; }

    // Clear all
    void Clear();

private:
    // Tree-sitter state
    TSParser* fParser;
    TSParser* fInlineParser;  // For inline markdown (emphasis, strong, links, etc.)
    TSTree* fTree;
    const char* fSourceText;  // We need to keep a copy for tree-sitter
    char* fSourceCopy;

    // Output
    std::vector<StyleRun> fStyleRuns;
    BMessage              fOutline;

    // Style configuration
    std::map<StyleRun::Type, BFont> fFonts;
    std::map<StyleRun::Type, rgb_color> fForegroundColors;
    std::map<StyleRun::Type, rgb_color> fBackgroundColors;

    // Syntax highlighter (not owned)
    SyntaxHighlighter* fSyntaxHighlighter;

    // Options
    bool fDebugEnabled;
    bool fUseUnicodeSymbols;

    // Processing
    void ProcessNode(TSNode node, int depth = 0);
    void ProcessInlineContent(TSNode node);
    void ProcessInlineNode(TSNode node, int32 baseOffset);
    void CreateStyleRun(int32 offset, int32 length, StyleRun::Type type,
                       const BString& language = "", const BString& url = "",
                       const BString& text = "");
    void ApplySyntaxHighlighting(int32 codeOffset, int32 codeLength,
                                 const char* code, const char* language);

    // Outline building
    void BuildOutline();
    void ProcessNodeForOutline(TSNode node, int32 parentOffset);

    // Utilities
    void InitializeDefaultStyles();
    StyleRun::Type GetStyleTypeForNode(TSNode node) const;
    int GetHeadingLevel(TSNode node) const;
    BString GetNodeText(TSNode node) const;
    void DebugPrintNode(TSNode node, int depth) const;

    // Unicode replacements
    const char* GetListBulletSymbol() const;
    const char* GetTaskCheckedSymbol() const;
    const char* GetTaskUncheckedSymbol() const;
};
