/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include <MenuItem.h>
#include <assert.h>
#include <GradientLinear.h>
#include <Messenger.h>
#include <Polygon.h>
#include <Region.h>
#include <ScrollView.h>
#include <stdio.h>
#include <Window.h>

#include "MarkdownParser.h"
#include "SyntaxHighlighter.h"
#include "EditorTextView.h"
#include "Messages.h"
#include "MessageUtil.h"

using namespace std;

EditorTextView::EditorTextView(StatusBar *statusBar, BHandler *editorHandler)
: BTextView("editor_text_view")
{
	SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
	SetLowUIColor(ViewUIColor());

    MakeEditable(true);
    SetStylable(true);
    SetDoesUndo(true);
    SetWordWrap(false);
    SetFontAndColor(be_plain_font);

    fStatusBar = statusBar;
    fEditorHandler = editorHandler;

    // setup markdown syntax parser and styler
    fMarkdownParser = new MarkdownParser();

    // Setup fonts
    BFont textFont(be_plain_font);
    BFont codeFont(be_fixed_font);
    BFont headingFont(be_bold_font);

    for (int i = 1; i <= 6; i++) {
        headingFont.SetSize(24 - (i - 1) * 2);
        fMarkdownParser->SetHeadingFont(i, headingFont);
    }

    /* Initialize styles - reuse your existing style setup
    fTextStyle.color = {0, 0, 0, 255};
    fTextStyle.spacing = 1.2f;

    fCodeStyle.color = {60, 60, 60, 255};
    fCodeStyle.spacing = 1.0f;

    // Setup heading styles
    for (int i = 0; i < 6; i++) {
        fHeadingStyles[i].color = {0, 0, 0, 255};
        fHeadingStyles[i].font = *be_bold_font;
        fHeadingStyles[i].font.SetSize(24 - i * 2);
    }*/

    fMarkdownParser->SetTextFont(textFont);
    fMarkdownParser->SetCodeFont(codeFont);

    // Enable syntax highlighting
    fSyntaxHighlighter = new SyntaxHighlighter();
    fMarkdownParser->SetSyntaxHighlighter(fSyntaxHighlighter);

    fTextHighlights = new map<int32, text_highlight*>();
}

EditorTextView::~EditorTextView() {
    RemoveSelf();

    delete fMarkdownParser;
    delete fSyntaxHighlighter;

    for (auto highlight : *fTextHighlights) {
        free(highlight.second);
    }
    fTextHighlights->clear();
    delete fTextHighlights;
}

void EditorTextView::MessageReceived(BMessage* message) {
    switch (message->what) {
        case MSG_INSERT_ENTITY:
        {
            printf("TV: insert entity:\n");
            const char* label = message->GetString(MSG_PROP_LABEL);
            if (label != NULL) {
                printf("insert entity %s\n", label);
            }
            break;
        }
        default:
        {
            BTextView::MessageReceived(message);
        }
    }
}

void EditorTextView::SetText(const char* text, const text_run_array* runs) {
    ClearHighlights();

    BTextView::SetText(text, runs);
    MarkupText(0, TextLength());

    UpdateStatus();
}

void EditorTextView::SetText(BFile* file, int32 offset, size_t size) {
    ClearHighlights();

    BTextView::SetText(file, offset, size);
    MarkupText(offset, TextLength());

    UpdateStatus();
}

// hook methods
void EditorTextView::DeleteText(int32 start, int32 finish) {
    ClearHighlights();  // TODO: only clear highlights in range

    BTextView::DeleteText(start, finish);
    MarkupText(start, finish);

    UpdateStatus();
}

void EditorTextView::InsertText(const char* text, int32 length, int32 offset,
                                const text_run_array* runs)
{
    BTextView::InsertText(text, length, offset, runs);
    MarkupText(offset, offset + length);
    UpdateStatus();
}

void EditorTextView::KeyDown(const char* bytes, int32 numBytes) {
    BTextView::KeyDown(bytes, numBytes);

    UpdateStatus();
}

void EditorTextView::MouseDown(BPoint where) {
    if (TextLength() == 0) return;

    BPoint absoluteLoc;
    uint32 buttons;
    GetMouse(&absoluteLoc, &buttons);

    if (buttons & B_PRIMARY_MOUSE_BUTTON) {
        BTextView::MouseDown(where);
        UpdateStatus();
        int32 offset = OffsetAt(where);

        if ((modifiers() & B_COMMAND_KEY) != 0) {
            // highlight block
            int32 begin, end;
            fMarkdownParser->GetMarkupBoundariesAt(offset, &begin, &end, BLOCK, BOTH);
            if (begin >= 0 && end > 0) {
                printf("selecting text from %d - %d\n", begin, end);
                Highlight(begin, end, NULL, &linkColor, false, true);
            } else {
                printf("got no boundaries for offset %d!\n", offset);
            }
        } else {
            auto data = fMarkdownParser->GetMarkupStackAt(offset);
            BString stack;
            for (auto item : *data) {
                stack << "@" << item->offset << ": " << MarkdownParser::GetMarkupClassName(item->markup_class)
                      << MarkdownParser::GetMarkupItemName(item) << "]";
                if (item != *data->end())
                    stack << " | ";
            }
            printf("markup stack at offset %d (%zu items): %s\n", offset, data->size(), stack.String());
        }
    } else if (buttons & B_SECONDARY_MOUSE_BUTTON) {
        // show context menu
        int32 startSelection, endSelection;
        GetSelection(&startSelection, &endSelection);

        BPopUpMenu *fContextMenu = new BPopUpMenu("editorContextMenu", false, false);

        BMenu *contextMenu;
        uint32 msgCode;

        if (startSelection != endSelection) {   // add label to selection
            contextMenu = fContextMenu;         // no further menu level necessary
            msgCode = MSG_ADD_HIGHLIGHT;
        } else {
            contextMenu = new BMenu("Insert");  // insert a new label
            fContextMenu->AddItem(contextMenu);
            msgCode = MSG_INSERT_ENTITY;
        }
        // TODO: make this dynamically configurable and change to proper MIME types later
        BMenuItem *contextItem = new BMenuItem("Person", MessageUtil::CreateBMessage(
            new BMessage(msgCode), "label", "Person"), '1');
        contextMenu->AddItem(contextItem);

        contextItem = new BMenuItem("Location", MessageUtil::CreateBMessage(
            new BMessage(msgCode), "label", "Location"), '2');
        contextMenu->AddItem(contextItem);

        contextItem = new BMenuItem("Date", MessageUtil::CreateBMessage(
            new BMessage(msgCode), "label", "Date"), '3');
        contextMenu->AddItem(contextItem);

        contextMenu->AddSeparatorItem();    // now come the meta labels

        contextItem = new BMenuItem("Category", MessageUtil::CreateBMessage(
            new BMessage(msgCode), "label", "Category"), '4');
        contextMenu->AddItem(contextItem);

        contextItem = new BMenuItem("Topic", MessageUtil::CreateBMessage(
            new BMessage(msgCode), "label", "Topic"), '5');
        contextMenu->AddItem(contextItem);

        contextItem = new BMenuItem("Tag", MessageUtil::CreateBMessage(
            new BMessage(msgCode), "label", "Tag"), '6');
        contextMenu->AddItem(contextItem);

        contextMenu->SetTargetForItems(fEditorHandler);

        ConvertToScreen(&where);
        fContextMenu->Go(where, true, false, true);
    }
}

void EditorTextView::MouseMoved(BPoint where, uint32 code, const BMessage* dragMessage) {
    BTextView::MouseMoved(where, code, dragMessage);
}

void EditorTextView::Draw(BRect updateRect) {
    BTextView::Draw(updateRect);

    // redraw text highlights if any inside updateRect
	int32 start, end;
	GetSelection(&start, &end);
	Select(start, start);

	BTextView::Draw(updateRect);
	Select(start, end);

    // TODO: optimize later via smart map lookup
    // int32 updateOffset = OffsetAt(updateRect.LeftTop());
    // auto highlight = fTextHighlights->find(updateOffset); //todo: get nearest offsets to updateRect boundaries

    for (auto highlight : *fTextHighlights) {
        auto textHighlight = highlight.second;
        if (textHighlight->region->Intersects(updateRect)) {
            RedrawHighlight(textHighlight);
        }
    }
}

void EditorTextView::HighlightSelection(const rgb_color *fgColor, const rgb_color *bgColor, bool generated, bool outline) {
    int32 startSelection, endSelection;
    GetSelection(&startSelection, &endSelection);
    if (startSelection == endSelection) {
        printf("nothing to highlight.\n");
        return;
    }
    Highlight(startSelection, endSelection, fgColor, bgColor, generated, outline);
}

void
EditorTextView::Highlight(int32 startOffset, int32 endOffset,
                          const rgb_color *fgColor, const rgb_color *bgColor,
                          bool generated, bool outline)
{
	// pin offsets at reasonable values
	if (startOffset < 0)
		startOffset = 0;
	else if (startOffset > TextLength())
		startOffset = TextLength() - 1;
	if (endOffset < 0)
		endOffset = 0;
	else if (endOffset > TextLength())
		endOffset = TextLength() - 1;

	if (startOffset >= endOffset)
		return;

    printf("Highlight: from %d - %d\n", startOffset, endOffset);

	BRegion selRegion;
	GetTextRegion(startOffset, endOffset, &selRegion);

    text_highlight *highlight;
    auto savedHighlight = fTextHighlights->find(startOffset);

    // add to saved highlights if not already there
    if (savedHighlight == fTextHighlights->end()) {
        printf("Highlight: store new highlight in map...\n");
        highlight = new text_highlight;
        fTextHighlights->insert({startOffset, highlight});
    } else {
        // update existing highlight with new values (we don't support overlapping highlights as an efficiency tradeoff)
        printf("Highlight: update existing highlight in map...\n");
        highlight = savedHighlight->second;
        delete highlight->region;
        delete highlight->fgColor;
        delete highlight->bgColor;
    }

    highlight->startOffset = startOffset;
    highlight->endOffset   = endOffset;
    highlight->region      = new BRegion(selRegion);

    rgb_color hiCol = HighColor();
    rgb_color loCol = LowColor();

    rgb_color highlightFgColor = (fgColor != NULL ? *fgColor : hiCol);
    rgb_color highlightBgColor = (bgColor != NULL ? *bgColor : loCol);

    highlight->fgColor     = new rgb_color(highlightFgColor);
    highlight->bgColor     = new rgb_color(highlightBgColor);

    highlight->generated   = generated;
    highlight->outline     = outline;

    Invalidate(new BRegion(selRegion));
}

void EditorTextView::RedrawHighlight(text_highlight* highlight)
{
    const rgb_color *fgColor = highlight->fgColor;
    const rgb_color *bgColor = highlight->bgColor;
    BRegion *region = highlight->region;

    const rgb_color oldHi = HighColor();
    const rgb_color oldLo = LowColor();

    SetHighColor(*fgColor);
    SetLowColor(*bgColor);
	SetDrawingMode(B_OP_BLEND);

    if (highlight->outline) {
        BRegion* region = highlight->region;
        int32 regionRects = region->CountRects();
        BPoint points[regionRects * 4];

        for (int32 rectNum = 0; rectNum < regionRects; rectNum++) {
            int32 rectPoint = 0;
            BRect rect = region->RectAt(rectNum);
            points[rectPoint++] = rect.LeftTop();
            points[rectPoint++] = rect.RightTop();
            points[rectPoint++] = rect.RightBottom();
            points[rectPoint++] = rect.LeftBottom();
        }
        BPolygon poly(points, regionRects * 4);
        StrokePolygon(&poly);
    }
    else if (highlight->generated) {    // does not make sense to set both outline and generated to true, different use
        BGradientLinear gradient;

        gradient.SetStart(BPoint(0.0, 0.0));
        gradient.SetEnd(region->RectAt(region->CountRects()-1).RightBottom());
        gradient.AddColor(*fgColor, 0.0);
        gradient.AddColor(*bgColor, 255.0);

        FillRegion(region, gradient);
    } else {
        FillRegion(region, B_SOLID_LOW);
    }
    SetHighColor(oldHi);
    SetLowColor(oldLo);
}

void EditorTextView::ClearHighlights() {
    for (auto highlight : *fTextHighlights) {
        highlight.second = NULL;
    }
    fTextHighlights->clear();
    Invalidate(Bounds());
}

void EditorTextView::UpdateStatus() {
    int32 start, end, line;
    line = CurrentLine();
    GetSelection(&start, &end);

    fStatusBar->UpdatePosition(end, CurrentLine(), end - OffsetAt(line));
    fStatusBar->UpdateSelection(start, end);

    // update outline in status from block / span info contained in text info stack
    BMessage* outline = GetOutlineAt(start, true);
    fStatusBar->UpdateOutline(outline);
}

// interaction with MarkupStyler - should become its own class later
void EditorTextView::MarkupText(const char* markdown) {
    if (TextLength() == 0 || markdown == null || strlen(markdown) == 0) ) {
        return;
    }

    // Parse full text
    if (!fMarkdownParser->ParseMarkdown(markdown)) {
        // Handle error
        return;
    }

    // TODO: handle render text WITH or WITHOUT markdown syntax!
    //       for styled edit without markup, set the plain text in BTextView
    //SetText(parser.GetPlainText().String());

    // Apply all styling
    const std::vector<TextRun>& runs = fMarkdownParser->GetTextRuns();
    for (const TextRun& run : runs) {
        // Use BTextView's text_run_array to apply styling
        SetFontAndColor(run.offset, run.offset + run.length,
                       &run.font, B_FONT_ALL,
                       &run.color);

        // Apply syntax highlighting for code blocks
        if (run.type == TextRun::CODE_BLOCK && !run.language.IsEmpty()) {
            HighlightCodeBlock(run.offset, run.length, run.language.String());
        }
    }

    // Store outline for navigation
    fOutline = fMarkdownParser->GetOutline();
}

void EditorTextView::MarkupRange(const char* markdown, int startLine, int endLine)
{
    // Parse just the updated range
    if (!fMarkdownParser->ParseIncrementalUpdate(startLine, endLine, markdown)) {
        // Fall back to full update
        SetMarkdownText(markdown);
        return;
    }

    // Get the text offset for the start line
    int32 startOffset = fMarkdownParser->GetTextOffsetForLine(startLine);
    int32 endOffset = fMarkdownParser->GetTextOffsetForLine(endLine + 1);

    if (endOffset == 0) {
        endOffset = TextLength();
    }

    // Delete old text range
    Delete(startOffset, endOffset);

    // Insert new styled text
    Insert(startOffset, fMarkdownParser->GetPlainText().String(), fMarkdownParser->GetPlainText().Length());

    // Re-apply styles to the updated range
    const std::vector<TextRun>& runs = fMarkdownParser->GetTextRuns();
    for (const TextRun& run : runs) {
        if (run.offset >= startOffset) {
            SetFontAndColor(run.offset, run.offset + run.length,
                           &run.font, B_FONT_ALL,
                           &run.color);
        }
    }
}

// Example: Syntax highlighting integration point
void EditorTextView::HighlightCodeBlock(int32 offset, int32 length, const char* language)
{
    // This is where you'd plug in your syntax highlighter
    // The parser gives you the code block location and language

    if (fSyntaxHighlighter) {
        BString code;
        GetText(offset, length, code.LockBuffer(length));
        code.UnlockBuffer();

        std::vector<SyntaxToken> tokens = fSyntaxHighlighter->Tokenize(code.String(), language);

        for (const SyntaxToken& token : tokens) {
            rgb_color color = GetColorForTokenType(token.type);
            SetFontAndColor(offset + token.offset,
                           offset + token.offset + token.length,
                           nullptr, 0, &color);
        }
    }
}

// Helper to apply a run
void EditorTextView::ApplyTextRun(const TextRun& run)
{
    SetFontAndColor(run.offset, run.offset + run.length,
                   &run.font, B_FONT_ALL, &run.color);

    // Special handling for code blocks with syntax highlighting
    if (run.type == TextRun::CODE_BLOCK && !run.language.IsEmpty()) {
        HighlightCodeBlock(run.offset, run.length, run.language.String());
    }
}

void EditorTextView::ShowOutlineMenu()
{
    BPoint point = fTextView->PointAt(fTextView->SelectionStart());
    fTextView->ConvertToScreen(&point);

    BPopUpMenu* menu = new BPopUpMenu("outline");
    BuildOutlineMenu(&fOutline, menu);

    BMenuItem* selected = menu->Go(point, false, true);
    if (selected && selected->Message()) {
        int32 offset;
        if (selected->Message()->FindInt32("offset", &offset) == B_OK) {
            fTextView->Select(offset, offset);
            fTextView->ScrollToSelection();
        }
    }

    delete menu;
}

void EditorTextView::BuildOutlineMenu(BMessage* outline, BMenu* menu)
{
    int32 index = 0;
    BMessage child;

    while (outline->FindMessage("children", index, &child) == B_OK) {
        BString text;
        int32 offset;

        child.FindString("text", &text);
        child.FindInt32("offset", &offset);

        BMessage* msg = new BMessage('GOTO');
        msg->AddInt32("offset", offset);

        // Check for nested children
        int32 childCount = 0;
        type_code type;
        child.GetInfo("children", &type, &childCount);

        if (childCount > 0) {
            BMenu* submenu = new BMenu(text.String());
            BuildOutlineMenu(&child, submenu);
            menu->AddItem(submenu);
        } else {
            menu->AddItem(new BMenuItem(text.String(), msg));
        }

        index++;
    }
}

void PrintOutline(BMessage* outline, int indent = 0)
{
    int32 index = 0;
    BMessage child;

    while (outline->FindMessage("children", index, &child) == B_OK) {
        BString text;
        int32 level, line, offset;

        child.FindString("text", &text);
        child.FindInt32("level", &level);
        child.FindInt32("line", &line);
        child.FindInt32("offset", &offset);

        // Print with indentation
        for (int i = 0; i < indent; i++) {
            printf("  ");
        }
        printf("H%d: %s (line %d, offset %d)\n", level, text.String(), line, offset);

        // Recursively print children
        PrintOutline(&child, indent + 1);

        index++;
    }
}

// Get all siblings at current level
void GetSiblings(BMessage* parent, int32 targetLevel, std::vector<BString>& siblings)
{
    int32 index = 0;
    BMessage child;

    while (parent->FindMessage("children", index, &child) == B_OK) {
        int32 level;
        BString text;

        if (child.FindInt32("level", &level) == B_OK && level == targetLevel) {
            child.FindString("text", &text);
            siblings.push_back(text);
        }

        index++;
    }
}

// Navigate to next sibling at same level
BMessage* FindNextSibling(BMessage* outline, int32 currentOffset, int32 currentLevel)
{
    int32 index = 0;
    BMessage child;
    bool foundCurrent = false;

    while (outline->FindMessage("children", index, &child) == B_OK) {
        int32 offset, level;
        child.FindInt32("offset", &offset);
        child.FindInt32("level", &level);

        if (foundCurrent && level == currentLevel) {
            return &child; // Next sibling found
        }

        if (offset == currentOffset) {
            foundCurrent = true;
        }

        // Check nested children
        if (!foundCurrent) {
            BMessage* nested = FindNextSibling(&child, currentOffset, currentLevel);
            if (nested) return nested;
        }

        index++;
    }

    return nullptr;
}

// Build breadcrumb trail: Chapter 1 > Section 1.2 > Subsection 1.2.1
void BuildBreadcrumbs(BMessage* outline, int32 targetOffset, std::vector<BString>& crumbs)
{
    int32 index = 0;
    BMessage child;

    while (outline->FindMessage("children", index, &child) == B_OK) {
        int32 offset;
        BString text;

        child.FindInt32("offset", &offset);
        child.FindString("text", &text);

        // Check if target is within this heading's range
        // (would need to track end offsets in real implementation)
        if (offset <= targetOffset) {
            crumbs.push_back(text);

            // Recursively search children
            BuildBreadcrumbs(&child, targetOffset, crumbs);
            return;
        }

        index++;
    }
}


// utility functions
void EditorTextView::BuildContextMenu() {
}

void EditorTextView::BuildContextSelectionMenu() {
}

