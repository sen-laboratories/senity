/*
 * Copyright 2024-2025, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "../common/ColorDefs.h"
#include "../common/Messages.h"
#include "../common/SenConnector.h"
#include "EditorTextView.h"
#include "StyleRun.h"

#include <sen/Sen.h>

#include <Bitmap.h>
#include <IconUtils.h>
#include <Clipboard.h>
#include <File.h>
#include <MenuItem.h>
#include <Message.h>
#include <Messenger.h>
#include <MimeType.h>
#include <Node.h>
#include <NodeInfo.h>
#include <Path.h>
#include <PopUpMenu.h>
#include <Region.h>
#include <Window.h>
#include <cstring>
#include <strings.h>
#include <vector>
#include <spdlog/spdlog.h>

static rgb_color GetColorForType(StyleRun::Type type) {
    for (size_t i = 0; i < sizeof(COLOR_MAP) / sizeof(COLOR_MAP[0]); i++) {
        if (COLOR_MAP[i].type == type) {
            return COLOR_MAP[i].color;
        }
    }
    // Default to black
    return {0, 0, 0, 255};
}

struct NodeHighlightData {
    bool isHighlighted;
    rgb_color fgColor;
    rgb_color bgColor;
    bool isOutline;
    bool isGenerated;

    NodeHighlightData()
        : isHighlighted(false)
        , fgColor({0, 0, 0, 255})
        , bgColor({255, 255, 255, 255})
        , isOutline(false)
        , isGenerated(false)
    {}
};

EditorTextView::EditorTextView(StatusBar *statusView, BHandler *editorHandler)
    : BTextView("EditorTextView", B_WILL_DRAW | B_FRAME_EVENTS)
    , fEditorHandler(editorHandler)
    , fStatusBar(statusView)
    , fTextHighlights(new std::map<int32, text_highlight*>())
{
    // Initialize parser
    fMarkdownParser = new MarkdownParser();

    // Initialize syntax highlighter (optional)
    fSyntaxHighlighter = new SyntaxHighlighter();
    fMarkdownParser->SetSyntaxHighlighter(fSyntaxHighlighter);

    // Setup fonts
    fTextFont = new BFont(be_plain_font);
    fLinkFont = new BFont(be_plain_font);
    fLinkFont->SetFace(B_UNDERSCORE_FACE);
    fCodeFont = new BFont(be_fixed_font);

    // Configure parser fonts and colors using map
    fMarkdownParser->SetFont(StyleRun::Type::NORMAL, *fTextFont);
    fMarkdownParser->SetFont(StyleRun::Type::CODE_INLINE, *fCodeFont);
    fMarkdownParser->SetFont(StyleRun::Type::CODE_BLOCK, *fCodeFont);
    fMarkdownParser->SetFont(StyleRun::Type::LINK, *fLinkFont);

    fMarkdownParser->SetColor(StyleRun::Type::NORMAL, GetColorForType(StyleRun::Type::NORMAL));
    fMarkdownParser->SetColor(StyleRun::Type::LINK, GetColorForType(StyleRun::Type::LINK));
    fMarkdownParser->SetColor(StyleRun::Type::CODE_INLINE, GetColorForType(StyleRun::Type::CODE_INLINE));
    fMarkdownParser->SetColor(StyleRun::Type::CODE_BLOCK, GetColorForType(StyleRun::Type::CODE_BLOCK));

    // Table styling - headers should be bold, all table text use fixed font
    fTableFont = new BFont(be_fixed_font);
    fTableHeaderFont = new BFont(be_fixed_font);
    fTableHeaderFont->SetFace(B_BOLD_FACE);

    fMarkdownParser->SetFont(StyleRun::Type::TABLE_HEADER, *fTableHeaderFont);
    fMarkdownParser->SetFont(StyleRun::Type::TABLE_CELL, *fTableFont);
    fMarkdownParser->SetFont(StyleRun::Type::TABLE_DELIMITER, *fTableFont);
    fMarkdownParser->SetFont(StyleRun::Type::TABLE_ROW_DELIMITER, *fTableFont);
    fMarkdownParser->SetColor(StyleRun::Type::TABLE_HEADER, GetColorForType(StyleRun::Type::TABLE_HEADER));
    fMarkdownParser->SetColor(StyleRun::Type::TABLE_CELL, GetColorForType(StyleRun::Type::TABLE_CELL));
    fMarkdownParser->SetColor(StyleRun::Type::TABLE_DELIMITER, GetColorForType(StyleRun::Type::TABLE_DELIMITER));
    fMarkdownParser->SetColor(StyleRun::Type::TABLE_ROW_DELIMITER, GetColorForType(StyleRun::Type::TABLE_ROW_DELIMITER));

    // Heading fonts
    for (int i = 0; i < 6; i++) {
        BFont headingFont(be_bold_font);
        headingFont.SetSize(24 - i * 2);
        fMarkdownParser->SetFont((StyleRun::Type)(StyleRun::Type::HEADING_1 + i), headingFont);
        fMarkdownParser->SetColor((StyleRun::Type)(StyleRun::Type::HEADING_1 + i),
                                  GetColorForType((StyleRun::Type)(StyleRun::Type::HEADING_1 + i)));
    }

    SetStylable(true);
    SetAutoindent(true);
}

EditorTextView::~EditorTextView()
{
    // Clean up user_data from all nodes
    ClearHighlights();

    delete fMarkdownParser;
    delete fSyntaxHighlighter;
    delete fTextFont;
    delete fLinkFont;
    delete fCodeFont;

    ClearIconCache();

    // Clean up highlight map (now mostly unused but keep for compatibility)
    if (fTextHighlights) {
        for (auto& pair : *fTextHighlights) {
            delete pair.second->region;
            delete pair.second;
        }
        delete fTextHighlights;
    }
}

void
EditorTextView::ClearIconCache()
{
    for (auto& pair : fIconCache) {
        delete pair.second;
    }
    fIconCache.clear();
}


void EditorTextView::AttachedToWindow()
{
    fEditorMessenger.SetTo(fEditorHandler);
    spdlog::debug("editorMessenger is {}", fEditorMessenger.IsValid() ? "valid" : "INVALID");

    BTextView::AttachedToWindow();
    BTextView::MakeFocus(true);
}

void EditorTextView::Draw(BRect updateRect)
{
    BTextView::Draw(updateRect);

    DrawIcons(updateRect);

    // Draw Unicode symbols over ASCII markdown characters
    if (fMarkdownParser) {
        // Get all runs in visible area
        int32 startOffset = OffsetAt(updateRect.LeftTop());
        int32 endOffset = OffsetAt(updateRect.RightBottom());
        if (endOffset < startOffset) endOffset = TextLength();

        std::vector<StyleRun> runs = fMarkdownParser->GetStyleRunsInRange(startOffset, endOffset);

        for (const StyleRun& run : runs) {
            // Only process runs with replacement text
            if (run.text.IsEmpty()) {
                continue;
            }

            // Get the actual text in document
            char* buffer = new char[run.length + 1];
            GetText(run.offset, run.length, buffer);
            buffer[run.length] = '\0';

            // Only draw if different from replacement
            if (strcmp(buffer, run.text.String()) != 0) {
                // Get position (top of line) and height
                float height;
                BPoint topPoint = PointAt(run.offset, &height);

                // Calculate width of text to cover
                float textWidth = run.font.StringWidth(buffer);

                // Create rect covering the ASCII text
                BRect coverRect(topPoint.x, topPoint.y,
                               topPoint.x + textWidth, topPoint.y + height);

                // Cover ASCII with background
                rgb_color viewColor = ViewColor();
                SetHighColor(viewColor);
                FillRect(coverRect);

                // DrawString needs baseline position, not top
                // Get font metrics to calculate baseline
                font_height fh;
                run.font.GetHeight(&fh);
                BPoint baseline(topPoint.x, topPoint.y + fh.ascent);

                // Draw Unicode symbol at baseline
                SetHighColor(run.foreground);
                SetFont(&run.font);
                DrawString(run.text.String(), baseline);
            }

            delete[] buffer;
        }
    }

    if (!fTextHighlights || fTextHighlights->empty()) {
        return;
    }

    // redraw any highlights in sight
    for (const auto& pair : *fTextHighlights) {
        text_highlight* hl = pair.second;
        if (!hl || !hl->region) continue;

        // Check if highlight intersects update rect
        if (!hl->region->Intersects(updateRect)) {
            continue;
        }

        // Recalculate region in case text has reflowed
        BPoint startPoint = PointAt(hl->startOffset);
        BPoint endPoint = PointAt(hl->endOffset);

        font_height fh;
        GetFontHeight(&fh);
        float height = fh.ascent + fh.descent + fh.leading;

        BRect highlightRect(startPoint.x, startPoint.y,
                           endPoint.x, startPoint.y + height);

        if (hl->outline) {
            SetHighColor(*hl->fgColor);
            StrokeRect(highlightRect);
        } else {
            SetHighColor(*hl->bgColor);
            SetDrawingMode(B_OP_ALPHA);
            SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
            FillRect(highlightRect);
            SetDrawingMode(B_OP_COPY);
        }
    }
}

void
EditorTextView::DrawIcons(BRect updateRect)
{
    // Calculate icon size based on line height
    font_height fh;
    GetFontHeight(&fh);
    float lineHeight = ceilf(fh.ascent + fh.descent + fh.leading);
    float iconSize = lineHeight * 0.9f; // 90% of line height for some breathing room
    float yOffset = (lineHeight - iconSize) / 2.0f;

    // Overdraw icon placeholders
    int32 offsetStart = OffsetAt(updateRect.LeftTop());
    int32 offsetEnd   = OffsetAt(updateRect.RightBottom());
    int32 textLength  = offsetEnd - offsetStart + 1;

    char text[textLength];
    GetText(offsetStart, textLength, text);

    for (int32 offset = offsetStart; offset < offsetEnd; offset++) {
        if (strncmp(&text[offset], ICON_PLACEHOLDER_CHAR,
                    strlen(ICON_PLACEHOLDER_CHAR)) == 0) {

            // Extract link URL
            BString linkUrl = ExtractLinkUrl(offset);
            if (linkUrl.Length() == 0) {
                continue;
            }

            // Look up icon
            auto it = fIconCache.find(linkUrl.String());
            if (it != fIconCache.end() && it->second) {
                BBitmap* icon = it->second;

                // Get position and create scaled rect
                BPoint iconPos = PointAt(offset);
                BRect iconRect(
                    iconPos.x,
                    iconPos.y + yOffset,
                    iconPos.x + iconSize,
                    iconPos.y + yOffset + iconSize
                );

                // Draw if in update region
                if (updateRect.Intersects(iconRect)) {
                    SetDrawingMode(B_OP_ALPHA);
                    DrawBitmap(icon, iconRect);
                    SetDrawingMode(B_OP_COPY);
                }
            }

            offset += strlen(ICON_PLACEHOLDER_CHAR) - 1;
        }
    }
}

BString
EditorTextView::ExtractLinkUrl(int32 placeholderOffset)
{
    //FIXME replace this Claude cruft with proper GetNode() in ts_ API
    const char* text = Text();
    int32 textLength = TextLength();

    // Skip placeholder
    int32 pos = placeholderOffset + strlen(ICON_PLACEHOLDER_CHAR);

    // Find opening parenthesis
    while (pos < textLength && text[pos] != '(') {
        if (text[pos] == '\n' || pos - placeholderOffset > 256) {
            return BString();
        }
        pos++;
    }

    if (pos >= textLength || text[pos] != '(') {
        return BString();
    }

    pos++; // Skip '('

    // Extract URL until ')'
    int32 urlStart = pos;
    while (pos < textLength && text[pos] != ')') {
        pos++;
    }

    if (pos >= textLength) {
        return BString();
    }

    BString url(&text[urlStart], pos - urlStart);
    return url;
}

void EditorTextView::SetText(BFile *file, int32 offset, size_t size)
{
    if (!file) return;

    char* buffer = new char[size + 1];
    ssize_t bytesRead = file->ReadAt(offset, buffer, size);

    if (bytesRead > 0) {
        buffer[bytesRead] = '\0';
        SetText(buffer);
    }

    delete[] buffer;
}

void EditorTextView::SetText(const char* text, const text_run_array* runs)
{
    BTextView::SetText(text, runs);

    if (text && fMarkdownParser) {
        MarkupText(text);
    }

    UpdateStatus();
}

void EditorTextView::DeleteText(int32 start, int32 finish)
{
    if (!fMarkdownParser) {
        BTextView::DeleteText(start, finish);
        return;
    }

    // Get line and column info BEFORE deletion
    int32 startLine = LineAt(start);
    int32 oldEndLine = LineAt(finish);
    int32 oldLength = finish - start;

    // Calculate columns (offset from line start)
    int32 startColumn = start - OffsetAt(startLine);
    int32 oldEndColumn = finish - OffsetAt(oldEndLine);

    // Perform deletion
    BTextView::DeleteText(start, finish);

    // Get new end line and column AFTER deletion
    int32 newEndLine = LineAt(start);
    int32 newEndColumn = start - OffsetAt(newEndLine);

    // Get text efficiently using GetText
    int32 textLen = TextLength();
    char* fullText = new char[textLen + 1];
    GetText(0, textLen, fullText);
    fullText[textLen] = '\0';

    // Incremental parse with line and column counts
    fMarkdownParser->ParseIncremental(fullText, start, oldLength, 0,
                                     startLine, startColumn,
                                     oldEndLine, oldEndColumn,
                                     newEndLine, newEndColumn);

    delete[] fullText;

    // Apply styles to affected block (find both start and end)
    int32 blockStart = FindBlockStart(startLine);
    int32 blockEnd = FindBlockEnd(startLine);

    ApplyStyles(blockStart, blockEnd - blockStart);

    // Send outline update notification
    SendOutlineUpdate();

    UpdateStatus();
}

void EditorTextView::InsertText(const char* text, int32 length, int32 offset, const text_run_array* runs)
{
    if (!fMarkdownParser) {
        BTextView::InsertText(text, length, offset, runs);
        return;
    }

    // Get line and column info BEFORE insertion
    int32 startLine = LineAt(offset);
    int32 oldEndLine = startLine;  // Same line before insertion

    // Calculate columns
    int32 startColumn = offset - OffsetAt(startLine);
    int32 oldEndColumn = startColumn;  // Same position before insertion

    // Perform insertion
    BTextView::InsertText(text, length, offset, runs);

    // Get new end line and column AFTER insertion
    int32 newEndLine = LineAt(offset + length);
    int32 newEndColumn = (offset + length) - OffsetAt(newEndLine);

    // Get text efficiently using GetText
    int32 textLen = TextLength();
    char* fullText = new char[textLen + 1];
    GetText(0, textLen, fullText);
    fullText[textLen] = '\0';

    // Incremental parse with line and column counts
    fMarkdownParser->ParseIncremental(fullText, offset, 0, length,
                                     startLine, startColumn,
                                     oldEndLine, oldEndColumn,
                                     newEndLine, newEndColumn);

    delete[] fullText;

    // Apply styles to affected block (find both start and end)
    int32 blockStart = FindBlockStart(startLine);
    int32 blockEnd = FindBlockEnd(startLine);

    ApplyStyles(blockStart, blockEnd - blockStart);

    // Send outline update notification
    SendOutlineUpdate();

    UpdateStatus();
}

void EditorTextView::MessageReceived(BMessage* message)
{
    switch (message->what) {
        case B_REFS_RECEIVED:
            spdlog::debug("B_REFS_RECEIVED.");
        case B_SIMPLE_DATA:
        {
            HandleFileDrop(message);
            break;
        }
        case 'HLIT':
        {
            // Handle highlight message
            int32 start, end;
            int32 colorCode;

            if (message->FindInt32("start", &start) == B_OK &&
                message->FindInt32("end", &end) == B_OK &&
                message->FindInt32("color", &colorCode) == B_OK) {

                // Map color code to actual color (using ColorDefs.h constants)
                // This assumes COLOR_MAGENTA, COLOR_PURPLE, etc. are defined
                rgb_color color;
                switch (colorCode) {
                    case COLOR_MAGENTA:
                        color = {255, 0, 255, 255};
                        break;
                    case COLOR_PURPLE:
                        color = {128, 0, 128, 255};
                        break;
                    case COLOR_GOLD:
                        color = {255, 215, 0, 255};
                        break;
                    case COLOR_CYAN:
                        color = {0, 255, 255, 255};
                        break;
                    default:
                        color = {255, 255, 0, 255}; // yellow fallback
                        break;
                }

                Highlight(start, end, &color, nullptr, false, false);
            }
            break;
        }

        case 'CLRH':
        {
            // Clear highlight in range
            int32 start, end;
            if (message->FindInt32("start", &start) == B_OK &&
                message->FindInt32("end", &end) == B_OK) {
                // Remove highlights in this range
                auto it = fTextHighlights->begin();
                while (it != fTextHighlights->end()) {
                    text_highlight* hl = it->second;
                    if (hl->startOffset >= start && hl->endOffset <= end) {
                        BRect invalidRect = hl->region->Frame();
                        delete hl->region;
                        delete hl;
                        it = fTextHighlights->erase(it);
                        Invalidate(invalidRect);
                    } else {
                        ++it;
                    }
                }
            }
            break;
        }

        default:
            BTextView::MessageReceived(message);
            break;
    }
}

void EditorTextView::MouseDown(BPoint where)
{
    uint32 buttons;
    Window()->CurrentMessage()->FindInt32("buttons", (int32*)&buttons);

    UpdateStatus();

    if (buttons & B_SECONDARY_MOUSE_BUTTON) {
        // get selection offsets
        int32 start, end;
        GetSelection(&start, &end);

        // Right-click: build and show context menu
        if (start != end) {
            BuildContextSelectionMenu();
        } else {
            BuildContextMenu();
        }
    } else {
        BTextView::MouseDown(where);
    }
}

void EditorTextView::MouseUp(BPoint where)
{
    UpdateStatus();
    BTextView::MouseUp(where);
}

void EditorTextView::KeyDown(const char* bytes, int32 numBytes)
{
    UpdateStatus();
    BTextView::KeyDown(bytes, numBytes);
}

void EditorTextView::MarkupText(const char* text)
{
    if (!fMarkdownParser) {
        return;
    }

    fMarkdownParser->Parse(text);
    ApplyStyles(0, strlen(text));
}

int32 EditorTextView::FindBlockStart(int32 line) const
{
    // Find the start of the markdown block containing this line
    if (line <= 0) return 0;

    int32 offset = OffsetAt(line);

    // Walk backwards to find a blank line or start of document
    while (line > 0) {
        int32 lineStart = OffsetAt(line);
        int32 lineEnd = OffsetAt(line + 1);

        // Check if line is blank (only whitespace)
        bool blank = true;
        for (int32 i = lineStart; i < lineEnd; i++) {
            char c = ByteAt(i);
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
                blank = false;
                break;
            }
        }

        if (blank) {
            return OffsetAt(line + 1);  // Start of next line
        }

        line--;
    }

    return 0;
}

int32 EditorTextView::FindBlockEnd(int32 line) const
{
    // Find the end of the markdown block containing this line
    int32 totalLines = CountLines();

    // Walk forward to find a blank line or end of document
    while (line < totalLines - 1) {
        int32 lineStart = OffsetAt(line);
        int32 lineEnd = OffsetAt(line + 1);

        // Check if line is blank
        bool blank = true;
        for (int32 i = lineStart; i < lineEnd; i++) {
            char c = ByteAt(i);
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
                blank = false;
                break;
            }
        }

        if (blank) {
            return lineStart;  // End at start of blank line
        }

        line++;
    }

    return TextLength();
}

void EditorTextView::ApplyStyles(int32 offset, int32 length)
{
    if (!fMarkdownParser || length == 0) {
        return;
    }

    std::vector<StyleRun> runs = fMarkdownParser->GetStyleRunsInRange(offset, offset + length);

    // First, fill entire range with NORMAL style to prevent style bleeding
    BFont normalFont = *fTextFont;
    rgb_color normalColor = GetColorForType(StyleRun::Type::NORMAL);
    SetFontAndColor(offset, offset + length, &normalFont, B_FONT_ALL, &normalColor);

    // Then apply specific styles
    for (const StyleRun& run : runs) {
        int32 start = run.offset;
        int32 end = run.offset + run.length;

        // Clamp to requested range
        if (start < offset) start = offset;
        if (end > offset + length) end = offset + length;

        // Apply font face modifiers
        BFont font = run.font;

        if (run.type == StyleRun::Type::UNDERLINE) {
            font.SetFace(font.Face() | B_UNDERSCORE_FACE);
        }

        if (run.type == StyleRun::Type::STRIKETHROUGH) {
            font.SetFace(font.Face() | B_STRIKEOUT_FACE);
        }

        // Table headers should be bold
        if (run.type == StyleRun::Type::TABLE_HEADER) {
            font.SetFace(font.Face() | B_BOLD_FACE);
        }

        // Apply font and color styling
        SetFontAndColor(start, end, &font, B_FONT_ALL, &run.foreground);
    }

    // Invalidate to trigger Draw() for Unicode symbol rendering
    Invalidate();

    // Notify editor handler about outline update (e.g., for window title)
    // Note: StatusBar outline update happens in UpdateStatus() for better context
    BMessage* outline = fMarkdownParser->GetOutline();
    if (outline) {
        BMessage outlineCopy(*outline);
        fEditorMessenger.SendMessage(&outlineCopy);
    }
}

void EditorTextView::HighlightSelection(const rgb_color *fgColor, const rgb_color *bgColor,
                                        bool generated, bool outline)
{
    int32 start, end;
    GetSelection(&start, &end);

    if (start < end) {
        Highlight(start, end, fgColor, bgColor, generated, outline);
    }
}

void EditorTextView::Highlight(int32 startOffset, int32 endOffset,
                               const rgb_color *fgColor, const rgb_color *bgColor,
                               bool generated, bool outline)
{
    if (!fMarkdownParser) return;

    // Default colors
    static const rgb_color defaultText = {0, 0, 0, 255};
    static const rgb_color defaultBg = {255, 255, 255, 255};

    // Store highlight in our map (offset-based, not node-based)
    text_highlight* highlight = new text_highlight();
    highlight->startOffset = startOffset;
    highlight->endOffset = endOffset;
    highlight->generated = generated;
    highlight->outline = outline;
    highlight->fgColor = fgColor ? fgColor : &defaultText;
    highlight->bgColor = bgColor ? bgColor : &defaultBg;
    highlight->region = new BRegion();

    // Calculate region for this highlight
    BPoint startPoint = PointAt(startOffset);
    BPoint endPoint = PointAt(endOffset);

    font_height fh;
    GetFontHeight(&fh);
    float height = fh.ascent + fh.descent + fh.leading;

    // Simple single-line region for now
    BRect highlightRect(startPoint.x, startPoint.y,
                       endPoint.x, startPoint.y + height);
    highlight->region->Include(highlightRect);

    // Store by start offset
    (*fTextHighlights)[startOffset] = highlight;

    // Invalidate to trigger redraw
    Invalidate(highlightRect);
}

void EditorTextView::ClearHighlights()
{
    if (!fTextHighlights) return;

    // Free all highlight data
    for (auto& pair : *fTextHighlights) {
        delete pair.second->region;
        delete pair.second;
    }
    fTextHighlights->clear();

    Invalidate();
}

void
EditorTextView::HandleFileDrop(BMessage* dropMessage)
{
    // Get drop position
    BPoint dropPoint;
    int32 dropOffset = 0;

    if (dropMessage->FindPoint("_drop_point_", &dropPoint) == B_OK) {
        ConvertFromScreen(&dropPoint);
        dropOffset = OffsetAt(dropPoint);
    } else {
        int32 start, end;
        GetSelection(&start, &end);
        dropOffset = start;
    }

    entry_ref ref;
    status_t result = dropMessage->FindRef("refs", &ref);
    if (result != B_OK) {
        spdlog::warn("no file ref found in drop message!");
        return;
    }

    // Get file info
    BEntry entry(&ref);
    BPath path;
    entry.GetPath(&path);

    // Build link URL
    BString linkText(ref.name);
    BString linkUrl;

    // SEN:ID if available
    char senId[SEN_ID_LEN];
    result = SenConnector::QueryForSenId(&ref, senId);

    if (result == B_OK) {
        linkUrl.SetToFormat("sen://%lld", senId);
    } else {
        // SEN server not running / could not get ID -> fall back to path
        linkUrl.SetToFormat("file://%s", path.Path());
    }

    // Load and cache icon
    BBitmap icon(BRect(0, 0, 31, 31), B_CMAP8);
    result = LoadFileIcon(&ref, &icon, B_LARGE_ICON);

    if (result == B_OK) {
        fIconCache[linkUrl.String()] = new BBitmap(icon);
    } else {
        spdlog::warn("failed to retrieve icon for file {}: {}", path.Path(), strerror(result));
    }

    // Build markdown link with icon placeholder
    BString markdownLink;
    markdownLink.SetToFormat("%s[%s](%s)",
        ICON_PLACEHOLDER_CHAR, linkText.String(), linkUrl.String());

    // Insert at drop position
    Insert(dropOffset, markdownLink.String(), markdownLink.Length());
    Select(dropOffset + markdownLink.Length(), dropOffset + markdownLink.Length());

    // Re-parse for syntax highlighting
    MarkupText(Text());
}

status_t EditorTextView::LoadFileIcon(const entry_ref* ref, BBitmap* icon, icon_size size)
{
    // for the icon, always get the original file
    BEntry targetEntry(ref, true);
    spdlog::debug("target entry for ref {} is: {}", ref->name, targetEntry.Name());
    BNode node(&targetEntry);
    spdlog::debug("node status is {}", strerror(node.InitCheck()) );
    BNodeInfo nodeInfo(&node);

    // first, try node icon for specific icon
    status_t result = nodeInfo.GetIcon(icon, size);
    if (result != B_OK) {
        spdlog::debug("could not get icon for node: {}", strerror(result));
        // get generic icon from MIME DB
        char type[B_MIME_TYPE_LENGTH];
        nodeInfo.GetType(type);
        spdlog::debug("get MIME icon for type {}", type);

        BMimeType mimeType(type);
        result = mimeType.GetIcon(icon, size);
    }

    return result;
}

BMessage* EditorTextView::GetOutlineAt(int32 offset, bool withNames)
{
    if (!fMarkdownParser) {
        return nullptr;
    }

    TSNode node = fMarkdownParser->GetHeadingAtOffset(offset);
    if (ts_node_is_null(node)) {
        return nullptr;
    }

    BMessage* outline = new BMessage();
    outline->what = MSG_OUTLINE;
    outline->AddString("type", "single");

    fMarkdownParser->ExtractHeadingInfo(node, outline, withNames);

    return outline;
}

BMessage* EditorTextView::GetDocumentOutline(bool withNames, bool withDetails)
{
    if (!fMarkdownParser) {
        return new BMessage();
    }

    return fMarkdownParser->GetOutline();
}

BMessage* EditorTextView::GetHeadingContext(int32 offset)
{
    if (!fMarkdownParser) {
        return nullptr;
    }

    BMessage* context = new BMessage();
    fMarkdownParser->GetHeadingContext(offset, context);

    return context->IsEmpty() ? nullptr : context;
}

BMessage* EditorTextView::GetHeadingsInRange(int32 startOffset, int32 endOffset)
{
    if (!fMarkdownParser) {
        return nullptr;
    }

    std::vector<TSNode> allHeadings = fMarkdownParser->FindAllHeadings();

    BMessage* result = new BMessage();
    result->what = MSG_OUTLINE;
    result->AddString("type", "range");
    result->AddInt32("start_offset", startOffset);
    result->AddInt32("end_offset", endOffset);

    for (const TSNode& heading : allHeadings) {
        int32 offset = ts_node_start_byte(heading);
        if (offset >= startOffset && offset < endOffset) {
            BMessage headingMsg;
            fMarkdownParser->ExtractHeadingInfo(heading, &headingMsg, true);
            result->AddMessage("heading", &headingMsg);
        }
    }

    return result->IsEmpty() ? nullptr : result;
}

BMessage* EditorTextView::GetSiblingHeadings(int32 offset)
{
    if (!fMarkdownParser) {
        return nullptr;
    }

    TSNode heading = fMarkdownParser->GetHeadingAtOffset(offset);
    if (ts_node_is_null(heading)) {
        return nullptr;
    }

    std::vector<TSNode> siblings = fMarkdownParser->FindSiblingHeadings(heading);

    BMessage* result = new BMessage();
    result->what = MSG_OUTLINE;
    result->AddString("type", "siblings");
    result->AddInt32("reference_offset", offset);

    for (const TSNode& sibling : siblings) {
        BMessage headingMsg;
        fMarkdownParser->ExtractHeadingInfo(sibling, &headingMsg, true);
        result->AddMessage("heading", &headingMsg);
    }

    return result->IsEmpty() ? nullptr : result;
}

void EditorTextView::UpdateStatus()
{
    if (!fStatusBar) return;

    int32 start, end;
    GetSelection(&start, &end);
    int32 currentOffset = start;

    // Get line number from parser
    int32 line = fMarkdownParser ? fMarkdownParser->GetLineForOffset(start) : 0;

    int32 column = 0;
    if (TextLength() > 0) {
        column = start - OffsetAt(CurrentLine());
    }

    // Update position/selection display
    if (start == end) {
        fStatusBar->UpdatePosition(start, line, column);
    } else {
        fStatusBar->UpdateSelection(start, end);
    }

    // Update outline context (breadcrumb trail) using fast TreeSitter query
    BMessage* context = GetHeadingContext(currentOffset);
    if (context) {
        fStatusBar->UpdateOutline(context);
        delete context;
    } else {
        fStatusBar->UpdateOutline(nullptr);
    }

    // update outline position in outline panel
    BMessage updateSelection(MSG_SELECTION_CHANGED);
    updateSelection.AddInt32("offsetStart", start);
    updateSelection.AddInt32("offsetEnd", end);

    fEditorMessenger.SendMessage(&updateSelection);
}

void EditorTextView::BuildContextMenu()
{
    // TODO: Implement general context menu
}

void EditorTextView::BuildContextSelectionMenu()
{
    int32 start, end;
    GetSelection(&start, &end);

    if (start == end) {
        return;
    }

    if (start > end) {
        std::swap(start, end);
    }

    BPopUpMenu* menu = new BPopUpMenu("selection_context");

    // Add highlight options with semantic colors
    BMessage* personMsg = new BMessage('HLIT');
    personMsg->AddInt32("start", start);
    personMsg->AddInt32("end", end);
    personMsg->AddInt32("color", COLOR_MAGENTA);
    menu->AddItem(new BMenuItem("Highlight as Person", personMsg));

    BMessage* locationMsg = new BMessage('HLIT');
    locationMsg->AddInt32("start", start);
    locationMsg->AddInt32("end", end);
    locationMsg->AddInt32("color", COLOR_PURPLE);
    menu->AddItem(new BMenuItem("Highlight as Location", locationMsg));

    BMessage* topicMsg = new BMessage('HLIT');
    topicMsg->AddInt32("start", start);
    topicMsg->AddInt32("end", end);
    topicMsg->AddInt32("color", COLOR_GOLD);
    menu->AddItem(new BMenuItem("Highlight as Topic", topicMsg));

    BMessage* contextMsg = new BMessage('HLIT');
    contextMsg->AddInt32("start", start);
    contextMsg->AddInt32("end", end);
    contextMsg->AddInt32("color", COLOR_CYAN);
    menu->AddItem(new BMenuItem("Highlight as Context", contextMsg));

    menu->AddSeparatorItem();

    BMessage* clearMsg = new BMessage('CLRH');
    clearMsg->AddInt32("start", start);
    clearMsg->AddInt32("end", end);
    menu->AddItem(new BMenuItem("Clear Highlight", clearMsg));

    menu->SetTargetForItems(this);

    BPoint point = PointAt(start);
    ConvertToScreen(&point);

    menu->SetAsyncAutoDestruct(true);
    menu->Go(point, true, false, true);
}

void EditorTextView::RedrawHighlight(text_highlight *highlight)
{
    if (!highlight || !highlight->region) return;

    Invalidate(highlight->region->Frame());
}

void EditorTextView::SendOutlineUpdate()
{
    if (!fMarkdownParser) return;

    BMessage update(MSG_OUTLINE_UPDATE);
    BMessage* outline = fMarkdownParser->GetOutline();
    if (outline) {
        update.AddMessage("outline", outline);
    }

    spdlog::debug("SendOutlineUpdate...");

    fEditorMessenger.SendMessage(&update);
}
