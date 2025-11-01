/*
 * Copyright 2024-2025, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "ColorDefs.h"
#include "EditorTextView.h"

#include <iostream>
#include <Clipboard.h>
#include <File.h>
#include <MenuItem.h>
#include <Message.h>
#include <Messenger.h>
#include <Region.h>
#include <Window.h>

#include <cstdio>
#include <cstring>

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

    // Configure parser fonts and colors
    fMarkdownParser->SetFont(StyleRun::NORMAL, *fTextFont);
    fMarkdownParser->SetFont(StyleRun::CODE_INLINE, *fCodeFont);
    fMarkdownParser->SetFont(StyleRun::CODE_BLOCK, *fCodeFont);
    fMarkdownParser->SetFont(StyleRun::LINK, *fLinkFont);

    fMarkdownParser->SetColor(StyleRun::NORMAL, textColor);
    fMarkdownParser->SetColor(StyleRun::LINK, linkColor);
    fMarkdownParser->SetColor(StyleRun::CODE_INLINE, codeColor);
    fMarkdownParser->SetColor(StyleRun::CODE_BLOCK, codeColor);

    // Heading fonts
    for (int i = 0; i < 6; i++) {
        BFont headingFont(be_bold_font);
        headingFont.SetSize(24 - i * 2);
        fMarkdownParser->SetFont((StyleRun::Type)(StyleRun::HEADING_1 + i), headingFont);
        fMarkdownParser->SetColor((StyleRun::Type)(StyleRun::HEADING_1 + i), headerColor);
    }

    SetStylable(true);
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

    // Clean up highlight map (now mostly unused but keep for compatibility)
    if (fTextHighlights) {
        for (auto& pair : *fTextHighlights) {
            delete pair.second->region;
            delete pair.second;
        }
        delete fTextHighlights;
    }
}

void EditorTextView::Draw(BRect updateRect)
{
    BTextView::Draw(updateRect);

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
            std::cout << "redraw outline" << std::endl;
            SetHighColor(*hl->fgColor);
            StrokeRect(highlightRect);
        } else {
            std::cout << "redraw highlight" << std::endl;
            SetHighColor(*hl->bgColor);
            SetDrawingMode(B_OP_ALPHA);
            SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
            FillRect(highlightRect);
            SetDrawingMode(B_OP_COPY);
        }
    }
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
    BTextView::DeleteText(start, finish);

    if (fMarkdownParser) {
        MarkupText(Text());
    }

    UpdateStatus();
}

void EditorTextView::InsertText(const char* text, int32 length, int32 offset, const text_run_array* runs)
{
    BTextView::InsertText(text, length, offset, runs);

    if (fMarkdownParser) {
        MarkupText(Text());
    }

    UpdateStatus();
}

void EditorTextView::MessageReceived(BMessage* message)
{
    switch (message->what) {
        case 'HLIT':
        {
            std::cout << "got highlight message:" << std::endl;
            message->PrintToStream();

            // Apply highlight from context menu
            int32 start, end, colorIndex;
            if (message->FindInt32("start", &start) == B_OK &&
                message->FindInt32("end", &end) == B_OK &&
                message->FindInt32("color", &colorIndex) == B_OK) {

                ColorDefs colorDefs;
                rgb_color* color = colorDefs.GetColor((COLOR_NAME)colorIndex);

                if (color) {
                    // Light background version of the color (add alpha/blend)
                    rgb_color bgColor = *color;
                    bgColor.alpha = 80;  // Semi-transparent

                    Highlight(start, end, color, &bgColor, false, false);
                }
            }
            break;
        }

        case 'CLRH':
        {
            // Clear highlights in range
            int32 start, end;
            if (message->FindInt32("start", &start) == B_OK &&
                message->FindInt32("end", &end) == B_OK) {

                // Remove highlights that overlap this range
                std::vector<int32> toRemove;
                for (auto& pair : *fTextHighlights) {
                    text_highlight* hl = pair.second;
                    if (hl->startOffset < end && hl->endOffset > start) {
                        toRemove.push_back(pair.first);
                    }
                }

                for (int32 key : toRemove) {
                    auto it = fTextHighlights->find(key);
                    if (it != fTextHighlights->end()) {
                        delete it->second->region;
                        delete it->second;
                        fTextHighlights->erase(it);
                    }
                }

                Invalidate();
            }
            break;
        }

        case B_COPY:
        case B_CUT:
        case B_PASTE:
            BTextView::MessageReceived(message);
            UpdateStatus();
            break;

        default:
            BTextView::MessageReceived(message);
            break;
    }
}

void EditorTextView::KeyDown(const char* bytes, int32 numBytes)
{
    BTextView::KeyDown(bytes, numBytes);
    UpdateStatus();
}

void EditorTextView::MouseDown(BPoint where)
{
    uint32 buttons = 0;
    Window()->CurrentMessage()->FindInt32("buttons", (int32*)&buttons);

    // Right-click - show context menu
    if (buttons & B_SECONDARY_MOUSE_BUTTON) {
        int32 start, end;
        GetSelection(&start, &end);

        if (start < end) {
            // Has selection - show highlight menu
            BuildContextSelectionMenu();
        } else {
            // No selection - show general menu
            BuildContextMenu();
        }
        return;
    }

    // Normal left-click handling
    BTextView::MouseDown(where);

    // Check if clicking on a link
    int32 offset = OffsetAt(where);
    TSNode node = fMarkdownParser->GetNodeAtOffset(offset);

    if (!ts_node_is_null(node)) {
        // Check for link
        const char* nodeType = ts_node_type(node);
        if (strcmp(nodeType, "inline_link") == 0 || strcmp(nodeType, "shortcut_link") == 0) {
            // Get URL from link_destination child
            TSNode destNode = ts_node_child_by_field_name(node, "link_destination", 16);
            if (!ts_node_is_null(destNode)) {
                uint32_t start = ts_node_start_byte(destNode);
                uint32_t end = ts_node_end_byte(destNode);
                const char* source = Text();
                BString url(source + start, end - start);

                // Handle link click - send message to editor handler
                BMessage linkMsg('LINK');
                linkMsg.AddString("url", url);
                linkMsg.AddInt32("offset", offset);

                if (fEditorHandler) {
                    fEditorHandler->MessageReceived(&linkMsg);
                }
                return;
            }
        }
    }

    // Check if clicking on a highlight (offset-based, not node-based)
    for (const auto& pair : *fTextHighlights) {
        text_highlight* hl = pair.second;
        if (offset >= hl->startOffset && offset < hl->endOffset) {
            // Handle highlight click
            BMessage highlightMsg('HLIT');
            highlightMsg.AddInt32("offset", offset);
            highlightMsg.AddInt32("start", hl->startOffset);
            highlightMsg.AddInt32("end", hl->endOffset);

            if (fEditorHandler) {
                fEditorHandler->MessageReceived(&highlightMsg);
            }
            return;
        }
    }

    UpdateStatus();
}

void EditorTextView::MouseMoved(BPoint where, uint32 code, const BMessage* dragMessage)
{
    BTextView::MouseMoved(where, code, dragMessage);

    if (code == B_INSIDE_VIEW) {
        int32 offset = OffsetAt(where);
        TSNode node = fMarkdownParser->GetNodeAtOffset(offset);

        if (!ts_node_is_null(node)) {
            const char* nodeType = ts_node_type(node);
            if (strcmp(nodeType, "inline_link") == 0 || strcmp(nodeType, "shortcut_link") == 0) {
                SetViewCursor(&linkCursor);
            } else {
                // TODO: handle contextMenuCursor
                SetViewCursor(B_CURSOR_I_BEAM);
            }
        } else {
            SetViewCursor(B_CURSOR_I_BEAM);
        }
    }
}

void EditorTextView::MarkupText(const char* text)
{
    if (!text || !fMarkdownParser) return;

    // Full parse
    if (!fMarkdownParser->Parse(text)) {
        return;
    }

    // Apply all style runs
    const std::vector<StyleRun>& runs = fMarkdownParser->GetStyleRuns();

    for (const StyleRun& run : runs) {
        if (run.offset < 0 || run.length <= 0) continue;

        SetFontAndColor(
            run.offset,
            run.offset + run.length,
            &run.font,
            B_FONT_ALL,
            &run.foreground
        );
    }

    // Notify editor handler about outline update
    BMessage* outline = fMarkdownParser->GetOutline();
    if (fEditorHandler) {
        BMessenger(fEditorHandler).SendMessage(outline);
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

    // Store highlight in our map (offset-based, not node-based)
    text_highlight* highlight = new text_highlight();
    highlight->startOffset = startOffset;
    highlight->endOffset = endOffset;
    highlight->generated = generated;
    highlight->outline = outline;
    highlight->fgColor = fgColor ? fgColor : &textColor;
    highlight->bgColor = bgColor ? bgColor : &backgroundColor;
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

BMessage* EditorTextView::GetOutlineAt(int32 offset, bool withNames)
{
    BMessage* msg = new BMessage('OUTL');

    if (!fMarkdownParser) {
        return msg;  // Return empty message, not nullptr
    }

    TSNode node = fMarkdownParser->GetNodeAtOffset(offset);
    if (ts_node_is_null(node)) {
        return msg;  // Return empty message, not nullptr
    }

    // Check if this is a heading node
    const char* nodeType = ts_node_type(node);
    if (strcmp(nodeType, "atx_heading") != 0) {
        // Not a heading, return empty
        return msg;
    }

    // Get heading level
    int level = 1;
    uint32_t childCount = ts_node_child_count(node);
    for (uint32_t i = 0; i < childCount; i++) {
        TSNode child = ts_node_child(node, i);
        const char* childType = ts_node_type(child);

        if (strncmp(childType, "atx_h", 5) == 0 && childType[5] >= '1' && childType[5] <= '6') {
            level = childType[5] - '0';
            break;
        }
    }

    // Get line number
    int32 line = fMarkdownParser->GetLineForOffset(offset);

    // Build message
    msg->AddInt32("level", level);
    msg->AddInt32("line", line);
    msg->AddInt32("offset", offset);

    // Extract heading text if requested
    if (withNames) {
        BString headingText;
        for (uint32_t i = 0; i < childCount; i++) {
            TSNode child = ts_node_child(node, i);
            if (strcmp(ts_node_type(child), "heading_content") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                const char* source = Text();
                headingText.SetTo(source + start, end - start);
                headingText.Trim();
                break;
            }
        }

        if (headingText.Length() > 0) {
            msg->AddString("text", headingText);
        }
    }

    return msg;
}

BMessage* EditorTextView::GetDocumentOutline(bool withNames, bool withDetails)
{
    if (!fMarkdownParser) {
        return new BMessage();  // Return empty message, not nullptr
    }

    return fMarkdownParser->GetOutline();
}

void EditorTextView::UpdateStatus()
{
    if (!fStatusBar) return;

    int32 start, end;
    GetSelection(&start, &end);

    // Get line number from parser
    int32 line = fMarkdownParser ? fMarkdownParser->GetLineForOffset(start) : 0;

    int32 column = 0;
    if (TextLength() > 0) {
        column = start - OffsetAt(CurrentLine());
    }

    if (start == end) {
        // Just cursor position, no selection
        fStatusBar->UpdatePosition(start, line, column);
    } else {
        // Has selection
        fStatusBar->UpdateSelection(start, end);
    }
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
        return; // no selection
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

    // Show menu at selection
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
