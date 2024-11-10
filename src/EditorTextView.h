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

public:
                    EditorTextView(BRect viewFrame, BRect textBounds, StatusBar *statusView, BHandler *editorHandler);
    virtual         ~EditorTextView();
    virtual void    SetText(BFile *file, size_t size);

	virtual	void    DeleteText(int32 start, int32 finish);
	virtual	void    InsertText(const char* text, int32 length, int32 offset,
                               const text_run_array* runs = NULL);

    virtual void    KeyDown(const char* bytes, int32 numBytes);

private:
    BMessenger      *fMessenger;
    StatusBar       *fStatusBar;
    MarkdownStyler  *fMarkdownStyler;
    void            MarkupText(int32 start = 0, int32 end = -1);
    void            UpdateStatus();
    BFont*          fLinkFont;
    BFont*          fCodeFont;

};
