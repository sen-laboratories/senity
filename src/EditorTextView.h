/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#pragma once

#include <PopUpMenu.h>
#include <stack>
#include <SupportDefs.h>
#include <TextView.h>

#include "MarkdownParser.h"
#include "StatusBar.h"

const rgb_color linkColor   = ui_color(B_LINK_TEXT_COLOR);
const rgb_color codeColor   = ui_color(B_SHADOW_COLOR);
const rgb_color textColor   = ui_color(B_DOCUMENT_TEXT_COLOR);
const rgb_color headerColor = ui_color(B_CONTROL_HIGHLIGHT_COLOR);  // todo: use tinting

class EditorTextView : public BTextView {

typedef struct text_highlight {
    int32 startOffset;
    int32 endOffset;
    BRegion          *region;
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
    void            Highlight(int32 startOffset, int32 endOffset, const rgb_color *fgColor = NULL, const rgb_color *bgColor = NULL);

private:
    void            MarkupText(int32 start, int32 end);
    void            StyleText(text_data* markupInfo,
                              stack<text_run> *styleStack,
                              BFont* font, rgb_color* color);
    void            SetBlockStyle(text_data* markupInfo, BFont* font, rgb_color* color);
    void            SetSpanStyle(text_data* markupInfo, BFont* font, rgb_color* color);
    void            SetTextStyle(text_data* markupInfo, BFont *font, rgb_color *color);

    BMessage*       GetOutlineAt(int32 offset, bool withNames = false);
    BMessage*       GetDocumentOutline(bool withNames = false, bool withDetails = false);
    void            UpdateStatus();

    void            BuildContextMenu();
    void            BuildContextSelectionMenu();

    BHandler*       fEditorHandler;
    StatusBar*      fStatusBar;
    MarkdownParser* fMarkdownParser;
    BFont*          fTextFont;
    BFont*          fLinkFont;
    BFont*          fCodeFont;

    map<int32, text_highlight*> *fTextHighlights;
};
