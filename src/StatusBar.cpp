/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include <LayoutBuilder.h>
#include <string>

#include "StatusBar.h"

StatusBar::StatusBar() : BView("status_bar", B_HORIZONTAL | B_WILL_DRAW | B_PULSE_NEEDED) {
    fLine = new BTextControl("line`", "-", new BMessage('line'));
    fColumn = new BTextControl("column", "-", new BMessage('clmn'));
    fOffset = new BTextControl("offset", "-", new BMessage('offs'));
    fSelection = new BTextControl("selection", "-", new BMessage('slct'));

    SetLayout(new BGridLayout(B_HORIZONTAL, 0));
	BLayoutBuilder::Grid<>((BGridLayout*)GetLayout())
		.Add(fLine, 0, 0)
        .Add(fColumn, 1, 0)
        .Add(fOffset, 2, 0)
        .Add(fSelection, 3,0)
		.End();
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
    selection << selectionStart << " - " << selectionEnd
              << "(" << (selectionEnd - selectionStart) << ")";
    fOffset->SetText(selection.String());
}


