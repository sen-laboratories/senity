/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#pragma once

#include <StringList.h>
#include <GroupView.h>
#include <SupportDefs.h>
#include <TextControl.h>

class StatusBar : public BView {

public:
                  StatusBar();
    virtual      ~StatusBar();
    void          UpdatePosition(int32 offset, int32 line, int32 column);
    void          UpdateSelection(int32 selectionStart, int32 selectionEnd);
    void          UpdateOutline(const BStringList* outlineItems);

private:
    BTextControl *fLine;
    BTextControl *fColumn;
    BTextControl *fOffset;
    BTextControl *fSelection;
    // detail info on text outline from markup parser
    BTextControl *fOutline;
};
