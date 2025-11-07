/*
 * Copyright 2024-2025, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#pragma once

#include <GroupView.h>
#include <ScrollView.h>
#include <SupportDefs.h>

#include "../common/ColorDefs.h"
#include "EditorTextView.h"
#include "StatusBar.h"

class EditorView : public BView {

public:
                    EditorView(BHandler* parent);
    virtual         ~EditorView();
    virtual void    MessageReceived(BMessage* message);

    void            SetText(BFile *file, size_t size);
    void            SetText(const char* text);

private:
    BHandler*       fParentHandler;
    EditorTextView* fTextView;
    BScrollView*	fScrollView;
    StatusBar*      fStatusBar;
    ColorDefs*      fColorDefs;
};
