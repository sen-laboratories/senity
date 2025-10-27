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

// Structure to store in cmark node's user_data
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
    // Base text drawing
    BTextView::Draw(updateRect);

    if (!fTextHighlights || fTextHighlights->empty()) return;

    // Draw all highlights
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
    CalculateAndMarkupRange(start, finish - start);

    UpdateStatus();
}

void EditorTextView::InsertText(const char* text, int32 length, int32 offset, const text_run_array* runs)
{
    BTextView::InsertText(text, length, offset, runs);
    CalculateAndMarkupRange(offset, length);

    UpdateStatus();
}

void EditorTextView::CalculateAndMarkupRange(int32 offset, int32 length)
{
    if (TextLength() > 0 && fMarkdownParser) {
        // find the stretch from offset to affected line
        int32 startLine = LineAt(offset);
        int32 startOffset = OffsetAt(startLine);

        int32 endLine = LineAt(offset + length);
        if (startLine == endLine)
            endLine++;

        int32 endOffset = OffsetAt(endLine);

        // Ensure proper ordering
        if (startOffset > endOffset) {
            std::swap(startOffset, endOffset);
            std::swap(startLine, endLine);
        }

        // Re-parse affected region - this does full parse now (safer)
        MarkupRange(startLine, endLine, startOffset, endOffset);
    }
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
    cmark_node* node = fMarkdownParser->GetNodeAtOffset(offset);

    if (node) {
        // Check for link
        if (cmark_node_get_type(node) == CMARK_NODE_LINK) {
            const char* url = cmark_node_get_url(node);
            if (url) {
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
        cmark_node* node = fMarkdownParser->GetNodeAtOffset(offset);

        if (node && cmark_node_get_type(node) == CMARK_NODE_LINK) {
            SetViewCursor(&linkCursor);
        } else {
            // TODO: handle contextMenuCursor
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
    const BMessage& outline = fMarkdownParser->GetOutline();
    if (fEditorHandler) {
        BMessage outlineMsg('OUTL');
        outlineMsg.AddMessage("outline", &outline);
        BMessenger(fEditorHandler).SendMessage(&outlineMsg);
    }
}

void EditorTextView::MarkupRange(int32 startLine, int32 endLine, int32 startOffset, int32 endOffset)
{
    if (TextLength() == 0 || !fMarkdownParser) return;

    // Get full text (parser needs it for correct offset calculations)
    const char* fullText = Text();

    // Use incremental parsing
    if (fMarkdownParser->ParseIncremental(fullText, startLine, endLine)) {
        // Reset styling in affected range
        SetFontAndColor(startOffset, endOffset, fTextFont, B_FONT_ALL, &textColor);

        // Apply style runs
        const std::vector<StyleRun>& runs = fMarkdownParser->GetStyleRuns();
        for (const StyleRun& run : runs) {
            int32 runEnd = run.offset + run.length;

            if (runEnd > startOffset && run.offset < endOffset) {
                SetFontAndColor(
                    run.offset,
                    run.offset + run.length,
                    &run.font,
                    B_FONT_ALL,
                    &run.foreground
                );
            }
        }
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
    BMessage* msg = new BMessage('NODE');

    if (!fMarkdownParser) {
        return msg;  // Return empty message, not nullptr
    }

    cmark_node* node = fMarkdownParser->GetNodeAtOffset(offset);
    if (!node) {
        return msg;  // Return empty message, not nullptr
    }

    // Walk up to find heading
    while (node && cmark_node_get_type(node) != CMARK_NODE_HEADING) {
        node = cmark_node_parent(node);
    }

    if (!node || cmark_node_get_type(node) != CMARK_NODE_HEADING) {
        return msg;  // Return empty message, not nullptr
    }

    // Build message for this heading
    msg->AddInt32("level", cmark_node_get_heading_level(node));
    msg->AddInt32("line", cmark_node_get_start_line(node));
    msg->AddInt32("offset", offset);

    // Extract heading text
    BString headingText;
    cmark_node* child = cmark_node_first_child(node);
    while (child) {
        if (cmark_node_get_type(child) == CMARK_NODE_TEXT) {
            const char* text = cmark_node_get_literal(child);
            if (text) headingText << text;
        }
        child = cmark_node_next(child);
    }

    if (withNames && headingText.Length() > 0) {
        msg->AddString("text", headingText);
    }

    return msg;
}

BMessage* EditorTextView::GetDocumentOutline(bool withNames, bool withDetails)
{
    if (!fMarkdownParser) {
        return new BMessage();  // Return empty message, not nullptr
    }

    const BMessage& outline = fMarkdownParser->GetOutline();

    // Return a copy
    BMessage* copy = new BMessage(outline);
    return copy;
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

void EditorTextView::AdjustHighlightsForInsert(int32 offset, int32 length)
{
    if (!fTextHighlights || fTextHighlights->empty()) return;

    // Shift all highlights after the insert point
    std::map<int32, text_highlight*> adjusted;

    for (auto& pair : *fTextHighlights) {
        text_highlight* hl = pair.second;

        if (hl->startOffset >= offset) {
            // Highlight is entirely after insert - shift it
            hl->startOffset += length;
            hl->endOffset += length;
        } else if (hl->endOffset > offset) {
            // Highlight spans the insert point - extend it
            hl->endOffset += length;
        }

        // Re-insert with new start offset as key
        adjusted[hl->startOffset] = hl;
    }

    *fTextHighlights = adjusted;
}

void EditorTextView::AdjustHighlightsForDelete(int32 start, int32 finish)
{
    if (!fTextHighlights || fTextHighlights->empty()) return;

    int32 length = finish - start;
    std::map<int32, text_highlight*> adjusted;

    for (auto& pair : *fTextHighlights) {
        text_highlight* hl = pair.second;

        if (hl->endOffset <= start) {
            // Highlight is entirely before delete - keep as is
            adjusted[hl->startOffset] = hl;
        } else if (hl->startOffset >= finish) {
            // Highlight is entirely after delete - shift it back
            hl->startOffset -= length;
            hl->endOffset -= length;
            adjusted[hl->startOffset] = hl;
        } else if (hl->startOffset >= start && hl->endOffset <= finish) {
            // Highlight is entirely within deleted range - remove it
            delete hl->region;
            delete hl;
        } else {
            // Highlight partially overlaps - adjust boundaries
            if (hl->startOffset < start) {
                hl->endOffset = start;  // Trim end
            } else {
                hl->startOffset = start;  // Trim start
            }
            hl->endOffset -= length;
            adjusted[hl->startOffset] = hl;
        }
    }

    *fTextHighlights = adjusted;
}
