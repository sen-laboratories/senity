#include "MarkdownParser.h"
#include "SyntaxHighlighter.h"
#include <cstring>
#include <algorithm>
#include <stdio.h>

// Unicode symbols for better visual presentation
static const char* UNICODE_BULLET = "•";           // U+2022 BULLET
static const char* UNICODE_CHECKBOX_UNCHECKED = "☐"; // U+2610 BALLOT BOX
static const char* UNICODE_CHECKBOX_CHECKED = "☑";   // U+2611 BALLOT BOX WITH CHECK

MarkdownParser::MarkdownParser()
    : fParser(nullptr)
    , fTree(nullptr)
    , fSourceText(nullptr)
    , fSourceCopy(nullptr)
    , fSyntaxHighlighter(nullptr)
    , fDebugEnabled(false)
    , fUseUnicodeSymbols(true)
{
    // Create tree-sitter parser
    fParser = ts_parser_new();
    ts_parser_set_language(fParser, tree_sitter_markdown());

    InitializeDefaultStyles();
}

MarkdownParser::~MarkdownParser()
{
    Clear();

    if (fParser) {
        ts_parser_delete(fParser);
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

    fFonts[StyleRun::NORMAL] = plainFont;
    fFonts[StyleRun::CODE_INLINE] = fixedFont;
    fFonts[StyleRun::CODE_BLOCK] = fixedFont;
    fFonts[StyleRun::STRONG] = boldFont;

    BFont emphasisFont(plainFont);
    emphasisFont.SetFace(B_ITALIC_FACE);
    fFonts[StyleRun::EMPHASIS] = emphasisFont;

    for (int i = 0; i < 6; i++) {
        BFont headingFont(be_bold_font);
        headingFont.SetSize(24 - i * 2);
        fFonts[(StyleRun::Type)(StyleRun::HEADING_1 + i)] = headingFont;
    }

    // Default colors
    rgb_color black = {0, 0, 0, 255};
    rgb_color white = {255, 255, 255, 255};
    rgb_color blue = {0, 102, 204, 255};
    rgb_color gray = {60, 60, 60, 255};
    rgb_color lightGray = {245, 245, 245, 255};
    rgb_color green = {0, 150, 0, 255};
    rgb_color purple = {128, 0, 128, 255};
    rgb_color teal = {0, 128, 128, 255};
    rgb_color orange = {255, 102, 0, 255};

    fForegroundColors[StyleRun::NORMAL] = black;
    fForegroundColors[StyleRun::CODE_INLINE] = gray;
    fForegroundColors[StyleRun::CODE_BLOCK] = black;
    fForegroundColors[StyleRun::LINK] = blue;
    fForegroundColors[StyleRun::LIST_BULLET] = gray;
    fForegroundColors[StyleRun::LIST_NUMBER] = gray;
    fForegroundColors[StyleRun::TASK_MARKER_UNCHECKED] = gray;
    fForegroundColors[StyleRun::TASK_MARKER_CHECKED] = green;

    // Syntax highlighting colors (matching SyntaxHighlighter defaults)
    fForegroundColors[StyleRun::SYNTAX_KEYWORD] = blue;
    fForegroundColors[StyleRun::SYNTAX_TYPE] = teal;
    fForegroundColors[StyleRun::SYNTAX_FUNCTION] = purple;
    fForegroundColors[StyleRun::SYNTAX_STRING] = green;
    fForegroundColors[StyleRun::SYNTAX_NUMBER] = orange;
    fForegroundColors[StyleRun::SYNTAX_COMMENT] = gray;
    fForegroundColors[StyleRun::SYNTAX_OPERATOR] = black;

    fBackgroundColors[StyleRun::NORMAL] = white;
    fBackgroundColors[StyleRun::CODE_INLINE] = lightGray;
    fBackgroundColors[StyleRun::CODE_BLOCK] = lightGray;
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
                                      int32 editOffset, int32 oldLength, int32 newLength)
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
    }

    // Create edit descriptor for tree-sitter
    TSInputEdit edit;
    edit.start_byte = editOffset;
    edit.old_end_byte = editOffset + oldLength;
    edit.new_end_byte = editOffset + newLength;

    // Calculate line/column - use memchr for fast newline counting
    auto countLines = [](const char* text, int32 offset) -> TSPoint {
        uint32_t line = 0;
        uint32_t column = 0;
        const char* p = text;
        const char* end = text + offset;

        while (p < end) {
            const char* newline = (const char*)memchr(p, '\n', end - p);
            if (newline) {
                line++;
                p = newline + 1;
                column = 0;
            } else {
                column = end - p;
                break;
            }
        }

        return {line, column};
    };

    edit.start_point = countLines(fSourceText, editOffset);
    edit.old_end_point = countLines(fSourceText, editOffset + oldLength);
    edit.new_end_point = countLines(markdownText, editOffset + newLength);

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
            CreateStyleRun(startByte, length, StyleRun::TASK_MARKER_UNCHECKED,
                          "", "", UNICODE_CHECKBOX_UNCHECKED);
        } else {
            CreateStyleRun(startByte, length, StyleRun::TASK_MARKER_UNCHECKED);
        }
        return; // Don't recurse into task markers
    }

    if (strcmp(nodeType, "task_list_marker_checked") == 0) {
        if (fUseUnicodeSymbols) {
            CreateStyleRun(startByte, length, StyleRun::TASK_MARKER_CHECKED,
                          "", "", UNICODE_CHECKBOX_CHECKED);
        } else {
            CreateStyleRun(startByte, length, StyleRun::TASK_MARKER_CHECKED);
        }
        return;
    }

    // Handle list markers
    if (strcmp(nodeType, "list_marker_minus") == 0 ||
        strcmp(nodeType, "list_marker_plus") == 0 ||
        strcmp(nodeType, "list_marker_star") == 0) {
        if (fUseUnicodeSymbols) {
            CreateStyleRun(startByte, length, StyleRun::LIST_BULLET,
                          "", "", UNICODE_BULLET);
        } else {
            CreateStyleRun(startByte, length, StyleRun::LIST_BULLET);
        }
        return;
    }

    // Determine style based on node type
    StyleRun::Type styleType = GetStyleTypeForNode(node);

    if (styleType != StyleRun::NORMAL) {
        // Extract additional info for special nodes
        BString language;
        BString url;

        if (styleType == StyleRun::CODE_BLOCK) {
            // Try to get language from info_string node
            TSNode infoNode = ts_node_child_by_field_name(node, "info_string", 11);
            if (!ts_node_is_null(infoNode)) {
                language = GetNodeText(infoNode);
                language.Trim();
            }
        } else if (styleType == StyleRun::LINK) {
            // Try to get URL from link_destination node
            TSNode destNode = ts_node_child_by_field_name(node, "link_destination", 16);
            if (!ts_node_is_null(destNode)) {
                url = GetNodeText(destNode);
            }
        }

        CreateStyleRun(startByte, length, styleType, language, url);

        // Special handling for code blocks with syntax highlighting
        if (styleType == StyleRun::CODE_BLOCK && fSyntaxHighlighter && !language.IsEmpty()) {
            const char* code = fSourceText + startByte;
            ApplySyntaxHighlighting(startByte, length, code, language.String());
        }
    }

    // Recursively process children
    uint32_t childCount = ts_node_child_count(node);
    for (uint32_t i = 0; i < childCount; i++) {
        TSNode child = ts_node_child(node, i);
        ProcessNode(child, depth + 1);
    }
}

StyleRun::Type MarkdownParser::GetStyleTypeForNode(TSNode node) const
{
    const char* type = ts_node_type(node);

    // Headings
    if (strcmp(type, "atx_heading") == 0) {
        int level = GetHeadingLevel(node);
        return (StyleRun::Type)(StyleRun::HEADING_1 + level - 1);
    }

    // Code
    if (strcmp(type, "fenced_code_block") == 0 || strcmp(type, "indented_code_block") == 0) {
        return StyleRun::CODE_BLOCK;
    }
    if (strcmp(type, "code_span") == 0) {
        return StyleRun::CODE_INLINE;
    }

    // Emphasis
    if (strcmp(type, "emphasis") == 0) {
        return StyleRun::EMPHASIS;
    }
    if (strcmp(type, "strong_emphasis") == 0) {
        return StyleRun::STRONG;
    }

    // Links
    if (strcmp(type, "inline_link") == 0 || strcmp(type, "shortcut_link") == 0) {
        return StyleRun::LINK;
    }

    // Blockquote
    if (strcmp(type, "block_quote") == 0) {
        return StyleRun::BLOCKQUOTE;
    }

    // List markers handled in ProcessNode

    return StyleRun::NORMAL;
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

void MarkdownParser::BuildOutline()
{
    fOutline.MakeEmpty();
    fOutline.AddString("type", "outline");

    if (!fTree) {
        if (fDebugEnabled) {
            printf("BuildOutline - No tree available\n");
        }
        return;
    }

    if (fDebugEnabled) {
        printf("\n=== Building Outline ===\n");
    }

    TSNode root = ts_tree_root_node(fTree);

    // Traverse tree looking for headings
    TSTreeCursor cursor = ts_tree_cursor_new(root);

    do {
        TSNode node = ts_tree_cursor_current_node(&cursor);
        const char* nodeType = ts_node_type(node);

        if (strcmp(nodeType, "atx_heading") == 0) {
            int level = GetHeadingLevel(node);

            // Extract heading text from heading_content child
            BString text;
            uint32_t childCount = ts_node_child_count(node);
            for (uint32_t i = 0; i < childCount; i++) {
                TSNode child = ts_node_child(node, i);
                if (strcmp(ts_node_type(child), "heading_content") == 0) {
                    text = GetNodeText(child);
                    text.Trim();
                    break;
                }
            }

            if (fDebugEnabled) {
                printf("Found heading L%d at %u: '%s'\n", level,
                       ts_node_start_byte(node), text.String());
            }

            BMessage heading;
            heading.AddString("text", text);
            heading.AddInt32("level", level);
            heading.AddInt32("offset", ts_node_start_byte(node));
            heading.AddInt32("length", ts_node_end_byte(node) - ts_node_start_byte(node));

            fOutline.AddMessage("heading", &heading);
        }
    } while (ts_tree_cursor_goto_next_sibling(&cursor) ||
             ts_tree_cursor_goto_first_child(&cursor));

    ts_tree_cursor_delete(&cursor);
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
                styleType = StyleRun::SYNTAX_KEYWORD;
                break;
            case SyntaxToken::TYPE:
                styleType = StyleRun::SYNTAX_TYPE;
                break;
            case SyntaxToken::FUNCTION:
                styleType = StyleRun::SYNTAX_FUNCTION;
                break;
            case SyntaxToken::STRING:
                styleType = StyleRun::SYNTAX_STRING;
                break;
            case SyntaxToken::NUMBER:
                styleType = StyleRun::SYNTAX_NUMBER;
                break;
            case SyntaxToken::COMMENT:
                styleType = StyleRun::SYNTAX_COMMENT;
                break;
            case SyntaxToken::OPERATOR:
                styleType = StyleRun::SYNTAX_OPERATOR;
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
        "CODE_INLINE", "CODE_BLOCK", "EMPHASIS", "STRONG", "LINK", "LINK_URL",
        "LIST_BULLET", "LIST_NUMBER", "BLOCKQUOTE",
        "TASK_UNCHECKED", "TASK_CHECKED"
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

    int32 count = 0;
    if (fOutline.GetInfo("heading", NULL, &count) != B_OK) {
        printf("No headings in outline\n");
        return;
    }

    printf("Found %d headings:\n", count);

    for (int32 i = 0; i < count; i++) {
        BMessage heading;
        if (fOutline.FindMessage("heading", i, &heading) == B_OK) {
            BString text;
            int32 level = 0;
            int32 offset = 0;

            heading.FindString("text", &text);
            heading.FindInt32("level", &level);
            heading.FindInt32("offset", &offset);

            // Indent based on level
            for (int j = 1; j < level; j++) {
                printf("  ");
            }
            printf("L%d [%d]: %s\n", level, offset, text.String());
        }
    }
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
