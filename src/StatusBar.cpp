/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include <LayoutBuilder.h>
#include <string>
#include <SeparatorView.h>

#include "StatusBar.h"

StatusBar::StatusBar() : BView("status_bar", 0) {
    fLine = new BTextControl("line", "-", new BMessage('line'));
    fColumn = new BTextControl("column", "-", new BMessage('clmn'));
    fOffset = new BTextControl("offset", "-", new BMessage('offs'));
    fSelection = new BTextControl("selection", "-", new BMessage('slct'));
    fSelection->SetEnabled(false);
    fSelection->SetExplicitMinSize(BSize(64.0, be_plain_font->Size()));

    fOutline = new BTextControl("Outline", "-", new BMessage('Tout'));
    fOutline->SetEnabled(false);
    fOutline->SetExplicitMinSize(BSize(180.0, be_plain_font->Size()));

	BLayoutBuilder::Group<>(this, B_HORIZONTAL, 0)
		.Add(fLine)
        .Add(fColumn)
        .Add(fOffset)
        .Add(fSelection)
        .Add(fOutline)
        .AddGlue(1.0);

    UpdatePosition(0, 1, 0);
    UpdateSelection(0, 0);
}

StatusBar::~StatusBar() {
    delete fLine;
    delete fColumn;
    delete fOffset;
    delete fSelection;
    delete fOutline;
}

void StatusBar::UpdatePosition(int32 offset, int32 line, int32 column) {
    fOffset ->SetText(std::to_string(offset).c_str());
    fLine   ->SetText(std::to_string(line  ).c_str());
    fColumn ->SetText(std::to_string(column).c_str());
}

void StatusBar::UpdateSelection(int32 selectionStart, int32 selectionEnd) {
    BString selection;
    if (selectionStart != selectionEnd) {
        selection << selectionStart << " - " << selectionEnd
                  << " (" << (selectionEnd - selectionStart) << " chars)";
    } else {
        selection << "-";
    }
    fSelection->SetText(selection.String());
}

void StatusBar::UpdateOutline(const BMessage* outlineItems) {
    if (outlineItems == NULL || outlineItems->IsEmpty()) {
    fOutline->SetText("");
        return;
    }
    BString outline;
  /*  for (int i = 0; i < outlineItems->CountStrings(); i++) {
        if (i > 0) {
            outline << " > ";
        }
        outline << outlineItems->StringAt(i);
    }
    fOutline->SetText(outline.String());
    */
}

