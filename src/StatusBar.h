/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#pragma once

#include <GroupView.h>
#include <SupportDefs.h>
#include <TextControl.h>

#define OUTLINE_SEPARATOR "\xE2\x86\x92"

class StatusBar : public BView {

public:
                  StatusBar();
    virtual      ~StatusBar();
    void          UpdatePosition(int32 offset, int32 line, int32 column);
    void          UpdateSelection(int32 selectionStart, int32 selectionEnd);
    void          UpdateOutline(const BMessage* outlineItems);

private:
    BTextControl *fLine;
    BTextControl *fColumn;
    BTextControl *fOffset;
    BTextControl *fSelection;
    // detail info on text outline from markup parser
    BTextControl *fOutline;
};
