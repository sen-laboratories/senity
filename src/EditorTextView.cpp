/*
 * Copyright 2024-2025, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "EditorTextView.h"

#include <Clipboard.h>
#include <File.h>
#include <MenuItem.h>
#include <Message.h>
#include <Messenger.h>
#include <Region.h>
#include <Window.h>

#include <cstdio>
#include <cstring>
#include <set>

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

    if (!fMarkdownParser) return;

    // Draw highlights stored in cmark nodes
    int32 startOffset = OffsetAt(updateRect.LeftTop());
    int32 endOffset = OffsetAt(updateRect.RightBottom());

    // Prevent going out of bounds
    int32 textLen = TextLength();
    if (endOffset > textLen) endOffset = textLen;
    if (startOffset < 0) startOffset = 0;

    // Track which nodes we've already drawn to avoid duplicates
    std::set<cmark_node*> drawnNodes;

    // Walk through visible text and check for highlighted nodes
    for (int32 offset = startOffset; offset <= endOffset; offset++) {
        cmark_node* node = fMarkdownParser->GetNodeAtOffset(offset);
        if (!node) continue;

        // Skip if already drawn
        if (drawnNodes.find(node) != drawnNodes.end()) {
            continue;
        }

        // Safely get highlight data
        void* userData = cmark_node_get_user_data(node);
        if (!userData) continue;

        NodeHighlightData* highlight = static_cast<NodeHighlightData*>(userData);

        // Validate the pointer by checking reasonable values
        if (!highlight->isHighlighted) {
            continue;
        }

        drawnNodes.insert(node);

        // Get the full node range
        int32 nodeStart = offset;
        int32 nodeEnd = offset + 20; // Estimate, ideally calculate from node

        if (nodeEnd > textLen) nodeEnd = textLen;

        BPoint startPoint = PointAt(nodeStart);
        BPoint endPoint = PointAt(nodeEnd);

        font_height fh;
        GetFontHeight(&fh);
        float height = fh.ascent + fh.descent + fh.leading;

        BRect highlightRect(startPoint.x, startPoint.y,
                          endPoint.x, startPoint.y + height);

        if (highlight->isOutline) {
            SetHighColor(highlight->fgColor);
            StrokeRect(highlightRect);
        } else {
            SetHighColor(highlight->bgColor);
            FillRect(highlightRect);
        }

        // Skip to end of this node
        offset = nodeEnd;
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

    // Partial re-parse
    const char* text = Text();
    if (text && fMarkdownParser) {
        int32 line = fMarkdownParser->GetLineForOffset(start);
        int32 endLine = fMarkdownParser->GetLineForOffset(finish);

        // Re-parse affected region
        MarkupRange(text, line, endLine);
    }

    UpdateStatus();
}

void EditorTextView::InsertText(const char* text, int32 length, int32 offset,
                                 const text_run_array* runs)
{
    BTextView::InsertText(text, length, offset, runs);

    // Full re-parse immediately
    const char* fullText = Text();
    if (fullText && fMarkdownParser) {
        // Clear ALL styling first
        SetFontAndColor(0, TextLength(), fTextFont, B_FONT_ALL, &textColor);

        // Then re-apply markdown
        MarkupText(fullText);
    }

    UpdateStatus();
}

void EditorTextView::MessageReceived(BMessage* message)
{
    switch (message->what) {
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
    BTextView::MouseDown(where);

    // Check if clicking on a link or highlighted node
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

        // Check for highlighted node
        NodeHighlightData* highlight = (NodeHighlightData*)cmark_node_get_user_data(node);
        if (highlight && highlight->isHighlighted) {
            // Handle highlight click
            BMessage highlightMsg('HLIT');
            highlightMsg.AddInt32("offset", offset);

            if (fEditorHandler) {
                fEditorHandler->MessageReceived(&highlightMsg);
            }
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
            // Change cursor to hand over links
            SetViewCursor(B_CURSOR_SYSTEM_DEFAULT); // TODO: use hand cursor
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
    const BMessage& outline = fMarkdownParser->GetOutline();
    if (fEditorHandler) {
        BMessage outlineMsg('OUTL');
        outlineMsg.AddMessage("outline", &outline);
        BMessenger(fEditorHandler).SendMessage(&outlineMsg);
    }
}

void EditorTextView::MarkupRange(const char* text, int32 startLine, int32 endLine)
{
    if (!text || !fMarkdownParser) return;

    // Use incremental parsing
    if (!fMarkdownParser->ParseIncremental(text, startLine, endLine)) {
        // Fall back to full parse if incremental fails
        if (!fMarkdownParser->Parse(text)) {
            return;
        }
    }

    // Apply all style runs (incremental parse updates the full run list)
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

    // Get all nodes in this range
    for (int32 offset = startOffset; offset < endOffset; offset++) {
        cmark_node* node = fMarkdownParser->GetNodeAtOffset(offset);
        if (!node) continue;

        // Check if already has highlight data
        NodeHighlightData* data = (NodeHighlightData*)cmark_node_get_user_data(node);
        if (!data) {
            data = new NodeHighlightData();
            cmark_node_set_user_data(node, data);
        }

        // Set highlight properties
        data->isHighlighted = true;
        data->fgColor = fgColor ? *fgColor : textColor;
        data->bgColor = bgColor ? *bgColor : ui_color(B_DOCUMENT_BACKGROUND_COLOR);
        data->isOutline = outline;
        data->isGenerated = generated;

        // Skip to next node boundary to avoid setting same node multiple times
        // (This is a simple optimization)
    }

    // Invalidate the highlighted region
    BPoint startPoint = PointAt(startOffset);
    BPoint endPoint = PointAt(endOffset);
    BRect invalidRect(startPoint, endPoint);
    invalidRect.InsetBy(-2, -2);
    Invalidate(invalidRect);
}

void EditorTextView::ClearHighlights()
{
    if (!fMarkdownParser) return;

    // Get document root
    cmark_node* node = fMarkdownParser->GetNodeAtOffset(0);
    if (!node) return;

    // Find document root
    while (cmark_node_parent(node)) {
        node = cmark_node_parent(node);
    }

    // Walk tree and free all user_data
    cmark_iter* iter = cmark_iter_new(node);
    cmark_event_type ev_type;

    while ((ev_type = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
        if (ev_type == CMARK_EVENT_ENTER) {
            cmark_node* cur = cmark_iter_get_node(iter);
            NodeHighlightData* data = (NodeHighlightData*)cmark_node_get_user_data(cur);

            if (data) {
                delete data;
                cmark_node_set_user_data(cur, nullptr);
            }
        }
    }

    cmark_iter_free(iter);

    // Clear old highlight map (legacy)
    for (auto& pair : *fTextHighlights) {
        delete pair.second->region;
        delete pair.second;
    }
    fTextHighlights->clear();

    Invalidate();
}

BMessage* EditorTextView::GetOutlineAt(int32 offset, bool withNames)
{
    if (!fMarkdownParser) return nullptr;

    cmark_node* node = fMarkdownParser->GetNodeAtOffset(offset);
    if (!node) return nullptr;

    // Walk up to find heading
    while (node && cmark_node_get_type(node) != CMARK_NODE_HEADING) {
        node = cmark_node_parent(node);
    }

    if (!node || cmark_node_get_type(node) != CMARK_NODE_HEADING) {
        return nullptr;
    }

    // Build message for this heading
    BMessage* msg = new BMessage('NODE');
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
    if (!fMarkdownParser) return nullptr;

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

    // Calculate column: count characters from start of line to cursor
    int32 column = 0;
    const char* text = Text();
    if (text) {
        // Find start of current line
        int32 lineStart = start;
        while (lineStart > 0 && text[lineStart - 1] != '\n') {
            lineStart--;
        }

        // Column is offset from line start
        column = start - lineStart;
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
    // TODO: Implement context menu
}

void EditorTextView::BuildContextSelectionMenu()
{
    // TODO: Implement selection context menu
}

void EditorTextView::RedrawHighlight(text_highlight *highlight)
{
    if (!highlight || !highlight->region) return;

    Invalidate(highlight->region->Frame());
}
