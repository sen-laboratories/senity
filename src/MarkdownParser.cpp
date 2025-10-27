#include "MarkdownParser.h"
#include "SyntaxHighlighter.h"
#include <cstring>
#include <algorithm>

MarkdownParser::MarkdownParser()
    : fDocument(nullptr)
    , fSyntaxHighlighter(nullptr)
{
    InitializeDefaultStyles();
}

MarkdownParser::~MarkdownParser()
{
    Clear();
}

void MarkdownParser::Clear()
{
    if (fDocument) {
        cmark_node_free(fDocument);
        fDocument = nullptr;
    }

    fStyleRuns.clear();
    fOutline.MakeEmpty();
    fLineToNode.clear();
    fOffsetToNode.clear();
    fNodeToOffset.clear();
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

    // Headings
    for (int i = 0; i < 6; i++) {
        BFont headingFont(be_bold_font);
        headingFont.SetSize(24 - i * 2);
        fFonts[(StyleRun::Type)(StyleRun::HEADING_1 + i)] = headingFont;
    }

    BFont emphFont(be_plain_font);
    emphFont.SetFace(B_ITALIC_FACE);
    fFonts[StyleRun::EMPHASIS] = emphFont;

    BFont strongFont(be_plain_font);
    strongFont.SetFace(B_BOLD_FACE);
    fFonts[StyleRun::STRONG] = strongFont;

    // Default colors
    rgb_color black = {0, 0, 0, 255};
    rgb_color white = {255, 255, 255, 255};
    rgb_color blue = {0, 102, 204, 255};
    rgb_color darkGray = {60, 60, 60, 255};
    rgb_color lightGray = {245, 245, 245, 255};
    rgb_color mediumGray = {128, 128, 128, 255};

    // Foreground colors
    fForegroundColors[StyleRun::NORMAL] = black;
    fForegroundColors[StyleRun::CODE_INLINE] = darkGray;
    fForegroundColors[StyleRun::CODE_BLOCK] = black;
    fForegroundColors[StyleRun::LINK] = blue;
    fForegroundColors[StyleRun::LINK_URL] = mediumGray;
    fForegroundColors[StyleRun::EMPHASIS] = black;
    fForegroundColors[StyleRun::STRONG] = black;
    fForegroundColors[StyleRun::BLOCKQUOTE] = mediumGray;

    for (int i = 0; i < 6; i++) {
        fForegroundColors[(StyleRun::Type)(StyleRun::HEADING_1 + i)] = black;
    }

    // Background colors
    fBackgroundColors[StyleRun::NORMAL] = white;
    fBackgroundColors[StyleRun::CODE_INLINE] = lightGray;
    fBackgroundColors[StyleRun::CODE_BLOCK] = lightGray;

    // All others default to white background
    for (int i = 0; i < (int)StyleRun::BLOCKQUOTE; i++) {
        if (fBackgroundColors.find((StyleRun::Type)i) == fBackgroundColors.end()) {
            fBackgroundColors[(StyleRun::Type)i] = white;
        }
    }
}

bool MarkdownParser::Parse(const char* markdownText)
{
    if (!markdownText) {
        return false;
    }

    Clear();

    // Create parser with GFM extensions
    cmark_parser* parser = cmark_parser_new(CMARK_OPT_DEFAULT);

    // Enable GFM extensions
    cmark_gfm_core_extensions_ensure_registered();
    cmark_syntax_extension* table = cmark_find_syntax_extension("table");
    cmark_syntax_extension* strikethrough = cmark_find_syntax_extension("strikethrough");
    cmark_syntax_extension* tasklist = cmark_find_syntax_extension("tasklist");

    if (table) cmark_parser_attach_syntax_extension(parser, table);
    if (strikethrough) cmark_parser_attach_syntax_extension(parser, strikethrough);
    if (tasklist) cmark_parser_attach_syntax_extension(parser, tasklist);

    // Parse
    cmark_parser_feed(parser, markdownText, strlen(markdownText));
    fDocument = cmark_parser_finish(parser);
    cmark_parser_free(parser);

    if (!fDocument) {
        return false;
    }

    // Build indices
    BuildLineIndex();

    // Process AST and create style runs
    ProcessNode(fDocument, markdownText);

    // Build outline
    BuildOutline();

    return true;
}

bool MarkdownParser::ParseIncremental(const char* markdownText, int32 startLine, int32 endLine)
{
    if (!markdownText) {
        return false;
    }

    // Calculate byte offsets for the affected range
    int32 startOffset = 0;
    int32 endOffset = strlen(markdownText);

    int32 currentLine = 1;
    for (const char* p = markdownText; *p; p++) {
        if (currentLine == startLine) {
            startOffset = p - markdownText;
        }
        if (currentLine == endLine + 1) {
            endOffset = p - markdownText;
            break;
        }
        if (*p == '\n') {
            currentLine++;
        }
    }

    if (!Parse(markdownText)) {
        return false;
    }

    return true;
}

void MarkdownParser::ProcessNode(cmark_node* node, const char* sourceText)
{
    if (!node || !sourceText) return;

    cmark_iter* iter = cmark_iter_new(node);
    cmark_event_type ev_type;

    while ((ev_type = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
        if (ev_type == CMARK_EVENT_ENTER) {
            cmark_node* cur = cmark_iter_get_node(iter);
            cmark_node_type type = cmark_node_get_type(cur);

            int32 startOffset = GetNodeStartOffset(cur, sourceText);
            int32 endOffset = GetNodeEndOffset(cur, sourceText);

            if (startOffset < 0 || endOffset <= startOffset) {
                continue; // Skip invalid nodes
            }

            int32 length = endOffset - startOffset;

            // Record node position
            fNodeToOffset[cur] = startOffset;
            fOffsetToNode[startOffset] = cur;

            switch (type) {
                case CMARK_NODE_HEADING:
                {
                    int level = cmark_node_get_heading_level(cur);
                    if (level < 1 || level > 6) level = 1;
                    StyleRun::Type headingType = (StyleRun::Type)(StyleRun::HEADING_1 + level - 1);
                    CreateStyleRun(startOffset, length, headingType);
                    break;
                }

                case CMARK_NODE_CODE_BLOCK:
                {
                    const char* lang = cmark_node_get_fence_info(cur);
                    const char* code = cmark_node_get_literal(cur);

                    CreateStyleRun(startOffset, length, StyleRun::CODE_BLOCK,
                                 lang ? lang : "");

                    // Apply syntax highlighting if available
                    if (fSyntaxHighlighter && lang && strlen(lang) > 0 && code) {
                        // Find actual code content offset (skip fence line)
                        const char* codeStart = sourceText + startOffset;
                        const char* firstNewline = strchr(codeStart, '\n');

                        if (firstNewline && firstNewline < sourceText + endOffset) {
                            int32 codeContentOffset = (firstNewline - sourceText) + 1;
                            int32 codeLength = strlen(code);
                            ApplySyntaxHighlighting(codeContentOffset, codeLength, code, lang);
                        }
                    }
                    break;
                }

                case CMARK_NODE_CODE:
                {
                    CreateStyleRun(startOffset, length, StyleRun::CODE_INLINE);
                    break;
                }

                case CMARK_NODE_EMPH:
                {
                    CreateStyleRun(startOffset, length, StyleRun::EMPHASIS);
                    break;
                }

                case CMARK_NODE_STRONG:
                {
                    CreateStyleRun(startOffset, length, StyleRun::STRONG);
                    break;
                }

                case CMARK_NODE_LINK:
                {
                    const char* url = cmark_node_get_url(cur);
                    CreateStyleRun(startOffset, length, StyleRun::LINK, "", url ? url : "");
                    break;
                }

                case CMARK_NODE_ITEM:
                {
                    cmark_node* list = cmark_node_parent(cur);
                    if (list) {
                        cmark_list_type listType = cmark_node_get_list_type(list);
                        StyleRun::Type bulletType = (listType == CMARK_BULLET_LIST) ?
                                                   StyleRun::LIST_BULLET : StyleRun::LIST_NUMBER;

                        // Style just the bullet marker (first few chars)
                        int32 markerLen = (listType == CMARK_BULLET_LIST) ? 2 : 3;
                        if (markerLen <= length) {
                            CreateStyleRun(startOffset, markerLen, bulletType);
                        }
                    }
                    break;
                }

                case CMARK_NODE_BLOCK_QUOTE:
                {
                    CreateStyleRun(startOffset, length, StyleRun::BLOCKQUOTE);
                    break;
                }

                default:
                    break;
            }
        }
    }

    cmark_iter_free(iter);
}

void MarkdownParser::CreateStyleRun(int32 offset, int32 length, StyleRun::Type type,
                                   const BString& language, const BString& url)
{
    if (offset < 0 || length <= 0) return;

    StyleRun run;
    run.offset = offset;
    run.length = length;
    run.type = type;

    // Apply configured font and colors
    auto fontIt = fFonts.find(type);
    run.font = (fontIt != fFonts.end()) ? fontIt->second : *be_plain_font;

    auto fgIt = fForegroundColors.find(type);
    run.foreground = (fgIt != fForegroundColors.end()) ? fgIt->second : rgb_color{0, 0, 0, 255};

    auto bgIt = fBackgroundColors.find(type);
    run.background = (bgIt != fBackgroundColors.end()) ? bgIt->second : rgb_color{255, 255, 255, 255};

    run.language = language;
    run.url = url;
    run.listLevel = 0;

    fStyleRuns.push_back(run);
}

void MarkdownParser::ApplySyntaxHighlighting(int32 codeOffset, int32 codeLength,
                                             const char* code, const char* language)
{
    if (!fSyntaxHighlighter || !fSyntaxHighlighter->SupportsLanguage(language)) {
        return;
    }

    std::vector<SyntaxToken> tokens = fSyntaxHighlighter->Tokenize(code, language);

    for (const SyntaxToken& token : tokens) {
        StyleRun run;
        run.offset = codeOffset + token.offset;
        run.length = token.length;

        auto fontIt = fFonts.find(StyleRun::CODE_BLOCK);
        run.font = (fontIt != fFonts.end()) ? fontIt->second : *be_fixed_font;

        // Map syntax token to style run type and color
        switch (token.type) {
            case SyntaxToken::KEYWORD:
                run.type = StyleRun::SYNTAX_KEYWORD;
                run.foreground = {0, 0, 255, 255};  // Blue
                break;
            case SyntaxToken::TYPE:
                run.type = StyleRun::SYNTAX_TYPE;
                run.foreground = {0, 128, 128, 255};  // Teal
                break;
            case SyntaxToken::FUNCTION:
                run.type = StyleRun::SYNTAX_FUNCTION;
                run.foreground = {128, 0, 128, 255};  // Purple
                break;
            case SyntaxToken::STRING:
                run.type = StyleRun::SYNTAX_STRING;
                run.foreground = {0, 128, 0, 255};  // Green
                break;
            case SyntaxToken::NUMBER:
                run.type = StyleRun::SYNTAX_NUMBER;
                run.foreground = {255, 102, 0, 255};  // Orange
                break;
            case SyntaxToken::COMMENT:
                run.type = StyleRun::SYNTAX_COMMENT;
                run.foreground = {128, 128, 128, 255};  // Gray
                break;
            default:
                continue;  // Skip unstyled tokens
        }

        auto bgIt = fBackgroundColors.find(StyleRun::CODE_BLOCK);
        run.background = (bgIt != fBackgroundColors.end()) ? bgIt->second : rgb_color{245, 245, 245, 255};

        fStyleRuns.push_back(run);
    }
}

void MarkdownParser::BuildLineIndex()
{
    if (!fDocument) return;

    fLineToNode.clear();

    cmark_iter* iter = cmark_iter_new(fDocument);
    cmark_event_type ev_type;

    while ((ev_type = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
        if (ev_type == CMARK_EVENT_ENTER) {
            cmark_node* node = cmark_iter_get_node(iter);

            if (cmark_node_get_type(node) & CMARK_NODE_TYPE_BLOCK) {
                int startLine = cmark_node_get_start_line(node);
                fLineToNode[startLine] = node;
            }
        }
    }

    cmark_iter_free(iter);
}

void MarkdownParser::BuildOutline()
{
    if (!fDocument) return;

    fOutline.MakeEmpty();
    BuildOutlineRecursive(fDocument, fOutline, 1);
}

void MarkdownParser::BuildOutlineRecursive(cmark_node* startNode, BMessage& parent, int minLevel)
{
    if (!startNode) return;

    cmark_iter* iter = cmark_iter_new(startNode);
    cmark_event_type ev_type;

    std::vector<BMessage*> levelStack;
    levelStack.push_back(&parent);

    while ((ev_type = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
        if (ev_type == CMARK_EVENT_ENTER) {
            cmark_node* node = cmark_iter_get_node(iter);

            if (cmark_node_get_type(node) == CMARK_NODE_HEADING) {
                int level = cmark_node_get_heading_level(node);

                // Extract heading text from children
                BString text;
                cmark_node* child = cmark_node_first_child(node);
                while (child) {
                    if (cmark_node_get_type(child) == CMARK_NODE_TEXT) {
                        const char* literal = cmark_node_get_literal(child);
                        if (literal) text << literal;
                    }
                    child = cmark_node_next(child);
                }

                // Create heading message
                BMessage heading;
                heading.AddString("text", text);
                heading.AddInt32("level", level);
                heading.AddInt32("line", cmark_node_get_start_line(node));

                auto it = fNodeToOffset.find(node);
                heading.AddInt32("offset", (it != fNodeToOffset.end()) ? it->second : 0);

                // Adjust level stack to proper depth
                while (levelStack.size() > (size_t)level) {
                    levelStack.pop_back();
                }

                // Add to parent at current level
                BMessage* targetParent = &parent;
                if (!levelStack.empty()) {
                    targetParent = levelStack.back();
                }

                targetParent->AddMessage("children", &heading);

                // Push this heading as potential parent for deeper levels
                if (levelStack.size() == (size_t)level) {
                    // Note: In real implementation, we'd need to store the actual
                    // message pointer, but BMessage doesn't easily allow this.
                    // For now, this builds a flat structure with level info.
                }
            }
        }
    }

    cmark_iter_free(iter);
}

cmark_node* MarkdownParser::GetNodeAtLine(int line) const
{
    auto it = fLineToNode.upper_bound(line);
    if (it == fLineToNode.begin()) return nullptr;

    --it;
    cmark_node* node = it->second;

    int startLine = cmark_node_get_start_line(node);
    int endLine = cmark_node_get_end_line(node);

    return (line >= startLine && line <= endLine) ? node : nullptr;
}

cmark_node* MarkdownParser::GetNodeAtOffset(int32 textOffset) const
{
    auto it = fOffsetToNode.upper_bound(textOffset);
    if (it == fOffsetToNode.begin()) return nullptr;

    --it;
    return it->second;
}

int32 MarkdownParser::GetLineForOffset(int32 textOffset) const
{
    cmark_node* node = GetNodeAtOffset(textOffset);
    return node ? cmark_node_get_start_line(node) : 0;
}

int32 MarkdownParser::GetNodeStartOffset(cmark_node* node, const char* sourceText) const
{
    if (!node || !sourceText) return -1;

    int startLine = cmark_node_get_start_line(node);
    int startColumn = cmark_node_get_start_column(node);

    // cmark columns are 1-indexed, we need 0-indexed
    int32 offset = 0;
    int currentLine = 1;
    int currentColumn = 1;

    for (const char* p = sourceText; *p; p++, offset++) {
        if (currentLine == startLine && currentColumn == startColumn) {
            return offset;
        }

        if (*p == '\n') {
            currentLine++;
            currentColumn = 1;
        } else {
            currentColumn++;
        }
    }

    return -1;
}

int32 MarkdownParser::GetNodeEndOffset(cmark_node* node, const char* sourceText) const
{
    if (!node || !sourceText) return -1;

    int endLine = cmark_node_get_end_line(node);
    int endColumn = cmark_node_get_end_column(node);

    int32 offset = 0;
    int currentLine = 1;
    int currentColumn = 1;

    for (const char* p = sourceText; *p; p++, offset++) {
        if (currentLine == endLine && currentColumn == endColumn) {
            return offset + 1;  // <-- Include the last character!
        }

        if (*p == '\n') {
            currentLine++;
            currentColumn = 1;
        } else {
            currentColumn++;
        }
    }

    return strlen(sourceText);
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
