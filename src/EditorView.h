/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#pragma once

#include <GroupView.h>
#include <ScrollView.h>
#include <SupportDefs.h>
#include <TextView.h>

#include "MarkdownStyler.h"
#include "StatusBar.h"

class EditorView : public BView {

public:
                EditorView();
    virtual     ~EditorView();
    void        SetText(BFile *file, size_t size);

private:
    MarkdownStyler* fMarkdownStyler;

    void            MarkupText(int32 start = 0, int32 end = -1);

    BTextView*      fTextView;
    BScrollView*	fScrollView;
    StatusBar*      fStatusBar;
};
