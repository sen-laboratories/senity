/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include <LayoutBuilder.h>
#include <string>

#include "StatusBar.h"

StatusBar::StatusBar() : BView("status_bar", B_HORIZONTAL | B_WILL_DRAW | B_PULSE_NEEDED) {
    fLine = new BTextControl("line", "-", new BMessage('line'));
    fColumn = new BTextControl("column", "-", new BMessage('clmn'));
    fOffset = new BTextControl("offset", "-", new BMessage('offs'));
    fSelection = new BTextControl("selection", "-", new BMessage('slct'));
    fSelection->SetEnabled(false);
    fSelection->SetExplicitMinSize(BSize(64.0, be_plain_font->Size()));

    SetLayout(new BGroupLayout(B_HORIZONTAL, 0));
	BLayoutBuilder::Group<>((BGroupLayout*)GetLayout())
		.Add(fLine)
        .Add(fColumn)
        .Add(fOffset)
        .Add(fSelection)
        .AddGlue(3.0)
		.End();

    UpdatePosition(0, 1, 0);
    UpdateSelection(0, 0);
}

StatusBar::~StatusBar() {
    delete fLine;
    delete fColumn;
    delete fOffset;
    delete fSelection;
}

void StatusBar::UpdatePosition(int32 offset, int32 line, int32 column) {
    fOffset ->SetText(std::to_string(offset).c_str());
    fLine   ->SetText(std::to_string(line  ).c_str());
    fColumn ->SetText(std::to_string(column).c_str());
}

void StatusBar::UpdateSelection(int32 selectionStart, int32 selectionEnd) {
    BString selection;
    if (selectionStart != selectionEnd) {
        selection << selectionStart << " - " << selectionEnd;
    } else {
        selection << "-";
    }
    selection << "(" << (selectionEnd - selectionStart) << " chars)";

    fSelection->SetText(selection.String());
}


