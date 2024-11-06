/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#pragma once

#include <GridView.h>
#include <SupportDefs.h>
#include <TextControl.h>

class StatusBar : public BView {

public:
                  StatusBar();
    virtual      ~StatusBar();
    void          UpdatePosition(int32 offset, int32 line, int32 column);
    void          UpdateSelection(int32 selectionStart, int32 selectionEnd);

private:
    BTextControl *fLine;
    BTextControl *fColumn;
    BTextControl *fOffset;
    BTextControl *fSelection;
};
