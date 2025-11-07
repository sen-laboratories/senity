#include "MarkdownParser.h"
#include "SyntaxHighlighter.h"
#include "../common/Messages.h"

#include <cstring>
#include <algorithm>
#include <stdio.h>

// Unicode symbols for better visual presentation
static const char* UNICODE_BULLET = "•";           // U+2022 BULLET
static const char* UNICODE_CHECKBOX_UNCHECKED = "☐"; // U+2610 BALLOT BOX
static const char* UNICODE_CHECKBOX_CHECKED = "✅";   // U+2611 BALLOT BOX WITH CHECK

MarkdownParser::MarkdownParser()
    : fParser(nullptr)
    , fInlineParser(nullptr)
    , fTree(nullptr)
    , fSourceText(nullptr)
    , fSourceCopy(nullptr)
    , fSyntaxHighlighter(nullptr)
    , fDebugEnabled(true)
    , fUseUnicodeSymbols(true)
{
    // Create tree-sitter parser for block-level markdown
    fParser = ts_parser_new();
    ts_parser_set_language(fParser, tree_sitter_markdown());

    // Create parser for inline markdown (emphasis, strong, links, etc.)
    fInlineParser = ts_parser_new();
    ts_parser_set_language(fInlineParser, tree_sitter_markdown_inline());

    InitializeDefaultStyles();
}

MarkdownParser::~MarkdownParser()
{
    Clear();

    if (fParser) {
        ts_parser_delete(fParser);
    }
    if (fInlineParser) {
        ts_parser_delete(fInlineParser);
    }
}

void MarkdownParser::Clear()
{
    if (fTree) {
        ts_tree_delete(fTree);
        fTree = nullptr;
    }

    if (fSourceCopy) {
        delete[] fSourceCopy;
        fSourceCopy = nullptr;
    }

    fSourceText = nullptr;
    fStyleRuns.clear();
    fOutline.MakeEmpty();
}

void MarkdownParser::InitializeDefaultStyles()
{
    // Default fonts
    BFont plainFont(be_plain_font);
    BFont fixedFont(be_fixed_font);
    BFont boldFont(be_bold_font);

    fFonts[StyleRun::Type::NORMAL] = plainFont;
    fFonts[StyleRun::Type::CODE_INLINE] = fixedFont;
    fFonts[StyleRun::Type::CODE_BLOCK] = fixedFont;
    fFonts[StyleRun::Type::STRONG] = boldFont;

    BFont emphasisFont(plainFont);
    emphasisFont.SetFace(B_ITALIC_FACE);
    fFonts[StyleRun::Type::EMPHASIS] = emphasisFont;

    for (int i = 0; i < 6; i++) {
        BFont headingFont(be_bold_font);
        headingFont.SetSize(24 - i * 2);
        fFonts[(StyleRun::Type)(StyleRun::Type::HEADING_1 + i)] = headingFont;
    }

    // Table styles
    fFonts[StyleRun::Type::TABLE_HEADER] = boldFont;
    fFonts[StyleRun::Type::TABLE_CELL] = plainFont;
    fFonts[StyleRun::Type::TABLE_DELIMITER] = plainFont;
    fFonts[StyleRun::Type::TABLE_ROW_DELIMITER] = plainFont;

    // Default colors
    rgb_color black = {0, 0, 0, 255};
    rgb_color white = {255, 255, 255, 255};
    rgb_color blue = {0, 102, 204, 255};
    rgb_color gray = {60, 60, 60, 255};
    rgb_color lightGray = {245, 245, 245, 255};
    rgb_color borderGray = {180, 180, 180, 255};
    rgb_color delimiterGray = {150, 150, 150, 255};
    rgb_color green = {0, 150, 0, 255};
    rgb_color purple = {128, 0, 128, 255};
    rgb_color teal = {0, 128, 128, 255};
    rgb_color orange = {255, 102, 0, 255};

    fForegroundColors[StyleRun::Type::NORMAL] = black;
    fForegroundColors[StyleRun::Type::CODE_INLINE] = gray;
    fForegroundColors[StyleRun::Type::CODE_BLOCK] = black;
    fForegroundColors[StyleRun::Type::LINK] = blue;
    fForegroundColors[StyleRun::Type::LIST_BULLET] = gray;
    fForegroundColors[StyleRun::Type::LIST_NUMBER] = gray;
    fForegroundColors[StyleRun::Type::TASK_MARKER_UNCHECKED] = gray;
    fForegroundColors[StyleRun::Type::TASK_MARKER_CHECKED] = green;
    fForegroundColors[StyleRun::Type::TABLE_HEADER] = black;
    fForegroundColors[StyleRun::Type::TABLE_CELL] = black;
    fForegroundColors[StyleRun::Type::TABLE_DELIMITER] = borderGray;
    fForegroundColors[StyleRun::Type::TABLE_ROW_DELIMITER] = delimiterGray;

    // Syntax highlighting colors (matching SyntaxHighlighter defaults)
    fForegroundColors[StyleRun::Type::SYNTAX_KEYWORD] = blue;
    fForegroundColors[StyleRun::Type::SYNTAX_TYPE] = teal;
    fForegroundColors[StyleRun::Type::SYNTAX_FUNCTION] = purple;
    fForegroundColors[StyleRun::Type::SYNTAX_STRING] = green;
    fForegroundColors[StyleRun::Type::SYNTAX_NUMBER] = orange;
    fForegroundColors[StyleRun::Type::SYNTAX_COMMENT] = gray;
    fForegroundColors[StyleRun::Type::SYNTAX_OPERATOR] = black;

    fBackgroundColors[StyleRun::Type::NORMAL] = white;
    fBackgroundColors[StyleRun::Type::CODE_INLINE] = lightGray;
    fBackgroundColors[StyleRun::Type::CODE_BLOCK] = lightGray;
    fBackgroundColors[StyleRun::Type::TABLE_HEADER] = white;
    fBackgroundColors[StyleRun::Type::TABLE_CELL] = white;
    fBackgroundColors[StyleRun::Type::TABLE_DELIMITER] = white;
    fBackgroundColors[StyleRun::Type::TABLE_ROW_DELIMITER] = white;
}

bool MarkdownParser::Parse(const char* markdownText)
{
    if (!markdownText || !fParser) {
        if (fDebugEnabled) {
            printf("MarkdownParser::Parse - Invalid input or parser not initialized\n");
        }
        return false;
    }

    Clear();

    // Keep a copy of the source (tree-sitter needs it)
    size_t len = strlen(markdownText);
    fSourceCopy = new char[len + 1];
    strcpy(fSourceCopy, markdownText);
    fSourceText = fSourceCopy;

    if (fDebugEnabled) {
        printf("\n=== Parsing Markdown (%zu bytes) ===\n", len);
    }

    // Parse with tree-sitter
    fTree = ts_parser_parse_string(fParser, nullptr, fSourceText, len);

    if (!fTree) {
        if (fDebugEnabled) {
            printf("MarkdownParser::Parse - Failed to create parse tree\n");
        }
        return false;
    }

    if (fDebugEnabled) {
        DumpTree();
    }

    // Process the tree
    TSNode root = ts_tree_root_node(fTree);
    ProcessNode(root, 0);

    BuildOutline();

    if (fDebugEnabled) {
        DumpStyleRuns();
        DumpOutline();
    }

    return true;
}

bool MarkdownParser::ParseIncremental(const char* markdownText,
                                      int32 editOffset, int32 oldLength, int32 newLength,
                                      int32 startLine, int32 startColumn,
                                      int32 oldEndLine, int32 oldEndColumn,
                                      int32 newEndLine, int32 newEndColumn)
{
    if (!markdownText || !fParser || !fTree) {
        // No previous tree, do full parse
        if (fDebugEnabled) {
            printf("MarkdownParser::ParseIncremental - No previous tree, doing full parse\n");
        }
        return Parse(markdownText);
    }

    if (fDebugEnabled) {
        printf("\n=== Incremental Parse ===\n");
        printf("Edit: offset=%d, oldLen=%d, newLen=%d\n", editOffset, oldLength, newLength);
        printf("Start: line=%d, col=%d\n", startLine, startColumn);
        printf("OldEnd: line=%d, col=%d\n", oldEndLine, oldEndColumn);
        printf("NewEnd: line=%d, col=%d\n", newEndLine, newEndColumn);
    }

    // Create edit descriptor for tree-sitter
    TSInputEdit edit;
    edit.start_byte = editOffset;
    edit.old_end_byte = editOffset + oldLength;
    edit.new_end_byte = editOffset + newLength;

    // Use provided line and column counts directly
    edit.start_point = {(uint32_t)startLine, (uint32_t)startColumn};
    edit.old_end_point = {(uint32_t)oldEndLine, (uint32_t)oldEndColumn};
    edit.new_end_point = {(uint32_t)newEndLine, (uint32_t)newEndColumn};

    // Tell tree-sitter about the edit
    ts_tree_edit(fTree, &edit);

    // Update source text
    if (fSourceCopy) {
        delete[] fSourceCopy;
    }

    size_t len = strlen(markdownText);
    fSourceCopy = new char[len + 1];
    strcpy(fSourceCopy, markdownText);
    fSourceText = fSourceCopy;

    // Incrementally re-parse
    TSTree* newTree = ts_parser_parse_string(fParser, fTree, fSourceText, len);

    if (!newTree) {
        if (fDebugEnabled) {
            printf("MarkdownParser::ParseIncremental - Failed to create new tree\n");
        }
        return false;
    }

    // Check what changed
    if (fDebugEnabled) {
        TSRange* ranges;
        uint32_t range_count;
        ranges = ts_tree_get_changed_ranges(fTree, newTree, &range_count);
        printf("Changed ranges: %u\n", range_count);
        for (uint32_t i = 0; i < range_count; i++) {
            printf("  Range %u: [%u, %u)\n", i,
                   ranges[i].start_byte, ranges[i].end_byte);
        }
        free(ranges);
    }

    // Replace old tree
    ts_tree_delete(fTree);
    fTree = newTree;

    // Re-process (tree-sitter did the smart caching for us!)
    fStyleRuns.clear();
    TSNode root = ts_tree_root_node(fTree);
    ProcessNode(root, 0);

    // Rebuild outline
    BuildOutline();

    if (fDebugEnabled) {
        DumpStyleRuns();
    }

    return true;
}

void MarkdownParser::ProcessNode(TSNode node, int depth)
{
    if (fDebugEnabled && depth == 0) {
        printf("\n=== Processing Parse Tree ===\n");
    }

    const char* nodeType = ts_node_type(node);

    // Debug output
    if (fDebugEnabled) {
        DebugPrintNode(node, depth);
    }

    // Handle pipe characters (they're unnamed nodes)
    if (strcmp(nodeType, "|") == 0) {
        uint32_t startByte = ts_node_start_byte(node);
        uint32_t endByte = ts_node_end_byte(node);
        CreateStyleRun(startByte, endByte - startByte, StyleRun::Type::TABLE_DELIMITER);
        return;
    }

    // Only process named nodes for styling
    if (!ts_node_is_named(node)) {
        return;
    }

    uint32_t startByte = ts_node_start_byte(node);
    uint32_t endByte = ts_node_end_byte(node);
    int32 length = endByte - startByte;

    // Handle task list markers specially
    if (strcmp(nodeType, "task_list_marker_unchecked") == 0) {
        if (fUseUnicodeSymbols) {
            CreateStyleRun(startByte, length, StyleRun::Type::TASK_MARKER_UNCHECKED,
                          "", "", UNICODE_CHECKBOX_UNCHECKED);
        } else {
            CreateStyleRun(startByte, length, StyleRun::Type::TASK_MARKER_UNCHECKED);
        }
        return; // Don't recurse into task markers
    }

    if (strcmp(nodeType, "task_list_marker_checked") == 0) {
        if (fUseUnicodeSymbols) {
            CreateStyleRun(startByte, length, StyleRun::Type::TASK_MARKER_CHECKED,
                          "", "", UNICODE_CHECKBOX_CHECKED);
        } else {
            CreateStyleRun(startByte, length, StyleRun::Type::TASK_MARKER_CHECKED);
        }
        return;
    }

    // Handle list markers
    if (strcmp(nodeType, "list_marker_minus") == 0 ||
        strcmp(nodeType, "list_marker_plus") == 0 ||
        strcmp(nodeType, "list_marker_star") == 0) {
        if (fUseUnicodeSymbols) {
            CreateStyleRun(startByte, length, StyleRun::Type::LIST_BULLET,
                          "", "", UNICODE_BULLET);
        } else {
            CreateStyleRun(startByte, length, StyleRun::Type::LIST_BULLET);
        }
        return;
    }

    // Handle table delimiter row
    if (strcmp(nodeType, "pipe_table_delimiter_row") == 0) {
        // Style the entire delimiter row (|---|---|) separately from individual pipes
        CreateStyleRun(startByte, length, StyleRun::Type::TABLE_ROW_DELIMITER);
        return; // Don't recurse into delimiter row
    }

    // Determine style based on node type
    StyleRun::Type styleType = GetStyleTypeForNode(node);

    if (fDebugEnabled && (strcmp(nodeType, "pipe_table_cell") == 0)) {
        printf("  Cell [%u,%u) styleType=%d (%s)\n", startByte, endByte,
               styleType, styleType == StyleRun::Type::TABLE_HEADER ? "TABLE_HEADER" :
                         styleType == StyleRun::Type::TABLE_CELL ? "TABLE_CELL" : "OTHER");
    }

    if (styleType != StyleRun::Type::NORMAL) {
        // Extract additional info for special nodes
        BString language;
        BString url;

        if (styleType == StyleRun::Type::CODE_BLOCK) {
            // Try to get language from info_string node
            TSNode infoNode = ts_node_child_by_field_name(node, "info_string", 11);
            if (!ts_node_is_null(infoNode)) {
                language = GetNodeText(infoNode);
                language.Trim();
            }
        } else if (styleType == StyleRun::Type::LINK) {
            // Try to get URL from link_destination node
            TSNode destNode = ts_node_child_by_field_name(node, "link_destination", 16);
            if (!ts_node_is_null(destNode)) {
                url = GetNodeText(destNode);
            }
        }

        CreateStyleRun(startByte, length, styleType, language, url);

        // Special handling for code blocks with syntax highlighting
        if (styleType == StyleRun::Type::CODE_BLOCK && fSyntaxHighlighter && !language.IsEmpty()) {
            const char* code = fSourceText + startByte;
            ApplySyntaxHighlighting(startByte, length, code, language.String());
        }
    }

    // Special handling for inline content (paragraph, heading_content, etc.)
    // BUT NOT for table cells - they need special handling to preserve table styling
    if (strcmp(nodeType, "inline") == 0 || strcmp(nodeType, "paragraph") == 0) {
        // Parse inline markdown within this node
        ProcessInlineContent(node);
    } else if (strcmp(nodeType, "pipe_table_cell") == 0) {
        // For table cells, process inline content but the cell itself already has styling
        // We process children manually to handle inline formatting within cells
        uint32_t childCount = ts_node_child_count(node);
        for (uint32_t i = 0; i < childCount; i++) {
            TSNode child = ts_node_child(node, i);
            const char* childType = ts_node_type(child);

            // Only process inline nodes within cells
            if (strcmp(childType, "inline") == 0) {
                ProcessInlineContent(child);
            }
        }
        return; // Don't recurse further, we handled children manually
    }

    // Recursively process children
    uint32_t childCount = ts_node_child_count(node);
    for (uint32_t i = 0; i < childCount; i++) {
        TSNode child = ts_node_child(node, i);
        ProcessNode(child, depth + 1);
    }
}

void MarkdownParser::ProcessInlineContent(TSNode node)
{
    uint32_t startByte = ts_node_start_byte(node);
    uint32_t endByte = ts_node_end_byte(node);
    int32 length = endByte - startByte;

    if (length <= 0 || !fSourceText || !fInlineParser) return;

    // Parse inline content with inline parser
    const char* inlineText = fSourceText + startByte;
    TSTree* inlineTree = ts_parser_parse_string(fInlineParser, nullptr, inlineText, length);

    if (!inlineTree) return;

    // Process inline tree to find emphasis, strong, links, etc.
    TSNode inlineRoot = ts_tree_root_node(inlineTree);
    ProcessInlineNode(inlineRoot, startByte);

    ts_tree_delete(inlineTree);
}

void MarkdownParser::ProcessInlineNode(TSNode node, int32 baseOffset)
{
    const char* nodeType = ts_node_type(node);

    if (!ts_node_is_named(node)) {
        return;
    }

    uint32_t startByte = ts_node_start_byte(node);
    uint32_t endByte = ts_node_end_byte(node);
    int32 length = endByte - startByte;
    int32 absoluteOffset = baseOffset + startByte;

    // Detect inline formatting
    StyleRun::Type styleType = StyleRun::Type::NORMAL;

    if (strcmp(nodeType, "strong_emphasis") == 0) {
        styleType = StyleRun::Type::STRONG;
    } else if (strcmp(nodeType, "emphasis") == 0) {
        styleType = StyleRun::Type::EMPHASIS;
    } else if (strcmp(nodeType, "code_span") == 0) {
        styleType = StyleRun::Type::CODE_INLINE;
    } else if (strcmp(nodeType, "inline_link") == 0 || strcmp(nodeType, "shortcut_link") == 0) {
        styleType = StyleRun::Type::LINK;
    }

    if (styleType != StyleRun::Type::NORMAL) {
        CreateStyleRun(absoluteOffset, length, styleType);
    }

    // Recurse to children
    uint32_t childCount = ts_node_child_count(node);
    for (uint32_t i = 0; i < childCount; i++) {
        TSNode child = ts_node_child(node, i);
        ProcessInlineNode(child, baseOffset);
    }
}

StyleRun::Type MarkdownParser::GetStyleTypeForNode(TSNode node) const
{
    const char* type = ts_node_type(node);

    // Headings
    if (strcmp(type, "atx_heading") == 0) {
        int level = GetHeadingLevel(node);
        return (StyleRun::Type)(StyleRun::Type::HEADING_1 + level - 1);
    }

    // Code
    if (strcmp(type, "fenced_code_block") == 0 || strcmp(type, "indented_code_block") == 0) {
        return StyleRun::Type::CODE_BLOCK;
    }
    if (strcmp(type, "code_span") == 0) {
        return StyleRun::Type::CODE_INLINE;
    }

    // Emphasis
    if (strcmp(type, "emphasis") == 0) {
        return StyleRun::Type::EMPHASIS;
    }
    if (strcmp(type, "strong_emphasis") == 0) {
        return StyleRun::Type::STRONG;
    }

    // Links
    if (strcmp(type, "inline_link") == 0 || strcmp(type, "shortcut_link") == 0) {
        return StyleRun::Type::LINK;
    }

    // Blockquote
    if (strcmp(type, "block_quote") == 0) {
        return StyleRun::Type::BLOCKQUOTE;
    }

    // Tables - only style cells and delimiters, not container nodes
    // The pipe_table_header and pipe_table_row are just grouping containers
    // and should return NORMAL to avoid creating overlapping style runs
    if (strcmp(type, "pipe_table_cell") == 0) {
        // Check if this cell is in a header row by looking at parent
        TSNode parent = ts_node_parent(node);
        const char* parentType = ts_node_type(parent);
        if (strcmp(parentType, "pipe_table_header") == 0) {
            return StyleRun::Type::TABLE_HEADER;
        }
        return StyleRun::Type::TABLE_CELL;
    }

    // List markers handled in ProcessNode

    return StyleRun::Type::NORMAL;
}

int MarkdownParser::GetHeadingLevel(TSNode node) const
{
    // Get the heading marker to determine level
    uint32_t childCount = ts_node_child_count(node);
    for (uint32_t i = 0; i < childCount; i++) {
        TSNode child = ts_node_child(node, i);
        const char* childType = ts_node_type(child);

        // The marker tells us the level
        if (strncmp(childType, "atx_h", 5) == 0) {
            // Extract level from "atx_h1_marker", "atx_h2_marker", etc.
            if (strlen(childType) >= 6 && childType[5] >= '1' && childType[5] <= '6') {
                return childType[5] - '0';
            }
        }
    }

    return 1;  // Default
}

BString MarkdownParser::GetNodeText(TSNode node) const
{
    if (ts_node_is_null(node) || !fSourceText) {
        return BString();
    }

    uint32_t start = ts_node_start_byte(node);
    uint32_t end = ts_node_end_byte(node);

    if (end <= start) {
        return BString();
    }

    return BString(fSourceText + start, end - start);
}

void MarkdownParser::CreateStyleRun(int32 offset, int32 length, StyleRun::Type type,
                                   const BString& language, const BString& url,
                                   const BString& text)
{
    if (offset < 0 || length <= 0) return;

    StyleRun run;
    run.offset = offset;
    run.length = length;
    run.type = type;

    auto fontIt = fFonts.find(type);
    run.font = (fontIt != fFonts.end()) ? fontIt->second : *be_plain_font;

    auto fgIt = fForegroundColors.find(type);
    run.foreground = (fgIt != fForegroundColors.end()) ? fgIt->second : rgb_color{0, 0, 0, 255};

    auto bgIt = fBackgroundColors.find(type);
    run.background = (bgIt != fBackgroundColors.end()) ? bgIt->second : rgb_color{255, 255, 255, 255};

    run.language = language;
    run.url = url;
    run.text = text;

    fStyleRuns.push_back(run);
}

void MarkdownParser::ProcessNodeForOutline(TSNode node, int32 parentOffset)
{
    const char* nodeType = ts_node_type(node);

    // Check if this is a heading
    if (strcmp(nodeType, "atx_heading") == 0) {
        int level = GetHeadingLevel(node);
        int32 currentOffset = ts_node_start_byte(node);

        // Extract heading text using field name (heading_content)
        BString text;
        TSNode contentNode = ts_node_child_by_field_name(node, "heading_content", 15);
        if (!ts_node_is_null(contentNode)) {
            text = GetNodeText(contentNode);
            text.Trim();
        } else {
            text = "unknown heading";
        }

        if (fDebugEnabled) {
            printf("Found heading L%d at %u: '%s' (parent: %d)\n", level,
                   currentOffset, text.String(), parentOffset);
        }

        BMessage heading;
        heading.AddString("text", text);
        heading.AddInt32("level", level);
        heading.AddInt32("offset", currentOffset);
        heading.AddInt32("length", ts_node_end_byte(node) - ts_node_start_byte(node));
        heading.AddInt32("parent_offset", parentOffset);

        fOutline.AddMessage("heading", &heading);

        // This heading becomes parent for its descendants
        parentOffset = currentOffset;
    }

    // Recurse into all children, passing current parent
    uint32_t childCount = ts_node_child_count(node);
    for (uint32_t i = 0; i < childCount; i++) {
        ProcessNodeForOutline(ts_node_child(node, i), parentOffset);
    }
}

void MarkdownParser::BuildOutline()
{
    fOutline.MakeEmpty();
    fOutline.what = MSG_OUTLINE;
    fOutline.AddString("type", "document");  // Type field for flavor

    if (!fTree) {
        if (fDebugEnabled) {
            printf("BuildOutline - No tree available\n");
        }
        return;
    }

    TSNode root = ts_tree_root_node(fTree);
    ProcessNodeForOutline(root, -1);  // Start with no parent (-1)
}

TSNode MarkdownParser::GetNodeAtOffset(int32 offset) const
{
    if (!fTree) {
        return ts_null_node;
    }

    TSNode root = ts_tree_root_node(fTree);
    return ts_node_descendant_for_byte_range(root, offset, offset);
}

int32 MarkdownParser::GetLineForOffset(int32 offset) const
{
    if (!fSourceText || offset < 0) return 1;

    // Use memchr for fast newline counting
    int32 line = 1;
    const char* p = fSourceText;
    const char* end = fSourceText + offset;

    while (p < end) {
        const char* newline = (const char*)memchr(p, '\n', end - p);
        if (newline) {
            line++;
            p = newline + 1;
        } else {
            break;
        }
    }

    return line;
}

// Fast outline query methods using TreeSitter API directly

TSNode MarkdownParser::GetHeadingAtOffset(int32 offset) const
{
    if (!fTree) {
        return ts_null_node;
    }

    TSNode node = GetNodeAtOffset(offset);

    // Walk up tree to find heading node
    while (!ts_node_is_null(node)) {
        const char* nodeType = ts_node_type(node);
        if (strcmp(nodeType, "atx_heading") == 0) {
            return node;
        }
        node = ts_node_parent(node);
    }

    return ts_null_node;
}

std::vector<TSNode> MarkdownParser::FindAllHeadings() const
{
    std::vector<TSNode> headings;

    if (!fTree) {
        return headings;
    }

    TSNode root = ts_tree_root_node(fTree);

    // Use tree-sitter's cursor for efficient traversal
    TSTreeCursor cursor = ts_tree_cursor_new(root);

    bool descend = true;
    while (true) {
        if (descend) {
            TSNode node = ts_tree_cursor_current_node(&cursor);
            const char* nodeType = ts_node_type(node);

            if (strcmp(nodeType, "atx_heading") == 0) {
                headings.push_back(node);
            }

            // Try to go to first child
            if (ts_tree_cursor_goto_first_child(&cursor)) {
                descend = true;
                continue;
            }
        }

        // Try to go to next sibling
        if (ts_tree_cursor_goto_next_sibling(&cursor)) {
            descend = true;
            continue;
        }

        // Go up and try next sibling
        if (!ts_tree_cursor_goto_parent(&cursor)) {
            break;
        }
        descend = false;
    }

    ts_tree_cursor_delete(&cursor);
    return headings;
}

TSNode MarkdownParser::FindParentHeading(int32 offset) const
{
    if (!fTree) {
        return ts_null_node;
    }

    // Get all headings
    std::vector<TSNode> headings = FindAllHeadings();

    // Find the current heading at offset (if any)
    TSNode currentHeading = GetHeadingAtOffset(offset);
    int32 currentLevel = ts_node_is_null(currentHeading)
        ? 999
        : GetHeadingLevelFromNode(currentHeading);
    int32 currentOffset = ts_node_is_null(currentHeading)
        ? offset
        : ts_node_start_byte(currentHeading);

    // Walk backwards to find parent (heading with lower level before current)
    for (auto it = headings.rbegin(); it != headings.rend(); ++it) {
        TSNode heading = *it;
        int32 headingOffset = ts_node_start_byte(heading);

        // Must be before current position
        if (headingOffset >= currentOffset) {
            continue;
        }

        int32 level = GetHeadingLevelFromNode(heading);
        if (level < currentLevel) {
            return heading;
        }
    }

    return ts_null_node;
}

std::vector<TSNode> MarkdownParser::FindSiblingHeadings(TSNode heading) const
{
    std::vector<TSNode> siblings;

    if (!fTree || ts_node_is_null(heading)) {
        return siblings;
    }

    int32 targetLevel = GetHeadingLevelFromNode(heading);
    int32 headingOffset = ts_node_start_byte(heading);

    // Get all headings
    std::vector<TSNode> allHeadings = FindAllHeadings();

    // Find parent heading
    TSNode parent = FindParentHeading(headingOffset);
    int32 parentOffset = ts_node_is_null(parent)
        ? -1
        : ts_node_start_byte(parent);

    // Find next heading at parent level or higher (defines scope)
    int32 endOffset = INT32_MAX;
    if (parentOffset >= 0) {
        int32 parentLevel = GetHeadingLevelFromNode(parent);
        for (const TSNode& node : allHeadings) {
            int32 nodeOffset = ts_node_start_byte(node);
            if (nodeOffset > headingOffset) {
                int32 level = GetHeadingLevelFromNode(node);
                if (level <= parentLevel) {
                    endOffset = nodeOffset;
                    break;
                }
            }
        }
    }

    // Collect siblings (same level, same parent)
    for (const TSNode& node : allHeadings) {
        int32 nodeOffset = ts_node_start_byte(node);

        // Must be after parent and before next parent-level heading
        if (nodeOffset > parentOffset && nodeOffset < endOffset) {
            int32 level = GetHeadingLevelFromNode(node);
            if (level == targetLevel) {
                siblings.push_back(node);
            }
        }
    }

    return siblings;
}

void MarkdownParser::GetHeadingContext(int32 offset, BMessage* context) const
{
    if (!context || !fTree) {
        return;
    }

    context->MakeEmpty();
    context->what = 'OUTL';
    context->AddString("type", "context");

    // Get all headings
    std::vector<TSNode> allHeadings = FindAllHeadings();

    // Build context stack (breadcrumb trail)
    std::vector<TSNode> contextStack;

    for (const TSNode& heading : allHeadings) {
        int32 headingOffset = ts_node_start_byte(heading);

        // Only consider headings before current offset
        if (headingOffset > offset) {
            break;
        }

        int32 level = GetHeadingLevelFromNode(heading);

        // Remove headings at same or deeper level from stack
        while (!contextStack.empty()) {
            int32 stackLevel = GetHeadingLevelFromNode(contextStack.back());
            if (stackLevel >= level) {
                contextStack.pop_back();
            } else {
                break;
            }
        }

        // Add this heading to stack
        contextStack.push_back(heading);
    }

    // Build context message from stack
    for (const TSNode& heading : contextStack) {
        BMessage headingMsg;
        ExtractHeadingInfo(heading, &headingMsg, true);
        context->AddMessage("heading", &headingMsg);
    }
}

void MarkdownParser::ExtractHeadingInfo(TSNode node, BMessage* msg, bool withText) const
{
    if (!msg || ts_node_is_null(node)) {
        return;
    }

    int32 level = GetHeadingLevelFromNode(node);
    int32 offset = ts_node_start_byte(node);
    int32 length = ts_node_end_byte(node) - offset;

    msg->AddInt32("level", level);
    msg->AddInt32("offset", offset);
    msg->AddInt32("length", length);
    msg->AddInt32("line", GetLineForOffset(offset));

    if (withText) {
        // Extract heading text
        TSNode contentNode = ts_node_child_by_field_name(node, "heading_content", 15);
        if (!ts_node_is_null(contentNode)) {
            BString text = GetNodeText(contentNode);
            text.Trim();
            msg->AddString("text", text);
        } else {
            // Fallback: look for inline child
            uint32_t childCount = ts_node_named_child_count(node);
            for (uint32_t i = 0; i < childCount; i++) {
                TSNode child = ts_node_named_child(node, i);
                if (strcmp(ts_node_type(child), "inline") == 0) {
                    BString text = GetNodeText(child);
                    text.Trim();
                    msg->AddString("text", text);
                    break;
                }
            }
        }
    }
}

int MarkdownParser::GetHeadingLevelFromNode(TSNode node) const
{
    if (ts_node_is_null(node)) {
        return 0;
    }

    // Look for atx_h1..atx_h6 child
    uint32_t childCount = ts_node_child_count(node);
    for (uint32_t i = 0; i < childCount; i++) {
        TSNode child = ts_node_child(node, i);
        const char* childType = ts_node_type(child);

        if (strncmp(childType, "atx_h", 5) == 0 &&
            childType[5] >= '1' && childType[5] <= '6') {
            return childType[5] - '0';
        }
    }

    return 1;  // Default to level 1
}

std::vector<StyleRun> MarkdownParser::GetStyleRunsInRange(int32 startOffset, int32 endOffset) const
{
    std::vector<StyleRun> result;

    // Filter style runs that overlap with the requested range
    for (const StyleRun& run : fStyleRuns) {
        int32 runEnd = run.offset + run.length;

        // Check if run overlaps with requested range
        if (run.offset < endOffset && runEnd > startOffset) {
            result.push_back(run);
        }
    }

    return result;
}

void MarkdownParser::SetFont(StyleRun::Type type, const BFont& font)
{
    fFonts[type] = font;
}

void MarkdownParser::SetColor(StyleRun::Type type, rgb_color foreground, rgb_color background)
{
    fForegroundColors[type] = foreground;
    fBackgroundColors[type] = background;
}

void MarkdownParser::SetSyntaxHighlighter(SyntaxHighlighter* highlighter)
{
    fSyntaxHighlighter = highlighter;
}

void MarkdownParser::ApplySyntaxHighlighting(int32 codeOffset, int32 codeLength,
                                             const char* code, const char* language)
{
    if (!fSyntaxHighlighter || !language) return;

    // Get syntax tokens from the highlighter
    std::vector<SyntaxToken> tokens = fSyntaxHighlighter->Tokenize(code, language);

    // Create style runs for each token
    for (const SyntaxToken& token : tokens) {
        // Map token type to style run type
        StyleRun::Type styleType;
        switch (token.type) {
            case SyntaxToken::KEYWORD:
                styleType = StyleRun::Type::SYNTAX_KEYWORD;
                break;
            case SyntaxToken::TYPE:
                styleType = StyleRun::Type::SYNTAX_TYPE;
                break;
            case SyntaxToken::FUNCTION:
                styleType = StyleRun::Type::SYNTAX_FUNCTION;
                break;
            case SyntaxToken::STRING:
                styleType = StyleRun::Type::SYNTAX_STRING;
                break;
            case SyntaxToken::NUMBER:
                styleType = StyleRun::Type::SYNTAX_NUMBER;
                break;
            case SyntaxToken::COMMENT:
                styleType = StyleRun::Type::SYNTAX_COMMENT;
                break;
            case SyntaxToken::OPERATOR:
                styleType = StyleRun::Type::SYNTAX_OPERATOR;
                break;
            default:
                continue; // Skip NORMAL tokens
        }

        // Create style run at the correct offset (relative to document)
        CreateStyleRun(codeOffset + token.offset, token.length, styleType);
    }
}

// Debug functions

void MarkdownParser::DebugPrintNode(TSNode node, int depth) const
{
    for (int i = 0; i < depth; i++) {
        printf("  ");
    }

    const char* type = ts_node_type(node);
    uint32_t start = ts_node_start_byte(node);
    uint32_t end = ts_node_end_byte(node);
    bool named = ts_node_is_named(node);

    printf("%s [%u, %u) %s", type, start, end, named ? "named" : "");

    // Show text for small nodes
    if (end - start <= 40 && fSourceText) {
        printf(" \"");
        for (uint32_t i = start; i < end; i++) {
            char c = fSourceText[i];
            if (c == '\n') printf("\\n");
            else if (c == '\t') printf("\\t");
            else printf("%c", c);
        }
        printf("\"");
    }

    printf("\n");
}

void MarkdownParser::DumpTree() const
{
    if (!fTree) {
        printf("No parse tree available\n");
        return;
    }

    printf("\n=== Parse Tree ===\n");
    TSNode root = ts_tree_root_node(fTree);

    // Use tree-sitter's built-in S-expression printer
    char* sexp = ts_node_string(root);
    printf("%s\n", sexp);
    free(sexp);
}

void MarkdownParser::DumpStyleRuns() const
{
    printf("\n=== Style Runs (%zu) ===\n", fStyleRuns.size());

    const char* typeNames[] = {
        "NORMAL", "HEADING_1", "HEADING_2", "HEADING_3", "HEADING_4", "HEADING_5", "HEADING_6",
        "CODE_INLINE", "CODE_BLOCK", "EMPHASIS", "STRONG", "UNDERLINE", "STRIKETHROUGH",
        "LINK", "LINK_URL", "LIST_BULLET", "LIST_NUMBER", "BLOCKQUOTE",
        "TASK_UNCHECKED", "TASK_CHECKED",
        "TABLE_HEADER", "TABLE_CELL", "TABLE_DELIMITER", "TABLE_ROW_DELIMITER",
        "SYNTAX_KEYWORD", "SYNTAX_TYPE", "SYNTAX_FUNCTION",
        "SYNTAX_STRING", "SYNTAX_NUMBER", "SYNTAX_COMMENT", "SYNTAX_OPERATOR"
    };

    for (size_t i = 0; i < fStyleRuns.size(); i++) {
        const StyleRun& run = fStyleRuns[i];
        const char* typeName = (run.type < sizeof(typeNames)/sizeof(typeNames[0]))
                              ? typeNames[run.type] : "UNKNOWN";

        printf("  [%d] offset=%d, len=%d, type=%s",
               (int)i, run.offset, run.length, typeName);

        if (!run.language.IsEmpty()) {
            printf(", lang=%s", run.language.String());
        }
        if (!run.url.IsEmpty()) {
            printf(", url=%s", run.url.String());
        }
        if (!run.text.IsEmpty()) {
            printf(", text='%s'", run.text.String());
        }

        // Show snippet
        if (fSourceText && run.length <= 40) {
            printf(" \"");
            for (int32 i = 0; i < run.length && i < 40; i++) {
                char c = fSourceText[run.offset + i];
                if (c == '\n') printf("\\n");
                else if (c == '\t') printf("\\t");
                else printf("%c", c);
            }
            printf("\"");
        }

        printf("\n");
    }
}

void MarkdownParser::DumpOutline() const
{
    printf("\n=== Outline ===\n");
    fOutline.PrintToStream();
}

const char* MarkdownParser::GetListBulletSymbol() const
{
    return UNICODE_BULLET;
}

const char* MarkdownParser::GetTaskCheckedSymbol() const
{
    return UNICODE_CHECKBOX_CHECKED;
}

const char* MarkdownParser::GetTaskUncheckedSymbol() const
{
    return UNICODE_CHECKBOX_UNCHECKED;
}
