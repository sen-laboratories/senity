/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#pragma once

#include <SupportDefs.h>
#include <TextView.h>

#include "MarkdownStyler.h"
#include "StatusBar.h"

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
    void            MarkupText(int32 start = 0, int32 end = -1);
    void            StyleText(text_data* markupInfo);
    void            CalcStyle(BMessage* outline, BFont *font, rgb_color *color);
    void            GetBlockStyle(MD_BLOCKTYPE blockType, BMessage *detail, BFont *font, rgb_color *color);
    void            GetSpanStyle(MD_SPANTYPE spanType, BMessage *detail, BFont *font, rgb_color *color);
    void            ClearTextInfo(int32 start, int32 end);
    BMessage*       GetOutlineAt(int32 offset, bool withNames = false);
    markup_stack*   GetTextStackFrom(int32 offset, int32* blockEnd = NULL);
    markup_stack*   GetTextStackTo(int32 offset, int32* blockStart = NULL);
    markup_stack*   GetTextStackForBlockAt(int32 offset, int32* blockStart = NULL, int32* blockEnd = NULL);

    void            UpdateStatus();

    BMessenger*     fMessenger;
    StatusBar*      fStatusBar;
    MarkdownStyler* fMarkdownStyler;
    BFont*          fLinkFont;
    BFont*          fCodeFont;
    text_info*      fTextInfo;
};
