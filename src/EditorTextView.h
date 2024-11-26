/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#pragma once

#include <SupportDefs.h>
#include <TextView.h>

#include "MarkdownParser.h"
#include "StatusBar.h"

const rgb_color linkColor = ui_color(B_LINK_TEXT_COLOR);
const rgb_color codeColor = ui_color(B_SHADOW_COLOR);
const rgb_color textColor = ui_color(B_DOCUMENT_TEXT_COLOR);

class EditorTextView : public BTextView {

//todo move to separate file later
enum message_codes {
    TEXTVIEW_POSITION_UPDATED = 'Tpos'
};

#define TEXTVIEW_POSITION_UPDATED_OFFSET = "offset";

public:
                    EditorTextView(BRect viewFrame, BRect textBounds, StatusBar *statusView, BHandler *editorHandler);
    virtual         ~EditorTextView();

    virtual void    AttachedToWindow();

    virtual void    SetText(BFile *file, int32 offset, size_t size);
    virtual void    SetText(const char* text, const text_run_array* runs = NULL);

	virtual	void    DeleteText(int32 start, int32 finish);
	virtual	void    InsertText(const char* text, int32 length, int32 offset,
                               const text_run_array* runs = NULL);

    virtual void    KeyDown(const char* bytes, int32 numBytes);
    virtual	void	MouseDown(BPoint where);
    virtual	void    MouseMoved(BPoint where, uint32 code,
                               const BMessage* dragMessage);

private:
    void            MarkupText(int32 start, int32 end);
    void            StyleText(text_data* markupInfo, BFont *font, rgb_color *color);
    void            SetBlockStyle(MD_BLOCKTYPE blockType, BMessage* detail, BFont* font, rgb_color* color);
    void            SetSpanStyle(MD_SPANTYPE spanType, BMessage* detail, BFont* font, rgb_color* color);
    void            SetTextStyle(MD_TEXTTYPE textType, BFont *font, rgb_color *color);
    BMessage*       GetOutlineAt(int32 offset, bool withNames = false);
    void            UpdateStatus();

    BMessenger*     fMessenger;
    StatusBar*      fStatusBar;
    MarkdownParser* fMarkdownParser;
    BFont*          fLinkFont;
    BFont*          fCodeFont;
};
