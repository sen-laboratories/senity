/*
 * Copyright 2024-2025, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

#include <Cursor.h>
#include <PopUpMenu.h>
#include <stack>
#include <SupportDefs.h>
#include <TextView.h>

#include "MarkdownParser.h"
#include "StatusBar.h"
#include "SyntaxHighlighter.h"

const rgb_color linkColor   = ui_color(B_LINK_TEXT_COLOR);
const rgb_color codeColor   = {80, 80, 80, 255};  // Dark gray
const rgb_color textColor   = ui_color(B_DOCUMENT_TEXT_COLOR);
const rgb_color backgroundColor = ui_color(B_DOCUMENT_BACKGROUND_COLOR);
const rgb_color headerColor = ui_color(B_CONTROL_HIGHLIGHT_COLOR);  // todo: use tinting

const BCursor linkCursor        = BCursor(B_CURSOR_ID_FOLLOW_LINK);
const BCursor contextMenuCursor = BCursor(B_CURSOR_ID_CONTEXT_MENU);

class EditorTextView : public BTextView {

typedef struct text_highlight {
    int32           startOffset;
    int32           endOffset;
    bool            generated = false;
    bool            outline = false;
    BRegion         *region;
    const rgb_color *fgColor;
    const rgb_color *bgColor;
} text_highlight;

#define TEXTVIEW_OFFSET = "offset";

public:
                    EditorTextView(StatusBar *statusView, BHandler *editorHandler);
    virtual         ~EditorTextView();

    virtual void    Draw(BRect updateRect);

    virtual void    SetText(BFile *file, int32 offset, size_t size);
    virtual void    SetText(const char* text, const text_run_array* runs = NULL);

	virtual	void    DeleteText(int32 start, int32 finish);
	virtual	void    InsertText(const char* text, int32 length, int32 offset,
                               const text_run_array* runs = NULL);

    virtual	void    MessageReceived(BMessage* message);

    virtual void    KeyDown(const char* bytes, int32 numBytes);
    virtual	void	MouseDown(BPoint where);
    virtual	void    MouseMoved(BPoint where, uint32 code,
                               const BMessage* dragMessage);

    // highlighting/labelling
    void            HighlightSelection(const rgb_color *fgColor = NULL, const rgb_color *bgColor = NULL,
                                       bool generated = false, bool outline = false);
    void            Highlight(int32 startOffset, int32 endOffset,
                              const rgb_color *fgColor = NULL, const rgb_color *bgColor = NULL,
                              bool generated = false, bool outline = false);
    void            ClearHighlights();

private:
    void            AdjustHighlightsForInsert(int32, int32);
    void            AdjustHighlightsForDelete(int32, int32);

    void            MarkupText(const char* text);

    BMessage*       GetOutlineAt(int32 offset, bool withNames = false);
    BMessage*       GetDocumentOutline(bool withNames = false, bool withDetails = false);

    void            UpdateStatus();
    void            RedrawHighlight(text_highlight *highlight);

    void            BuildContextMenu();
    void            BuildContextSelectionMenu();

    BHandler*       fEditorHandler;
    StatusBar*      fStatusBar;
    MarkdownParser* fMarkdownParser;
    BFont*          fTextFont;
    BFont*          fLinkFont;
    BFont*          fCodeFont;

    std::map<int32, text_highlight*>    *fTextHighlights;
    SyntaxHighlighter                   *fSyntaxHighlighter;
};
